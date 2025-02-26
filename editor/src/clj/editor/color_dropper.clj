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

(ns editor.color-dropper
  (:require [dynamo.graph :as g]
            [editor.handler :as handler]
            [editor.ui :as ui])
  (:import [javafx.geometry Rectangle2D]
           [javafx.scene Cursor Scene]
           [javafx.scene.canvas Canvas GraphicsContext]
           [javafx.scene.image PixelReader WritableImage]
           [javafx.scene.input KeyCode KeyEvent MouseEvent]
           [javafx.scene.layout StackPane]
           [javafx.scene.paint Color]
           [javafx.scene.shape Circle]
           [javafx.scene.robot Robot]
           [javafx.stage Screen Stage StageStyle Modality]
           [java.awt MouseInfo]))

(set! *warn-on-reflection* true)

(g/defnode ColorDropper
  (property canvas Canvas)
  (property color Color)
  (property size g/Any)
  (property image WritableImage))

(defn- highlight-pixel!
  [^GraphicsContext graphics-context x y size]
  (doto graphics-context
    (.setFill Color/TRANSPARENT)
    (.setStroke Color/RED)
    (.strokeRect x y size size)))

(defn- paint-pixel!
  [^GraphicsContext graphics-context x y size color]
  (doto graphics-context
    (.setFill Color/TRANSPARENT)
    (.setStroke Color/GRAY)
    (.strokeRect x y size size)
    (.setFill color)
    (.fillRect x y size size)))

(defn- in-bounds?
  [view-node x y]
  (let [{:keys [width height]} (g/node-value view-node :size)]
    (and (< 0 x width)
         (< 0 y height))))

(defn mouse-move-handler!
  [view-node ^Stage stage ^Canvas canvas ^MouseEvent e]
  (.consume e)
  (when (in-bounds? view-node (.getX e) (.getY e))
    (let [^GraphicsContext graphics-context (.getGraphicsContext2D canvas)
          ^WritableImage image (g/node-value view-node :image)
          ^PixelReader pixel-reader (.getPixelReader image)
          mouse-x (.getX e)
          mouse-y (.getY e)
          delta-x (- (.getScreenX e) mouse-x)
          delta-y (- (.getScreenY e) mouse-y)
          adjusted-x (+ mouse-x delta-x)
          adjusted-y (+ mouse-y delta-y)
          pixel-range (range -4 5)
          pixel-size 10
          half-pixel-size (/ pixel-size 2)
          color (.getColor pixel-reader adjusted-x adjusted-y)
          radius (/ (* pixel-size (count pixel-range)) 2)
          mask (doto (Circle.)
                 (.setCenterX (+ mouse-x))
                 (.setCenterY (+ mouse-y))
                 (.setRadius radius))]
      (g/set-property! view-node :color color)
      (.clearRect graphics-context 0 0 (.getWidth canvas) (.getHeight canvas))

      (doseq [x-range pixel-range
              y-range pixel-range
              :let [x (+ mouse-x (* x-range pixel-size) (- half-pixel-size))
                    y (+ mouse-y (* y-range pixel-size) (- half-pixel-size))
                    color (.getColor pixel-reader (+ adjusted-x x-range) (+ adjusted-y y-range))]]
        (when (in-bounds? view-node x y)
          (paint-pixel! graphics-context x y pixel-size color)))

      (highlight-pixel! graphics-context (- mouse-x half-pixel-size) (- mouse-y half-pixel-size) pixel-size)

      (.setClip canvas mask))))

(defn- apply-and-deactivate!
  [view-node ^Stage stage pick-fn]
  (.close stage)
  (when pick-fn
    (pick-fn (g/node-value view-node :color))))

(defn- key-pressed-handler!
  [view-node ^Stage stage pick-fn ^KeyEvent event]
  (.consume event)
  (condp = (.getCode event)
    KeyCode/ENTER (apply-and-deactivate! view-node stage pick-fn)
    KeyCode/ESCAPE (.close stage)
    nil))

(defn- get-size []
  (reduce (fn [size ^Screen screen]
            (let [screen-bounds (.getBounds screen)]
              (-> size
                  (update :width + (.getWidth screen-bounds))
                  (update :height + (.getHeight screen-bounds)))))
          {:width 0 :height 0}
          (Screen/getScreens)))

(defn- capture! 
  [view-node]
  (let [{:keys [width height]} (g/node-value view-node :size)
        capture-rect (Rectangle2D. 0 0 width height)
        writable-image (WritableImage. width height)
        image (.getScreenCapture (Robot.) writable-image capture-rect)]
    (g/set-property! view-node :image image)))

(defn make-color-dropper! [graph]
  (g/make-node! graph ColorDropper))

(defn- create-stage! [width height]
   (doto (Stage.)
    (.initModality Modality/APPLICATION_MODAL)
    (.initStyle StageStyle/TRANSPARENT)
    (.initOwner (ui/main-stage))
    (.setX 0)
    (.setY 0)
    (.setWidth width)
    (.setHeight height)
    (.setAlwaysOnTop true)
    (.setResizable false)))

(defn activate!
  [view-node pick-fn]
  (let [size (get-size)
        stage (create-stage! (:width size) (:height size))
        pane (doto (StackPane.)
               (.setStyle "-fx-background-color: transparent;"))
        canvas (doto (Canvas.)
                 (.setCursor Cursor/NONE))
        scene (doto (Scene. pane)
                (.setFill Color/TRANSPARENT))]
    (g/set-property! view-node :size size)
    (ui/add-child! pane canvas)
    (.setScene stage scene)
    (.bind (.widthProperty canvas) (.widthProperty pane))
    (.bind (.heightProperty canvas) (.heightProperty pane))
    
    (capture! view-node)

    (doto pane
      (.requestFocus)
      (.addEventHandler KeyEvent/ANY (ui/event-handler event (.consume event)))
      (.addEventHandler KeyEvent/KEY_PRESSED (ui/event-handler event (key-pressed-handler! view-node stage pick-fn event)))
      (.addEventHandler MouseEvent/MOUSE_MOVED (ui/event-handler event (mouse-move-handler! view-node stage canvas event)))
      (.addEventHandler MouseEvent/MOUSE_PRESSED (ui/event-handler event (apply-and-deactivate! view-node stage pick-fn))))
    
    (.show stage)))

(handler/defhandler :color-dropper :global
  (active? [color-dropper evaluation-context user-data] true)
  (run [color-dropper user-data]
       (when-let [{:keys [pick-fn]} user-data]
         (activate! color-dropper pick-fn))))
