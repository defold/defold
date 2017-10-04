(ns editor.menu-test
  (:require [clojure.test :refer :all]
            [editor.menu :as menu]))

(defonce main-menu-data [{:label "File"
                          :id ::file
                          :children [{:label "New"
                                      :id ::new
                                      :command :new}
                                     {:label "Open"
                                      :id ::open
                                      :command :open}]}
                         {:label "Edit"
                          :id ::edit
                          :children [{:label "Undo"
                                      :icon "icons/undo.png"
                                      :command :undo}
                                     {:label "Redo"
                                      :icon "icons/redo.png"
                                      :command :redo}]}
                         {:label "Help"
                          :children [{:label "About"
                                      :command :about}]}])

(defonce scene-menu-data [{:label "Scene"
                           :children [{:label "Do stuff"}
                                      {:label :separator
                                       :id ::scene-end}]}])

(defonce tile-map-data [{:label "Tile Map"
                         :children [{:label "Erase Tile"}]}])

(deftest main-menu
  (menu/with-test-menus
    (menu/extend-menu ::menubar nil main-menu-data)
    (menu/extend-menu ::menubar ::edit scene-menu-data)
    (menu/extend-menu ::menubar ::scene-end tile-map-data)
    (let [m (menu/realize-menu ::menubar)]
      (is (some? (get-in m [2 :children]))))))
