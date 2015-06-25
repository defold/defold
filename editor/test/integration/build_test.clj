(ns integration.build-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system]]
            [editor.math :as math]
            [editor.project :as project]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util])
  (:import [com.dynamo.gameobject.proto GameObject GameObject$CollectionDesc GameObject$CollectionInstanceDesc GameObject$InstanceDesc
            GameObject$EmbeddedInstanceDesc GameObject$PrototypeDesc]
           [com.dynamo.textureset.proto TextureSetProto$TextureSet]
           [com.dynamo.render.proto Font$FontMap]
           [editor.types Region]
           [editor.workspace BuildResource]
           [java.awt.image BufferedImage]
           [java.io File]
           [javax.imageio ImageIO]
           [javax.vecmath Point3d Matrix4d]))

(def project-path "resources/build_project/SideScroller")

(defn- ->BuildResource
  ([project path]
    (->BuildResource project path nil))
  ([project path prefix]
    (let [node (test-util/resource-node project path)]
      (workspace/make-build-resource (:resource node) prefix))))

(deftest build-game-project
  (testing "Building a project"
           (with-clean-system
             (let [workspace     (test-util/setup-workspace! world project-path)
                   project       (test-util/setup-project! workspace)
                   path          "/game.project"
                   resource-node (test-util/resource-node project path)
                   build-results (project/build project resource-node)
                   content-by-source (into {} (map #(do [(workspace/proj-path (:resource (:resource %))) (:content %)]) build-results))
                   exp-paths [path "/main/main.collection" "/input/game.input_binding" "/builtins/render/default.render"]]
               (doseq [path exp-paths]
                 (is (contains? content-by-source path)))
               (let [content (get content-by-source "/main/main.collection")
                     desc (GameObject$CollectionDesc/parseFrom content)
                     go-ext (:build-ext (workspace/get-resource-type workspace "go"))]
                 (doseq [inst (.getInstancesList desc)]
                   (is (.endsWith (.getPrototype inst) go-ext))))))))

(deftest build-coll-hierarchy
  (testing "Building collection hierarchies"
           (with-clean-system
             (let [workspace     (test-util/setup-workspace! world project-path)
                   project       (test-util/setup-project! workspace)
                   path          "/hierarchy/base.collection"
                   resource-node (test-util/resource-node project path)
                   build-results (project/build project resource-node)
                   content-by-source (into {} (map #(do [(workspace/proj-path (:resource (:resource %))) (:content %)]) build-results))]
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
                             content-by-source (into {} (map #(do [(workspace/proj-path (:resource (:resource %))) (:content %)])
                                                             build-results))
                             content-by-target (into {} (map #(do [(workspace/proj-path (:resource %)) (:content %)])
                                                             build-results))]]
                 (is (= 1 (count-exts (keys content-by-target) "goc")))
                 (is (= 1 (count-exts (keys content-by-target) "spritec")))
                 (let [content (get content-by-source path)
                      desc (GameObject$CollectionDesc/parseFrom content)
                      target-paths (set (map #(workspace/proj-path (:resource %)) build-results))]
                  (doseq [inst (.getInstancesList desc)
                          :let [prototype (.getPrototype inst)]]
                    (is (contains? target-paths prototype))
                    (let [content (get content-by-target prototype)
                          desc (GameObject$PrototypeDesc/parseFrom content)]
                      (doseq [comp (.getComponentsList desc)
                              :let [component (.getComponent comp)]]
                        (is (contains? target-paths component)))))))))))

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
                     content-by-source (into {} (map #(do [(workspace/proj-path (:resource (:resource %))) (:content %)])
                                                     build-results))
                     content-by-target (into {} (map #(do [(workspace/proj-path (:resource %)) (:content %)])
                                                     build-results))]
                 (is (= 1 (count-exts (keys content-by-target) "goc")))
                 (is (= 1 (count-exts (keys content-by-target) "spritec")))
                 (let [go-node (first-source (first-source resource-node :child-scenes) :source)
                       comp-node (first-source go-node :child-scenes)]
                   (g/transact (g/delete-node comp-node))
                   (let [build-results (project/build project resource-node)
                         content-by-target (into {} (map #(do [(workspace/proj-path (:resource %)) (:content %)])
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
                     main-collection (project/resolve-resource-node resource-node "/main/main.collection")]
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
                     cache-count (count @(:build-cache project))]
                 (g/transact
                   (for [[node label] (g/sources-of resource-node :dep-build-targets)]
                     (g/delete-node node)))
                 (project/build project resource-node)
                 (is (< (count @(:build-cache project)) cache-count)))))))

(deftest prune-fs-build-cache
  (testing "Verify the fs build cache works as expected"
           (with-clean-system
             (let [workspace     (test-util/setup-workspace! world project-path)
                   project       (test-util/setup-project! workspace)]
               (let [path "/main/main.collection"
                     resource-node (test-util/resource-node project path)
                     _ (project/build-and-write project resource-node)
                     cache-count (count @(:fs-build-cache project))]
                 (g/transact
                   (for [[node label] (g/sources-of resource-node :dep-build-targets)]
                     (g/delete-node node)))
                 (project/build-and-write project resource-node)
                 (is (< (count @(:fs-build-cache project)) cache-count)))))))

(deftest build-atlas
  (testing "Building atlas"
           (with-clean-system
             (let [workspace     (test-util/setup-workspace! world project-path)
                   project       (test-util/setup-project! workspace)
                   path          "/background/background.atlas"
                   resource-node (test-util/resource-node project path)
                   build-results (project/build project resource-node)
                   content-by-source (into {} (map #(do [(workspace/proj-path (:resource (:resource %))) (:content %)]) build-results))
                   content-by-target (into {} (map #(do [(workspace/proj-path (:resource %)) (:content %)]) build-results))]
               (let [content (get content-by-source path)
                     desc (TextureSetProto$TextureSet/parseFrom content)]
                 (is (contains? content-by-target (.getTexture desc))))))))

(deftest build-font
  (testing "Building font"
           (with-clean-system
             (let [workspace     (test-util/setup-workspace! world project-path)
                   project       (test-util/setup-project! workspace)
                   path          "/fonts/score.font"
                   resource-node (test-util/resource-node project path)
                   build-results (project/build project resource-node)
                   content-by-source (into {} (map #(do [(workspace/proj-path (:resource (:resource %))) (:content %)]) build-results))
                   content-by-target (into {} (map #(do [(workspace/proj-path (:resource %)) (:content %)]) build-results))]
               (let [content (get content-by-source path)
                     desc (Font$FontMap/parseFrom content)]
                 (is (= 1024 (.getImageWidth desc)))
                 (is (= 128 (.getImageHeight desc))))))))
