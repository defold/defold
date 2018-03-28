(ns integration.build-test
  (:require [clojure.test :refer :all]
            [clojure.java.io :as io]
            [clojure.set :as set]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system]]
            [editor.math :as math]
            [editor.fs :as fs]
            [editor.game-project :as game-project]
            [editor.defold-project :as project]
            [editor.pipeline :as pipeline]
            [editor.progress :as progress]
            [editor.protobuf :as protobuf]
            [editor.workspace :as workspace]
            [editor.resource :as resource]
            [util.murmur :as murmur]
            [integration.test-util :refer [with-loaded-project] :as test-util])
  (:import [com.dynamo.gameobject.proto GameObject$CollectionDesc GameObject$PrototypeDesc]
           [com.dynamo.gamesystem.proto GameSystem$CollectionProxyDesc]
           [com.dynamo.textureset.proto TextureSetProto$TextureSet]
           [com.dynamo.render.proto Font$FontMap]
           [com.dynamo.particle.proto Particle$ParticleFX]
           [com.dynamo.sound.proto Sound$SoundDesc]
           [com.dynamo.rig.proto Rig$RigScene Rig$Skeleton Rig$AnimationSet Rig$MeshSet]
           [com.dynamo.model.proto ModelProto$Model]
           [com.dynamo.physics.proto Physics$CollisionObjectDesc]
           [com.dynamo.label.proto Label$LabelDesc]
           [com.dynamo.lua.proto Lua$LuaModule]
           [com.dynamo.gui.proto Gui$SceneDesc]
           [com.dynamo.spine.proto Spine$SpineModelDesc]
           [java.io ByteArrayOutputStream File]
           [org.apache.commons.io FilenameUtils IOUtils]))

(def project-path "test/resources/build_project/SideScroller")

(defn- ->BuildResource
  ([project path]
   (->BuildResource project path nil))
  ([project path prefix]
   (let [node (test-util/resource-node project path)]
     (workspace/make-build-resource (g/node-value node :resource) prefix))))

(def target-pb-classes {"rigscenec" Rig$RigScene
                        "skeletonc" Rig$Skeleton
                        "animationsetc" Rig$AnimationSet
                        "meshsetc" Rig$MeshSet
                        "texturesetc" TextureSetProto$TextureSet
                        "collectionproxyc" GameSystem$CollectionProxyDesc
                        "collectionc" GameObject$CollectionDesc})

(defn- target [path targets]
  (let [ext (FilenameUtils/getExtension path)
        pb-class (get target-pb-classes ext)]
    (when (nil? pb-class)
      (throw (ex-info (str "No target-pb-classes entry for extension \"" ext "\", path \"" path "\".")
                      {:ext ext
                       :path path})))
    (protobuf/bytes->map pb-class (get targets path))))

(defn- approx? [as bs]
  (every? #(< % 0.00001)
          (map #(Math/abs (- %1 %2))
               as bs)))

