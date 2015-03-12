(ns editor.input
  (:require [dynamo.types :as t])
  (:import  [com.defold.editor Start UIUtil]
            [java.io File]
            [java.lang Runnable System]
            [java.nio.file Paths]
            [java.awt Font]
            [java.awt.image BufferedImage]
            [javafx.application Platform]
            [javafx.fxml FXMLLoader]
            [javafx.collections FXCollections ObservableList]
            [javafx.scene Scene Node Parent]
            [javafx.stage Stage FileChooser]
            [javafx.scene.image Image ImageView WritableImage PixelWriter]
            [javafx.scene.input InputEvent MouseEvent MouseButton ScrollEvent]
            [javafx.event ActionEvent EventHandler EventType]
            [javafx.scene.control Button TitledPane TextArea TreeItem Menu MenuItem MenuBar Tab ProgressBar]
            [javafx.scene.layout AnchorPane StackPane HBox Priority]
            [javafx.embed.swing SwingFXUtils]
            [javax.vecmath Point3d Matrix4d Vector4d Matrix3d Vector3d]
            [com.jogamp.opengl.util.awt TextRenderer Screenshot]
            [dynamo.types Camera AABB Region]))

(set! *warn-on-reflection* true)

(def ActionType (t/enum :scroll :mouse-pressed :mouse-released :mouse-clicked :mouse-moved :undefined))

(def ButtonType (t/enum :none :primary :middle :secondary))

(def action-map {ScrollEvent/SCROLL :scroll
                 MouseEvent/MOUSE_PRESSED :mouse-pressed
                 MouseEvent/MOUSE_RELEASED :mouse-released
                 MouseEvent/MOUSE_CLICKED :mouse-clicked
                 MouseEvent/MOUSE_MOVED :mouse-moved
                 MouseEvent/MOUSE_DRAGGED :mouse-moved})

(defn translate-action [^EventType jfx-action]
  (get action-map jfx-action :undefined))

(def button-map {MouseButton/NONE :none
                 MouseButton/PRIMARY :primary
                 MouseButton/SECONDARY :secondary
                 MouseButton/MIDDLE :middle})

(defn translate-button [^MouseButton jfx-button]
  (get button-map jfx-button :none))

(defn action-from-jfx [^InputEvent jfx-event]
  (let [type (translate-action (.getEventType jfx-event))
        action {:type type}]
    (case type
      :undefined action
      :scroll (let [scroll-event ^ScrollEvent jfx-event]
                (assoc action
                       :x (.getX scroll-event)
                       :y (.getY scroll-event)
                       :delta-x (.getDeltaX scroll-event)
                       :delta-y (.getDeltaY scroll-event)
                       :alt (.isAltDown scroll-event)
                       :shift (.isShiftDown scroll-event)
                       :meta (.isMetaDown scroll-event)
                       :control (.isControlDown scroll-event)))
      (let [mouse-event ^MouseEvent jfx-event]
        (assoc action
               :button (translate-button (.getButton mouse-event))
               :x (.getX mouse-event)
               :y (.getY mouse-event)
               :alt (.isAltDown mouse-event)
               :shift (.isShiftDown mouse-event)
               :meta (.isMetaDown mouse-event)
               :control (.isControlDown mouse-event)))
      ))
)
