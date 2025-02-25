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
  (:import [javafx.scene Cursor]
           [javafx.scene.canvas Canvas GraphicsContext]
           [javafx.scene.image WritableImage PixelReader]
           [javafx.scene.input MouseEvent]
           [javafx.scene.layout StackPane]
           [javafx.scene.paint Color]
           [javafx.scene.shape Circle]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(g/defnode ColorDropper
  (property canvas Canvas)
  (property color Color)
  (property screenshot WritableImage)
  (property dropper-area StackPane)
  (property pick-fn g/Any))

(defn mouse-move-handler!
  [color-dropper ^MouseEvent e]
  (.consume e)
  (let [^Canvas canvas (g/node-value color-dropper :canvas)
        ^GraphicsContext graphics-context (.getGraphicsContext2D canvas)
        ^WritableImage screenshot (g/node-value color-dropper :screenshot)
        ^PixelReader pixel-reader (.getPixelReader screenshot)
        mouse-x (.getX e)
        mouse-y (.getY e)
        pixel-range (range -4 5)
        pixel-size 12
        half-pixel-size (/ pixel-size 2)
        color (.getColor pixel-reader mouse-x mouse-y)
        radius (/ (* pixel-size (count pixel-range)) 2)
        mask (doto (Circle.)
               (.setCenterX mouse-x)
               (.setCenterY mouse-y)
               (.setRadius radius))]
    (g/set-property! color-dropper :color color)
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
    
    (.setClip canvas mask)))

(defn mouse-press-handler!
  [color-dropper ^MouseEvent _e]
  (.setVisible (g/node-value color-dropper :dropper-area) false)
  (when-let [pick-fn (g/node-value color-dropper :pick-fn)]
    (g/set-property! color-dropper :pick-fn nil)
    (pick-fn (g/node-value color-dropper :color))))

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

    (doto dropper-area
      (.addEventHandler MouseEvent/MOUSE_MOVED (ui/event-handler event (mouse-move-handler! view-node event)))
      (.addEventHandler MouseEvent/MOUSE_PRESSED (ui/event-handler event (mouse-press-handler! view-node event))))

    view-node))

(defn activate!
  [color-dropper pick-fn]
  (.setVisible (g/node-value color-dropper :dropper-area) true)
  (g/transact
    (concat (g/set-property color-dropper :pick-fn pick-fn)
            (g/set-property color-dropper :screenshot (.snapshot (ui/main-root) nil nil)))))

(handler/defhandler :color-dropper :global
  (active? [color-dropper evaluation-context user-data] true)
  (run [color-dropper user-data]
       (when-let [{:keys [pick-fn]} user-data]
         (activate! color-dropper pick-fn))))
