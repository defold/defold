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

(ns editor.ui-test
  (:require [cljfx.api :as fx]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.handler :as handler]
            [editor.localization :as localization]
            [editor.ui :as ui]
            [integration.test-util :as test-util]
            [support.test-support :as test-support])
  (:import [javafx.scene Scene]
           [javafx.scene.control ComboBox ListView Menu MenuBar MenuItem SelectionMode Tab TabPane TreeItem TreeView]
           [javafx.scene.control.skin TabPaneSkin]
           [javafx.scene.layout Pane VBox]))

(defn- make-fake-stage []
  (let [root (VBox.)
        stage (ui/make-stage)
        scene (Scene. root)]
    (.setScene stage scene)
    stage))

(defn fixture [f]
  (with-redefs [handler/state-atom (atom handler/empty-state)
                ui/*main-stage* (atom (ui/run-now (make-fake-stage)))]
    (f)))

(use-fixtures :each fixture)

(deftest extend-menu-test
  (handler/register-menu! ::menubar
    [{:label (localization/message "command.file")
      :children [{:label (localization/message "command.file.new")
                  :id ::new}]}])
  (handler/register-menu! ::save-menu ::new
    [{:label (localization/message "command.file.save-all")}])
  (handler/register-menu! ::quit-menu ::new
    [{:label (localization/message "command.app.quit")}])
  (is (= (handler/realize-menu ::menubar) [{:label (localization/message "command.file")
                                            :children [{:label (localization/message "command.file.new")
                                                        :id ::new}
                                                       {:label (localization/message "command.file.save-all")}
                                                       {:label (localization/message "command.app.quit")}]}])))

(defrecord TestSelectionProvider [selection]
  handler/SelectionProvider
  (selection [this] selection)
  (succeeding-selection [this] [])
  (alt-selection [this] []))

(defn- make-menu-items [scene menu-id command-context]
  (g/with-auto-evaluation-context evaluation-context
    (#'ui/make-menu-items scene (handler/realize-menu menu-id) [command-context] {} test-util/localization evaluation-context)))

(deftest menu-test
  (test-support/with-clean-system
    (handler/register-menu! ::my-menu
      [{:label (localization/message "command.file")
        :children [{:label (localization/message "command.file.open")
                    :id ::open
                    :command :file.open}
                   {:label (localization/message "command.file.save-all")
                    :command :file.save}]}])

    (handler/defhandler :file.open :global
        (enabled? [selection] true)
        (run [selection] 123))

    (handler/defhandler :file.save :global
      (enabled? [selection] true)
      (run [selection] 124))

    (let [root (Pane.)
          scene (ui/run-now (Scene. root))
          command-context {:name :global :env {:selection []}}]
     (let [menu-items (make-menu-items scene ::my-menu command-context)]
       (is (= 1 (count menu-items)))
       (is (instance? Menu (first menu-items)))
       (is (= 2 (count (.getItems (first menu-items)))))
       (is (instance? MenuItem (first (.getItems (first menu-items)))))))))


(deftest options-menu-test
  (test-support/with-clean-system
    (handler/register-menu! ::my-menu
      [{:label (localization/message "command.edit.add-embedded-component")
        :command :edit.add-embedded-component}])

    (handler/defhandler :edit.add-embedded-component :global
      (run [user-data] user-data)
      (active? [user-data] (or (not user-data) (= user-data 1)))
      (options [user-data] (when-not user-data [{:label (localization/message "command.file.new")
                                                 :command :edit.add-embedded-component
                                                 :user-data 1}
                                                {:label (localization/message "command.file.open")
                                                 :command :edit.add-embedded-component
                                                 :user-data 2}])))

    (let [command-context {:name :global :env {}}]
      (let [menu-items (make-menu-items nil ::my-menu command-context)]
        (is (= 1 (count menu-items)))
        (is (= 1 (count (.getItems (first menu-items)))))))))

(g/defnode FakeAppView
  (property active-tab g/Any))

(deftest toolbar-test
  @(fx/on-fx-thread
     (test-support/with-clean-system
       (handler/register-menu! ::my-menu
         [{:label (localization/message "command.file.open")
           :command :file.open
           :id ::open}])

       (handler/defhandler :file.open :global
         (enabled? [selection] true)
         (run [selection] 123)
         (state [] false))

       (handler/defhandler :file.save :global
         (enabled? [selection] true)
         (run [selection] 124)
         (state [] false))

       (let [root (Pane.)
             tab (Tab. "tab" root)
             tab-pane (TabPane.)
             app-view (g/make-node! world FakeAppView :active-tab tab)
             scene (Scene. tab-pane)
             selection-provider (TestSelectionProvider. [])]
         (ui/user-data! scene :localization test-util/localization)
         (.add (.getTabs tab-pane) tab)
         (.setSkin tab-pane (TabPaneSkin. tab-pane)) ;;
         (.setId root "toolbar")
         (ui/context! root :global {:app-view app-view} selection-provider)
         (ui/register-toolbar scene root "#toolbar" ::my-menu)
         (ui/refresh scene)
         (let [c1 (do (ui/refresh scene) (.getChildren root))
               c2 (do (ui/refresh scene) (.getChildren root))]
           (is (= 1 (count c1) (count c2)))
           (is (= (.get c1 0) (.get c2 0))))

         (handler/register-menu! ::extra ::open
           [{:label (localization/message "command.file.save-all")
             :command :file.save}])
         (ui/refresh scene)
         (is (= 2 (count (.getChildren root))))))))

(deftest menubar-test
  @(fx/on-fx-thread
     (test-support/with-clean-system
       (handler/register-menu! ::my-menu
         [{:label (localization/message "command.file")
           :children
           [{:label (localization/message "command.file.open")
             :id ::open
             :command :file.open}]}])

       (handler/defhandler :file.open :global
         (enabled? [selection] true)
         (run [selection] 123))

       (handler/defhandler :file.save :global
         (enabled? [selection] true)
         (run [selection] 124))

       (let [root (Pane.)
             scene (Scene. root)
             selection-provider (TestSelectionProvider. [])
             menubar (MenuBar.)]
         (ui/user-data! scene :localization test-util/localization)
         (ui/context! root :global {} selection-provider)
         (.add (.getChildren root) menubar)
         (.setId menubar "menubar")
         (ui/register-menubar scene menubar ::my-menu)
         (ui/refresh scene)
         (let [c1 (do (ui/refresh scene) (.getItems (first (.getMenus menubar))))
               c2 (do (ui/refresh scene) (.getItems (first (.getMenus menubar))))]
           (is (= 1 (count c1) (count c2)))
           (is (= (.get c1 0) (.get c2 0))))
         (handler/register-menu! ::extra ::open
           [{:label (localization/message "command.file.save-all")
             :command :file.save}])
         (ui/refresh scene)
         (let [c1 (do (ui/refresh scene) (.getItems (first (.getMenus menubar))))
               c2 (do (ui/refresh scene) (.getItems (first (.getMenus menubar))))]
           (is (= 2 (count c1) (count c2)))
           (is (= (.get c1 0) (.get c2 0))))))))

(deftest list-view-test
  (let [selected-items (atom nil)]
    (ui/run-now
      (let [root (ui/main-root)
            list (doto (ListView.)
                   (ui/items! [:a :b :c :d]))]
        (doto (.getSelectionModel list)
          (.setSelectionMode SelectionMode/MULTIPLE))
        (ui/observe-list (ui/selection list)
                         (fn [_ values]
                           (reset! selected-items values)))
        (ui/add-child! root list)
        (doto (.getSelectionModel list)
          (.selectRange 1 3))))
    (is (= [:b :c] @selected-items))))

(deftest observe-selection-test
  (testing "TreeView"
    (let [results (atom {:observed-view nil :selection-owner nil :selected-items nil})]
      (ui/run-now
        (let [root (ui/main-root)
              tree-item-a (TreeItem. :a)
              tree-item-b (TreeItem. :b)
              tree-view (doto (TreeView.)
                          (.setRoot (doto (TreeItem. :root)
                                      (.setExpanded true)
                                      (-> .getChildren
                                          (.addAll [tree-item-a
                                                    tree-item-b])))))]
          (swap! results assoc :observed-view tree-view)
          (ui/add-child! root tree-view)
          (ui/observe-selection tree-view (fn [selection-owner selected-items]
                                            (swap! results assoc
                                                   :selection-owner selection-owner
                                                   :selected-items selected-items)))
          (doto (.getSelectionModel tree-view)
            (.setSelectionMode SelectionMode/MULTIPLE)
            (.select 1)
            (.select 2))))
      (let [{:keys [observed-view selection-owner selected-items]} @results]
        (is (instance? TreeView observed-view))
        (is (identical? observed-view selection-owner))
        (is (= [:a :b] selected-items)))))
  (testing "ListView"
    (let [results (atom {:observed-view nil :selection-owner nil :selected-items nil})]
      (ui/run-now
        (let [root (ui/main-root)
              list-view (doto (ListView.)
                          (ui/selection-mode! :multiple)
                          (ui/items! [:a :b :c]))]
          (swap! results assoc :observed-view list-view)
          (ui/add-child! root list-view)
          (ui/observe-selection list-view (fn [selection-owner selected-items]
                                            (swap! results assoc
                                                   :selection-owner selection-owner
                                                   :selected-items selected-items)))
          (ui/select! list-view :b)
          (ui/select! list-view :c)))
      (let [{:keys [observed-view selection-owner selected-items]} @results]
        (is (instance? ListView observed-view))
        (is (identical? observed-view selection-owner))
        (is (= [:b :c] selected-items)))))
  (testing "ComboBox"
    (let [results (atom {:observed-view nil :selection-owner nil :selected-items nil})]
      (ui/run-now
        (let [root (ui/main-root)
              combo-box (doto (ComboBox.)
                          (ui/items! [:a :b :c]))]
          (swap! results assoc :observed-view combo-box)
          (ui/add-child! root combo-box)
          (ui/observe-selection combo-box (fn [selection-owner selected-items]
                                            (swap! results assoc
                                                   :selection-owner selection-owner
                                                   :selected-items selected-items)))
          (ui/select! combo-box :b)))
      (let [{:keys [observed-view selection-owner selected-items]} @results]
        (is (instance? ComboBox observed-view))
        (is (identical? observed-view selection-owner))
        (is (= [:b] selected-items))))))
