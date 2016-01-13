(ns integration.build-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system]]
            [editor.math :as math]
            [editor.project :as project]
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
           [com.dynamo.spine.proto Spine$SpineScene]
           [com.dynamo.mesh.proto Mesh$MeshDesc]
           [com.dynamo.model.proto Model$ModelDesc]
           [com.dynamo.properties.proto PropertiesProto$PropertyDeclarations]
           [com.dynamo.lua.proto Lua$LuaModule]
           [editor.types Region]
           [editor.workspace BuildResource]
           [java.awt.image BufferedImage]
           [java.io File]
           [javax.imageio ImageIO]
           [javax.vecmath Point3d Matrix4d]
           [org.apache.commons.io FilenameUtils]))

(def project-path "resources/build_project/SideScroller")

(defn- ->BuildResource
  ([project path]
    (->BuildResource project path nil))
  ([project path prefix]
    (let [node (test-util/resource-node project path)]
      (workspace/make-build-resource (g/node-value node :resource) prefix))))

(def pb-cases [{:label "ParticleFX"
                :path "/main/blob.particlefx"
                :pb-class Particle$ParticleFX
                :test-fn (fn [pb]
                           (is (= -10.0 (get-in pb [:emitters 0 :modifiers 0 :position 0]))))}
               {:label "Sound"
                :path "/main/sound.sound"
                :pb-class Sound$SoundDesc
                :resource-fields [:sound]}])

(defn- run-pb-case [case content-by-source content-by-target]
  (testing (str "Testing " (:label case))
           (let [content (get content-by-source (:path case))
                 pb (protobuf/bytes->map (:pb-class case) content)
                 test-fn (:test-fn case)
                 res-fields [:sound]]
            (when test-fn
              (test-fn pb))
            (doseq [field (:resource-fields case)]
              (is (contains? content-by-target (get pb field)))
              (is (> (count (get content-by-target (get pb field))) 0))))))

