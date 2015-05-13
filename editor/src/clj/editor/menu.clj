(ns editor.menu
  (:require [dynamo.graph :as g]
            [dynamo.types :as t]
            [editor.jfx :as jfx]
            [editor.ui :as ui])
  (:import [com.defold.editor Start]
           [com.jogamp.opengl.util.awt Screenshot]
           [java.awt Desktop]
           [javafx.application Platform]
           [javafx.collections FXCollections ObservableList]
           [javafx.embed.swing SwingFXUtils]
           [javafx.event ActionEvent EventHandler]
           [javafx.fxml FXMLLoader]
           [javafx.geometry Insets]
           [javafx.scene Scene Node Parent]
           [javafx.scene.control Button ColorPicker Label TextField TitledPane TextArea TreeItem TreeCell Menu MenuItem MenuBar ContextMenu Tab ProgressBar]
           [javafx.scene.image Image ImageView WritableImage PixelWriter]
           [javafx.scene.input MouseEvent KeyCombination]
           [javafx.scene.layout AnchorPane GridPane StackPane HBox Priority]
           [javafx.scene.paint Color]
           [javafx.stage Stage FileChooser]
           [javafx.util Callback]
           [java.io File]
           [java.nio.file Paths]
           [java.util.prefs Preferences]
           [javax.media.opengl GL GL2 GLContext GLProfile GLDrawableFactory GLCapabilities]))


(defn merge-menus [menu other-menu]
  (let [first-set (into #{} (map :label menu))
        other-map (into {} (map (fn [other] {(:label other) other}) other-menu))
        merged (map (fn [item]
                      (if (:children item)
                        (assoc item :children (merge-menus (:children item) (:children (get other-map (:label item)))))
                        item))
                    menu)
        new (filter (fn [other] (not (first-set (:label other)))) other-menu)]
    (concat merged new)))

(defn clj->jfx [{:keys [label icon children handler-fn acc enable-fn] :as item}]
  (let [label (if (fn? label) (label) label)
        ^MenuItem menu-item (if children
                              (Menu. label)
                              (MenuItem. label))]
    (when icon (.setGraphic menu-item (jfx/get-image-view icon)))
    (when acc (.setAccelerator menu-item (KeyCombination/keyCombination acc)))
    (when enable-fn (.setDisable menu-item (not (enable-fn))))
    (cond
      children
      (let [populate-fn (fn [] (let [^Menu mi menu-item
                                    ^"[Ljavafx.scene.control.MenuItem;" items (map clj->jfx children)]
                                (.setAll (.getItems mi) items)))]
        (populate-fn)
        (.addEventHandler menu-item Menu/ON_SHOWING (ui/event-handler event (populate-fn))))
      handler-fn
      (.addEventHandler menu-item ActionEvent/ACTION (ui/event-handler event (handler-fn event))))
    menu-item))

(defn make-menus [menus]
  (map clj->jfx menus))

(defn make-context-menu [menu]
  (let [context-menu (ContextMenu.)
        populate-fn (fn [] (let [^"[Ljavafx.scene.control.MenuItem;" items (map clj->jfx menu)]
                           (.setAll (.getItems context-menu) items)))]
    (populate-fn)
    (.setOnShowing context-menu (ui/event-handler event populate-fn))
    context-menu))
