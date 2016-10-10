(ns integration.build-test
  (:require [clojure.test :refer :all]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system]]
            [editor.math :as math]
            [editor.defold-project :as project]
            [editor.progress :as progress]
            [editor.protobuf :as protobuf]
            [editor.workspace :as workspace]
            [editor.resource :as resource]
            [integration.test-util :as test-util])
  (:import [com.dynamo.gameobject.proto GameObject GameObject$CollectionDesc GameObject$CollectionInstanceDesc GameObject$InstanceDesc
            GameObject$EmbeddedInstanceDesc GameObject$PrototypeDesc]
           [com.dynamo.graphics.proto Graphics$TextureImage]
           [com.dynamo.textureset.proto TextureSetProto$TextureSet]
           [com.dynamo.render.proto Font$FontMap]
           [com.dynamo.particle.proto Particle$ParticleFX]
           [com.dynamo.sound.proto Sound$SoundDesc]
           [com.dynamo.rig.proto Rig$RigScene Rig$Skeleton Rig$AnimationSet Rig$MeshSet]
           [com.dynamo.mesh.proto Mesh$MeshDesc]
           [com.dynamo.model.proto Model$ModelDesc]
           [com.dynamo.physics.proto Physics$CollisionObjectDesc]
           [com.dynamo.properties.proto PropertiesProto$PropertyDeclarations]
           [com.dynamo.lua.proto Lua$LuaModule]
           [com.dynamo.gui.proto Gui$SceneDesc]
           [com.dynamo.spine.proto Spine$SpineModelDesc]
           [editor.types Region]
           [editor.workspace BuildResource]
           [java.awt.image BufferedImage]
           [java.io File]
           [javax.imageio ImageIO]
           [javax.vecmath Point3d Matrix4d]
           [org.apache.commons.io FilenameUtils]))

(def project-path "test/resources/build_project/SideScroller")

(defn- ->BuildResource
  ([project path]
   (->BuildResource project path nil))
  ([project path prefix]
   (let [node (test-util/resource-node project path)]
     (workspace/make-build-resource (g/node-value node :resource) prefix))))

(def target-pb-classes {"skeletonc" Rig$Skeleton
                        "animationsetc" Rig$AnimationSet
                        "meshsetc" Rig$MeshSet
                        "texturesetc" TextureSetProto$TextureSet})

(defn- target [path targets]
  (let [ext (FilenameUtils/getExtension path)
        pb-class (get target-pb-classes ext)]
    (protobuf/bytes->map pb-class (get targets path))))

(def pb-cases [{:label    "ParticleFX"
                :path     "/main/blob.particlefx"
                :pb-class Particle$ParticleFX
                :test-fn  (fn [pb targets]
                            (is (= -10.0 (get-in pb [:emitters 0 :modifiers 0 :position 0]))))}
               {:label           "Sound"
                :path            "/main/sound.sound"
                :pb-class        Sound$SoundDesc
                :resource-fields [:sound]}
               {:label           "Collision Object"
                :path            "/collisionobject/tile_map.collisionobject"
                :pb-class        Physics$CollisionObjectDesc
                :resource-fields [:collision-shape]}
               {:label           "Collision Object"
                :path            "/collisionobject/convex_shape.collisionobject"
                :pb-class        Physics$CollisionObjectDesc
                :test-fn (fn [pb targets]
                           (is (= "" (:collision-shape pb)))
                           (is (= 1 (count (get-in pb [:embedded-collision-shape :shapes])))))}
               {:label           "Tile Source"
                :path            "/tile_source/simple.tilesource"
                :pb-class        TextureSetProto$TextureSet
                :test-fn (fn [pb targets]
                           (is (= "default" (:collision-group (first (:convex-hulls pb)))))
                           (is (< 0 (count (:convex-hull-points pb)))))}
               {:label "Spine Scene"
                :path "/player/spineboy.spinescene"
                :pb-class Rig$RigScene
                :resource-fields [:texture-set :skeleton :animation-set :mesh-set]
                :test-fn (fn [pb targets]
                           (is (some? (-> pb :texture-set (target targets) :texture)))
                           (is (not= 0 (-> pb :mesh-set (target targets) :mesh-entries first :id)))
                           (is (< 0 (-> pb :mesh-set (target targets) :mesh-entries count)))
                           (is (< 0 (-> pb :animation-set (target targets) :animations count)))
                           (is (< 0 (-> pb :skeleton (target targets) :bones count))))}
               {:label "Spine Model"
                :path "/player/spineboy.spinemodel"
                :pb-class Spine$SpineModelDesc
                :resource-fields [:spine-scene :material]}])