(deftest build-game-project
  (with-clean-system
    (let [workspace     (test-util/setup-workspace! world project-path)
          project       (test-util/setup-project! workspace)
          path          "/game.project"
          resource-node (test-util/resource-node project path)
          build-results (project/build project resource-node)
          content-by-source (into {} (map #(do [(resource/proj-path (:resource (:resource %))) (:content %)]) build-results))
          content-by-target (into {} (map #(do [(resource/proj-path (:resource %)) (:content %)]) build-results))
          target-exts (into #{} (map #(:build-ext (resource/resource-type (:resource %))) build-results))
          exp-paths [path
                     "/main/main.collection"
                     "/main/main.script"
                     "/input/game.input_binding"
                     "/builtins/render/default.render"
                     "/builtins/render/default.render_script"
                     "/background/bg.png"
                     "/builtins/graphics/particle_blob.tilesource"
                     "/main/blob.tilemap"]
          exp-exts ["vpc" "fpc" "texturec"]]
      (doseq [case pb-cases]
        (run-pb-case case content-by-source content-by-target))
      (doseq [ext exp-exts]
        (is (contains? target-exts ext)))
      (doseq [path exp-paths]
        (is (contains? content-by-source path)))
      (let [content (get content-by-source "/main/main.collection")
            desc (GameObject$CollectionDesc/parseFrom content)
            go-ext (:build-ext (workspace/get-resource-type workspace "go"))]
        (doseq [inst (.getInstancesList desc)]
          (is (.endsWith (.getPrototype inst) go-ext)))))))

(deftest build-coll-hierarchy
  (testing "Building collection hierarchies"
           (with-clean-system
             (let [workspace     (test-util/setup-workspace! world project-path)
                   project       (test-util/setup-project! workspace)
                   path          "/hierarchy/base.collection"
                   resource-node (test-util/resource-node project path)
                   build-results (project/build project resource-node)
                   content-by-source (into {} (map #(do [(resource/proj-path (:resource (:resource %))) (:content %)]) build-results))]
               (let [content (get content-by-source "/hierarchy/base.collection")
                     desc (GameObject$CollectionDesc/parseFrom content)
                     instances (.getInstancesList desc)
                     go-ext (:build-ext (workspace/get-resource-type workspace "go"))]
                 (is (= 3 (count instances)))
                 (let [ids (loop [instances (vec instances)
                                  ids (transient #{})]
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
                     (is (every? ids (.getChildrenList inst))))))))))

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
                             build-results (project/build project resource-node)
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
           (with-clean-system
             (let [workspace     (test-util/setup-workspace! world project-path)
                   project       (test-util/setup-project! workspace)
                   path          "/main/raw_sound.go"
                   resource-node (test-util/resource-node project path)
                   build-results (project/build project resource-node)
                   content-by-source (into {} (map #(do [(resource/proj-path (:resource (:resource %))) (:content %)])
                                                   build-results))
                   content-by-target (into {} (map #(do [(resource/proj-path (:resource %)) (:content %)])
                                                   build-results))]
               (let [content (get content-by-source path)
                     desc (protobuf/bytes->map GameObject$PrototypeDesc content)
                     sound-path (get-in desc [:components 0 :component])
                     ext (FilenameUtils/getExtension sound-path)]
                 (is (= ext "soundc"))
                 (let [sound-desc (protobuf/bytes->map Sound$SoundDesc (content-by-target sound-path))]
                   (is (contains? content-by-target (:sound sound-desc)))))))))

(defn- first-source [node label]
  (ffirst (g/sources-of node label)))

(deftest break-merged-targets
  (testing "Verify equivalent game objects are not merged after being changed in memory"
           (with-clean-system
             (let [workspace     (test-util/setup-workspace! world project-path)
                   project       (test-util/setup-project! workspace)]
               (let [path "/merge/merge_embed.collection"
                     resource-node (test-util/resource-node project path)
                     build-results (project/build project resource-node)
                     content-by-source (into {} (map #(do [(resource/proj-path (:resource (:resource %))) (:content %)])
                                                     build-results))
                     content-by-target (into {} (map #(do [(resource/proj-path (:resource %)) (:content %)])
                                                     build-results))]
                 (is (= 1 (count-exts (keys content-by-target) "goc")))
                 (is (= 1 (count-exts (keys content-by-target) "spritec")))
                 (let [go-node (first-source (first-source resource-node :child-scenes) :source)
                       comp-node (first-source go-node :child-scenes)]
                   (g/transact (g/delete-node comp-node))
                   (let [build-results (project/build project resource-node)
                         content-by-target (into {} (map #(do [(resource/proj-path (:resource %)) (:content %)])
                                                         build-results))]
                     (is (= 1 (count-exts (keys content-by-target) "goc")))
                     (is (= 1 (count-exts (keys content-by-target) "spritec"))))))))))

(deftest build-cached
  (testing "Verify the build cache works as expected"
           (with-clean-system
             (let [workspace     (test-util/setup-workspace! world project-path)
                   project       (test-util/setup-project! workspace)]
               (let [path "/game.project"
                     resource-node (test-util/resource-node project path)
                     first-build-results (project/build project resource-node)
                     second-build-results (project/build project resource-node)
                     main-collection (test-util/resource-node project "/main/main.collection")]
                 (is (every? #(> (count %) 0) [first-build-results second-build-results]))
                 (is (not-any? :cached first-build-results))
                 (is (every? :cached second-build-results))
                 (g/transact (g/set-property main-collection :name "my-test-name"))
                 (let [build-results (project/build project resource-node)]
                   (is (> (count build-results) 0))
                   (is (not-every? :cached build-results)))
                 (project/clear-build-cache project)
                 (let [build-results (project/build project resource-node)]
                   (is (> (count build-results) 0))
                   (is (not-any? :cached first-build-results))))))))

(deftest prune-build-cache
  (testing "Verify the build cache works as expected"
           (with-clean-system
             (let [workspace     (test-util/setup-workspace! world project-path)
                   project       (test-util/setup-project! workspace)]
               (let [path "/main/main.collection"
                     resource-node (test-util/resource-node project path)
                     _ (project/build project resource-node)
                     cache-count (count @(g/node-value project :build-cache))]
                 (g/transact
                   (for [[node-id label] (g/sources-of resource-node :dep-build-targets)]
                     (g/delete-node node-id)))
                 (project/build project resource-node)
                 (is (< (count @(g/node-value project :build-cache)) cache-count)))))))

(deftest prune-fs-build-cache
  (testing "Verify the fs build cache works as expected"
           (with-clean-system
             (let [workspace     (test-util/setup-workspace! world project-path)
                   project       (test-util/setup-project! workspace)]
               (let [path "/main/main.collection"
                     resource-node (test-util/resource-node project path)
                     _ (project/build-and-write project resource-node)
                     cache-count (count @(g/node-value project :fs-build-cache))]
                 (g/transact
                   (for [[node-id label] (g/sources-of resource-node :dep-build-targets)]
                     (g/delete-node node-id)))
                 (project/build-and-write project resource-node)
                 (is (< (count @(g/node-value project :fs-build-cache)) cache-count)))))))

(deftest build-atlas
  (testing "Building atlas"
           (with-clean-system
             (let [workspace     (test-util/setup-workspace! world project-path)
                   project       (test-util/setup-project! workspace)
                   path          "/background/background.atlas"
                   resource-node (test-util/resource-node project path)
                   build-results (project/build project resource-node)
                   content-by-source (into {} (map #(do [(resource/proj-path (:resource (:resource %))) (:content %)]) build-results))
                   content-by-target (into {} (map #(do [(resource/proj-path (:resource %)) (:content %)]) build-results))]
               (let [content (get content-by-source path)
                     desc (TextureSetProto$TextureSet/parseFrom content)]
                 (is (contains? content-by-target (.getTexture desc))))))))

(deftest build-font
  (testing "Building font"
           (with-clean-system
             (let [workspace     (test-util/setup-workspace! world project-path)
                   project       (test-util/setup-project! workspace)
                   path          "/fonts/score.font"]
               (let [resource-node (test-util/resource-node project path)
                     build-results (project/build project resource-node)
                     content-by-source (into {} (map #(do [(resource/proj-path (:resource (:resource %))) (:content %)]) build-results))
                     content-by-target (into {} (map #(do [(resource/proj-path (:resource %)) (:content %)]) build-results))
                     content (get content-by-source path)
                     desc (protobuf/bytes->map Font$FontMap content)]
                 (is (= 1024 (:cache-width desc)))
                 (is (= 256 (:cache-height desc))))))))

(deftest build-spine-scene
  (testing "Building spine scene"
           (with-clean-system
             (let [workspace     (test-util/setup-workspace! world project-path)
                   project       (test-util/setup-project! workspace)
                   path          "/player/spineboy.spinescene"
                   resource-node (test-util/resource-node project path)
                   build-results (project/build project resource-node)
                   content-by-source (into {} (map #(do [(resource/proj-path (:resource (:resource %))) (:content %)]) build-results))
                   content-by-target (into {} (map #(do [(resource/proj-path (:resource %)) (:content %)]) build-results))]
               (let [content (get content-by-source path)
                     desc (Spine$SpineScene/parseFrom content)
                     bones (-> desc (.getSkeleton) (.getBonesList))
                     meshes (-> desc (.getMeshSet) (.getMeshEntriesList) (first) (.getMeshesList))]
                 (is (contains? content-by-target (.getTextureSet desc))))))))

(deftest build-mesh
  (testing "Building mesh"
           (with-clean-system
             (let [workspace     (test-util/setup-workspace! world project-path)
                   project       (test-util/setup-project! workspace)
                   path          "/model/book_of_defold.dae"
                   resource-node (test-util/resource-node project path)
                   build-results (project/build project resource-node)
                   content-by-source (into {} (map #(do [(resource/proj-path (:resource (:resource %))) (:content %)]) build-results))
                   content-by-target (into {} (map #(do [(resource/proj-path (:resource %)) (:content %)]) build-results))]
               (let [content (get content-by-source path)
                     desc (Mesh$MeshDesc/parseFrom content)]
                 #_(prn desc)
                 #_(is (contains? content-by-target (.getTextureSet desc))))))))

(deftest build-model
  (testing "Building model"
           (with-clean-system
             (let [workspace     (test-util/setup-workspace! world project-path)
                   project       (test-util/setup-project! workspace)
                   path          "/model/book_of_defold.model"
                   resource-node (test-util/resource-node project path)
                   build-results (project/build project resource-node)
                   content-by-source (into {} (map #(do [(resource/proj-path (:resource (:resource %))) (:content %)]) build-results))
                   content-by-target (into {} (map #(do [(resource/proj-path (:resource %)) (:content %)]) build-results))]
               (let [content (get content-by-source path)
                     desc (Model$ModelDesc/parseFrom content)]
                 #_(prn desc)
                 #_(is (contains? content-by-target (.getTextureSet desc))))))))

(deftest build-script
  (with-clean-system
    (let [workspace     (test-util/setup-workspace! world project-path)
          project       (test-util/setup-project! workspace)
          path          "/script/props.script"
          resource-node (test-util/resource-node project path)
          build-results (project/build project resource-node)
          content-by-source (into {} (map #(do [(resource/proj-path (:resource (:resource %))) (:content %)]) build-results))
          content-by-target (into {} (map #(do [(resource/proj-path (:resource %)) (:content %)]) build-results))]
      (let [content (get content-by-source path)
            lua (Lua$LuaModule/parseFrom content)
            decl (-> lua (.getProperties))]
        (is (< 0 (.getNumberEntriesCount decl)))
        (is (< 0 (.getHashEntriesCount decl)))
        (is (< 0 (.getUrlEntriesCount decl)))
        (is (< 0 (.getVector3EntriesCount decl)))
        (is (< 0 (.getVector4EntriesCount decl)))
        (is (< 0 (.getQuatEntriesCount decl)))
        (is (< 0 (.getBoolEntriesCount decl)))
        (is (< 0 (.getFloatValuesCount decl)))
        (is (< 0 (.getHashValuesCount decl)))
        (is (< 0 (.getStringValuesCount decl)))))))

(deftest build-script-properties
  (with-clean-system
    (let [workspace     (test-util/setup-workspace! world project-path)
          project       (test-util/setup-project! workspace)
          path          "/script/props.go"
          resource-node (test-util/resource-node project path)
          build-results (project/build project resource-node)
          content-by-source (into {} (map #(do [(resource/proj-path (:resource (:resource %))) (:content %)]) build-results))
          content-by-target (into {} (map #(do [(resource/proj-path (:resource %)) (:content %)]) build-results))]
      (let [content (get content-by-source path)
            desc (GameObject$PrototypeDesc/parseFrom content)
            decl (-> desc (.getComponents 0) (.getPropertyDecls))]
        (is (< 0 (.getNumberEntriesCount decl)))
        (is (< 0 (.getHashEntriesCount decl)))
        (is (< 0 (.getUrlEntriesCount decl)))
        (is (< 0 (.getVector3EntriesCount decl)))
        (is (< 0 (.getVector4EntriesCount decl)))
        (is (< 0 (.getQuatEntriesCount decl)))
        (is (< 0 (.getBoolEntriesCount decl)))
        (is (< 0 (.getFloatValuesCount decl)))
        (is (< 0 (.getHashValuesCount decl)))
        (is (< 0 (.getStringValuesCount decl)))))))
