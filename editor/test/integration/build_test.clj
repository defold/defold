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
            GameObject$EmbeddedInstanceDesc]
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
      (BuildResource. (:resource node) prefix))))

(deftest build-game-project
  (testing "Building a project"
           (with-clean-system
             (let [workspace     (test-util/setup-workspace! world project-path)
                   project       (test-util/setup-project! workspace)
                   path          "/game.project"
                   resource-node (test-util/resource-node project path)
                   build-results (into {} (map #(do [(:resource %) (:content %)]) (project/build project resource-node)))
                   exp-paths [path]]
               (let [resource (workspace/make-build-resource (workspace/file-resource workspace "/main/main.collection"))
                     result (get build-results resource)
                     desc (GameObject$CollectionDesc/parseFrom result)
                     go-ext (:build-ext (workspace/get-resource-type workspace "go"))]
                 (doseq [inst (.getInstancesList desc)]
                   (is (.endsWith (.getPrototype inst) go-ext))))
               (doseq [path exp-paths]
                 (is (not (empty? (get build-results (->BuildResource project path))))))))))

(deftest build-coll-hierarchy
  (testing "Building collection hierarchies"
           (with-clean-system
             (let [workspace     (test-util/setup-workspace! world project-path)
                   project       (test-util/setup-project! workspace)
                   path          "/hierarchy/base.collection"
                   resource-node (test-util/resource-node project path)
                   build-results (into {} (map #(do [(:resource %) (:content %)]) (project/build project resource-node)))]
               (let [resource (workspace/make-build-resource (workspace/file-resource workspace "/hierarchy/base.collection"))
                     result (get build-results resource)
                     desc (GameObject$CollectionDesc/parseFrom result)
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
