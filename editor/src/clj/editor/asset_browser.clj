(ns editor.asset-browser
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
           [javafx.scene.input MouseEvent]
           [javafx.scene.layout AnchorPane GridPane StackPane HBox Priority]
           [javafx.scene.paint Color]
           [javafx.stage Stage FileChooser]
           [javafx.util Callback]
           [java.io File]
           [java.nio.file Paths]
           [java.util.prefs Preferences]
           [java.awt Desktop]
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
        (not= :folder (workspace/source-type (.getValue this))))
      (getChildren []
        (let [children (proxy-super getChildren)]
          (when-not @cached
            (reset! cached true)
            (.setAll children (list-children (.getValue this))))
          children)))))

(defn- setup-asset-browser [workspace tree-view open-resource-fn]
  (let [handler (reify EventHandler
                  (handle [this e]
                    (when (= 2 (.getClickCount e))
                      (let [item (-> tree-view (.getSelectionModel) (.getSelectedItem))
                            resource (.getValue item)]
                        (when (= :file (workspace/source-type resource))
                          (open-resource-fn resource))))))]
    (.setOnMouseClicked tree-view handler)
    (.setCellFactory tree-view (reify Callback (call ^TreeCell [this view]
                                                 (proxy [TreeCell] []
                                                   (updateItem [resource empty]
                                                     (proxy-super updateItem resource empty)
                                                     (let [name (or (and (not empty) (not (nil? resource)) (workspace/resource-name resource)) nil)]
                                                       (proxy-super setText name))
                                                     (proxy-super setGraphic (jfx/get-image-view (workspace/resource-icon resource))))))))
    (.setRoot tree-view (tree-item (g/node-value workspace :resource-tree)))))

(defn make-asset-browser [workspace tree-view open-resource-fn]
  (setup-asset-browser workspace tree-view open-resource-fn))