(defn- run-pb-case [case content-by-source content-by-target]
  (testing (str "Testing " (:label case))
    (let [content    (get content-by-source (:path case))
          pb         (protobuf/bytes->map (:pb-class case) content)
          test-fn    (:test-fn case)
          res-fields [:sound]]
      (when test-fn
        (test-fn pb content-by-target))
      (doseq [field (:resource-fields case)]
        (is (contains? content-by-target (get pb field)))
        (is (> (count (get content-by-target (get pb field))) 0))))))

(defmacro with-build-results [path & forms]
  `(with-clean-system
     (let [~'workspace         (test-util/setup-workspace! ~'world project-path)
           ~'project           (test-util/setup-project! ~'workspace)
           ~'path              ~path
           ~'resource-node     (test-util/resource-node ~'project ~path)
           ~'build-results     (project/build ~'project ~'resource-node {})
           ~'content-by-source (into {} (keep #(when-let [~'r (:resource (:resource %))]
                                                 [(resource/proj-path ~'r) (:content %)])
                                              ~'build-results))
           ~'content-by-target (into {} (keep #(when-let [~'r (:resource %)]
                                                 [(resource/proj-path ~'r) (:content %)])
                                             ~'build-results))]
       ~@forms)))

(deftest build-game-project
  (with-build-results "/game.project"
    (let [target-exts (into #{} (map #(:build-ext (resource/resource-type (:resource %))) build-results))
          exp-paths   [path
                       "/main/main.collection"
                       "/main/main.script"
                       "/input/game.input_binding"
                       "/builtins/render/default.render"
                       "/builtins/render/default.render_script"
                       "/background/bg.png"
                       "/tile_source/simple.tilesource"
                       "/main/blob.tilemap"
                       "/collisionobject/tile_map.collisionobject"
                       "/collisionobject/convex_shape.collisionobject"]
          exp-exts    ["vpc" "fpc" "texturec"]]
      (doseq [case pb-cases]
        (run-pb-case case content-by-source content-by-target))
      (doseq [ext exp-exts]
        (is (contains? target-exts ext)))
      (doseq [path exp-paths]
        (is (contains? content-by-source path)))
      (let [content (get content-by-source "/main/main.collection")
            desc    (GameObject$CollectionDesc/parseFrom content)
            go-ext  (:build-ext (workspace/get-resource-type workspace "go"))]
        (doseq [inst (.getInstancesList desc)]
          (is (.endsWith (.getPrototype inst) go-ext)))))))

(deftest build-coll-hierarchy
  (testing "Building collection hierarchies"
    (with-build-results "/hierarchy/base.collection"
      (let [content   (get content-by-source "/hierarchy/base.collection")
            desc      (GameObject$CollectionDesc/parseFrom content)
            instances (.getInstancesList desc)
            go-ext    (:build-ext (workspace/get-resource-type workspace "go"))]
        (is (= 3 (count instances)))
        (let [ids (loop [instances (vec instances)
                         ids       (transient #{})]
                    (if-let [inst (first instances)]
                      (do
                        (is (.startsWith (.getId inst) "/sub1/sub2"))
                        (is (.endsWith (.getPrototype inst) go-ext))
                        (if (.endsWith (.getId inst) "empty_child")
                          (is (<= (Math/abs (- 10 (.getX (.getPosition inst)))) math/epsilon))
                          (is (<= (Math/abs (- 30 (.getX (.getPosition inst)))) math/epsilon)))
                        (recur (rest instances) (conj! ids (.getId inst))))
                      (persistent! ids)))]
          (doseq [inst instances]
            (is (every? ids (.getChildrenList inst)))))))))

(defn- count-exts [paths ext]
  (count (filter #(.endsWith % ext) paths)))

(deftest merge-gos
  (testing "Verify equivalent game objects are merged"
    (with-clean-system
      (let [workspace     (test-util/setup-workspace! world project-path)
            project       (test-util/setup-project! workspace)]
        (doseq [path ["/merge/merge_embed.collection"
                      "/merge/merge_refs.collection"]
                :let [resource-node (test-util/resource-node project path)
                      build-results (project/build project resource-node {})
                      content-by-source (into {} (map #(do [(resource/proj-path (:resource (:resource %))) (:content %)])
                                                      build-results))
                      content-by-target (into {} (map #(do [(resource/proj-path (:resource %)) (:content %)])
                                                      build-results))]]
          (is (= 1 (count-exts (keys content-by-target) "goc")))
          (is (= 1 (count-exts (keys content-by-target) "spritec")))
          (let [content (get content-by-source path)
                desc (GameObject$CollectionDesc/parseFrom content)
                target-paths (set (map #(resource/proj-path (:resource %)) build-results))]
            (doseq [inst (.getInstancesList desc)
                    :let [prototype (.getPrototype inst)]]
              (is (contains? target-paths prototype))
              (let [content (get content-by-target prototype)
                    desc (GameObject$PrototypeDesc/parseFrom content)]
                (doseq [comp (.getComponentsList desc)
                        :let [component (.getComponent comp)]]
                  (is (contains? target-paths component)))))))))))

(deftest embed-raw-sound
  (testing "Verify raw sound components (.wav or .ogg) are converted to embedded sounds (.sound)"
    (with-build-results "/main/raw_sound.go"
      (let [content    (get content-by-source path)
            desc       (protobuf/bytes->map GameObject$PrototypeDesc content)
            sound-path (get-in desc [:components 0 :component])
            ext        (FilenameUtils/getExtension sound-path)]
        (is (= ext "soundc"))
        (let [sound-desc (protobuf/bytes->map Sound$SoundDesc (content-by-target sound-path))]
          (is (contains? content-by-target (:sound sound-desc))))))))

(defn- first-source [node label]
  (ffirst (g/sources-of node label)))

(deftest break-merged-targets
  (testing "Verify equivalent game objects are not merged after being changed in memory"
    (with-build-results "/merge/merge_embed.collection"
      (is (= 1 (count-exts (keys content-by-target) "goc")))
      (is (= 1 (count-exts (keys content-by-target) "spritec")))
      (let [go-node   (first-source (first-source resource-node :child-scenes) :source-id)
            comp-node (first-source go-node :child-scenes)]
        (g/transact (g/delete-node comp-node))
        (let [build-results     (project/build project resource-node {})
              content-by-target (into {} (map #(do [(resource/proj-path (:resource %)) (:content %)])
                                              build-results))]
          (is (= 1 (count-exts (keys content-by-target) "goc")))
          (is (= 1 (count-exts (keys content-by-target) "spritec"))))))))

(deftest build-cached
  (testing "Verify the build cache works as expected"
    (with-clean-system
      (let [workspace            (test-util/setup-workspace! world project-path)
            project              (test-util/setup-project! workspace)
            path                 "/game.project"
            resource-node        (test-util/resource-node project path)
            first-build-results  (project/build project resource-node {})
            second-build-results (project/build project resource-node {})
            main-collection      (test-util/resource-node project "/main/main.collection")]
        (is (every? #(> (count %) 0) [first-build-results second-build-results]))
        (is (not-any? :cached first-build-results))
        (is (every? :cached second-build-results))
        (g/transact (g/set-property main-collection :name "my-test-name"))
        (let [build-results (project/build project resource-node {})]
          (is (> (count build-results) 0))
          (is (not-every? :cached build-results)))
        (reset! (g/node-value project :build-cache) {})
        (let [build-results (project/build project resource-node {})]
          (is (> (count build-results) 0))
          (is (not-any? :cached first-build-results)))))))

(defn- build-path [workspace proj-path]
  (str (workspace/build-path workspace) proj-path))

(defn- abs-project-path [workspace proj-path]
  (str (workspace/project-path workspace) proj-path))

(defn mtime [path]
  (.lastModified (File. path)))

(deftest build-and-write-cached
  (testing "Verify the build cache works when building to disk"
    (with-clean-system
      (let [workspace (test-util/setup-scratch-workspace! world project-path)
            project (test-util/setup-project! workspace)
            path "/game.project"
            game-project (test-util/resource-node project "/game.project")
            main-collection (test-util/resource-node project "/main/main.collection")]
        (project/build-and-write project game-project {})
        (let [first-mtime (mtime (build-path workspace "/main/main.collectionc"))]
          (Thread/sleep 1000)
          (project/build-and-write project game-project {})
          (let [test-mtime (mtime (build-path workspace "/main/main.collectionc"))]
            (is (= first-mtime test-mtime)))
          (g/transact (g/set-property main-collection :name "my-test-name"))
          (Thread/sleep 1000)
          (project/build-and-write project game-project {})
          (let [test-mtime (mtime (build-path workspace "/main/main.collectionc"))]
            (is (not (= first-mtime test-mtime)))))))))

(deftest prune-build-cache
  (testing "Verify the build cache works as expected"
    (with-clean-system
      (let [workspace     (test-util/setup-workspace! world project-path)
            project       (test-util/setup-project! workspace)
            path          "/main/main.collection"
            resource-node (test-util/resource-node project path)
            _             (project/build project resource-node {})
            cache-count   (count @(g/node-value project :build-cache))]
        (g/transact
         (for [[node-id label] (g/sources-of resource-node :dep-build-targets)]
           (g/delete-node node-id)))
        (project/build project resource-node {})
        (is (< (count @(g/node-value project :build-cache)) cache-count))))))

(deftest prune-fs-build-cache
  (testing "Verify the fs build cache works as expected"
    (with-clean-system
      (let [workspace     (test-util/setup-workspace! world project-path)
            project       (test-util/setup-project! workspace)
            path          "/main/main.collection"
            resource-node (test-util/resource-node project path)
            _             (project/build-and-write project resource-node {})
            cache-count   (count @(g/node-value project :fs-build-cache))]
        (g/transact
         (for [[node-id label] (g/sources-of resource-node :dep-build-targets)]
           (g/delete-node node-id)))
        (project/build-and-write project resource-node {})
        (is (< (count @(g/node-value project :fs-build-cache)) cache-count))))))

(deftest build-atlas
  (testing "Building atlas"
    (with-build-results "/background/background.atlas"
      (let [content (get content-by-source "/background/background.atlas")
            desc    (TextureSetProto$TextureSet/parseFrom content)]
        (is (contains? content-by-target (.getTexture desc)))))))

(deftest build-atlas-with-error
  (testing "Building atlas with error"
    (with-clean-system
      (let [workspace         (test-util/setup-workspace! world project-path)
            project           (test-util/setup-project! workspace)
            path              "/background/background.atlas"
            resource-node     (test-util/resource-node project path)
            _                 (g/set-property! resource-node :margin -42)
            build-error       (atom nil)
            build-results     (project/build project resource-node {:render-progress! progress/null-render-progress!
                                                                    :render-error!    #(reset! build-error %)})]
        (is (nil? build-results))
        (is (instance? internal.graph.error_values.ErrorValue @build-error))))))

(deftest build-font
  (testing "Building font"
    (with-build-results "/fonts/score.font"
      (let [content (get content-by-source "/fonts/score.font")
            desc    (protobuf/bytes->map Font$FontMap content)]
        (is (= 1024 (:cache-width desc)))
        (is (= 256 (:cache-height desc)))))))

(deftest build-mesh
  (testing "Building mesh"
    (with-build-results "/model/book_of_defold.dae"
      (let [content (get content-by-source "/model/book_of_defold.dae")
            desc    (Mesh$MeshDesc/parseFrom content)]
        (is (= "Book" (-> desc (.getComponentsList) (first) (.getName))))))))

(deftest build-model
  (testing "Building model"
    (with-build-results "/model/book_of_defold.model"
      (let [content (get content-by-source "/model/book_of_defold.model")
            desc    (Model$ModelDesc/parseFrom content)]
        (is (= "/model/book_of_defold.meshc" (-> desc (.getMesh))))))))

(deftest build-script-properties
  (with-build-results "/script/props.collection"
    (doseq [[res-path pb decl-path] [["/script/props.script" Lua$LuaModule [:properties]]
                                     ["/script/props.go" GameObject$PrototypeDesc [:components 0 :property-decls]]
                                     ["/script/props.collection" GameObject$CollectionDesc [:instances 0 :component-properties 0 :property-decls]]
                                     ["/script/props.collection" GameObject$CollectionDesc [:instances 1 :component-properties 0 :property-decls]]]]
      (let [content (get content-by-source res-path)
            desc (protobuf/bytes->map pb content)
            decl (get-in desc decl-path)]
        (is (not-empty (:number-entries decl)))
        (is (not-empty (:hash-entries decl)))
        (is (not-empty (:url-entries decl)))
        (is (not-empty (:vector3-entries decl)))
        (is (not-empty (:vector4-entries decl)))
        (is (not-empty (:quat-entries decl)))
        (is (not-empty (:bool-entries decl)))
        (is (not-empty (:float-values decl)))
        (is (not-empty (:hash-values decl)))
        (is (not-empty (:string-values decl))))))
  (with-build-results "/script/sub_props.collection"
    (doseq [[res-path pb decl-path] [["/script/sub_props.collection" GameObject$CollectionDesc [:instances 0 :component-properties 0 :property-decls]]]]
      (let [content (get content-by-source res-path)
            desc (protobuf/bytes->map pb content)
            decl (get-in desc decl-path)]
        (is (not-empty (:number-entries decl)))
        (is (not-empty (:hash-entries decl)))
        (is (not-empty (:url-entries decl)))
        (is (not-empty (:vector3-entries decl)))
        (is (not-empty (:vector4-entries decl)))
        (is (not-empty (:quat-entries decl)))
        (is (not-empty (:bool-entries decl)))
        (is (not-empty (:float-values decl)))
        (is (not-empty (:hash-values decl)))
        (is (not-empty (:string-values decl)))))
    ;; Sub-collections should not be built separately
    (is (not (contains? content-by-source "/script/props.collection"))))
  (with-build-results "/script/sub_sub_props.collection"
    (doseq [[res-path pb decl-path] [["/script/sub_sub_props.collection" GameObject$CollectionDesc [:instances 0 :component-properties 0 :property-decls]]]]
      (let [content (get content-by-source res-path)
            desc (protobuf/bytes->map pb content)
            decl (get-in desc decl-path)]
        (is (not-empty (:number-entries decl)))
        (is (not-empty (:hash-entries decl)))
        (is (not-empty (:url-entries decl)))
        (is (not-empty (:vector3-entries decl)))
        (is (not-empty (:vector4-entries decl)))
        (is (not-empty (:quat-entries decl)))
        (is (not-empty (:bool-entries decl)))
        (is (not-empty (:float-values decl)))
        (is (not-empty (:hash-values decl)))
        (is (not-empty (:string-values decl)))))
    ;; Sub-collections should not be built separately
    (is (not (contains? content-by-source "/script/sub_props.collection")))))

(deftest build-gui-templates
  (with-clean-system
    ;; Reads from test_project rather than build_project
    (let [workspace         (test-util/setup-workspace! world)
          project           (test-util/setup-project! workspace)
          path              "/gui/scene.gui"
          resource-node     (test-util/resource-node project path)
          build-results     (project/build project resource-node {})
          content-by-source (into {} (map #(do [(resource/proj-path (:resource (:resource %))) (:content %)]) build-results))
          content-by-target (into {} (map #(do [(resource/proj-path (:resource %)) (:content %)]) build-results))
          content           (get content-by-source path)
          desc              (protobuf/pb->map (Gui$SceneDesc/parseFrom content))]
      (is (= ["box" "pie" "sub_scene/sub_box" "box1" "text"] (mapv :id (:nodes desc))))
      (let [sub (get-in desc [:nodes 2])]
        (is (= "layer" (:layer sub)))
        (is (= 0.5 (:alpha sub)))
        (is (= [1.0 1.0 1.0 1.0] (:scale sub)))
        (is (= [0.0 0.0 0.0 1.0] (:rotation sub)))
        (is (= [1100.0 640.0 0.0 1.0] (:position sub))))
      (is (contains? content-by-source "/graphics/atlas.atlas"))
      (is (contains? content-by-source "/fonts/big_score.font"))
      (let [textures (zipmap (map :name (:textures desc)) (map :texture (:textures desc)))]
        (is (= "/gui/gui.texturesetc" (get textures "main")))
        (is (= "/graphics/atlas.texturesetc" (get textures "sub_main"))))
      (let [fonts (zipmap (map :name (:fonts desc)) (map :font (:fonts desc)))]
        (is (= "/builtins/fonts/system_font.fontc" (get fonts "system_font")))
        (is (= "/fonts/big_score.fontc" (get fonts "sub_font")))))))

(deftest build-game-project
  (with-build-results "/game.project"
    (let [content (get content-by-source "/game.project")]
      (is (some? (re-find #"/main/main\.collectionc" (String. content "UTF-8")))))))

(deftest build-game-project-with-error
  (with-clean-system
    (let [workspace           (test-util/setup-workspace! world project-path)
          project             (test-util/setup-project! workspace)
          path                "/game.project"
          resource-node       (test-util/resource-node project path)
          atlas-path          "/background/background.atlas"
          atlas-resource-node (test-util/resource-node project atlas-path)
          _                   (g/set-property! atlas-resource-node :inner-padding -42)
          build-error         (atom nil)
          build-results       (project/build project resource-node {:render-progress! progress/null-render-progress!
                                                                    :render-error!    #(reset! build-error %)})]
      (is (nil? build-results))
      (is (instance? internal.graph.error_values.ErrorValue @build-error)))))

(defmacro with-setting [path value & body]
  ;; assumes game-project in scope
  (let [path-list (string/split path #"/")]
    `(let [initial-settings# (g/node-value ~'game-project :raw-settings)]
           (g/transact (g/update-property ~'game-project :raw-settings conj {:path ~path-list :value ~value}))
           (try
             ~@body
             (finally
               (g/set-property ~'game-project :raw-settings initial-settings#))))))

(defn- check-file-contents [workspace specs]
  (doseq [[path content] specs]
    (let [file (File. (build-path workspace path))]
      (is (true? (.exists file)))
      (is (= (slurp file) content)))))

(deftest build-with-custom-resources
  (with-clean-system
    (let [workspace (test-util/setup-scratch-workspace! world "test/resources/custom_resources_project")
          project (test-util/setup-project! workspace)
          game-project (test-util/resource-node project "/game.project")]
      (with-setting "project/custom_resources" "root.stuff"
        (project/build-and-write project game-project {})
        (check-file-contents workspace [["root.stuff" "root.stuff"]])
      (with-setting "project/custom_resources" "/root.stuff"
        (project/build-and-write project game-project {})
        (check-file-contents workspace [["root.stuff" "root.stuff"]])
      (with-setting "project/custom_resources" "assets"
        (project/build-and-write project game-project {})
        (check-file-contents workspace
                             [["/assets/some.stuff" "some.stuff"]
                              ["/assets/some2.stuff" "some2.stuff"]]))
      (with-setting "project/custom_resources" "/assets"
        (project/build-and-write project game-project {})
        (check-file-contents workspace
                             [["/assets/some.stuff" "some.stuff"]
                              ["/assets/some2.stuff" "some2.stuff"]]))
      (with-setting "project/custom_resources" "/assets, root.stuff"
        (project/build-and-write project game-project {})
        (check-file-contents workspace
                             [["/assets/some.stuff" "some.stuff"]
                              ["/assets/some2.stuff" "some2.stuff"]
                              ["/root.stuff" "root.stuff"]]))
      (with-setting "project/custom_resources" "assets, root.stuff, /more_assets/"
        (project/build-and-write project game-project {})
        (check-file-contents workspace
                             [["/assets/some.stuff" "some.stuff"]
                              ["/assets/some2.stuff" "some2.stuff"]
                              ["/root.stuff" "root.stuff"]
                              ["/more_assets/some_more.stuff" "some_more.stuff"]
                              ["/more_assets/some_more2.stuff" "some_more2.stuff"]]))
      (with-setting "project/custom_resources" "nonexistent_path"
        (project/build-and-write project game-project {})
        (doseq [path ["/assets/some.stuff" "/assets/some2.stuff"
                      "/root.stuff"
                      "/more_assets/some_more.stuff" "/more_assets/some_more2.stuff"]]
          (is (false? (.exists (File. (build-path workspace path))))))))))))

(deftest custom-resources-cached
  (testing "Check custom resources are only rebuilt when source has changed"
    (with-clean-system
      (let [workspace (test-util/setup-scratch-workspace! world "test/resources/custom_resources_project")
            project (test-util/setup-project! workspace)
            game-project (test-util/resource-node project "/game.project")]
        (with-setting "project/custom_resources" "assets"
          (project/build-and-write project game-project {})
          (let [initial-some-mtime (mtime (build-path workspace "/assets/some.stuff"))
                initial-some2-mtime (mtime (build-path workspace "/assets/some2.stuff"))]
            (Thread/sleep 1000)
            (spit (File. (abs-project-path workspace "/assets/some.stuff")) "new stuff")
            (workspace/resource-sync! workspace)
            (project/build-and-write project game-project {})
            (is (not (= initial-some-mtime (mtime (build-path workspace "/assets/some.stuff")))))
            (is (= initial-some2-mtime (mtime (build-path workspace "/assets/some2.stuff"))))))))))

                               
