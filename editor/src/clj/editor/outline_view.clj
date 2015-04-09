(ns editor.outline-view
  (:require [dynamo.graph :as g]
            [editor.jfx :as jfx]
            [editor.menu :as menu])
  (:import [com.defold.editor Start]
           [com.jogamp.opengl.util.awt Screenshot]
           [javafx.application Platform]
           [javafx.collections FXCollections ObservableList ListChangeListener]
           [javafx.embed.swing SwingFXUtils]
           [javafx.event ActionEvent EventHandler]
           [javafx.fxml FXMLLoader]
           [javafx.geometry Insets]
           [javafx.scene Scene Node Parent]
           [javafx.scene.control Button ColorPicker Label TextField TitledPane TextArea TreeItem TreeCell Menu MenuItem MenuBar Tab ProgressBar SelectionMode]
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
(declare tree-item)

; TreeItem creator
(defn- list-children [parent]
  (let [children (:children parent)]
    (if (empty? children)
      (FXCollections/emptyObservableList)
      (doto (FXCollections/observableArrayList)
        (.addAll (map tree-item children))))))

; NOTE: Without caching stack-overflow... WHY?
(defn tree-item [parent]
  (let [cached (atom false)]
    (proxy [TreeItem] [parent]
      (isLeaf []
        (empty? (:children parent)))
      (getChildren []
        (let [children (proxy-super getChildren)]
          (when-not @cached
            (reset! cached true)
            (.setAll children (list-children parent)))
          children)))))

(defn setup [tree node selection-fn]
  (-> tree
    (.getSelectionModel)
    (.setSelectionMode SelectionMode/MULTIPLE))
  (.setCellFactory tree (reify Callback (call ^TreeCell [this view]
                                          (proxy [TreeCell] []
                                            (updateItem [item empty]
                                              (proxy-super updateItem item empty)
                                              (if empty
                                                (do
                                                  (proxy-super setText nil)
                                                  (proxy-super setGraphic nil)
                                                  (proxy-super setContextMenu nil))
                                                (let [{:keys [label icon context-menu]} item]
                                                  (proxy-super setText label)
                                                  (proxy-super setGraphic (jfx/get-image-view icon))
                                                  (when context-menu (proxy-super setContextMenu (menu/make-context-menu context-menu))))))))))
  (-> tree
    (.getSelectionModel)
    (.getSelectedItems)
    (.addListener (reify ListChangeListener (onChanged [this change]
                                              ; TODO - handle selection order
                                              (selection-fn (map #(.getValue %1) (.getList change)))))))
  (let [root (when (:outline (g/outputs node))
               (tree-item (g/node-value node :outline)))]
    (.setRoot tree root)))
