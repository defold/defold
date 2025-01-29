;; Copyright 2020-2025 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;;
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;;
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns editor.input
  (:require [dynamo.graph :as g]
            [schema.core :as s])
  (:import [com.defold.editor Start UIUtil]
           [com.jogamp.opengl.util.awt TextRenderer]
           [editor.types Camera AABB Region]
           [java.awt Font]
           [java.awt.image BufferedImage]
           [javafx.application Platform]
           [javafx.collections FXCollections ObservableList]
           [javafx.embed.swing SwingFXUtils]
           [javafx.event ActionEvent EventHandler EventType]
           [javafx.fxml FXMLLoader]
           [javafx.scene Scene Node Parent]
           [javafx.scene.control Button TitledPane TextArea TreeItem Menu MenuItem MenuBar Tab ProgressBar]
           [javafx.scene.image Image ImageView WritableImage PixelWriter]
           [javafx.scene.input InputEvent MouseEvent MouseButton ScrollEvent]
           [javafx.scene.layout AnchorPane StackPane HBox Priority]
           [javafx.stage Stage FileChooser]
           [java.io File]
           [java.lang Runnable System]
           [javax.vecmath Point3d Matrix4d Vector4d Matrix3d Vector3d]))

(set! *warn-on-reflection* true)

(def ActionType (s/enum :scroll :mouse-pressed :mouse-released :mouse-clicked :mouse-moved :undefined))

(def ButtonType (s/enum :none :primary :middle :secondary))

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
               :control (.isControlDown mouse-event)
               :click-count (.getClickCount mouse-event))))))
