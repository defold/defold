(ns editor.text
  (:require [dynamo.graph :as g]
            [editor.core :as core]
            [editor.ui :as ui]
            [editor.workspace :as workspace])
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
           [javafx.scene.control Button ColorPicker Label TextField TitledPane TextArea TreeItem TreeCell Menu MenuItem MenuBar Tab ProgressBar]
           [javafx.scene.image Image ImageView WritableImage PixelWriter]
           [javafx.scene.input MouseEvent]
           [javafx.scene.layout AnchorPane GridPane StackPane HBox Priority Pane]
           [javafx.scene.paint Color]
           [javafx.stage Stage FileChooser]
           [javafx.util Callback]
           [java.io File]
           [java.nio.file Paths]
           [java.util.prefs Preferences]
           [javax.media.opengl GL GL2 GLContext GLProfile GLDrawableFactory GLCapabilities]))

(g/defnode TextView
  (inherits core/ResourceNode)

  (property text-area TextArea)

  )


(defn make-view [graph ^Parent parent resource-node opts]
  (let [text-area (TextArea.)]
    (.appendText text-area (slurp (:resource resource-node)))
    (.add (.getChildren ^Pane parent) text-area)
    (ui/fill-control text-area)
    (g/make-node! graph TextView :text-area text-area)))

(defn register-view-types [workspace]
  (workspace/register-view-type workspace
                                :id :text
                                :make-view-fn make-view))
