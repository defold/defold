(ns integration.asset-browser-test
  (:require [clojure.test :refer :all]
            [clojure.pprint :refer [pprint]]
            [dynamo.graph :as g]
            [dynamo.node :as n]
            [dynamo.system :as ds]
            [dynamo.system.test-support :refer [with-clean-system]]
            [dynamo.types :as t]
            [editor.workspace :as workspace]
            [editor.project :as project]
            [editor.cubemap :as cubemap]
            [editor.atlas :as atlas]
            [editor.image :as image]
            [editor.scene :as scene]
            [editor.platformer :as platformer]
            [editor.switcher :as switcher]
            [internal.clojure :as clojure])
  (:import [java.io File]
           [javax.imageio ImageIO]))

(def not-nil? (complement nil?))

(def project-path "resources/test_project")
(def branch "dummy-branch")

(defn- load-test-workspace []
  (let [workspace (g/transactional
                    (let [workspace (g/add (n/construct workspace/Workspace :root project-path))]
                      (cubemap/register-resource-types workspace)
                      (image/register-resource-types workspace)
                      (atlas/register-resource-types workspace)
                      (platformer/register-resource-types workspace)
                      (switcher/register-resource-types workspace)
                      workspace))]
    workspace))

(deftest workspace-tree
  (testing "The file system can be retrieved as a tree"
           (with-clean-system
             (let [workspace (load-test-workspace)
                   history-count 0 ; TODO retrieve actual undo-history count
                   root (g/node-value workspace :resource-tree)]
               (is (workspace/url root) "file:/")))))

(deftest asset-browser-search
  (testing "Searching for a resource produces a hit and renders a preview"
           (with-clean-system
             (let [workspace (load-test-workspace)]
               (let [project (g/transactional (let [project (g/add (n/construct project/Project))]
                                                (g/connect workspace :resource-list project :resources)
                                                project))
                     resources (g/node-value workspace :resource-list)
                     project (project/load-project project resources)
                     queries ["**/atlas.atlas" "**/env.cubemap" "**/level1.platformer" "**/level01.switcher"]]
                 (doseq [query queries
                         :let [results (project/find-resources project query)]]
                   (is (= 1 (count results)))
                     (let [resource-node (get (first results) 1)
                           resource-type (project/get-resource-type resource-node)
                           setup-rendering-fn (:setup-rendering-fn resource-type)]
                       (let [view (g/transactional
                                    (let [view (g/add (n/construct scene/PreviewView :width 128 :height 128))]
                                      (setup-rendering-fn resource-node view)
                                      view))
                             image (g/node-value view :frame)]
                         (is (not (nil? image)))
                         #_(let [file (File. "test.png")]
                            (ImageIO/write image "png" file))
                         )
                       ))
                 )))))

#_(asset-browser-search)
