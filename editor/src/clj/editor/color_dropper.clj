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
           [javafx.scene Cursor]
           [javafx.scene.canvas Canvas GraphicsContext]
           [javafx.scene.image PixelReader WritableImage]
           [javafx.scene.input KeyEvent MouseEvent]
           [javafx.scene.layout StackPane]
           [javafx.scene.paint Color]
           [javafx.scene.shape Circle]
           [javafx.scene.robot Robot]
           [javafx.stage Popup Screen]))

(set! *warn-on-reflection* true)

(g/defnode ColorDropper
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
    (and (<= 0 x width)
         (<= 0 y height))))

(def pixel-range (range -4 5))
(def pixel-size 10)
(def center 5)
(def diameter (* pixel-size (count pixel-range)))
(def radius (/ diameter 2))

(defn- mouse-move-handler!
  [view-node ^Canvas canvas ^MouseEvent e]
  (.consume e)
  (let [^GraphicsContext graphics-context (.getGraphicsContext2D canvas)
        ^WritableImage image (g/node-value view-node :image)
        ^PixelReader pixel-reader (.getPixelReader image)
        mouse-x (.getX e)
        mouse-y (.getY e)
        delta-x (- (.getScreenX e) mouse-x)
        delta-y (- (.getScreenY e) mouse-y)
        adjusted-x (+ mouse-x delta-x)
        adjusted-y (+ mouse-y delta-y)
        color (.getColor pixel-reader adjusted-x adjusted-y)
        mask (doto (Circle.)
               (.setCenterX (+ mouse-x))
               (.setCenterY (+ mouse-y))
               (.setRadius radius))]
    (g/set-property! view-node :color color)
    (.clearRect graphics-context (- mouse-x radius) (- mouse-y radius) diameter diameter)
    (.setClip canvas mask)
  
    (doseq [x-range pixel-range
            y-range pixel-range
            :let [x (+ mouse-x (* x-range pixel-size) (- center))
                  y (+ mouse-y (* y-range pixel-size) (- center))]]
      (when (in-bounds? view-node x y)
        (->> (.getColor pixel-reader (+ adjusted-x x-range) (+ adjusted-y y-range))
             (paint-pixel! graphics-context x y pixel-size))))
  
    (doto graphics-context
      (.setStroke Color/GRAY)
      (.strokeOval (- mouse-x radius) (- mouse-y radius) diameter diameter)
      (highlight-pixel! (- mouse-x center) (- mouse-y center) pixel-size))))

(defn- apply-and-deactivate!
  [view-node ^Popup popup pick-fn]
  (.hide popup)
  (pick-fn (g/node-value view-node :color))
  (ui/user-data! (ui/main-scene) ::ui/refresh-requested? true))

(defn- get-size []
  (reduce (fn [size ^Screen screen]
            (let [screen-bounds (.getVisualBounds screen)]
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

(defn- create-popup! ^Popup
  [width height]
   (doto (Popup.)
    (.setX 0)
    (.setY 0)
    (.setWidth width)
    (.setHeight height)))

(defn- activate!
  [view-node pick-fn]
  (let [size (get-size)
        popup (create-popup! (:width size) (:height size))
        pane (StackPane.)
        canvas (Canvas.)]
    (g/set-property! view-node :size size)
    (.add (.getContent popup) pane)
    (.addListener (.focusedProperty pane) (ui/change-listener _ _ _ (capture! view-node)))
    (.show popup (ui/main-stage))

    (doto pane
      (.requestFocus)
      (ui/add-child! canvas)
      (.setStyle "-fx-background-color: transparent;")
      (.setCursor Cursor/NONE)
      (.addEventHandler KeyEvent/ANY (ui/event-handler event (.hide popup)))
      (.addEventHandler MouseEvent/MOUSE_MOVED (ui/event-handler event (mouse-move-handler! view-node canvas event)))
      (.addEventHandler MouseEvent/MOUSE_PRESSED (ui/event-handler event (apply-and-deactivate! view-node popup pick-fn))))

    (doto canvas
      (.setWidth (:width size))
      (.setHeight (:height size)))))

(handler/defhandler :color-dropper :global
  (active? [color-dropper evaluation-context user-data] true)
  (run [color-dropper user-data]
       (when-let [{:keys [pick-fn]} user-data]
         (activate! color-dropper pick-fn))))
