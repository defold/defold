(ns integration.asset-browser-test
  (:require [clojure.java.io :as io]
            [clojure.test :refer :all]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.asset-browser :as asset-browser]
            [editor.dialogs :as dialogs]
            [editor.workspace :as workspace]
            [editor.defold-project :as project]
            [editor.fs :as fs]
            [editor.resource :as resource]
            [editor.resource-watch :as resource-watch]
            [integration.test-util :as test-util]
            [support.test-support :refer [with-clean-system]]))

(deftest workspace-tree
  (testing "The file system can be retrieved as a tree"
    (test-util/with-loaded-project
      (let [root (g/node-value workspace :resource-tree)]
        (is (= (resource/proj-path root) "/"))))))

(deftest asset-browser-search
  (testing "Searching for a resource produces a hit and renders a preview"
    (let [queries ["**/atlas.atlas" "**/env.cubemap"
                   "**/atlas.sprite" "**/atlas_sprite.go" "**/atlas_sprite.collection"]]
      (test-util/with-loaded-project
        (let [view-graph (g/make-graph! :history false :volatility 2)]
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

(deftest allow-resource-move
  (test-util/with-loaded-project
    (let [root-dir (workspace/project-path workspace)
          make-file (fn [proj-path]
                      (io/file root-dir proj-path))
          make-dir-resource (fn [proj-path opts]
                              (test-util/make-fake-file-resource workspace (.getPath root-dir) (make-file proj-path) nil opts))]

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

      (testing "Returns false for move to reserved project path"
        (let [root-resource (make-dir-resource "" {})]
          (doseq [reserved-dir-name resource-watch/reserved-proj-paths]
            (let [fake-source-dir (str "subdir" reserved-dir-name)]
              (is (not (asset-browser/allow-resource-move? root-resource [(make-file fake-source-dir)])))))))

      (testing "Returns true for writable destination"
        (is (true? (asset-browser/allow-resource-move? (make-dir-resource "dest" {})
                                                       [(make-file "a.txt")
                                                        (make-file "b.txt")]))))

      (testing "Returns true for conflicting files"
        (is (true? (asset-browser/allow-resource-move? (make-dir-resource "dest" {})
                                                       [(make-file "a.txt")
                                                        (make-file "readonly/a.txt")])))))))

(deftest paste
  (with-clean-system
    (let [workspace (test-util/setup-scratch-workspace! world)
          root-dir (workspace/project-path workspace)
          make-file (partial io/file root-dir)
          make-dir-resource (fn [path opts] (test-util/make-fake-file-resource workspace (.getPath root-dir) (make-file path) nil (merge opts {:source-type :folder})))
          root-resource (make-dir-resource "" {})
          writable-dir-1 (make-dir-resource "writable-dir-1" {})
          writable-dir-2 (make-dir-resource "writable-dir-2" {})
          read-only-dir-1 (make-dir-resource "read-only-dir-1" {:read-only? true})
          read-only-dir-2 (make-dir-resource "read-only-dir-2" {:read-only? true})]
      (doseq [reserved-dir-name resource-watch/reserved-proj-paths]
        (fs/create-directories! (make-file (str "subdir" reserved-dir-name))))
      (workspace/resource-sync! workspace [])
      (testing "paste?"
        (are [files-on-clipboard? target-resources expected] (= expected (asset-browser/paste? files-on-clipboard? target-resources))
          false nil false
          false [writable-dir-1] false
          false [writable-dir-1 writable-dir-2] false
          false [read-only-dir-1] false
          false [read-only-dir-1 read-only-dir-2] false
          false [writable-dir-1 read-only-dir-1] false

          true nil false
          true [writable-dir-1] true
          true [writable-dir-1 writable-dir-2] false
          true [read-only-dir-1] false
          true [read-only-dir-1 read-only-dir-2] false
          true [writable-dir-1 read-only-dir-1] false))
      (testing "paste!"
        (are [target-resource src-files expected]
            (let [alerted (atom false)
                  message (atom "")]
              (binding [dialogs/make-alert-dialog (fn [text] (reset! alerted true) (reset! message text))]
                (asset-browser/paste! workspace target-resource src-files (constantly nil))
                (= expected (not (or @alerted (string/includes? message "reserved"))))))

          root-resource [(make-file "car/car.script")] true

          root-resource [(make-file "subdir/builtins")] false
          root-resource [(make-file "subdir/build")] false
          root-resource [(make-file "subdir/.internal")] false
          root-resource [(make-file "subdir/.git")] false)))))

(deftest rename
  (test-util/with-loaded-project
    (let [root-dir (workspace/project-path workspace)
          make-file (partial io/file root-dir)
          make-dir-resource (fn [path opts] (test-util/make-fake-file-resource workspace (.getPath root-dir) (make-file path) nil (merge opts {:source-type :folder})))
          make-file-resource (fn [path opts] (test-util/make-fake-file-resource workspace (.getPath root-dir) (make-file path) nil (merge opts {:source-type :file})))
          root-resource (make-dir-resource "" {})
          file-1 (make-file-resource "file-1.txt" {})
          file-2 (make-file-resource "file-2.abc" {})
          fixed-1 (make-file-resource "game.project" {})]
      (testing "rename?"
        (are [resources expected] (= expected (asset-browser/rename? resources))
          nil false
          [file-1] true
          [file-1 file-2] false
          [fixed-1] false))
      (testing "validate-rename"
        (are [parent-path new-name expected] (= expected (nil? (asset-browser/validate-new-resource-name parent-path new-name)))
          
          "" "fine" true
          "" "game.project" true
          
          "" "builtins" false
          "" "build" false
          "" ".internal" false
          "" ".git" false

          "subdir" "builtins" true
          "subdir" "build" true
          "subdir" ".internal" true
          "subdir" ".git" true)))))

(deftest delete
  (test-util/with-loaded-project
    (let [root-dir (workspace/project-path workspace)
          make-file (partial io/file root-dir)
          make-dir-resource (fn [path opts] (test-util/make-fake-file-resource workspace (.getPath root-dir) (make-file path) nil (merge opts {:source-type :folder})))
          make-file-resource (fn [path opts] (test-util/make-fake-file-resource workspace (.getPath root-dir) (make-file path) nil (merge opts {:source-type :file})))
          writable-dir-resource (make-dir-resource "writable-dir-resource" {})
          writable-file-resource (make-dir-resource "writable-file-resource" {})
          read-only-dir-resource (make-dir-resource "read-only-dir-resource-1" {:read-only? true})
          read-only-file-resource (make-file-resource "read-only-file-resource" {:read-only? true})
          fixed-file-resource (make-file-resource "game.project" {})
          root-resource (make-dir-resource "" {})
          fs-builtins-resource (make-dir-resource "builtins" {})]
      (testing "delete?"
        (are [resources expected] (= expected (boolean (asset-browser/delete? resources)))
          nil false
          [writable-file-resource] true
          [read-only-file-resource] false
          [writable-dir-resource] true
          [writable-file-resource] true
          [writable-file-resource read-only-dir-resource] false
          [writable-file-resource writable-dir-resource] true
          [fixed-file-resource] false
          [fs-builtins-resource] true ; this should never appear in the asset browser, but if we decide it should - it will be deletable
          )))))

(deftest new-folder
  (test-util/with-loaded-project
    (let [root-dir (workspace/project-path workspace)
          make-file (partial io/file root-dir)
          make-dir-resource (fn [path opts] (test-util/make-fake-file-resource workspace (.getPath root-dir) (make-file path) nil (merge opts {:source-type :folder})))
          make-file-resource (fn [path opts] (test-util/make-fake-file-resource workspace (.getPath root-dir) (make-file path) nil (merge opts {:source-type :file})))
          root-resource (make-dir-resource "" {})
          writable-dir (make-dir-resource "writable-dir" {})
          read-only-dir (make-dir-resource "read-only-dir-1" {:read-only? true})
          fs-builtins-resource (make-dir-resource "builtins" {})]
      (testing "new-folder?"
        (are [resources expected] (= expected (boolean (asset-browser/new-folder? resources)))
          nil false
          [root-resource] true
          [writable-dir] true
          [read-only-dir] false
          [writable-dir read-only-dir] false
          [fs-builtins-resource] true ; this should never appear in the asset browser, but if we decide it should - it will be possible to create dirs in it
          ))
      (testing "validate-new-folder-name"
        (are [parent-path new-name expected] (= expected (nil? (asset-browser/validate-new-folder-name parent-path new-name)))
          "" "my-folder" true

          "" "builtins" false
          "" "build" false
          "" ".internal" false
          "" ".git" false

          "subdir" "builtins" true
          "subdir" "build" true
          "subdir" ".internal" true
          "subdir" ".git" true)))))

(deftest drop-move
  (with-clean-system
    (let [workspace (test-util/setup-scratch-workspace! world)
          root-dir (workspace/project-path workspace)
          make-file (partial io/file root-dir)
          resource-map (g/node-value workspace :resource-map)]
      (testing "drag-moving game.project becomes copy"
        ;; moving /game.project and /car/car.script into the /collection directory
        (let [moved (asset-browser/drop-files! workspace [[(io/as-file (resource-map "/game.project")) (make-file "collection/game.project")]
                                                          [(io/as-file (resource-map "/car/car.script")) (make-file "collection/car.script")]]
                                               :move)]
          (workspace/resource-sync! workspace moved)
          (let [resource-map (g/node-value workspace :resource-map)]
            (is (some? (resource-map "/game.project")))
            (is (some? (resource-map "/collection/game.project")))
            (is (not (some? (resource-map "/car/car.script"))))
            (is (some? (resource-map "/collection/car.script")))))))))
