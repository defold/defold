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

(ns integration.editor-tab-test
  (:require [cljfx.api :as fx]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.app-view :as app-view]
            [editor.editor-tab :as editor-tab]
            [editor.ui :as ui]
            [editor.view :as view]
            [integration.test-util :as test-util])
  (:import [javafx.collections ObservableList]
           [javafx.event Event]
           [javafx.scene Scene]
           [javafx.scene.control SplitPane Tab TabPane]
           [javafx.scene.layout AnchorPane GridPane Region VBox]))

(set! *warn-on-reflection* true)

(g/defnode TestNonResourceWorkbenchView
  (inherits view/NonResourceWorkbenchView)
  (property parent g/Any))

(defn- make-test-view [parent _opts]
  (first
    (g/tx-nodes-added
      (g/transact
        {:undoable false}
        (g/make-node TestNonResourceWorkbenchView :parent parent)))))

(defn- make-test-tab-spec [_opts]
  {:instance-key ::test-tab
   :title "Test Tab"
   :tooltip "A tab that is not a resource"
   :style-classes #{"test-tab"}
   :make-view-fn make-test-view})

(use-fixtures :once
  (fn with-registered-test-tab-type [run-tests!]
    (editor-tab/register-type! ::test-tab {:make-tab-spec-fn make-test-tab-spec})
    (try
      (run-tests!)
      (finally
        (editor-tab/unregister-type! ::test-tab)))))

(defn- setup-editor-tabs-split!
  "Builds the parts of the editor tab area that the AppView reaches for: a
  SplitPane with a single TabPane, below the quick help controls that
  refresh-tab-panes updates."
  ^SplitPane [app-view]
  (let [editor-tabs-split (SplitPane.)]
    (.add (.getItems editor-tabs-split) (TabPane.))
    (ui/children! (AnchorPane.)
                  [editor-tabs-split
                   (doto (VBox.) (.setId "quick-help-box"))
                   (doto (GridPane.) (.setId "quick-help-items"))])
    (g/transact
      {:undoable false}
      (concat
        (g/set-property app-view :editor-tabs-split editor-tabs-split)
        (g/set-property app-view :localization test-util/localization)))
    editor-tabs-split))

(defn- tab-pane-tabs
  ^ObservableList [^SplitPane editor-tabs-split]
  (.getTabs ^TabPane (first (.getItems editor-tabs-split))))

(defn- open-test-tab! [app-view]
  (app-view/open-editor-tab! app-view test-util/localization ::test-tab {}))

(defn- resize-and-layout! [^Region region width height]
  (.resize region width height)
  (.applyCss region)
  (.layout region))

;; Verifies that resizing leaves hidden plain and wrapped tab content unchanged,
;; then sizes it on selection, guarding against the hidden viewport work in #13227.
(deftest hidden-editor-tab-layout-test
  @(fx/on-fx-thread
     (test-util/with-loaded-project
       (doseq [wrap-content-fn [nil
                                (fn [parent]
                                  (doto (VBox.)
                                    (ui/children! [parent])))]]
         (let [tab-pane (TabPane.)
               tab-spec (assoc (make-test-tab-spec {}) :wrap-content-fn wrap-content-fn)
               ^Tab first-tab (app-view/make-editor-tab! app-view test-util/localization (.getTabs tab-pane) tab-spec {})
               ^Tab second-tab (app-view/make-editor-tab! app-view test-util/localization (.getTabs tab-pane) tab-spec {})]
           (Scene. tab-pane)
           (.select (.getSelectionModel tab-pane) first-tab)
           (resize-and-layout! tab-pane 400.0 300.0)

           (let [first-bounds (.getLayoutBounds (.getContent first-tab))]
             (is (pos? (.getWidth first-bounds)))
             (is (pos? (.getHeight first-bounds)))

             (.select (.getSelectionModel tab-pane) second-tab)
             (resize-and-layout! tab-pane 640.0 480.0)

             (let [second-bounds (.getLayoutBounds (.getContent second-tab))]
               (is (= first-bounds (.getLayoutBounds (.getContent first-tab))))
               (is (> (.getWidth second-bounds) (.getWidth first-bounds)))
               (is (> (.getHeight second-bounds) (.getHeight first-bounds)))

               (.select (.getSelectionModel tab-pane) first-tab)
               (resize-and-layout! tab-pane 640.0 480.0)
               (is (= second-bounds (.getLayoutBounds (.getContent first-tab)))))))))))

