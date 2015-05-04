(ns editor.ui-test
  (:require [clojure.test :refer :all]
            [editor.ui :as ui])
  (:import [javafx.scene Scene]
           [javafx.scene.layout Pane]))

(defn fixture [f]
  (ui/init)
  (f))

(use-fixtures :each fixture)

(not (= ({:label "Open", :id :editor.ui-test/open} {:label "Save"} [:label "Quit"])
        [{:label "Open", :id :editor.ui-test/open} {:label "Save"} {:label "Quit"}]))

(deftest extend-menu-test
  (ui/extend-menu ::root-menu nil
                  [{:label "Open"
                    :id ::open}])
  (ui/extend-menu ::save-menu ::open
                  [{:label "Save"}])
  (ui/extend-menu ::quit-menu ::root-menu
                  [{:label "Quit"}])
  (is (= (#'ui/realize-menu ::root-menu) [{:label "Open"
                                           :id ::open}
                                          {:label "Save"}
                                          {:label "Quit"}])))

(deftest toolbar-test
  (ui/extend-menu ::my-menu nil
                  [{:label "Open"
                    :id ::open}])

  (let [root (Pane.)
        scene (ui/run-now (Scene. root))
        selection-provider nil
        command-context {}]
    (.setId root "toolbar")
    (ui/register-tool-bar scene command-context selection-provider "#toolbar" ::my-menu)
    (ui/run-now (ui/refresh scene))
    (let [c1 (ui/run-now (ui/refresh scene) (.getChildren root))
          c2 (ui/run-now (ui/refresh scene) (.getChildren root))]
      (is (= 1 (count c1) (count c2)))
      (is (= (.get c1 0) (.get c2 0))))

    (ui/extend-menu ::extra ::open
                    [{:label "Save"}])
    (ui/run-now (ui/refresh scene))
    (is (= 2 (count (.getChildren root))))))

