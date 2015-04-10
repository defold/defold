(ns editor.app-view
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [dynamo.node :as n]
            [dynamo.types :as t]
            [editor.asset-browser :as asset-browser]
            [editor.atlas :as atlas]
            [editor.collection :as collection]
            [editor.core :as core]
            [editor.cubemap :as cubemap]
            [editor.game-object :as game-object]
            [editor.graph-view :as graph-view]
            [editor.image :as image]
            [editor.jfx :as jfx]
            [editor.menu :as menu]
            [editor.outline-view :as outline-view]
            [editor.platformer :as platformer]
            [editor.project :as project]
            [editor.properties-view :as properties-view]
            [editor.scene :as scene]
            [editor.sprite :as sprite]
            [editor.switcher :as switcher]
            [editor.ui :as ui]
            [editor.workspace :as workspace])
  (:import [com.defold.editor Start]
           [com.jogamp.opengl.util.awt Screenshot]
           [java.awt Desktop]
           [javafx.animation AnimationTimer]
           [javafx.application Platform]
           [javafx.collections FXCollections ObservableList]
           [javafx.embed.swing SwingFXUtils]
           [javafx.event ActionEvent EventHandler]
           [javafx.fxml FXMLLoader]
           [javafx.geometry Insets]
           [javafx.scene Scene Node Parent]
           [javafx.scene.control Button ColorPicker Label TextField TitledPane TextArea TreeItem TreeCell Menu MenuItem MenuBar Tab ProgressBar]
           [javafx.scene.image Image ImageView WritableImage PixelWriter]
           [javafx.scene.input MouseEvent]
           [javafx.scene.layout AnchorPane GridPane StackPane HBox Priority]
           [javafx.scene.paint Color]
           [javafx.stage Stage FileChooser]
           [javafx.util Callback]
           [java.io File]
           [java.nio.file Paths]
           [java.util.prefs Preferences]
           [javax.media.opengl GL GL2 GLContext GLProfile GLDrawableFactory GLCapabilities]))

(def static-menu [{:label "File" :children []}
                  {:label "Edit" :children []}
                  {:label "Help" :children [{:label "About Defold"
                                             :handler-fn (fn [event] (prn "ABOUT!"))}]}])

(g/defnk produce-menu-bar [main-menu menu-bar]
  (.setAll (.getMenus menu-bar) (menu/make-menus main-menu))
  menu-bar)

(g/defnode AppView
  (property stage Stage)
  (property menu-bar MenuBar)
  (property refresh-timer AnimationTimer)
  (property auto-pulls t/Any)

  (input main-menus [t/Any])

  (output main-menu t/Any :cached (g/fnk [main-menus] (reduce menu/merge-menus static-menu main-menus)))
  (output menu-bar MenuBar :cached produce-menu-bar)
  
  (trigger stop-animation :deleted (fn [tx graph self label trigger]
                                     (.stop ^AnimationTimer (:refresh-timer self)))))

(defn make-app-view [graph stage menu-bar]
  (.setUseSystemMenuBar menu-bar true)
  (.setTitle stage "Defold Editor 2.0!")
  (let [app-view (first (g/tx-nodes-added (g/transact (g/make-node graph AppView :stage stage :menu-bar menu-bar))))]
    (let [refresh-timer (proxy [AnimationTimer] []
                          (handle [now]
                            (let [auto-pulls (:auto-pulls (g/refresh app-view))]
                              (doseq [[node label] auto-pulls]
                                (g/node-value node label)))))]
      (g/transact
        (concat
          (g/set-property app-view :refresh-timer refresh-timer)
          (g/set-property app-view :auto-pulls [[app-view :menu-bar]])))
      (.start refresh-timer))
    app-view))
