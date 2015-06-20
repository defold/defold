(ns integration.build-test
  (:require [clojure.test :refer :all]
            [clojure.pprint :refer [pprint]]
            [dynamo.graph :as g]
            [dynamo.graph.test-support :refer [with-clean-system]]
            [dynamo.types :as t]
            [dynamo.geom :as geom]
            [dynamo.util :as util]
            [editor.math :as math]
            [editor.atlas :as atlas]
            [editor.collection :as collection]
            [editor.core :as core]
            [editor.cubemap :as cubemap]
            [editor.game-object :as game-object]
            [editor.image :as image]
            [editor.platformer :as platformer]
            [editor.project :as project]
            [editor.scene :as scene]
            [editor.sprite :as sprite]
            [editor.switcher :as switcher]
            [editor.workspace :as workspace]
            [internal.render.pass :as pass]
            [integration.test-util :as test-util])
  (:import [dynamo.types Region]
           [com.dynamo.gameobject.proto GameObject GameObject$CollectionDesc GameObject$CollectionInstanceDesc GameObject$InstanceDesc
            GameObject$EmbeddedInstanceDesc GameObject$PrototypeDesc]
           [java.awt.image BufferedImage]
           [java.io File]
           [javax.vecmath Point3d Matrix4d]
           [javax.imageio ImageIO]
           [editor.workspace BuildResource]))

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
                   exp-paths [path]]
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

(deftest merge-gos
  (testing "Verify equivalent game objects are merged"
           (with-clean-system
             (let [workspace     (test-util/setup-workspace! world project-path)
                   project       (test-util/setup-project! workspace)]
               (doseq [path ["/merge/merge_embed.collection"
                             #_"/merge/merge_refs.collection"]
                       :let [resource-node (test-util/resource-node project path)
                             build-results (project/build project resource-node)
                             content-by-source (into {} (map #(do [(workspace/proj-path (:resource (:resource %))) (:content %)])
                                                             build-results))
                             content-by-target (into {} (map #(do [(workspace/proj-path (:resource %)) (:content %)])
                                                             build-results))]]
                 #_(prn (map #(workspace/proj-path (:resource %)) build-results))
                 #_(prn (map #(nth (:key %) 2) [(nth build-results 1)]))
                 #_(prn (map #(keys (nth (:key %) 2)) [(nth build-results 1)]))
                 (let [go-builds (map #(nth build-results %) [1 3])
                       instance-data (map #(get (nth (:key %) 2) :instance-data) go-builds)
                       values (map (fn [ls] (map :resource ls)) instance-data)]
                   #_(prn (map #(nth (:key %) 2) go-builds))
                   (prn values)
                   (prn (= (first values) (second values)))
                   #_(prn (reduce = (map #(get-in (nth (:key %) 2) [:instance-data :instance-msg]) go-builds))))
                 #_(is (= 3 (count build-results)))
                 #_(let [content (get content-by-source path)
                        desc (GameObject$CollectionDesc/parseFrom content)
                        target-paths (set (map #(workspace/proj-path (:resource %)) build-results))]
                    (doseq [inst (.getInstancesList desc)
                            :let [prototype (.getPrototype inst)]]
                      (is (contains? target-paths prototype))
                      (let [content (get content-by-target prototype)
                            desc (GameObject$PrototypeDesc/parseFrom content)]
                        (prn desc)))))))))

(merge-gos)

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