(def pb-cases {"/game.project"
               [{:label    "ParticleFX"
                 :path     "/main/blob.particlefx"
                 :pb-class Particle$ParticleFX
                 :test-fn  (fn [pb targets]
                             (is (= -10.0 (get-in pb [:emitters 0 :modifiers 0 :position 0]))))}
                {:label    "ParticleFX with complex setup"
                 :path     "/main/tail.particlefx"
                 :pb-class Particle$ParticleFX
                 :test-fn  (fn [pb targets]
                             (let [emitter (-> pb :emitters first)]
                               (is (= :size-mode-auto (:size-mode emitter)))
                               (is (not-empty (:properties emitter)))
                               (is (not-empty (:particle-properties emitter)))
                               (is (true? (every? (comp :points not-empty) (:properties emitter))))
                               (is (true? (every? (comp :points not-empty) (:particle-properties emitter))))
                               (is (= 6.0 (:duration emitter)))
                               (is (= 1.0 (:duration-spread emitter)))
                               (is (= 0.0 (:start-delay emitter)))
                               (is (= 2.0 (:start-delay-spread emitter)))
                               (let [modifier (-> emitter :modifiers first)]
                                 (is (not-empty (:properties modifier)))
                                 (is (true? (every? (comp :points not-empty) (:properties modifier))))
                                 (is (approx? [-20.0 10.0 -30.0] (:position modifier)))
                                 (is (approx? [0.0 0.0 -0.70710677 0.70710677] (:rotation modifier))))))}
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
                            (is (= 0 (-> pb :mesh-set (target targets) :mesh-entries first :id)))
                            (is (< 0 (-> pb :mesh-set (target targets) :mesh-entries count)))
                            (is (< 0 (-> pb :animation-set (target targets) :animations count)))
                            (is (< 0 (-> pb :skeleton (target targets) :bones count))))}
                {:label "Spine Scene with weighted mesh"
                 :path "/ladder/ladder.spinescene"
                 :pb-class Rig$RigScene
                 :resource-fields [:texture-set :skeleton :animation-set :mesh-set]
                 :test-fn (fn [pb targets]
                            (let [mesh-set (-> pb :mesh-set (target targets))]
                              (doseq [mesh-entry (:mesh-entries mesh-set)]
                                (doseq [mesh (:meshes mesh-entry)]
                                  (is (= (/ (count (:positions mesh)) 3)
                                         (/ (count (:bone-indices mesh)) 4)))))))}
                {:label "Spine Scene with IKs and IK animation"
                 :path "/raptor/raptor.spinescene"
                 :pb-class Rig$RigScene
                 :resource-fields [:texture-set :skeleton :animation-set :mesh-set]
                 :test-fn (fn [pb targets]
                            (is (some? (-> pb :texture-set (target targets) :texture)))
                            (is (= 0 (-> pb :mesh-set (target targets) :mesh-entries first :id)))
                            (is (< 0 (-> pb :mesh-set (target targets) :mesh-entries count)))
                            (is (< 0 (-> pb :animation-set (target targets) :animations count)))
                            (is (< 0 (-> pb :skeleton (target targets) :bones count)))
                            (is (< 0 (-> pb :skeleton (target targets) :iks count))))}
               {:label "Spine Model"
                :path "/player/spineboy.spinemodel"
                :pb-class Spine$SpineModelDesc
                :resource-fields [:spine-scene :material]}
               {:label "Label"
                :path "/main/label.label"
                :pb-class Label$LabelDesc
                :resource-fields [:font :material]
                :test-fn (fn [pb targets]
                           (is (= {:color [1.0 1.0 1.0 1.0],
                                   :line-break false,
                                   :scale [1.0 1.0 1.0 0.0],
                                   :blend-mode :blend-mode-alpha,
                                   :leading 1.0,
                                   :font "/builtins/fonts/system_font.fontc",
                                   :size [128.0 32.0 0.0 0.0],
                                   :tracking 0.0,
                                   :material "/builtins/fonts/label.materialc",
                                   :outline [0.0 0.0 0.0 1.0],
                                   :pivot :pivot-center,
                                   :shadow [0.0 0.0 0.0 1.0],
                                   :text "Label"}
                                 pb)))}
               {:label "Model"
                :path "/model/book_of_defold.model"
                :pb-class ModelProto$Model
                :resource-fields [:rig-scene :material]
                :test-fn (fn [pb targets]
                           (let [rig-scene (target (:rig-scene pb) targets)]
                             (is (= "" (:animation-set rig-scene)))
                             (is (= "" (:skeleton rig-scene)))
                             (is (= "" (:texture-set rig-scene)))

                             ;; TODO - id must be 0 currently because of the runtime
                             ;; (is (= (murmur/hash64 "Book") (-> pb :rig-scene (target targets) :mesh-set (target targets) :mesh-entries first :id))))})
                             (is (= 0 (-> rig-scene :mesh-set (target targets) :mesh-entries first :id)))))}
               {:label "Model with animations"
                :path "/model/treasure_chest.model"
                :pb-class ModelProto$Model
                :resource-fields [:rig-scene :material]
                :test-fn (fn [pb targets]
                           (let [rig-scene (target (:rig-scene pb) targets)
                                 animation-set (target (:animation-set rig-scene) targets)
                                 mesh-set (target (:mesh-set rig-scene) targets)
                                 skeleton (target (:skeleton rig-scene) targets)]
                             (is (= "" (:texture-set rig-scene)))

                             (let [animations (-> animation-set :animations)]
                               (is (= 2 (count animations)))
                               (is (= #{(murmur/hash64 "treasure_chest")
                                        (murmur/hash64 "treasure_chest_sub_animation/treasure_chest_anim_out")}
                                      (set (map :id animations)))))

                             (let [mesh (-> mesh-set :mesh-entries first :meshes first)]
                               (is (< 2 (-> mesh :indices count))))

                             ;; TODO - id must be 0 currently because of the runtime
                             ;; (is (= (murmur/hash64 "Book") (get-in pb [:mesh-entries 0 :id])))
                             (is (= 0 (-> mesh-set :mesh-entries first :id)))

                             (is (= 3 (count (:bones skeleton))))
                             (is (= (:bone-list mesh-set) (:bone-list animation-set)))
                             (is (set/subset? (:bone-list mesh-set) (set (map :id (:bones skeleton)))))))}]
               "/collection_proxy/with_collection.collectionproxy"
               [{:label "Collection proxy"
                 :path "/collection_proxy/with_collection.collectionproxy"
                 :pb-class GameSystem$CollectionProxyDesc
                 :resource-fields [:collection]}]
               "/gui/spine.gui"
               [{:label "Gui with spine scene"
                 :path "/gui/spine.gui"
                 :pb-class Gui$SceneDesc
                 :resource-fields [[:spine-scenes :spine-scene]]
                 :test-fn (fn [pb targets]
                            (let [main-node (first (filter #(= "spine" (:id %)) (:nodes pb)))
                                  nodes (into #{} (map :id (:nodes pb)))]
                              (is (= "" (:spine-skin main-node)))
                              (is (every? nodes ["spine" "spine/root" "box"]))))}]
               "/model/book_of_defold_no_tex.model"
               [{:label "Model with empty texture"
                 :path "/model/book_of_defold_no_tex.model"
                 :pb-class ModelProto$Model
                 :resource-fields [:rig-scene :material]
                 :test-fn (fn [pb targets]
                            (let [rig-scene (target (:rig-scene pb) targets)
                                  mesh-set (target (:mesh-set rig-scene) targets)]
                              (is (= "" (:texture-set rig-scene)))
                              (is (= [""] (:textures pb)))

                              (let [mesh (-> mesh-set :mesh-entries first :meshes first)]
                                (is (< 2 (-> mesh :indices count))))))}]})

(defn- run-pb-case [case content-by-source content-by-target]
  (testing (str "Testing " (:label case))
           (let [pb         (some->> (get content-by-source (:path case))
                              (protobuf/bytes->map (:pb-class case)))
                 test-fn    (:test-fn case)
                 res-fields [:sound]]
             (when test-fn
               (test-fn pb content-by-target))
             (doseq [field (:resource-fields case)]
               (doseq [field (if (vector? field)
                               (map-indexed (fn [i v] [(first field) i (second field)]) (get pb (first field)))
                               [[field]])
                       :let [path (get-in pb field)]]
                 (is (contains? content-by-target path))
                 (is (> (count (get content-by-target path)) 0)))))))

(defn- content-bytes [artifact]
  (with-open [in (io/input-stream (:resource artifact))
              out (ByteArrayOutputStream.)]
    (IOUtils/copy in out)
    (.toByteArray out)))

(defmacro with-build-results [path & forms]
  `(with-loaded-project project-path
     (let [~'path              ~path
           ~'resource-node     (test-util/resource-node ~'project ~path)
           evaluation-context# (g/make-evaluation-context)
           old-artifact-map#   (workspace/artifact-map ~'workspace)
           ~'build-results     (project/build! ~'project ~'resource-node evaluation-context# nil old-artifact-map# progress/null-render-progress!)
           ~'build-artifacts   (:artifacts ~'build-results)
           ~'_                 (when-not (contains? ~'build-results :error)
                                 (workspace/artifact-map! ~'workspace (:artifact-map ~'build-results))
                                 (workspace/etags! ~'workspace (:etags ~'build-results)))
           ~'_                 (g/update-cache-from-evaluation-context! evaluation-context#)
           ~'content-by-source (into {} (keep #(when-let [~'r (:resource (:resource %))]
                                                 [(resource/proj-path ~'r) (content-bytes %)])
                                              ~'build-artifacts))
           ~'content-by-target (into {} (keep #(when-let [~'r (:resource %)]
                                                 [(resource/proj-path ~'r) (content-bytes %)])
                                              ~'build-artifacts))]
       ~@forms)))

(deftest build-pb-cases
  (doseq [[path cases] pb-cases]
    (with-build-results path
      (doseq [case cases]
        (run-pb-case case content-by-source content-by-target)))))

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

(defn- project-build [project resource-node evaluation-context]
  (let [workspace (project/workspace project)
        old-artifact-map (workspace/artifact-map workspace)
        build-results (project/build! project resource-node evaluation-context nil old-artifact-map progress/null-render-progress!)]
    (when-not (contains? build-results :error)
      (workspace/artifact-map! workspace (:artifact-map build-results))
      (workspace/etags! workspace (:etags build-results)))
    build-results))

(defn- project-build-artifacts [project resource-node evaluation-context]
  (:artifacts (project-build project resource-node evaluation-context)))

(deftest merge-gos
  (testing "Verify equivalent game objects are merged"
    (with-loaded-project project-path
      (doseq [path ["/merge/merge_embed.collection"
                    "/merge/merge_refs.collection"]
              :let [resource-node (test-util/resource-node project path)
                    build-artifacts (project-build-artifacts project resource-node (g/make-evaluation-context))
                    content-by-source (into {} (map #(do [(resource/proj-path (:resource (:resource %))) (content-bytes %)])
                                                    build-artifacts))
                    content-by-target (into {} (map #(do [(resource/proj-path (:resource %)) (content-bytes %)])
                                                    build-artifacts))]]
        (is (= 1 (count-exts (keys content-by-target) "goc")))
        (is (= 1 (count-exts (keys content-by-target) "spritec")))
        (let [content      (get content-by-source path)
              desc         (GameObject$CollectionDesc/parseFrom content)
              target-paths (set (map #(resource/proj-path (:resource %)) build-artifacts))]
          (doseq [inst (.getInstancesList desc)
                  :let [prototype (.getPrototype inst)]]
            (is (contains? target-paths prototype))
            (let [content (get content-by-target prototype)
                  desc    (GameObject$PrototypeDesc/parseFrom content)]
              (doseq [comp (.getComponentsList desc)
                      :let [component (.getComponent comp)]]
                (is (contains? target-paths component))))))))))

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
  (with-build-results "/merge/merge_embed.collection"
    (is (= 1 (count-exts (keys content-by-target) "goc")))
    (is (= 1 (count-exts (keys content-by-target) "spritec")))
    (let [go-node   (first-source (first-source resource-node :child-scenes) :source-id)
          comp-node (first-source go-node :child-scenes)]
      (testing "Verify equivalent game objects are not merged after being changed in memory"
               (g/transact (g/delete-node comp-node))
               (let [build-artifacts   (project-build-artifacts project resource-node (g/make-evaluation-context))
                     content-by-target (into {} (map #(do [(resource/proj-path (:resource %)) (content-bytes %)])
                                                     build-artifacts))]
                 (is (= 2 (count-exts (keys content-by-target) "goc")))
                 (is (= 1 (count-exts (keys content-by-target) "spritec")))))
      (g/undo! (g/node-id->graph-id project))
      (testing "Verify equivalent sprites are not merged after being changed in memory"
               (let [sprite (test-util/prop-node-id comp-node :blend-mode)]
                 (test-util/prop! sprite :blend-mode :blend-mode-add)
                 (let [build-artifacts   (project-build-artifacts project resource-node (g/make-evaluation-context))
                       content-by-target (into {} (map #(do [(resource/proj-path (:resource %)) (content-bytes %)])
                                                       build-artifacts))]
                   (is (= 2 (count-exts (keys content-by-target) "goc")))
                   (is (= 2 (count-exts (keys content-by-target) "spritec")))))))))

(defmacro measure [& forms]
  `(let [start# (System/currentTimeMillis)]
     ~@forms
     (- (System/currentTimeMillis) start#)))

(deftest build-cached
  (testing "Verify the build cache works as expected"
    (with-loaded-project project-path
      (let [path          "/game.project"
            resource-node (test-util/resource-node project path)
            evaluation-context (g/make-evaluation-context)
            first-time    (measure (project-build-artifacts project resource-node evaluation-context))
            _ (g/update-cache-from-evaluation-context! evaluation-context)
            evaluation-context (g/make-evaluation-context)
            second-time   (measure (project-build-artifacts project resource-node evaluation-context))]
        (is (< (* 20 second-time) first-time))
        (let [atlas (test-util/resource-node project "/player/spineboy.atlas")]
          (g/transact (g/set-property atlas :margin 10))
          (let [third-time (measure (project-build-artifacts project resource-node (g/make-evaluation-context)))]
            (is (< (* 2 second-time) third-time))))))))

(defn- build-path [workspace proj-path]
  (io/file (workspace/build-path workspace) proj-path))

(defn- abs-project-path [workspace proj-path]
  (io/file (workspace/project-path workspace) proj-path))

(defn mtime [^File f]
  (.lastModified f))

(deftest build-atlas
  (testing "Building atlas"
    (with-build-results "/background/background.atlas"
      (let [content (get content-by-source "/background/background.atlas")
            desc    (TextureSetProto$TextureSet/parseFrom content)]
        (is (contains? content-by-target (.getTexture desc)))))))

(deftest build-atlas-with-error
  (testing "Building atlas with error"
    (with-loaded-project project-path
      (let [path              "/background/background.atlas"
            resource-node     (test-util/resource-node project path)
            _                 (g/set-property! resource-node :margin -42)
            build-results     (project-build project resource-node (g/make-evaluation-context))]
        (is (instance? internal.graph.error_values.ErrorValue (:error build-results)))))))

(deftest build-font
  (testing "Building font"
    (with-build-results "/fonts/score.font"
      (let [content (get content-by-source "/fonts/score.font")
            desc    (protobuf/bytes->map Font$FontMap content)]
        (is (= 1024 (:cache-width desc)))
        (is (= 256 (:cache-height desc)))))))

(deftest build-script
  (testing "Buildling a valid script succeeds"
    (with-build-results "/script/good.script"
      (let [content (get content-by-source "/script/good.script")
            module    (Lua$LuaModule/parseFrom content)
            source (.getSource module)]
        (is (pos? (.size (.getBytecode source))))
        (is (= "/script/good.script" (.getFilename source))))))
  (testing "Building a broken script fails"
    (with-build-results "/script/bad.script"
      (let [content (get content-by-source "/script/bad.script")]
        (is (nil? content))))))

(deftest build-script-properties
  (with-build-results "/script/props.collection"
    (doseq [[res-path pb decl-path] [["/script/props.script" Lua$LuaModule [:properties]]
                                     ["/script/props.go" GameObject$PrototypeDesc [:components 0 :property-decls]]
                                     ["/script/props.collection" GameObject$CollectionDesc [:instances 0 :component-properties 0 :property-decls]]]]
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
    (let [collection-content (get content-by-source "/script/props.collection")
          collection-desc (protobuf/bytes->map GameObject$CollectionDesc collection-content)
          instance-map (into {} (map (juxt :id identity) (:instances collection-desc)))
          embedded-props-target (:prototype (instance-map "/embedded_props"))
          embedded-content (content-by-target embedded-props-target)
          embedded-desc (protobuf/bytes->map GameObject$PrototypeDesc embedded-content)
          decl (get-in embedded-desc [:components 0 :property-decls])]
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

(deftest build-script-properties-override-values
  (with-build-results "/script/override.collection"
    (are [path pb-class val-path expected] (let [content (get content-by-source path)
                                                 desc (protobuf/bytes->map pb-class content)
                                                 float-values (get-in desc val-path)]
                                             (= [expected] float-values))
      "/script/override.script"      Lua$LuaModule             [:properties :float-values] 1.0
      "/script/override.go"          GameObject$PrototypeDesc  [:components 0 :property-decls :float-values] 2.0
      "/script/override.collection"  GameObject$CollectionDesc [:instances 0 :component-properties 0 :property-decls :float-values] 3.0))
  (with-build-results "/script/override_parent.collection"
    (are [path pb-class val-path expected] (let [content (get content-by-source path)
                                                 desc (protobuf/bytes->map pb-class content)
                                                 float-values (get-in desc val-path)]
                                             (= [expected] float-values))
      "/script/override_parent.collection" GameObject$CollectionDesc [:instances 0 :component-properties 0 :property-decls :float-values] 4.0)))

(deftest build-gui-templates
  ;; Reads from test_project rather than build_project
  (with-loaded-project
    (let [path              "/gui/scene.gui"
          resource-node     (test-util/resource-node project path)
          build-artifacts   (project-build-artifacts project resource-node (g/make-evaluation-context))
          content-by-source (into {} (map #(do [(resource/proj-path (:resource (:resource %))) (content-bytes %)]) build-artifacts))
          content-by-target (into {} (map #(do [(resource/proj-path (:resource %)) (content-bytes %)]) build-artifacts))
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
    (let [target-exts (into #{} (map #(:build-ext (resource/resource-type (:resource %))) build-artifacts))
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
      (doseq [ext exp-exts]
        (is (contains? target-exts ext)))
      (doseq [path exp-paths]
        (is (contains? content-by-source path)))
      (let [content (get content-by-source "/main/main.collection")
            desc    (GameObject$CollectionDesc/parseFrom content)
            go-ext  (:build-ext (workspace/get-resource-type workspace "go"))]
        (doseq [inst (.getInstancesList desc)]
          (is (.endsWith (.getPrototype inst) go-ext)))))
    (let [content (get content-by-source "/game.project")]
      (is (some? (re-find #"/main/main\.collectionc" (String. content "UTF-8")))))))

(deftest build-game-project-with-error
  (with-loaded-project project-path
    (let [path                "/game.project"
          resource-node       (test-util/resource-node project path)
          atlas-path          "/background/background.atlas"
          atlas-resource-node (test-util/resource-node project atlas-path)
          _                   (g/set-property! atlas-resource-node :inner-padding -42)
          build-results       (project-build project resource-node (g/make-evaluation-context))]
      (is (instance? internal.graph.error_values.ErrorValue (:error build-results))))))

(defmacro with-setting [path value & body]
  ;; assumes game-project in scope
  (let [path-list (string/split path #"/")]
    `(let [old-value# (game-project/get-setting ~'game-project ~path-list)]
       (game-project/set-setting! ~'game-project ~path-list ~value)
           (try
             ~@body
             (finally
               (game-project/set-setting! ~'game-project ~path-list old-value#))))))

(defn- check-file-contents [workspace specs]
  (doseq [[path content] specs]
    (let [file (build-path workspace path)]
      (is (true? (.exists file)))
      (is (= (slurp file) content)))))

(deftest build-with-custom-resources
  (with-loaded-project "test/resources/custom_resources_project"
    (let [game-project (test-util/resource-node project "/game.project")]
      (with-setting "project/custom_resources" "root.stuff"
        (project-build project game-project (g/make-evaluation-context))
        (check-file-contents workspace [["root.stuff" "root.stuff"]])
      (with-setting "project/custom_resources" "/root.stuff"
        (project-build project game-project (g/make-evaluation-context))
        (check-file-contents workspace [["root.stuff" "root.stuff"]])
      (with-setting "project/custom_resources" "assets"
        (project-build project game-project (g/make-evaluation-context))
        (check-file-contents workspace
                             [["assets/some.stuff" "some.stuff"]
                              ["assets/some2.stuff" "some2.stuff"]]))
      (with-setting "project/custom_resources" "/assets"
        (project-build project game-project (g/make-evaluation-context))
        (check-file-contents workspace
                             [["assets/some.stuff" "some.stuff"]
                              ["assets/some2.stuff" "some2.stuff"]]))
      (with-setting "project/custom_resources" "assets, root.stuff"
        (project-build project game-project (g/make-evaluation-context))
        (check-file-contents workspace
                             [["assets/some.stuff" "some.stuff"]
                              ["assets/some2.stuff" "some2.stuff"]
                              ["root.stuff" "root.stuff"]]))
      (with-setting "project/custom_resources" "assets, root.stuff, /more_assets/"
        (project-build project game-project (g/make-evaluation-context))
        (check-file-contents workspace
                             [["assets/some.stuff" "some.stuff"]
                              ["assets/some2.stuff" "some2.stuff"]
                              ["root.stuff" "root.stuff"]
                              ["more_assets/some_more.stuff" "some_more.stuff"]
                              ["more_assets/some_more2.stuff" "some_more2.stuff"]]))
      (with-setting "project/custom_resources" "nonexistent_path"
        (project-build project game-project (g/make-evaluation-context))
        (doseq [path ["assets/some.stuff" "assets/some2.stuff"
                      "root.stuff"
                      "more_assets/some_more.stuff" "more_assets/some_more2.stuff"]]
          (is (false? (.exists (build-path workspace path)))))))))))

(deftest custom-resources-cached
  (testing "Check custom resources are only rebuilt when source has changed"
    (with-clean-system
      (let [workspace (test-util/setup-scratch-workspace! world "test/resources/custom_resources_project")
            project (test-util/setup-project! workspace)
            game-project (test-util/resource-node project "/game.project")]
        (with-setting "project/custom_resources" "assets"
          (project-build project game-project (g/make-evaluation-context))
          (let [initial-some-mtime (mtime (build-path workspace "assets/some.stuff"))
                initial-some2-mtime (mtime (build-path workspace "assets/some2.stuff"))]
            (Thread/sleep 1000)
            (spit (abs-project-path workspace "assets/some.stuff") "new stuff")
            (workspace/resource-sync! workspace)
            (project-build project game-project (g/make-evaluation-context))
            (is (not (= initial-some-mtime (mtime (build-path workspace "assets/some.stuff")))))
            (is (= initial-some2-mtime (mtime (build-path workspace "assets/some2.stuff"))))))))))

(deftest dependencies-are-removed-from-game-project
  (with-loaded-project project-path
    (let [path           "/game.project"
          game-project   (test-util/resource-node project path)
          dependency-url "http://localhost:1234/dependency.zip"]
      (game-project/set-setting! game-project ["project" "dependencies"] dependency-url)
      (let [build-artifacts      (project-build-artifacts project game-project (g/make-evaluation-context))
            content-by-source    (into {} (keep #(when-let [r (:resource (:resource %))]
                                                   [(resource/proj-path r) (content-bytes %)]))
                                       build-artifacts)
            content              (get content-by-source "/game.project")
            game-project-content (String. content)]
        (is (not (.contains game-project-content dependency-url)))))))
