(ns integration.reload-test
  (:require [clojure.test :refer :all]
            [clojure.string :as str]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system undo-stack]]
            [editor.math :as math]
            [editor.project :as project]
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
           [com.dynamo.spine.proto Spine$SpineScene]
           [com.dynamo.mesh.proto Mesh$MeshDesc]
           [com.dynamo.model.proto Model$ModelDesc]
           [editor.types Region]
           [editor.workspace BuildResource]
           [editor.resource FileResource]
           [java.awt.image BufferedImage]
           [java.io File]
           [java.nio.file Files attribute.FileAttribute]
           [javax.imageio ImageIO]
           [javax.vecmath Point3d Matrix4d]
           [org.apache.commons.io FilenameUtils FileUtils]))

(def ^:dynamic *project-path* "resources/reload_project")

(defn- setup-scratch
  ([ws-graph] (setup-scratch ws-graph *project-path*))
  ([ws-graph project-path]
   (let [workspace (test-util/setup-scratch-workspace! ws-graph project-path)
         project (test-util/setup-project! workspace)]
     [workspace project])))

(defn- mkdirs [^File f]
  (let [parent (.getParentFile f)]
    (when (not (.exists parent))
      (.mkdirs parent))))

(defn- template [workspace name]
  (let [resource (workspace/file-resource workspace name)]
    (workspace/template (resource/resource-type resource))))

(defn- write-file [workspace name content]
  (let [f (File. (workspace/project-path workspace) name)]
    (mkdirs f)
    (if (not (.exists f))
      (.createNewFile f)
      (Thread/sleep 1100))
    (spit f content))
  (workspace/resource-sync! workspace))

(defn- read-file [workspace name]
  (slurp (str (workspace/project-path workspace) name)))

(defn- add-file [workspace name]
  (write-file workspace name (template workspace name)))

(defn- delete-file [workspace name]
  (let [f (File. (workspace/project-path workspace) name)]
    (.delete f))
  (workspace/resource-sync! workspace))

(defn- copy-file [workspace name new-name]
  (let [[f new-f] (mapv #(File. (workspace/project-path workspace) %) [name new-name])]
    (FileUtils/copyFile f new-f))
  (workspace/resource-sync! workspace))

(defn- move-file [workspace name new-name]
  (let [[f new-f] (mapv #(File. (workspace/project-path workspace) %) [name new-name])]
    (FileUtils/moveFile f new-f)
    (workspace/resource-sync! workspace true [[f new-f]])))

(defn- add-img [workspace name width height]
  (let [img (BufferedImage. width height BufferedImage/TYPE_INT_ARGB)
        type (FilenameUtils/getExtension name)
        f (File. (workspace/project-path workspace) name)]
    (when (.exists f)
      (Thread/sleep 1100))
    (ImageIO/write img type f)
    (workspace/resource-sync! workspace)))

(defn- has-undo? [project]
  (g/has-undo? (g/node-id->graph-id project)))

(defn- no-undo? [project]
  (not (has-undo? project)))

(deftest internal-file
  (with-clean-system
    (let [[workspace project] (setup-scratch world)]
      (testing "Add internal file"
               (add-file workspace "/test.collection")
               (let [node (project/get-resource-node project "/test.collection")]
                 (is (not (nil? node)))
                 (is (= "default" (g/node-value node :name))))
               (is (no-undo? project)))
      (testing "Change internal file"
               (write-file workspace "/test.collection" "name: \"test_name\"")
               (let [node (project/get-resource-node project "/test.collection")]
                 (is (not (nil? node)))
                 (is (= "test_name" (g/node-value node :name))))
               (is (no-undo? project)))
      (testing "Delete internal file"
               (delete-file workspace "/test.collection")
               (let [node (project/get-resource-node project "/test.collection")]
                 (is (nil? node)))
               (is (no-undo? project))))))

(deftest external-file
 (with-clean-system
   (let [[workspace project] (setup-scratch world)
         atlas-node-id (project/get-resource-node project "/atlas/empty.atlas")
         img-path "/test_img.png"
         anim-id (FilenameUtils/getBaseName img-path)]
     (is (not (nil? atlas-node-id)))
     (testing "Add external file, cleared history"
       (add-img workspace img-path 64 64)
       (let [node (project/get-resource-node project img-path)]
         (is (some? node)))
       (is (no-undo? project)))
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
           (is (= undo-count (count (undo-stack (g/node-id->graph-id project)))))
           (let [anim-data (g/node-value atlas-node-id :anim-data)
                 anim (get anim-data anim-id)]
             (is (and (= 128 (:width anim)) (= 128 (:height anim))))))
         (testing "Delete it, errors produced"
           (delete-file workspace img-path)
           (is (= undo-count (count (undo-stack (g/node-id->graph-id project)))))
                                        ; TODO - fix node pollution
           (log/without-logging
            (is (g/error? (g/node-value atlas-node-id :anim-data))))))))))

(deftest save-no-reload
  (with-clean-system
    (let [[workspace project] (setup-scratch world)]
      (testing "Add internal file"
               (add-file workspace "/test.collection")
               (let [node (project/get-resource-node project "/test.collection")]
                 (g/transact
                   (g/set-property node :name "new_name"))
                 (is (has-undo? project))
                 (project/save-all project)
                 (workspace/resource-sync! workspace)
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

(defn- image-label [atlas]
  (let [outline (g/node-value atlas :node-outline)]
    (:label (first (:children outline)))))

(deftest refactoring
  (with-clean-system
    (let [[workspace project] (setup-scratch world)
          node-id (project/get-resource-node project "/atlas/single.atlas")
          img-path "/test_img.png"
          new-img-path "/test_img2.png"]
      (add-img workspace img-path 64 64)
      (is (.endsWith (image-label node-id) img-path))
      (move-file workspace img-path new-img-path)
      (is (.endsWith (image-label node-id) new-img-path))
      (move-file workspace new-img-path img-path)
      (is (.endsWith (image-label node-id) img-path)))))

(deftest refactoring-sub-collection
  (with-clean-system
    (let [[workspace project] (setup-scratch world)
          node-id (project/get-resource-node project "/collection/sub_defaults.collection")
          coll-path "/collection/props.collection"
          new-coll-path "/collection/props2.collection"]
      (is (= (format "props (%s)" coll-path) (get-in (g/node-value node-id :node-outline) [:children 0 :label])))
      (move-file workspace coll-path new-coll-path)
      (is (= (format "props (%s)" new-coll-path) (get-in (g/node-value node-id :node-outline) [:children 0 :label]))))))

(deftest project-with-missing-parts-can-be-saved
  ;; missing embedded game object, sub collection
  (with-clean-system
    (let [[workspace project] (log/without-logging (setup-scratch world "resources/missing_project"))]
      (is (not (g/error? (project/save-data project)))))))

(deftest project-with-nil-parts-can-be-saved
  (with-clean-system
    (let [[workspace project] (log/without-logging (setup-scratch world "resources/nil_project"))]
      (is (not (g/error? (project/save-data project)))))))

(deftest broken-project-cannot-be-saved
  (with-clean-system
    (let [[workspace project] (log/without-logging (setup-scratch world "resources/broken_project"))]
      (is (g/error? (project/save-data project))))))

