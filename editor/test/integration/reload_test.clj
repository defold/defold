(ns integration.reload-test
  (:require [clojure.test :refer :all]
            [clojure.string :as str]
            [clojure.set :as set]
            [clojure.java.io :as io]
            [dynamo.graph :as g]
            [support.test-support :as test-support :refer [undo-stack write-until-new-mtime spit-until-new-mtime touch-until-new-mtime]]
            [editor.math :as math]
            [editor.defold-project :as project]
            [editor.fs :as fs]
            [editor.library :as library]
            [editor.dialogs :as dialogs]
            [editor.game-project :as game-project]
            [editor.game-object :as game-object]
            [editor.asset-browser :as asset-browser]
            [editor.progress :as progress]
            [editor.protobuf :as protobuf]
            [editor.atlas :as atlas]
            [editor.resource :as resource]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [service.log :as log])
  (:import [com.dynamo.gameobject.proto GameObject GameObject$CollectionDesc GameObject$CollectionInstanceDesc GameObject$InstanceDesc
            GameObject$EmbeddedInstanceDesc GameObject$PrototypeDesc]
           [com.dynamo.textureset.proto TextureSetProto$TextureSet]
           [com.dynamo.render.proto Font$FontMap]
           [com.dynamo.particle.proto Particle$ParticleFX]
           [com.dynamo.sound.proto Sound$SoundDesc]
           [com.dynamo.rig.proto Rig$RigScene]
           [editor.types Region]
           [editor.workspace BuildResource]
           [editor.resource FileResource]
           [java.awt.image BufferedImage]
           [java.io File]
           [javax.imageio ImageIO]
           [javax.vecmath Point3d Matrix4d]
           [org.apache.commons.io FilenameUtils]))

(def ^:private reload-project-path "test/resources/reload_project")

;; reload_project tree
;; .
;; ├── atlas
;; │   ├── ball.atlas
;; │   ├── empty.atlas
;; │   ├── pow.atlas
;; │   ├── powball.atlas
;; │   └── single.atlas
;; ├── collection
;; │   ├── props.collection
;; │   └── sub_defaults.collection
;; ├── game.project
;; ├── game_object
;; │   └── props.go
;; ├── graphics
;; │   ├── ball.png
;; │   └── pow.png
;; ├── gui
;; │   ├── new_sub_scene.gui
;; │   ├── scene.gui
;; │   └── sub_scene.gui
;; ├── input
;; │   └── game.input_binding
;; ├── label
;; │   ├── label.label
;; │   └── new_label.label
;; ├── main
;; │   ├── main.collection
;; │   ├── main.go
;; │   └── main.script
;; ├── script
;; │   └── props.script
;; ├── sprite
;; │   └── test.sprite
;; └── test.particlefx

(def ^:private lib-urls (library/parse-library-urls "file:/scriptlib file:/imagelib1 file:/imagelib2"))

(def ^:private scriptlib-url (first lib-urls)) ; /scripts/main.script
(def ^:private imagelib1-url (second lib-urls)) ; /images/{pow,paddle}.png

