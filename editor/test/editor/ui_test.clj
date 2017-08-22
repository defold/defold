(ns editor.ui-test
  (:require [clojure.test :refer :all]
            [editor.handler :as handler]
            [editor.menu :as menu]
            [editor.ui :as ui])
  (:import [javafx.scene Scene]
           [javafx.scene.control ComboBox ListView Menu MenuBar MenuItem SelectionMode TreeItem TreeView]
           [javafx.scene.layout Pane VBox]))

(defn- make-fake-stage []
  (let [root (VBox.)
        stage (ui/make-stage)
        scene (Scene. root)]
    (.setScene stage scene)
    stage))

(defn fixture [f]
  (with-redefs [menu/*menus* (atom {})
                handler/*handlers* (atom {})
                ui/*main-stage* (atom (ui/run-now (make-fake-stage)))]
    (f)))

(use-fixtures :each fixture)

(deftest extend-menu-test
  (ui/extend-menu ::menubar nil
                  [{:label "File"
                    :children [ {:label "New"
                                 :id ::new} ]}])
  (ui/extend-menu ::save-menu ::new
                  [{:label "Save"}])
  (ui/extend-menu ::quit-menu ::new
                  [{:label "Quit"}])
  (is (= (#'menu/realize-menu ::menubar) [{:label "File"
                                           :children [{:label "New"
                                                      :id ::new}
                                                      {:label "Save"}
                                                      {:label "Quit"}]}])))

(defrecord TestSelectionProvider [selection]
  handler/SelectionProvider
  (selection [this] selection)
  (succeeding-selection [this] [])
  (alt-selection [this] []))

(deftest menu-test
  (ui/extend-menu ::my-menu nil
                  [{:label "File"
                    :children [{:label "Open"
                                :id ::open
                                :command :open}
                               {:label "Save"
                                :command :save}]}])

  (handler/defhandler :open :global
      (enabled? [selection] true)
      (run [selection] 123))

  (handler/defhandler :save :global
    (enabled? [selection] true)
    (run [selection] 124))

  (let [root (Pane.)
        scene (ui/run-now (Scene. root))
        selection-provider (TestSelectionProvider. [])
        command-context {:name :global :env {:selection []}}]
   (let [menu-items (#'ui/make-menu-items scene (#'menu/realize-menu ::my-menu) [command-context] {})]
     (is (= 1 (count menu-items)))
     (is (instance? Menu (first menu-items)))
     (is (= 2 (count (.getItems (first menu-items)))))
     (is (instance? MenuItem (first (.getItems (first menu-items))))))))


(deftest options-menu-test
  (ui/extend-menu ::my-menu nil
                  [{:label "Add"
                    :command :add}])

  (handler/defhandler :add :global
    (run [user-data] user-data)
    (active? [user-data] (or (not user-data) (= user-data 1)))
    (options [user-data] (when-not user-data [{:label "first"
                                               :command :add
                                               :user-data 1}
                                              {:label "second"
                                               :command :add
                                               :user-data 2}])))

  (let [command-context {:name :global :env {}}]
    (let [menu-items (#'ui/make-menu-items nil (#'menu/realize-menu ::my-menu) [command-context] {})]
      (is (= 1 (count menu-items)))
      (is (= 1 (count (.getItems (first menu-items))))))))

(deftest toolbar-test
  (ui/extend-menu ::my-menu nil
                  [{:label "Open"
                    :command :open
                    :id ::open}])

  (handler/defhandler :open :global
    (enabled? [selection] true)
    (run [selection] 123)
    (state [] false))

  (handler/defhandler :save :global
    (enabled? [selection] true)
    (run [selection] 124)
    (state [] false))

  (let [root (Pane.)
        scene (ui/run-now (Scene. root))
        selection-provider (TestSelectionProvider. [])]
    (.setId root "toolbar")
    (ui/context! root :global {} selection-provider)
    (ui/register-toolbar scene root "#toolbar" ::my-menu)
    (ui/run-now (ui/refresh scene))
    (let [c1 (ui/run-now (ui/refresh scene) (.getChildren root))
          c2 (ui/run-now (ui/refresh scene) (.getChildren root))]
      (is (= 1 (count c1) (count c2)))
      (is (= (.get c1 0) (.get c2 0))))

    (ui/extend-menu ::extra ::open
                    [{:label "Save"
                      :command :save}])
    (ui/run-now (ui/refresh scene))
    (is (= 2 (count (.getChildren root))))))

(deftest menubar-test
  (ui/extend-menu ::my-menu nil
                  [{:label "File"
                    :children
                    [{:label "Open"
                      :id ::open
                      :command :open}]}])

  (handler/defhandler :open :global
    (enabled? [selection] true)
    (run [selection] 123))

  (handler/defhandler :save :global
    (enabled? [selection] true)
    (run [selection] 124))

  (let [root (Pane.)
        scene (ui/run-now (Scene. root))
        selection-provider (TestSelectionProvider. [])
        menubar (MenuBar.)]
    (ui/context! root :global {} selection-provider)
    (ui/run-now (.add (.getChildren root) menubar))
    (.setId menubar "menubar")
    (ui/register-menubar scene menubar ::my-menu)
    (ui/run-now (ui/refresh scene))
    (let [c1 (ui/run-now (ui/refresh scene) (.getItems (first (.getMenus menubar))))
          c2 (ui/run-now (ui/refresh scene) (.getItems (first (.getMenus menubar))))]
      (is (= 1 (count c1) (count c2)))
      (is (= (.get c1 0) (.get c2 0))))
    (ui/extend-menu ::extra ::open
                    [{:label "Save"
                      :command :save}])
    (ui/run-now (ui/refresh scene))
    (let [c1 (ui/run-now (ui/refresh scene) (.getItems (first (.getMenus menubar))))
          c2 (ui/run-now (ui/refresh scene) (.getItems (first (.getMenus menubar))))]
      (is (= 2 (count c1) (count c2)))
      (is (= (.get c1 0) (.get c2 0))))))

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

(deftest tree-view-get-selected-items-bug-test
  ; Checks for the presence of a JavaFX bug.
  ; Some MultipleSelectionModel implementations are broken
  ; and selected-items will contain nil entries. If this
  ; tests fails after a JDK upgrade, please search the
  ; code for "DEFEDIT-648" and remove any workarounds!
  (let [selected-items (atom nil)]
    (ui/run-now
      (let [root (ui/main-root)
            tree-item-a (TreeItem. :a)
            tree-item-b (TreeItem. :b)
            tree-view (doto (TreeView.)
                        (.setRoot (doto (TreeItem. :root)
                                    (.setExpanded true)
                                    (-> .getChildren
                                        (.addAll [tree-item-a
                                                  tree-item-b])))))
            selection-model (.getSelectionModel tree-view)]
        (ui/add-child! root tree-view)
        (doto selection-model
          (.setSelectionMode SelectionMode/MULTIPLE)
          (.select 1)
          (.select 2)
          (.clearSelection 1))
        (reset! selected-items (.getSelectedItems selection-model))))
    (is (= [nil] @selected-items))))

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
