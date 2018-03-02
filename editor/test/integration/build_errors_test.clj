(ns integration.build-errors-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.build-errors-view :as build-errors-view]
            [editor.collection :as collection]
            [editor.defold-project :as project]
            [editor.workspace :as workspace]
            [editor.game-object :as game-object]
            [editor.progress :as progress]
            [editor.resource :as resource]
            [integration.test-util :as test-util]
            [internal.util :as util]
            [support.test-support :refer [with-clean-system]]))

(def ^:private project-path "test/resources/errors_project")

(defn- created-node [select-fn-call-logger]
  (let [calls (test-util/call-logger-calls select-fn-call-logger)
        args (last calls)
        selection (first args)
        node-id (first selection)]
    node-id))

(defn- build-error [render-error-fn-call-logger]
  (let [calls (test-util/call-logger-calls render-error-fn-call-logger)
        args (last calls)
        error-value (first args)]
    error-value))

(defn- add-empty-game-object! [workspace project collection]
  (let [select-fn (test-util/make-call-logger)]
    (collection/add-game-object workspace project collection collection select-fn)
    (let [embedded-go-instance (created-node select-fn)]
      (g/node-value embedded-go-instance :source-id))))

(defn- add-game-object-from-file! [workspace collection resource-path]
  (let [select-fn (test-util/make-call-logger)]
    (collection/add-game-object-file collection collection (test-util/resource workspace resource-path) select-fn)
    (created-node select-fn)))

(defn- add-component-from-file! [workspace game-object resource-path]
  (let [select-fn (test-util/make-call-logger)]
    (game-object/add-component-file game-object (test-util/resource workspace resource-path) select-fn)
    (created-node select-fn)))

(defn- error-resource [error-tree]
  (get-in error-tree [:children 0 :value :resource]))

(defn- error-resource-node [error-tree]
  (get-in error-tree [:children 0 :value :node-id]))

(defn- error-outline-node [error-tree]
  (get-in error-tree [:children 0 :children 0 :node-id]))

(defn- find-outline-node [outline-node labels]
  (loop [labels (seq labels)
         node-outline (g/node-value outline-node :node-outline)]
    (if (empty? labels)
      (:node-id node-outline)
      (when-let [child-outline (util/first-where #(= (first labels) (:label %)) (:children node-outline))]
        (recur (next labels) child-outline)))))

(deftest build-errors-test
  (test-util/with-loaded-project project-path
    (let [main-collection (test-util/resource-node project "/main/main.collection")
          game-object (add-empty-game-object! workspace project main-collection)
          resource (partial test-util/resource workspace)
          resource-node (partial test-util/resource-node project)
          outline-node (fn [resource-path labels] (find-outline-node (resource-node resource-path) labels))
          make-restore-point! #(test-util/make-graph-reverter (project/graph project))]

      (testing "Build error links to source node"
        (are [component-resource-path error-resource-path error-outline-path]
            (with-open [_ (make-restore-point!)]
              (add-component-from-file! workspace game-object component-resource-path)
              (let [old-artifact-map (workspace/artifact-map workspace)
                    build-results (project/build project main-collection (g/make-evaluation-context) nil old-artifact-map progress/null-render-progress!)
                    error-value (:error build-results)]
                (if (is (some? error-value) component-resource-path)
                  (let [error-tree (build-errors-view/build-resource-tree error-value)]
                    (is (= (resource error-resource-path)
                           (error-resource error-tree)))
                    (is (= (resource-node error-resource-path)
                           (error-resource-node error-tree)))
                    (is (= (outline-node error-resource-path error-outline-path)
                           (error-outline-node error-tree))))
                  (workspace/artifact-map! workspace (:artifact-map build-results))))
              true)

          "/errors/syntax_error.script"
          "/errors/syntax_error.script" []

          "/errors/button_break_self.gui"
          "/errors/button_break_self.gui" ["Nodes" "box"]

          "/errors/panel_using_button_break_self.gui"
          "/errors/button_break_self.gui" ["Nodes" "box"]

          "/errors/panel_break_button.gui"
          "/errors/panel_break_button.gui" ["Nodes" "button" "button/box"]

          "/errors/window_using_panel_break_button.gui"
          "/errors/panel_break_button.gui" ["Nodes" "button" "button/box"]

          "/errors/window_break_panel.gui"
          "/errors/window_break_panel.gui" ["Nodes" "panel" "panel/button"]

          "/errors/window_break_button.gui"
          "/errors/window_break_button.gui" ["Nodes" "panel" "panel/button" "panel/button/box"]))

      (testing "Build error does not link to missing files"
        (are [resource-path error-resource-path error-outline-path add-fn]
            (with-open [_ (make-restore-point!)]
              (add-fn resource-path)
              (let [old-artifact-map (workspace/artifact-map workspace)
                    build-results (project/build project main-collection (g/make-evaluation-context) nil old-artifact-map progress/null-render-progress!)
                    error-value (:error build-results)]
                (if (is (some? error-value) resource-path)
                  (let [error-tree (build-errors-view/build-resource-tree error-value)]
                    (is (= (resource error-resource-path)
                           (error-resource error-tree)))
                    (is (= (resource-node error-resource-path)
                           (error-resource-node error-tree)))
                    (is (= (outline-node error-resource-path error-outline-path)
                           (error-outline-node error-tree))))
                  (workspace/artifact-map! workspace (:artifact-map build-results))))
              true)

          "/file_not_found.gui"
          "/main/main.collection" ["go", "file_not_found"]
          (partial add-component-from-file! workspace game-object)

          "/file_not_found.go"
          "/main/main.collection" ["file_not_found"]
          (partial add-game-object-from-file! workspace main-collection)))

      (testing "Errors from the same source are not duplicated"
        (with-open [_ (make-restore-point!)]
          (doseq [path ["/errors/button_break_self.gui"
                        "/errors/panel_break_button.gui"
                        "/errors/panel_using_button_break_self.gui"
                        "/errors/panel_using_name_conflict_twice.gui"
                        "/errors/window_using_panel_break_button.gui"]]
            (add-component-from-file! workspace game-object path))
          (let [old-artifact-map (workspace/artifact-map workspace)
                build-results (project/build project main-collection (g/make-evaluation-context) nil old-artifact-map progress/null-render-progress!)
                error-value (:error build-results)]
            (if (is (some? error-value))
              (let [error-tree (build-errors-view/build-resource-tree error-value)]
                (is (= ["/errors/button_break_self.gui" "/errors/name_conflict.gui" "/errors/panel_break_button.gui"]
                       (sort (map #(resource/proj-path (get-in % [:value :resource])) (:children error-tree)))))
                (is (= 1 (count (get-in error-tree [:children 0 :children]))))
                (is (= 2 (count (get-in error-tree [:children 1 :children]))))
                (is (= 1 (count (get-in error-tree [:children 2 :children])))))
              (workspace/artifact-map! workspace (:artifact-map build-results)))))))))
