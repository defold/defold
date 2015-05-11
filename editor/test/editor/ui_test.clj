(ns editor.ui-test
  (:require [clojure.test :refer :all]
            [editor.ui :as ui]
            [editor.workspace :as workspace]
            [editor.handler :as handler])
  (:import [javafx.scene Scene]
           [javafx.scene.control Menu MenuBar MenuItem]
           [javafx.scene.layout Pane]))

(defn fixture [f]
  (with-redefs [ui/*menus* (atom {})
                handler/*handlers* (atom {})]
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
  (is (= (#'ui/realize-menu ::menubar) [{:label "File"
                                         :children [{:label "New"
                                                    :id ::new}
                                                    {:label "Save"}
                                                    {:label "Quit"}]}])))

(defrecord TestSelectionProvider [selection]
  workspace/SelectionProvider
  (selection [this] selection))

(deftest menu-test
  (ui/extend-menu ::my-menu nil
                  [{:label "File"
                    :children [{:label "Open"
                                :id ::open
                                :command :open}
                               {:label "Save"
                                :command :save}]}])

  (handler/defhandler :open
      (enabled? [selection] true)
      (run [selection] 123))

  (let [root (Pane.)
        scene (ui/run-now (Scene. root))
        selection-provider (TestSelectionProvider. [])
        command-context {}]
   (let [menus (#'ui/make-menu (#'ui/make-desc nil ::my-menu command-context selection-provider) (#'ui/realize-menu ::my-menu))]
     (is (= 1 (count menus)))
     (is (instance? Menu (first menus)))
     (is (= 2 (count (.getItems (first menus)))))
     (is (instance? MenuItem (first (.getItems (first menus))))))))

(deftest toolbar-test
  (ui/extend-menu ::my-menu nil
                  [{:label "Open"
                    :command :open
                    :id ::open}])

  (let [root (Pane.)
        scene (ui/run-now (Scene. root))
        selection-provider (TestSelectionProvider. [])
        command-context {}]
    (.setId root "toolbar")
    (ui/register-toolbar scene command-context selection-provider "#toolbar" ::my-menu)
    (ui/run-now (ui/refresh scene))
    (let [c1 (ui/run-now (ui/refresh scene) (.getChildren root))
          c2 (ui/run-now (ui/refresh scene) (.getChildren root))]
      (is (= 1 (count c1) (count c2)))
      (is (= (.get c1 0) (.get c2 0))))

    (ui/extend-menu ::extra ::open
                    [{:label "Save"}])
    (ui/run-now (ui/refresh scene))
    (is (= 2 (count (.getChildren root))))))

(deftest menubar-test
  (ui/extend-menu ::my-menu nil
                  [{:label "File"
                    :children
                    [{:label "Open"
                      :id ::open}]}])

  (let [root (Pane.)
        scene (ui/run-now (Scene. root))
        selection-provider (TestSelectionProvider. [])
        command-context {}
        menubar (MenuBar.)]
    (ui/run-now (.add (.getChildren root) menubar))
    (.setId menubar "menubar")
    (ui/register-menubar scene command-context selection-provider "#menubar" ::my-menu)
    (ui/run-now (ui/refresh scene))
    (let [c1 (ui/run-now (ui/refresh scene) (.getItems (first (.getMenus menubar))))
          c2 (ui/run-now (ui/refresh scene) (.getItems (first (.getMenus menubar))))]
      (is (= 1 (count c1) (count c2)))
      (is (= (.get c1 0) (.get c2 0))))
    (ui/extend-menu ::extra ::open
                    [{:label "Save"}])
    (ui/run-now (ui/refresh scene))
    (let [c1 (ui/run-now (ui/refresh scene) (.getItems (first (.getMenus menubar))))
          c2 (ui/run-now (ui/refresh scene) (.getItems (first (.getMenus menubar))))]
      (is (= 2 (count c1) (count c2)))
      (is (= (.get c1 0) (.get c2 0))))))


