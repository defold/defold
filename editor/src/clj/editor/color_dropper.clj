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
  (:import [javafx.scene Cursor Node]
           [javafx.scene.canvas Canvas GraphicsContext]
           [javafx.scene.image PixelReader WritableImage]
           [javafx.scene.input KeyCode KeyEvent MouseEvent]
           [javafx.scene.layout StackPane]
           [javafx.scene.paint Color]
           [javafx.scene.shape Circle]))

(set! *warn-on-reflection* true)

(g/defnode ColorDropper
  (property canvas Canvas)
  (property color Color)
  (property screenshot WritableImage)
  (property dropper-area StackPane)
  (property pick-fn g/Any))

(defn mouse-move-handler!
  [view-node ^MouseEvent e]
  (.consume e)
  (let [^Canvas canvas (g/node-value view-node :canvas)
        ^GraphicsContext graphics-context (.getGraphicsContext2D canvas)
        ^WritableImage screenshot (g/node-value view-node :screenshot)
        ^PixelReader pixel-reader (.getPixelReader screenshot)
        mouse-x (.getX e)
        mouse-y (.getY e)]
    (when (and (< 0 mouse-x (.getWidth canvas))
               (< 0 mouse-y (.getHeight canvas)))
      (let [pixel-range (range -4 5)
            pixel-size 12
            half-pixel-size (/ pixel-size 2)
            color (.getColor pixel-reader mouse-x mouse-y)
            radius (/ (* pixel-size (count pixel-range)) 2)
            mask (doto (Circle.)
                   (.setCenterX mouse-x)
                   (.setCenterY mouse-y)
                   (.setRadius radius))]
        (g/set-property! view-node :color color)
        (doto graphics-context
          (.clearRect 0 0 (.getWidth canvas) (.getHeight canvas))
          (.setStroke Color/GRAY))

        (doseq [x-range pixel-range
                y-range pixel-range
                :let [x1 (+ mouse-x (* x-range pixel-size) (- half-pixel-size))
                      y1 (+ mouse-y (* y-range pixel-size) (- half-pixel-size))]]
          (doto graphics-context
            (.setFill Color/TRANSPARENT)
            (.strokeRect x1 y1 pixel-size pixel-size)
            (.setFill (.getColor pixel-reader (+ mouse-x x-range) (+ mouse-y y-range)))
            (.fillRect x1 y1 pixel-size pixel-size)))

        (doto graphics-context
          (.setFill Color/TRANSPARENT)
          (.setStroke Color/RED)
          (.strokeRect (- mouse-x half-pixel-size)
                       (- mouse-y half-pixel-size)
                       pixel-size
                       pixel-size))

        (.setClip canvas mask)))))

(defn apply-color-and-deactivate!
  [view-node]
  (let [^Node dropper-area (g/node-value view-node :dropper-area)
        ^Canvas canvas (g/node-value view-node :canvas)
        ^GraphicsContext graphics-context (.getGraphicsContext2D canvas)]
    (.clearRect graphics-context 0 0 (.getWidth canvas) (.getHeight canvas))
    (.setVisible dropper-area false))
  
  (when-let [pick-fn (g/node-value view-node :pick-fn)]
    (g/set-property! view-node :pick-fn nil)
    (pick-fn (g/node-value view-node :color))))

(defn- key-pressed-handler!
  [view-node ^KeyEvent event]
  (let [^Node dropper-area (g/node-value view-node :dropper-area)]
    (condp = (.getCode event)
      KeyCode/ENTER (apply-color-and-deactivate! view-node)
      KeyCode/ESCAPE (.setVisible dropper-area false)
      nil)))

(defn- take-screenshot! 
  [view-node]
  (let [^Node dropper-area (g/node-value view-node :dropper-area)]
    (.setVisible dropper-area false)
    (g/set-property! view-node :screenshot (.snapshot (ui/main-root) nil nil))
    (.setVisible dropper-area true)))

(defn make-color-dropper! [graph ^StackPane dropper-area]
  (let [canvas (doto (Canvas. (.getWidth dropper-area) (.getHeight dropper-area))
                 (.setCursor Cursor/NONE))
        view-node (g/make-node! graph 
                                ColorDropper 
                                :canvas canvas
                                :dropper-area dropper-area)]
    (ui/add-child! dropper-area canvas)
    (.setVisible dropper-area false)
    (.bind (.widthProperty canvas) (.widthProperty dropper-area))
    (.bind (.heightProperty canvas) (.heightProperty dropper-area))

    (ui/observe (.widthProperty (ui/main-stage)) (fn [_ _ _] (take-screenshot! view-node)))
    (ui/observe (.heightProperty (ui/main-stage)) (fn [_ _ _] (take-screenshot! view-node)))

    (doto dropper-area
      (.setFocusTraversable true)
      (.addEventFilter KeyEvent/KEY_PRESSED (ui/event-handler event (key-pressed-handler! view-node event)))
      (.addEventHandler MouseEvent/MOUSE_MOVED (ui/event-handler event (mouse-move-handler! view-node event)))
      (.addEventHandler MouseEvent/MOUSE_PRESSED (ui/event-handler event (apply-color-and-deactivate! view-node))))

    view-node))

(defn activate!
  [view-node pick-fn]
  (g/set-property! view-node :pick-fn pick-fn)
  (take-screenshot! view-node))

(handler/defhandler :color-dropper :global
  (active? [color-dropper evaluation-context user-data] true)
  (run [color-dropper user-data]
       (when-let [{:keys [pick-fn]} user-data]
         (activate! color-dropper pick-fn))))