;; Verifies that moving a tab between groups keeps each group's selected content
;; responsive to resizing, guarding against using the globally active tab as the layout condition.
(deftest selected-editor-tabs-in-split-groups-layout-test
  @(fx/on-fx-thread
     (test-util/with-loaded-project
       (let [editor-tabs-split (SplitPane.)
             first-pane (TabPane.)
             second-pane (TabPane.)
             ^Tab first-tab (app-view/make-editor-tab! app-view test-util/localization (.getTabs first-pane) (make-test-tab-spec {}) {})
             ^Tab second-tab (app-view/make-editor-tab! app-view test-util/localization (.getTabs first-pane) (make-test-tab-spec {}) {})]
         (.add (.getItems editor-tabs-split) first-pane)
         (.add (.getItems editor-tabs-split) second-pane)
         (Scene. editor-tabs-split)
         (.select (.getSelectionModel first-pane) second-tab)
         (resize-and-layout! editor-tabs-split 800.0 300.0)

         (.remove (.getTabs first-pane) second-tab)
         (.add (.getTabs second-pane) second-tab)
         (.select (.getSelectionModel first-pane) first-tab)
         (.select (.getSelectionModel second-pane) second-tab)
         (resize-and-layout! editor-tabs-split 800.0 300.0)

         (let [first-bounds (.getLayoutBounds (.getContent first-tab))
               second-bounds (.getLayoutBounds (.getContent second-tab))]
           (is (pos? (.getWidth first-bounds)))
           (is (pos? (.getHeight first-bounds)))
           (is (pos? (.getWidth second-bounds)))
           (is (pos? (.getHeight second-bounds)))

           (resize-and-layout! editor-tabs-split 1040.0 400.0)
           (is (> (.getWidth (.getLayoutBounds (.getContent first-tab))) (.getWidth first-bounds)))
           (is (> (.getHeight (.getLayoutBounds (.getContent first-tab))) (.getHeight first-bounds)))
           (is (> (.getWidth (.getLayoutBounds (.getContent second-tab))) (.getWidth second-bounds)))
           (is (> (.getHeight (.getLayoutBounds (.getContent second-tab))) (.getHeight second-bounds))))))))

(deftest open-non-resource-tab-test
  (test-util/with-loaded-project
    (let [editor-tabs-split (setup-editor-tabs-split! app-view)
          ^Tab tab (open-test-tab! app-view)
          view-id (editor-tab/view-node-id tab)]

      (testing "it opens as an editor tab"
        (is (= [tab] (vec (tab-pane-tabs editor-tabs-split))))
        (is (= "Test Tab" (.getText tab)))
        (is (contains? (set (.getStyleClass tab)) "test-tab"))
        (is (g/node-instance? view/NonResourceWorkbenchView view-id)))

      (testing "it has no resource"
        (is (nil? (editor-tab/resource-node-id tab (g/make-evaluation-context))))
        (is (= {} (get (g/node-value app-view :open-views) view-id)))
        (is (not (contains? (set (g/node-value app-view :open-resource-nodes)) nil))))

      (testing "it asks for no sidebar panes"
        (is (= [] (get (g/node-value app-view :open-sidebar-panes) view-id))))

      (testing "opening it again selects the same tab"
        (is (identical? tab (open-test-tab! app-view)))
        (is (= [tab] (vec (tab-pane-tabs editor-tabs-split)))))

      (testing "its title survives a tab refresh"
        (g/node-value app-view :refresh-tab-panes)
        (is (= "Test Tab" (.getText tab))))

      (testing "resource synchronization does not close it"
        (app-view/remove-invalid-tabs! (.getItems editor-tabs-split)
                                       (g/node-value app-view :open-views))
        (is (= [tab] (vec (tab-pane-tabs editor-tabs-split))))))))

(deftest close-non-resource-tab-test
  (test-util/with-loaded-project
    (setup-editor-tabs-split! app-view)
    (let [game-project (test-util/resource-node project "/game.project")
          ^Tab tab (open-test-tab! app-view)
          view-id (editor-tab/view-node-id tab)]
      (test-util/with-ui-run-later-rebound
        (Event/fireEvent tab (Event. Tab/CLOSED_EVENT)))

      (testing "closing it disposes its view node without deleting project resource nodes"
        (is (nil? (g/node-by-id view-id)))
        (is (g/node-by-id game-project)))

      (testing "closing it detaches the view from the tab"
        (is (nil? (editor-tab/view-node-id tab)))
        (is (not (contains? (g/node-value app-view :open-views) view-id)))))))

(deftest remove-invalid-tabs-test
  (test-util/with-loaded-project
    (let [editor-tabs-split (setup-editor-tabs-split! app-view)
          ;; A resource-backed view whose resource node is gone reports no view
          ;; data, which is how its tab is closed during resource
          ;; synchronization.
          resourceless-view (first
                              (g/tx-nodes-added
                                (g/transact
                                  {:undoable false}
                                  (g/make-node view/WorkbenchView))))
          tab (doto (Tab. "Gone")
                (editor-tab/set-view-node-id! resourceless-view))]
      (g/transact
        {:undoable false}
        (g/connect resourceless-view :view-data app-view :open-views))
      (.add (tab-pane-tabs editor-tabs-split) tab)
      (app-view/remove-invalid-tabs! (.getItems editor-tabs-split)
                                     (g/node-value app-view :open-views))
      (is (= [] (vec (tab-pane-tabs editor-tabs-split)))))))
