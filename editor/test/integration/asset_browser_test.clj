(ns integration.asset-browser-test
  (:require [clojure.java.io :as io]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.asset-browser :as asset-browser]
            [editor.defold-project :as project]
            [editor.resource :as resource]
            [integration.test-util :as test-util]
            [support.test-support :refer [with-clean-system]]))

(deftest workspace-tree
  (testing "The file system can be retrieved as a tree"
    (with-clean-system
      (let [workspace     (test-util/setup-workspace! world)
            root          (g/node-value workspace :resource-tree)]
        (is (= (resource/proj-path root) "/"))))))

(deftest asset-browser-search
  (testing "Searching for a resource produces a hit and renders a preview"
    (let [queries ["**/atlas.atlas" "**/env.cubemap"
                   "**/atlas.sprite" "**/atlas_sprite.go" "**/atlas_sprite.collection"]]
      (with-clean-system
        (let [[workspace project app-view] (test-util/setup! world)
              view-graph (g/make-graph! :history false :volatility 2)]
          (doseq [query queries
                  :let [results (project/find-resources project query)]]
            (is (= 1 (count results)))
            (let [resource-node (get (first results) 1)
                  resource-type (project/get-resource-type resource-node)
                  view-type (first (:view-types resource-type))
                  make-preview-fn (:make-preview-fn view-type)
                  view-opts (assoc ((:id view-type) (:view-opts resource-type))
                                   :app-view app-view
                                   :project project)
                  view (make-preview-fn view-graph resource-node view-opts 128 128)]
              (let [image (g/node-value view :frame)]
                (is (not (nil? image)))))))))))

(deftest allow-resource-move-test
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          root-path "projects/game"
          make-file (fn [proj-path]
                      (io/file root-path proj-path))
          make-dir-resource (fn [proj-path opts]
                              (test-util/make-fake-file-resource workspace root-path (make-file proj-path) nil opts))]

      (testing "Returns false for readonly destination"
        (let [readonly-dir (make-dir-resource "readonly" {:read-only? true})]
          (is (false? (asset-browser/allow-resource-move? readonly-dir [])))
          (is (false? (asset-browser/allow-resource-move? readonly-dir [(make-file "a.txt")])))))

      (testing "Returns false if destination is below source"
        (let [src-path "player"
              dst-path "player/scripts"
              src-dir (make-file src-path)
              dst-dir (make-dir-resource dst-path {})]
          (is (false? (asset-browser/allow-resource-move? dst-dir [src-dir])))))

      (testing "Returns false if destination is the same as the source"
        (let [dir-path "player/scripts"]
          (is (false? (asset-browser/allow-resource-move? (make-dir-resource dir-path {})
                                                          [(make-file dir-path)]))))
        (let [dir-path "player"
              file-path "player/player.go"]
          (is (false? (asset-browser/allow-resource-move? (make-dir-resource dir-path {})
                                                          [(make-file file-path)])))))

      (testing "Returns true for writable destination"
        (is (true? (asset-browser/allow-resource-move? (make-dir-resource "dest" {})
                                                       [(make-file "a.txt")
                                                        (make-file "b.txt")]))))

      (testing "Returns true for conflicting files"
        (is (true? (asset-browser/allow-resource-move? (make-dir-resource "dest" {})
                                                       [(make-file "a.txt")
                                                        (make-file "readonly/a.txt")])))))))
