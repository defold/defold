(ns integration.asset-browser-test
  (:require [clojure.java.io :as io]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.asset-browser :as asset-browser]
            [editor.defold-project :as project]
            [editor.resource :as resource]
            [editor.util :as util]
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

(deftest move-entry-test
  (testing "File rename"
    (test-util/with-temp-dir! dir
      (test-util/make-file-tree! dir ["old-name.txt"])
      (let [src (io/file dir "old-name.txt")
            tgt (io/file dir "new-name.txt")]
        (is (= [[src tgt]] (util/move-entry! src tgt)))
        (is (= ["new-name.txt"] (test-util/file-tree dir))))))

  (testing "File move"
    (test-util/with-temp-dir! dir
      (test-util/make-file-tree! dir ["file.txt" {"directory" []}])
      (let [src (io/file dir "file.txt")
            tgt (io/file dir "directory" "file.txt")]
        (is (= [[src tgt]] (util/move-entry! src tgt)))
        (is (= [{"directory" ["file.txt"]}] (test-util/file-tree dir))))))

  (testing "Directory rename"
    (test-util/with-temp-dir! dir
      (test-util/make-file-tree! dir [{"old-name" ["file.txt"]}])
      (is (= [[(io/file dir "old-name" "file.txt") (io/file dir "new-name" "file.txt")]]
             (util/move-entry! (io/file dir "old-name") (io/file dir "new-name"))))
      (is (= [{"new-name" ["file.txt"]}] (test-util/file-tree dir)))))

  (testing "Directory move"
    (test-util/with-temp-dir! dir
      (test-util/make-file-tree! dir [{"project" []} {"archive" []}])
      (is (= [] (util/move-entry! (io/file dir "project") (io/file dir "archive" "project"))))
      (is (= [{"archive" [{"project" []}]}] (test-util/file-tree dir)))))

  (testing "Directory hierarchy"
    (test-util/with-temp-dir! dir
      (test-util/make-file-tree! dir [{"test" []}
                                      {"assets" [{"images" [{"atlas" ["myatlas.atlas"]}
                                                            {"game" [{"items" ["coin.png" "heart.png"]}]}]}]}])
      (is (= [[(io/file dir "assets" "images" "atlas" "myatlas.atlas") (io/file dir "test" "assets" "images" "atlas" "myatlas.atlas")]
              [(io/file dir "assets" "images" "game" "items" "coin.png") (io/file dir "test" "assets" "images" "game" "items" "coin.png")]
              [(io/file dir "assets" "images" "game" "items" "heart.png") (io/file dir "test" "assets" "images" "game" "items" "heart.png")]]
             (util/move-entry! (io/file dir "assets") (io/file dir "test" "assets"))))
      (is (= [{"test" [{"assets" [{"images" [{"atlas" ["myatlas.atlas"]}
                                             {"game" [{"items" ["coin.png" "heart.png"]}]}]}]}]}]
             (test-util/file-tree dir)))))

  (testing "File overwrite"
    (test-util/with-temp-dir! dir
      (test-util/make-file-tree! dir ["source.txt" "destination.txt"])
      (let [src (io/file dir "source.txt")
            tgt (io/file dir "destination.txt")
            src-contents (slurp src)]
        (is (= [[src tgt]] (util/move-entry! src tgt)))
        (is (= ["destination.txt"] (test-util/file-tree dir)))
        (is (= src-contents (slurp tgt))))))

  (testing "Directory merge"
    (test-util/with-temp-dir! dir
      (test-util/make-file-tree! dir [{"assets" [{"images" [{"atlas" ["myatlas.atlas"]}
                                                            {"game" [{"items" ["coin.png" "heart.png"]}]}]}]}
                                      {"images" [{"atlas" ["myatlas.atlas"]}
                                                 {"empty" []}
                                                 {"game" [{"items" ["cloud.png"]}]}]}])
      (let [src-atlas-contents (slurp (io/file dir "images" "atlas" "myatlas.atlas"))]
        (is (= [[(io/file dir "images" "atlas" "myatlas.atlas") (io/file dir "assets" "images" "atlas" "myatlas.atlas")]
                [(io/file dir "images" "game" "items" "cloud.png") (io/file dir "assets" "images" "game" "items" "cloud.png")]]
               (util/move-entry! (io/file dir "images") (io/file dir "assets" "images"))))
        (is (= [{"assets" [{"images" [{"atlas" ["myatlas.atlas"]}
                                      {"empty" []}
                                      {"game" [{"items" ["cloud.png" "coin.png" "heart.png"]}]}]}]}]
               (test-util/file-tree dir)))
        (is (= src-atlas-contents (slurp (io/file dir "assets" "images" "atlas" "myatlas.atlas"))))))))
