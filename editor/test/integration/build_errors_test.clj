;; Copyright 2020-2026 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;;
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;;
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns integration.build-errors-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.build :as build]
            [editor.build-errors-view :as build-errors-view]
            [editor.defold-project :as project]
            [editor.localization :as localization]
            [editor.resource :as resource]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [internal.util :as util]
            [util.fn :as fn]))

(def ^:private project-path "test/resources/errors_project")

(defn- build-error [render-error-fn-call-logger]
  (let [calls (fn/call-logger-calls render-error-fn-call-logger)
        args (last calls)
        error-value (first args)]
    error-value))

(defn- add-game-object-from-file! [workspace collection resource-path]
  (test-util/add-referenced-game-object! collection (test-util/resource workspace resource-path)))

(defn- add-component-from-file! [workspace game-object resource-path]
  (test-util/add-referenced-component! game-object (test-util/resource workspace resource-path)))

(defn- find-outline-node [outline-node labels]
  (loop [labels (seq labels)
         node-outline (g/node-value outline-node :node-outline)]
    (if (empty? labels)
      (:node-id node-outline)
      (when-let [child-outline (util/first-where #(= (first labels) (:label %)) (:children node-outline))]
        (recur (next labels) child-outline)))))

(def ^:private error-item-open-info-without-opts (comp pop :args build-errors-view/error-item-open-info))

(deftest build-errors-test
  (test-util/with-loaded-project project-path
    (let [main-collection (test-util/resource-node project "/main/main.collection")
          game-object (test-util/add-embedded-game-object! main-collection)
          resource (partial test-util/resource workspace)
          resource-node (partial test-util/resource-node project)
          outline-node (fn [resource-path labels] (find-outline-node (resource-node resource-path) labels))
          make-restore-point! #(test-util/make-graph-reverter (project/graph project))
          nodes-label (localization/message "outline.gui.nodes")]

      (testing "Build error links to source node"
        (are [component-resource-path error-resource-path error-outline-path]
            (with-open [_ (make-restore-point!)]
              (add-component-from-file! workspace game-object component-resource-path)
              (let [old-artifact-map (workspace/artifact-map workspace)
                    build-results (build/build-project! project main-collection old-artifact-map nil (g/make-evaluation-context))
                    error-value (:error build-results)]
                (if (is (some? error-value) component-resource-path)
                  (let [error-tree (build-errors-view/build-resource-tree error-value)
                        error-item-of-parent-resource (first (:children error-tree))
                        error-item-of-faulty-node (first (:children error-item-of-parent-resource))]
                    (is (= [(resource error-resource-path) (resource-node error-resource-path)]
                           (error-item-open-info-without-opts error-item-of-parent-resource)))
                    (is (= [(resource error-resource-path) (outline-node error-resource-path error-outline-path)]
                           (error-item-open-info-without-opts error-item-of-faulty-node))))
                  (workspace/artifact-map! workspace (:artifact-map build-results))))
              true)

          "/errors/syntax_error.script"
          "/errors/syntax_error.script" []

          "/errors/button_break_self.gui"
          "/errors/button_break_self.gui" [nodes-label "box"]

          "/errors/panel_using_button_break_self.gui"
          "/errors/button_break_self.gui" [nodes-label "box"]

          "/errors/panel_break_button.gui"
          "/errors/panel_break_button.gui" [nodes-label "button" "button/box"]

          "/errors/window_using_panel_break_button.gui"
          "/errors/panel_break_button.gui" [nodes-label "button" "button/box"]

          "/errors/window_break_panel.gui"
          "/errors/window_break_panel.gui" [nodes-label "panel" "panel/button"]

          "/errors/window_break_button.gui"
          "/errors/window_break_button.gui" [nodes-label "panel" "panel/button" "panel/button/box"]))

      (testing "Build errors from missing files link to referencing files, not referenced"
        (are [resource-path error-resource-path error-outline-path add-fn]
            (with-open [_ (make-restore-point!)]
              (add-fn resource-path)
              (let [old-artifact-map (workspace/artifact-map workspace)
                    build-results (build/build-project! project main-collection old-artifact-map nil (g/make-evaluation-context))
                    error-value (:error build-results)]
                (if (is (some? error-value) resource-path)
                  (let [error-tree (build-errors-view/build-resource-tree error-value)
                        error-item-of-parent-resource (first (:children error-tree))
                        error-item-of-faulty-node (first (:children error-item-of-parent-resource))]
                    (is (= [(resource error-resource-path) (resource-node error-resource-path)]
                           (error-item-open-info-without-opts error-item-of-parent-resource)))
                    (is (= [(resource error-resource-path) (outline-node error-resource-path error-outline-path)]
                           (error-item-open-info-without-opts error-item-of-faulty-node))))
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
                build-results (build/build-project! project main-collection old-artifact-map nil (g/make-evaluation-context))
                error-value (:error build-results)]
            (if (is (some? error-value))
              (let [error-tree (build-errors-view/build-resource-tree error-value)]
                (is (= ["/errors/button_break_self.gui" "/errors/name_conflict.gui" "/errors/panel_break_button.gui"]
                       (sort (map #(resource/proj-path (get-in % [:value :resource])) (:children error-tree)))))
                (is (= 1 (count (get-in error-tree [:children 0 :children]))))
                (is (= 2 (count (get-in error-tree [:children 1 :children]))))
                (is (= 1 (count (get-in error-tree [:children 2 :children])))))
              (workspace/artifact-map! workspace (:artifact-map build-results)))))))))
