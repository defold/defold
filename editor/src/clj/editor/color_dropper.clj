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
            [editor.ui :as ui])
  (:import [javafx.scene Cursor]
           [javafx.scene.canvas Canvas GraphicsContext]
           [javafx.scene.input MouseEvent]
           [javafx.scene.layout StackPane]
           [javafx.scene.paint Color]
           [javafx.scene.robot Robot]
           [java.awt MouseInfo]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(g/defnode ColorDropper
  (property canvas Canvas (dynamic visible (g/constantly false)))
  (property robot Robot)
  (property color Color)
  (property mouse-pressed-fn g/Any))

(defn mouse-move-handler
  [node ^MouseEvent e]
  (.consume e)
  (let [robot (g/node-value node :robot)
        ^Canvas canvas (g/node-value node :canvas)
        ^GraphicsContext graphics-context (.getGraphicsContext2D canvas)
        robot-location  (.getLocation (MouseInfo/getPointerInfo))
        robot-x (.getX e)
        robot-y (.getY e)
        mouse-x (.getX e)
        mouse-y (.getY e)
        pixel-range (range -2 3)
        pixel-size 10
        half-pixel-range (/ pixel-size 2)]
    (.setVisible canvas false)
    (.clearRect graphics-context 0 0 (.getWidth canvas) (.getHeight canvas))
    (g/set-property! node :color (.getPixelColor robot robot-x robot-y))

    (doseq [x-range pixel-range
            y-range pixel-range]
      (.setFill graphics-context (.getPixelColor robot (+ robot-x x-range) (+ robot-y y-range)))
      (.fillRect graphics-context
                 (+ mouse-x (* x-range pixel-size) (- half-pixel-range))
                 (+ mouse-y (* y-range pixel-size) (- half-pixel-range))
                 pixel-size
                 pixel-size))

    (doto graphics-context
      (.setFill Color/TRANSPARENT)
      (.setStroke Color/RED)
      (.strokeRect (- mouse-x half-pixel-range)
                   (- mouse-y half-pixel-range)
                   pixel-size
                   pixel-size))

    (.setVisible canvas true)))

(defn mouse-press-handler
  [canvas-pane node ^MouseEvent _e]
  (.setVisible canvas-pane false)
  (when-let [mouse-pressed-fn (g/node-value node :mouse-pressed-fn)]
    (g/set-property! node :mouse-pressed-fn nil)
    (mouse-pressed-fn (g/node-value node :color))))

(defn make-color-dropper! [graph ^StackPane canvas-pane]
  (let [canvas (Canvas. (.getWidth canvas-pane) (.getHeight canvas-pane))
        robot (Robot.)
        view-node (g/make-node! graph ColorDropper
                                :canvas canvas
                                :robot robot)]
    (.setCursor canvas Cursor/NONE)
    (ui/add-child! canvas-pane canvas)
    (.bind (.widthProperty canvas) (.widthProperty canvas-pane))
    (.bind (.heightProperty canvas) (.heightProperty canvas-pane))

    (doto canvas
      (.addEventHandler MouseEvent/MOUSE_MOVED (ui/event-handler event (mouse-move-handler view-node event)))
      (.addEventHandler MouseEvent/MOUSE_PRESSED (ui/event-handler event (mouse-press-handler canvas-pane view-node event))))

    view-node))
