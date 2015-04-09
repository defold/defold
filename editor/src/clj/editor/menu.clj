(ns editor.menu
  (:require [camel-snake-kebab :as camel]
            [clojure.java.io :as io]
            [dynamo.graph :as g]
            [dynamo.node :as n]
            [dynamo.system :as ds]
            [dynamo.types :as t]
            [editor.core :as core]
            [editor.ui :as ui]
            [editor.jfx :as jfx]
            [editor.workspace :as workspace]
            [editor.project :as project]
            [editor.asset-browser :as asset-browser]
            [editor.outline-view :as outline-view]
            [editor.properties-view :as properties-view]
            [editor.graph-view :as graph-view]
            [editor.scene :as scene]
            [editor.image :as image]
            [editor.collection :as collection]
            [editor.game-object :as game-object]
            [editor.atlas :as atlas]
            [editor.cubemap :as cubemap]
            [editor.platformer :as platformer]
            [editor.switcher :as switcher]
            [editor.sprite :as sprite]
            [internal.clojure :as clojure]
            [internal.disposal :as disp]
            [internal.graph.types :as gt]
            [service.log :as log])
  (:import [com.defold.editor Start]
           [com.jogamp.opengl.util.awt Screenshot]
           [javafx.application Platform]
           [javafx.collections FXCollections ObservableList]
           [javafx.embed.swing SwingFXUtils]
           [javafx.event ActionEvent EventHandler]
           [javafx.fxml FXMLLoader]
           [javafx.geometry Insets]
           [javafx.scene Scene Node Parent]
           [javafx.scene.control Button ColorPicker Label TextField TitledPane TextArea TreeItem TreeCell Menu MenuItem MenuBar Tab ProgressBar]
           [javafx.scene.image Image ImageView WritableImage PixelWriter]
           [javafx.scene.input MouseEvent KeyCombination]
           [javafx.scene.layout AnchorPane GridPane StackPane HBox Priority]
           [javafx.scene.paint Color]
           [javafx.stage Stage FileChooser]
           [javafx.util Callback]
           [java.io File]
           [java.nio.file Paths]
           [java.util.prefs Preferences]
           [java.awt Desktop]
           [javax.media.opengl GL GL2 GLContext GLProfile GLDrawableFactory GLCapabilities]))

(defn- merge-menus [menu other-menu]
  (let [first-set (into #{} (map :label menu))
        other-map (into {} (map (fn [other] {(:label other) other}) other-menu))
        merged (map (fn [item]
                      (if (:children item)
                        (assoc item :children (merge-menus (:children item) (:children (get other-map (:label item)))))
                        item))
                    menu)
        new (filter (fn [other] (not (first-set (:label other)))) other-menu)]
    (concat merged new)))

(defn- clj->jfx [{:keys [label icon children handler-fn acc enable-fn] :as item}]
  (let [label (if (fn? label) (label) label)
        menu-item (if children
                    (Menu. label)
                    (MenuItem. label))]
    (when icon (.setGraphic menu-item (jfx/get-image-view icon)))
    (when acc (.setAccelerator menu-item (KeyCombination/keyCombination acc)))
    (when enable-fn (.setDisable menu-item (not (enable-fn))))
    (cond
      children
      (let [populate-fn (fn [] (.setAll (.getItems menu-item) (map clj->jfx children)))]
        (populate-fn)
        (.addEventHandler menu-item Menu/ON_SHOWING (ui/event-handler event (populate-fn))))
      handler-fn
      (.addEventHandler menu-item ActionEvent/ACTION (ui/event-handler event (handler-fn event))))
    menu-item))

(defn- apply-menu [menu-bar menu]
  (.setAll (.getMenus menu-bar) (map clj->jfx menu)))

(g/defnk produce-menu-bar [static-menu menus menu-bar]
  (let [menu (reduce merge-menus static-menu menus)]
    (apply-menu menu-bar menu))
  menu-bar)

(g/defnode MenuView
  (property menu-bar MenuBar)
  (property static-menu t/Any)
  (input menus [t/Any])
  (output menu-bar MenuBar :cached produce-menu-bar))

(defn make-menu [graph menu-bar static-menu]
  (first (ds/tx-nodes-added (ds/transact (g/make-node graph MenuView :menu-bar menu-bar :static-menu static-menu)))))