;; Temporary hack to run tests in both implementations of the code editor resource nodes.
(defmacro with-clean-system [& forms]
  `(do
     (with-bindings {#'test-util/use-new-code-editor? false}
       (test-support/with-clean-system
         ~@forms))
     (with-bindings {#'test-util/use-new-code-editor? true}
       (test-support/with-clean-system
         ~@forms))))

(defn- setup-scratch
  ([ws-graph] (setup-scratch ws-graph reload-project-path))
  ([ws-graph project-path]
   (let [workspace (test-util/setup-scratch-workspace! ws-graph project-path)
         project (test-util/setup-project! workspace)]
     [workspace project])))

(defn- template [workspace name]
  (let [resource (workspace/file-resource workspace name)]
    (workspace/template (resource/resource-type resource))))

(def ^:dynamic *no-sync* nil)
(def ^:dynamic *moved-files* nil)

(defn- sync!
  ([workspace]
    (when (not *no-sync*) (workspace/resource-sync! workspace)))
  ([workspace moved-files]
    (if (not *no-sync*)
      (workspace/resource-sync! workspace moved-files)
      (do
        (swap! *moved-files* into moved-files)))))

(defmacro bulk-change [workspace & forms]
 `(with-bindings {#'*no-sync* true
                  #'*moved-files* (atom [])}
    ~@forms
    (workspace/resource-sync! ~workspace @*moved-files*)))

(defn- touch-file
  ([workspace name]
   (touch-file workspace name true))
  ([workspace name sync?]
   (let [f (File. (workspace/project-path workspace) name)]
     (fs/create-parent-directories! f)
     (touch-until-new-mtime f))
   (when sync?
     (sync! workspace))))

(defn- touch-files [workspace names]
  (doseq [name names]
    (touch-file workspace name false))
  (sync! workspace))

(defn- write-file [workspace name content]
  (let [f (File. (workspace/project-path workspace) name)]
    (fs/create-parent-directories! f)
    (spit-until-new-mtime f content))
  (sync! workspace))

(defn- read-file [workspace name]
  (slurp (str (workspace/project-path workspace) name)))

(defn- add-file [workspace name]
  (write-file workspace name (template workspace name)))

(defn- delete-file [workspace name]
  (let [f (File. (workspace/project-path workspace) name)]
    (fs/delete-file! f))
  (sync! workspace))

(defn- copy-file [workspace name new-name]
  (let [[f new-f] (mapv #(File. (workspace/project-path workspace) %) [name new-name])]
    (fs/copy-file! f new-f))
  (sync! workspace))

(defn- copy-directory [workspace name new-name]
  (let [[f new-f] (mapv #(File. (workspace/project-path workspace) %) [name new-name])]
    (fs/copy-directory! f new-f))
  (sync! workspace))

(defn- move-file [workspace name new-name]
  (let [[f new-f] (mapv #(File. (workspace/project-path workspace) %) [name new-name])]
    (fs/move-file! f new-f)
    (sync! workspace [[f new-f]])))

(defn- add-img [workspace name width height]
  (let [img (BufferedImage. width height BufferedImage/TYPE_INT_ARGB)
        type (FilenameUtils/getExtension name)
        f (File. (workspace/project-path workspace) name)]
    (write-until-new-mtime (fn [f] (ImageIO/write img type f)) f)
    (sync! workspace)))

(defn- has-undo? [project]
  (g/has-undo? (g/node-id->graph-id project)))

(defn- no-undo? [project]
  (not (has-undo? project)))

(defn- graph-nodes [node-id] (set (g/node-ids (g/graph (g/node-id->graph-id node-id)))))

(deftest internal-file
  (with-clean-system
    (let [[workspace project] (setup-scratch world)
          node-count (fn [] (count (graph-nodes project)))
          initial-node-count (node-count)]
      (testing "Add internal file"
        (add-file workspace "/test.collection")
        (let [initial-node (project/get-resource-node project "/test.collection")]
          (is (= (inc initial-node-count) (node-count)))
          (is (not (nil? initial-node)))
          (is (= "default" (g/node-value initial-node :name)))
          (is (no-undo? project))
          (testing "Change internal file"
            (write-file workspace "/test.collection" "name: \"test_name\"")
            (let [changed-node (project/get-resource-node project "/test.collection")]
              (is (= (inc initial-node-count) (node-count)))
              (is (not (nil? changed-node)))
              (is (not= initial-node changed-node))
              (is (= "test_name" (g/node-value changed-node :name)))
              (is (no-undo? project))
              (testing "Delete internal file"
                (delete-file workspace "/test.collection")
                (let [node (project/get-resource-node project "/test.collection")
                      defective-node ((g/node-value project :nodes-by-resource-path) "/test.collection")]
                  (is (= (inc initial-node-count) (node-count)))
                  (is (nil? node))
                  (is (= defective-node changed-node))
                  (is (seq (g/node-value defective-node :_output-jammers)))
                  (is (no-undo? project)))))))))))

(deftest external-file
 (with-clean-system
   (let [[workspace project] (setup-scratch world)
         atlas-node-id (project/get-resource-node project "/atlas/empty.atlas")
         img-path "/test_img.png"
         anim-id (FilenameUtils/getBaseName img-path)]
     (is (not (nil? atlas-node-id)))
     (testing "Add external file, cleared history"
       (add-img workspace img-path 64 64)
       (let [initial-node (project/get-resource-node project img-path)]
         (is (some? initial-node))
         (is (no-undo? project))
         (testing "Reference it, node added and linked"
           (g/transact
             (atlas/add-images atlas-node-id [(workspace/resolve-resource (g/node-value atlas-node-id :resource) img-path)]))
           (is (has-undo? project))
           (let [undo-count (count (undo-stack (g/node-id->graph-id project)))
                 anim-data (g/node-value atlas-node-id :anim-data)
                 anim (get anim-data anim-id)]
             (is (and (= 64 (:width anim)) (= 64 (:height anim))))
             (testing "Modify image, anim data updated"
               (add-img workspace img-path 128 128)
               ;; undo count should be unchanged as this is a modification of an external (non-loadable) resource
               ;; which should only invalidate the outputs of the resource node
               (is (= undo-count (count (undo-stack (g/node-id->graph-id project)))))
               (let [changed-node (project/get-resource-node project img-path)
                     anim-data (g/node-value atlas-node-id :anim-data)
                     anim (get anim-data anim-id)]
                 (is (= initial-node changed-node))
                 (is (and (= 128 (:width anim)) (= 128 (:height anim))))
                 (testing "Delete it, errors produced"
                   (delete-file workspace img-path)
                   (let [node (project/get-resource-node project img-path)
                         invalidated-node ((g/node-value project :nodes-by-resource-path) img-path)]
                     (is (nil? node))
                     (is (= initial-node invalidated-node))
                     (is (= nil (g/node-value invalidated-node :_output-jammers)))
                     ;; as above, undo count should be unchanged - just invalidate the outputs of the resource node
                     (is (= undo-count (count (undo-stack (g/node-id->graph-id project)))))
                     ;; TODO - fix node pollution
                     (log/without-logging
                       (is (g/error? (g/node-value atlas-node-id :anim-data)))))))))))))))

(deftest save-no-reload
  (with-clean-system
    (let [[workspace project] (setup-scratch world)]
      (testing "Add internal file"
        (add-file workspace "/test.collection")
        (let [node (project/get-resource-node project "/test.collection")]
          (g/transact
            (g/set-property node :name "new_name"))
          (is (has-undo? project))
          (project/save-all! project progress/null-render-progress!)
          (sync! workspace)
          (is (has-undo? project)))))))

(defn- find-error [type v]
  (if (= type (get-in v [:user-data :type]))
    true
    (some (partial find-error type) (:causes v))))

(defn- error? [type v]
  (and (g/error? v) (find-error type v)))

(defn- no-error? [v]
  (not (g/error? v)))

(deftest external-file-errors
  (with-clean-system
    (let [[workspace project] (setup-scratch world)
          atlas-node-id (project/get-resource-node project "/atlas/single.atlas")
          img-path "/test_img.png"]
      (log/without-logging
       (is (error? :file-not-found (g/node-value atlas-node-id :anim-data))))
      (add-img workspace img-path 64 64)
      (is (contains? (g/node-value atlas-node-id :anim-data) "test_img"))
      (delete-file workspace img-path)
      (log/without-logging
       (is (error? :file-not-found (g/node-value atlas-node-id :anim-data))))
      (add-img workspace img-path 64 64)
      (is (contains? (g/node-value atlas-node-id :anim-data) "test_img"))
      (write-file workspace img-path "this is not png format")
      (is (error? :invalid-content (g/node-value atlas-node-id :anim-data))))))


(defn- first-child [parent]
  (get-in (g/node-value parent :node-outline) [:children 0 :node-id]))

(defn- raw-tile-source [node]
  (project/get-resource-node (project/get-project node) (g/node-value node :tile-source-resource)))

(deftest resource-reference-error
  (with-clean-system
    (let [[workspace project] (setup-scratch world)]
      (testing "Tile source ok before writing broken content"
        (let [pfx-node (project/get-resource-node project "/test.particlefx")
              ts-node (raw-tile-source (first-child pfx-node))]
          (is (not (g/error? (g/node-value ts-node :extrude-borders))))
          (is (= nil (g/node-value ts-node :_output-jammers)))))
      (testing "Externally modifying to refer to non existing tile source results in defective node"
        (let [test-content (read-file workspace "/test.particlefx")
              broken-content (str/replace test-content
                                          "tile_source: \"/builtins/graphics/particle_blob.tilesource\""
                                          "tile_source: \"/builtins/graphics/particle_blob_does_not_exist.tilesource\"")]
          (log/without-logging (write-file workspace "/test.particlefx" broken-content))
          (let [pfx-node (project/get-resource-node project "/test.particlefx")
                ts-node (raw-tile-source (first-child pfx-node))]
            (is (g/error? (g/node-value ts-node :extrude-borders)))
            (is (seq (keys (g/node-value ts-node :_output-jammers))))))))))

(deftest internal-file-errors
  (with-clean-system
    (let [[workspace project] (setup-scratch world)
          node-id (project/get-resource-node project "/main/main.go")
          img-path "/test_img.png"
          atlas-path "/atlas/single.atlas"]
      (log/without-logging
       (is (error? :file-not-found (g/node-value node-id :scene))))
      (add-img workspace img-path 64 64)
      (is (no-error? (g/node-value node-id :scene)))
      (copy-file workspace atlas-path "/tmp.atlas")
      (delete-file workspace atlas-path)
      (is (error? :file-not-found (g/node-value node-id :scene)))
      (copy-file workspace "/tmp.atlas" atlas-path)
      (is (no-error? (g/node-value node-id :scene)))
      (log/without-logging (write-file workspace atlas-path "test"))
      (is (error? :invalid-content (g/node-value node-id :scene)))
      (copy-file workspace "/tmp.atlas" atlas-path)
      (is (no-error? (g/node-value node-id :scene))))))

(defn- image-link [atlas]
  (let [outline (g/node-value atlas :node-outline)]
    (:link (first (:children outline)))))

(deftest refactoring
  (with-clean-system
    (let [[workspace project] (setup-scratch world)
          node-id (project/get-resource-node project "/atlas/single.atlas")
          img-path "/test_img.png"
          img-res (workspace/resolve-workspace-resource workspace img-path)
          new-img-path "/test_img2.png"
          new-img-res (workspace/resolve-workspace-resource workspace new-img-path)]
      (add-img workspace img-path 64 64)
      (is (= (image-link node-id) img-res))
      (move-file workspace img-path new-img-path)
      (is (= (image-link node-id) new-img-res))
      (move-file workspace new-img-path img-path)
      (is (= (image-link node-id) img-res)))))

(deftest move-external-removed-added
  (with-clean-system
    (let [[workspace project] (setup-scratch world)
          pow (project/get-resource-node project "/graphics/pow.png")
          initial-graph-nodes (graph-nodes project)]

      (move-file workspace "/graphics/pow.png" "/graphics/pow2.png")

      (is (nil? (project/get-resource-node project "/graphics/pow.png")))

      (let [pow2 (project/get-resource-node project "/graphics/pow2.png")]
        ;; resource node simply repointed
        (is (= pow pow2))
        ;; no nodes added or removed
        (is (= (graph-nodes project) initial-graph-nodes))))))

(deftest move-internal-removed-added
  (with-clean-system
    (let [[workspace project] (setup-scratch world)
          main (project/get-resource-node project "/main/main.script")
          initial-graph-nodes (graph-nodes project)]

      (move-file workspace "/main/main.script" "/main/main2.script")

      (is (nil? (project/get-resource-node project "/main/main.script")))

      (let [main2 (project/get-resource-node project "/main/main2.script")]
        ;; resource node simply repointed
        (is (= main main2))
        ;; no nodes added or removed
        (is (= (graph-nodes project) initial-graph-nodes))))))

(defn- atlas-image-resources [atlas-id]
  (g/node-value atlas-id :image-resources))

(defn- resource [resource-node]
  (g/node-value resource-node :resource))

(deftest move-external-removed-changed
  ;; /atlas/powball.atlas has images /graphics/{pow, ball}.png
  ;; /atlas/[pow | ball].atlas has image /graphics/[pow | ball].png
  (with-clean-system
    (let [[workspace project] (setup-scratch world)
          atlas>powball (project/get-resource-node project "/atlas/powball.atlas")
          atlas>pow (project/get-resource-node project "/atlas/pow.atlas")
          atlas>ball (project/get-resource-node project "/atlas/ball.atlas")
          graphics>pow (project/get-resource-node project "/graphics/pow.png")
          graphics>ball (project/get-resource-node project "/graphics/ball.png")
          atlas>powball-image-resources (atlas-image-resources atlas>powball)
          atlas>pow-image-resources (atlas-image-resources atlas>pow)
          atlas>ball-image-resources (atlas-image-resources atlas>ball)
          initial-graph-nodes (graph-nodes project)]
      (is (= atlas>powball-image-resources [(resource graphics>pow) (resource graphics>ball)]))
      (is (= atlas>pow-image-resources [(resource graphics>pow)]))
      (is (= atlas>ball-image-resources [(resource graphics>ball)]))

      (move-file workspace "/graphics/pow.png" "/graphics/ball.png")

      ;; resource /graphics/pow.png removed
      (is (nil? (project/get-resource-node project "/graphics/pow.png")))
      ;; resource node for target reused
      (is (= graphics>ball (project/get-resource-node project "/graphics/ball.png")))
      ;; only pow removed, nothing added
      (is (graph-nodes project) (set/difference initial-graph-nodes #{graphics>pow}))
      ;; old reference to pow replaced by ball
      (is (= (atlas-image-resources atlas>powball)
             [(resource graphics>ball) (resource graphics>ball)]))
      (is (= (atlas-image-resources atlas>pow) [(resource graphics>ball)]))
      (is (= (atlas-image-resources atlas>ball) atlas>ball-image-resources)))))

(defn game-object-script-nodes [game-object-node-id]
  (keep (fn [node-id]
          (when (and (= game-object/ReferencedComponent (g/node-type* node-id))
                     (= "script" (:ext (resource/resource-type (g/node-value node-id :source-resource)))))
            (g/node-value node-id :source-id)))
        (g/node-value game-object-node-id :nodes)))

(deftest move-internal-removed-changed
  ;; /game_object/props.go has a script component /script/props.script
  ;; /main/main.go has a script component /main/main.script
  (with-clean-system
    (let [[workspace project] (setup-scratch world)
          game_object>props (project/get-resource-node project "/game_object/props.go")
          game_object>props-scripts (game-object-script-nodes game_object>props)
          script>props (project/get-resource-node project "/script/props.script")
          main>main-go (project/get-resource-node project "/main/main.go")
          main>main-go-scripts (game-object-script-nodes main>main-go)
          main>main-script (project/get-resource-node project "/main/main.script")
          initial-graph-nodes (graph-nodes project)]
      (is (= (map g/override-original game_object>props-scripts) [script>props]))
      (is (= (map g/override-original main>main-go-scripts) [main>main-script]))

      (move-file workspace "/script/props.script" "/main/main.script")

      ;; resource /script/props.script removed
      (is (nil? (project/get-resource-node project "/script/props.script")))
      ;; old resource node for /main/main.script is removed (but has been replaced/reloaded as below)
      (is (nil? (g/node-by-id main>main-script)))
      (let [game_object>props-scripts2 (game-object-script-nodes game_object>props)
            main>main-script2 (project/get-resource-node project "/main/main.script")
            main>main-go-scripts2 (game-object-script-nodes main>main-go)]
        ;; override-nodes remain, we change their override-original to reflect the move
        (is (= game_object>props-scripts game_object>props-scripts2))
        (is (= (map g/override-original game_object>props-scripts2) [main>main-script2]))
        (is (= main>main-go-scripts main>main-go-scripts2))
        (is (= (map g/override-original main>main-go-scripts2) [main>main-script2]))
        ;; new version of /main/main.script added, /script/props.script and old version of /main/main.script removed
        (is (= (graph-nodes project)
               (set/union
                 (set/difference initial-graph-nodes
                                 #{script>props main>main-script})
                 #{main>main-script2})))))))

(deftest move-external-changed-added
  ;; imagelib1 contains /images/{pow,paddle}.png, setup moves reload_project's
  ;; /graphics to /images - a plain removed/added move case.
  (with-clean-system
    (let [[workspace project] (setup-scratch world)
          atlas>powball (project/get-resource-node project "/atlas/powball.atlas")
          graphics>pow (project/get-resource-node project "/graphics/pow.png")
          graphics>ball (project/get-resource-node project "/graphics/ball.png")
          initial-graph-nodes (graph-nodes project)]
      (is (= (map resource/proj-path (atlas-image-resources atlas>powball))
             ["/graphics/pow.png" "/graphics/ball.png"]))

      (let [graphics-dir-resource (workspace/find-resource workspace "/graphics")]
        (asset-browser/rename graphics-dir-resource "images"))

      (let [images>pow (project/get-resource-node project "/images/pow.png")
            images>pow-resource (resource images>pow)]

        (is (= (map resource/proj-path (atlas-image-resources atlas>powball))
               ["/images/pow.png" "/images/ball.png"]))
        (is (= initial-graph-nodes (graph-nodes project)))

        ;; actual test
        (workspace/set-project-dependencies! workspace [imagelib1-url])
        (let [images-dir-resource (workspace/find-resource workspace "/images")]
          (asset-browser/rename images-dir-resource "graphics"))

        ;; The move of /images back to /graphics enabled the load of imagelib1, creating the following move cases:
        ;; /images/ball.png -> /graphics/ball.png: removed, added
        ;; /images/pow.png -> /graphics/pow.png: changed, added

        ;; powball atlas keeps referring to /images/pow.png from lib, but ball.png was moved
        (is (= (map resource/proj-path (atlas-image-resources atlas>powball)) ["/images/pow.png" "/graphics/ball.png"]))
        ;; resource node reused, resource updated
        (is (= images>pow (project/get-resource-node project "/images/pow.png")))
        (is (not= images>pow-resource (resource images>pow)))
        (let [graphics>pow2 (project/get-resource-node project "/graphics/pow.png")
              graphics>ball2 (project/get-resource-node project "/graphics/ball.png")
              images>paddle (project/get-resource-node project "/images/paddle.png")]
          ;; resource node reused
          (is (= graphics>ball graphics>ball2))
          ;; graphics>pow also reused: was refactored first to /images/pow.png, and now
          ;; "reloaded"/redirected to new resource
          (is (= images>pow graphics>pow))
          ;; new nodes for /graphics/pow.png and /images/paddle.png
          (is (= (graph-nodes project) (set/union initial-graph-nodes #{graphics>pow2 images>paddle}))))))))

(deftest move-internal-changed-added
  ;; We're using the scriptlib library (containing /scripts/main.script) that puts its scripts
  ;; in /scripts rather than /script used in the reload_project, slightly confusing.
  ;; Setup creates /scripts/main.script and a corresponding go /game_object/main.go with a
  ;; script component /scripts/main.script
  (with-clean-system
    (let [[workspace project] (setup-scratch world)]
      (copy-file workspace "/main/main.script" "/scripts/main.script")
      (write-file workspace
                  "/game_object/main.go"
                  "components { id: \"script\" component: \"/scripts/main.script\" }")
      (let [scripts>main (project/get-resource-node project "/scripts/main.script")
            game_object>main-go (project/get-resource-node project "/game_object/main.go")
            game_object>main-go-scripts (game-object-script-nodes game_object>main-go)
            initial-graph-nodes (graph-nodes project)]
        (is (= (map g/override-original game_object>main-go-scripts)
               [scripts>main]))

        (workspace/set-project-dependencies! workspace [scriptlib-url])
        (let [scripts-dir-resource (workspace/find-resource workspace "/scripts")]
          (asset-browser/rename scripts-dir-resource "project_scripts"))

        ;; the move of /scripts enabled the load of scriptlib, creating the move case:
        ;; /scripts/main.script -> /project_scripts/main.script: changed, added

        ;; resource node for old version of /scripts/main.script removed (has been replaced)
        (is (nil? (g/node-by-id scripts>main)))
        (let [scripts>main2 (project/get-resource-node project "/scripts/main.script")
              project_scripts>main (project/get-resource-node project "/project_scripts/main.script")
              game_object>main-go-scripts2 (game-object-script-nodes game_object>main-go)]
          ;; override nodes remain, override-original changed
          (is (= game_object>main-go-scripts game_object>main-go-scripts2))
          (is (= (map g/override-original game_object>main-go-scripts2)
                 [scripts>main2]))
          ;; added project_scripts/main.script and new version of /scripts/main.script, removed old version of /scripts/main.script
          (is (= (graph-nodes project)
                 (set/union (set/difference initial-graph-nodes #{scripts>main})
                            #{scripts>main2 project_scripts>main}))))))))

(deftest move-external-changed-changed
  ;; We're using imagelib1 again. Setup copies /graphics -> /images and creates
  ;; /atlas/images_powball.atlas that refers to {pow, ball}.png under /images
  (with-clean-system
    (let [[workspace project] (setup-scratch world)
          graphics>pow (project/get-resource-node project "/graphics/pow.png")
          graphics>ball (project/get-resource-node project "/graphics/ball.png")]
      (copy-directory workspace "/graphics" "/images")
      (write-file workspace "/atlas/images_powball.atlas" "images { image: \"/images/pow.png\" } images { image: \"/images/ball.png\" }")
      (let [atlas>images-powball (project/get-resource-node project "/atlas/images_powball.atlas")
            images>pow (project/get-resource-node project "/images/pow.png")
            images>pow-resource (resource images>pow)
            image>ball (project/get-resource-node project "/images/ball.png")
            initial-graph-nodes (graph-nodes project)]
        (workspace/set-project-dependencies! workspace [imagelib1-url])
        (binding [dialogs/make-resolve-file-conflicts-dialog (fn [src-dest-pairs] :overwrite)]
          (let [images-dir-resource (workspace/find-resource workspace "/images")]
            (asset-browser/rename images-dir-resource "graphics")))

        ;; The move of /images overwriting /graphics enabled the load of imagelib1, creating the following move cases:
        ;; /images/ball.png -> /graphics/ball.png: removed, changed
        ;; /images/pow.png -> /graphics/pow.png: changed, changed

        ;; images_powball.atlas keeps referring to /images/pow.png, but ball.png was moved & changed - reference updated
        (is (= (map resource/proj-path (atlas-image-resources atlas>images-powball)) ["/images/pow.png" "/graphics/ball.png"]))
        ;; resource node for /images/pow.png, /graphics/pow.png, /graphics/ball.png reused
        ;; /images/pow.png resource updated
        (is (= images>pow (project/get-resource-node project "/images/pow.png")))
        (is (not= images>pow-resource (resource images>pow)))
        (is (= graphics>pow (project/get-resource-node project "/graphics/pow.png")))
        (is (= graphics>ball (project/get-resource-node project "/graphics/ball.png")))
        (let [images>paddle (project/get-resource-node project "/images/paddle.png")]
          ;; images/paddle.png added, images/ball.png removed
          (is (= (graph-nodes project)
                 (set/union (set/difference initial-graph-nodes #{image>ball})
                            #{images>paddle}))))))))

(deftest move-internal-changed-changed
  ;; As earlier, we're using scriptlib which puts scripts in /scripts rather than /script
  ;; Setup creates /scripts/main.script + go /game_object/main.go with corresponding component
  (with-clean-system
    (let [[workspace project] (setup-scratch world)
          main>main-script (project/get-resource-node project "/main/main.script")]
      (copy-file workspace "/main/main.script" "/scripts/main.script")
      (write-file workspace
                  "/game_object/main.go"
                  "components { id: \"script\" component: \"/scripts/main.script\" }")
      (let [scripts>main (project/get-resource-node project "/scripts/main.script")
            game_object>main (project/get-resource-node project "/game_object/main.go")
            game_object>main-scripts (game-object-script-nodes game_object>main)
            initial-graph-nodes (graph-nodes project)]
        (is (= (map g/override-original game_object>main-scripts) [scripts>main]))

        (workspace/set-project-dependencies! workspace [scriptlib-url]) ; /scripts/main.script
        (binding [dialogs/make-resolve-file-conflicts-dialog (fn [src-dest-pairs] :overwrite)]
          (let [scripts-dir-resource (workspace/find-resource workspace "/scripts")]
            (asset-browser/rename scripts-dir-resource "main")))

        ;; the move of /scripts overwriting /main enabled the load of scriptlib, creating move case:
        ;; /scripts/main.script -> /main/main.script: changed, changed

        ;; resource node for old version of /scripts/main.script removed (replaced)
        (is (nil? (g/node-by-id scripts>main)))
        ;; resource node for old version of /main/main.script removed (replaced)
        (is (nil? (g/node-by-id main>main-script)))
        (let [scripts>main2 (project/get-resource-node project "/scripts/main.script")
              main>main-script2 (project/get-resource-node project "/main/main.script")
              game_object>main-scripts2 (game-object-script-nodes game_object>main)]
          ;; override nodes remain, override-original changed
          (is (= game_object>main-scripts game_object>main-scripts2))
          (is (= (map g/override-original game_object>main-scripts2) [scripts>main2]))
          (is (not= main>main-script main>main-script2))
          (is (not= scripts>main scripts>main2))
          ;; added nodes for new versions of /scripts/main.script and /main/main.script
          ;; removed nodes for old versions
          (is (= (graph-nodes project)
                 (set/union (set/difference initial-graph-nodes #{scripts>main main>main-script})
                            #{scripts>main2 main>main-script2}))))))))

(deftest rename-file-changing-case
  (with-clean-system
    (let [[workspace project] (setup-scratch world)
          graphics>ball (project/get-resource-node project "/graphics/ball.png")
          nodes-by-path (g/node-value project :nodes-by-resource-path)]
      (asset-browser/rename (resource graphics>ball) "Ball.png")
      (testing "Resource node :resource updated"
        (is (= (resource/proj-path (g/node-value graphics>ball :resource)) "/graphics/Ball.png")))
      (testing "Resource node map updated"
        (let [nodes-by-path' (g/node-value project :nodes-by-resource-path)]
          (is (= (set/difference (set (keys nodes-by-path)) (set (keys nodes-by-path'))) #{"/graphics/ball.png"}))
          (is (= (set/difference (set (keys nodes-by-path')) (set (keys nodes-by-path))) #{"/graphics/Ball.png"})))))))

(deftest rename-directory-with-dotfile
  (with-clean-system
    (let [[workspace project] (setup-scratch world)]
      (touch-file workspace "/graphics/.dotfile")
      (let [graphics-dir-resource (workspace/find-resource workspace "/graphics")]
        ;; This used to throw: java.lang.AssertionError: Assert failed: move of unknown resource "/graphics/.dotfile"
        (asset-browser/rename graphics-dir-resource "whatever")))))

(deftest move-external-removed-added-replacing-deleted
  ;; We used to end up with two resource nodes referring to the same resource (/graphics/ball.png)
  (with-clean-system
    (let [[workspace project] (setup-scratch world)
          initial-node-resources (g/node-value project :node-resources)]
      (copy-file workspace "/graphics/ball.png" "/ball.png")
      (delete-file workspace "/graphics/ball.png")
      (move-file workspace "/ball.png" "/graphics/ball.png")
      (let [node-resources (g/node-value project :node-resources)]
        (is (= (sort-by resource/proj-path initial-node-resources)
               (sort-by resource/proj-path node-resources)))))))

(defn- coll-link [coll]
  (get-in (g/node-value coll :node-outline) [:children 0 :link]))

(deftest refactoring-sub-collection
  (with-clean-system
    (let [[workspace project] (setup-scratch world)
          node-id (project/get-resource-node project "/collection/sub_defaults.collection")
          coll-path "/collection/props.collection"
          coll-res (workspace/resolve-workspace-resource workspace coll-path)
          new-coll-path "/collection/props2.collection"
          new-coll-res (workspace/resolve-workspace-resource workspace new-coll-path)]
      (is (= coll-res (coll-link node-id)))
      (move-file workspace coll-path new-coll-path)
      (is (= new-coll-res (coll-link node-id))))))

(deftest project-with-missing-parts-can-be-saved
  ;; missing embedded game object, sub collection
  (with-clean-system
    (let [[workspace project] (log/without-logging (setup-scratch world "test/resources/missing_project"))]
      (is (not (g/error? (project/all-save-data project)))))))

(deftest project-with-nil-parts-can-be-saved
  (with-clean-system
    (let [[workspace project] (log/without-logging (setup-scratch world "test/resources/nil_project"))]
      (is (not (g/error? (project/all-save-data project)))))))

(deftest broken-project-can-be-saved
  (with-clean-system
    (let [[workspace project] (log/without-logging (setup-scratch world "test/resources/broken_project"))]
      (is (not (g/error? (project/all-save-data project)))))))

(defn- gui-node [scene id]
  (let [nodes (into {} (map (fn [o] [(:label o) (:node-id o)])
                            (tree-seq (constantly true) :children (g/node-value scene :node-outline))))]
    (nodes id)))

(deftest gui-templates
  (with-clean-system
    (let [[workspace project] (setup-scratch world)
          node-id (project/get-resource-node project "/gui/scene.gui")
          sub-node-id (project/get-resource-node project "/gui/sub_scene.gui")
          or-node (gui-node node-id "sub_scene/sub_box")]
      (is (some? or-node))
      (write-file workspace "/gui/sub_scene.gui" (read-file workspace "/gui/new_sub_scene.gui"))
      (is (some? (gui-node node-id "sub_scene/sub_box2"))))))

(deftest label
  (with-clean-system
    (let [[workspace project] (setup-scratch world)]
      (let [node-id (project/get-resource-node project "/label/label.label")]
        (is (= "Original" (g/node-value node-id :text)))
        (is (= [1.0 1.0 1.0] (g/node-value node-id :scale)))
        (is (= [1.0 1.0 1.0 1.0] (g/node-value node-id :color)))
        (is (= [0.0 0.0 0.0 1.0] (g/node-value node-id :outline)))
        (is (= 1.0 (g/node-value node-id :leading)))
        (is (= 0.0 (g/node-value node-id :tracking)))
        (is (= :pivot-center (g/node-value node-id :pivot)))
        (is (= :blend-mode-alpha (g/node-value node-id :blend-mode)))
        (is (false? (g/node-value node-id :line-break))))
      (write-file workspace "/label/label.label" (read-file workspace "/label/new_label.label"))
      (let [node-id (project/get-resource-node project "/label/label.label")]
        (is (= "Modified" (g/node-value node-id :text)))
        (is (= [2.0 3.0 4.0] (g/node-value node-id :scale)))
        (is (= [1.0 0.0 0.0 1.0] (g/node-value node-id :color)))
        (is (= [1.0 1.0 1.0 1.0] (g/node-value node-id :outline)))
        (is (= 2.0 (g/node-value node-id :leading)))
        (is (= 1.0 (g/node-value node-id :tracking)))
        (is (= :pivot-n (g/node-value node-id :pivot)))
        (is (= :blend-mode-add (g/node-value node-id :blend-mode)))
        (is (true? (g/node-value node-id :line-break)))))))

(deftest game-project
  (with-clean-system
    (let [[workspace project] (setup-scratch world)
          node-id (project/get-resource-node project "/game.project")
          p ["display" "display_profiles"]
          disp-profs (get (g/node-value node-id :settings-map) p)
          path "/render/default.display_profiles"
          new-path "/render/default2.display_profiles"]
      (write-file workspace path "")
      (game-project/set-setting! node-id p (workspace/file-resource workspace path))
      (move-file workspace path new-path)
      (is (= new-path
            (resource/resource->proj-path (get (g/node-value node-id :settings-map) p)))))))

(deftest all-project-files
  (with-clean-system
    (let [[workspace project] (setup-scratch world)]
      (let [all-files (->>
                        (workspace/resolve-workspace-resource workspace "/")
                        (tree-seq (fn [r] (and (not (resource/read-only? r)) (resource/children r))) resource/children)
                        (filter (fn [r] (= (resource/source-type r) :file))))
            paths (map resource/proj-path all-files)]
        (bulk-change workspace
                     (touch-files workspace paths))
        (let [internal-paths (map resource/proj-path (filter (fn [r] (not (:stateless? (resource/resource-type r)))) all-files))
              saved-paths (set (map (fn [s] (resource/proj-path (:resource s))) (g/node-value project :save-data)))
              missing (filter #(not (contains? saved-paths %)) internal-paths)]
          (is (empty? missing)))))))

(deftest new-collection-modified-script
  ;; used to provoke exception because load steps of collection tried to access non-loaded script
  (with-clean-system
    (let [[workspace project] (setup-scratch world "test/resources/load_order_project")]
      (bulk-change workspace
                   (->> (read-file workspace "/referenced.collection")
                        (write-file workspace "/referenced2.collection"))
                   ;; NB! not touching /main/main.go
                   (touch-file workspace "/referenced.script")))))
