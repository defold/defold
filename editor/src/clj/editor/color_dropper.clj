;; Copyright 2020-2026 The Defold Foundation
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
  (:import [javafx.beans.value ChangeListener]
           [javafx.scene Cursor]
           [javafx.scene.canvas Canvas GraphicsContext]
           [javafx.scene.image PixelReader WritableImage]
           [javafx.scene.input KeyCode KeyEvent MouseEvent]
           [javafx.scene.layout StackPane]
           [javafx.scene.paint Color]
           [javafx.scene.shape Circle]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(g/defnode ColorDropper
  (property color Color)
  (property dropper-area StackPane)
  (property size-listener ChangeListener)
  (property image WritableImage))

(defn- paint-pixel!
  [^GraphicsContext graphics-context x y size color]
  (doto graphics-context
    (.strokeRect x y size size)
    (.setFill color)
    (.fillRect x y size size)))

(defn- create-mask!
  [x y radius]
  (doto (Circle.)
    (.setCenterX x)
    (.setCenterY y)
    (.setRadius radius)))

(defn- capture!
  [view-node ^Canvas canvas]
  (let [graphics-context ^GraphicsContext (.getGraphicsContext2D canvas)]
    (.clearRect graphics-context 0 0 (.getWidth canvas) (.getHeight canvas))
    (ui/refresh (ui/main-scene))
    (g/set-property! view-node :image (.snapshot (ui/main-root) nil nil))))

(defn- in-bounds?
  [^WritableImage image x y]
  (and (< 0 x (.getWidth image))
       (< 0 y (.getHeight image))))

(let [pixel-size 12
      pixel-center ^double (/ pixel-size 2)
      pixel-seq (range -4 5)
      diameter (* pixel-size (count pixel-seq))
      radius ^double (/ diameter 2)]
  (defn- paint-magnifier!
    [view-node ^Canvas canvas ^MouseEvent e]
    (.consume e)
    (when-let [image ^WritableImage (g/node-value view-node :image)]
      (let [graphics-context ^GraphicsContext (.getGraphicsContext2D canvas)
            pixel-reader ^PixelReader (.getPixelReader image)
            mouse-x (.getSceneX e)
            mouse-y (.getSceneY e)]
        (when (in-bounds? image mouse-x mouse-y)
          (g/set-property! view-node :color (.getColor pixel-reader mouse-x mouse-y)))
        (.clearRect graphics-context (- mouse-x radius) (- mouse-y radius) diameter diameter)
        (.setClip canvas (create-mask! mouse-x mouse-y radius))
        (.setStroke graphics-context Color/GRAY)

        (doseq [^int x-step pixel-seq
                ^int y-step pixel-seq
                :let [x (+ mouse-x (* x-step pixel-size) (- pixel-center))
                      y (+ mouse-y (* y-step pixel-size) (- pixel-center))]]
          (when (in-bounds? image x y)
            (->> (.getColor pixel-reader (+ mouse-x x-step) (+ mouse-y y-step))
                 (paint-pixel! graphics-context x y pixel-size))))

        (doto graphics-context
          (.strokeOval (- mouse-x radius) (- mouse-y radius) diameter diameter)
          (.setStroke Color/RED)
          (.strokeRect (- mouse-x pixel-center) (- mouse-y pixel-center) pixel-size pixel-size))))))

(defn- deactivate!
  [view-node]
  (let [dropper-area ^StackPane (g/node-value view-node :dropper-area)
        size-listener ^ChangeListener (g/node-value view-node :size-listener)
        main-view ^StackPane (ui/main-root)]
    (.removeListener (.widthProperty main-view) size-listener)
    (.removeListener (.heightProperty main-view) size-listener)
    (g/set-property! view-node :dropper-area nil)
    (g/set-property! view-node :size-listener nil)
    (-> (.getChildren main-view)
        (.remove dropper-area))))

(defn- apply-and-deactivate!
  [view-node pick-fn]
  (deactivate! view-node)
  (pick-fn (g/node-value view-node :color)))

(defn- key-pressed-handler!
  [view-node pick-fn ^KeyEvent event]
  (condp = (.getCode event)
    KeyCode/ENTER (apply-and-deactivate! view-node pick-fn)
    KeyCode/ESCAPE (deactivate! view-node)
    nil))

(defn make-color-dropper! [graph]
  (g/make-node! graph ColorDropper))

(defn activate!
  [view-node pick-fn ^MouseEvent event]
  (let [main-view ^StackPane (ui/main-root)
        canvas (Canvas. (.getWidth main-view) (.getHeight main-view))
        size-listener (reify ChangeListener
                        (changed [_ _ _ _]
                          (capture! view-node canvas)))
        dropper-area (doto (StackPane.)
                       (.setCursor Cursor/NONE)
                       (ui/add-child! canvas)
                       (.setStyle "-fx-background-color: transparent;"))]
    (ui/add-child! main-view dropper-area)
    (g/set-property! view-node :dropper-area dropper-area)
    (g/set-property! view-node :size-listener size-listener)

    (.bind (.widthProperty canvas) (.widthProperty dropper-area))
    (.bind (.heightProperty canvas) (.heightProperty dropper-area))

    (.addListener (.widthProperty main-view) size-listener)
    (.addListener (.heightProperty main-view) size-listener)

    (doto dropper-area
      (.addEventHandler KeyEvent/ANY (ui/event-handler event (key-pressed-handler! view-node pick-fn event)))
      (.addEventHandler MouseEvent/MOUSE_MOVED (ui/event-handler event (paint-magnifier! view-node canvas event)))
      (.addEventHandler MouseEvent/MOUSE_PRESSED (ui/event-handler event (apply-and-deactivate! view-node pick-fn)))
      (.requestFocus))
    
    (capture! view-node canvas)
    (paint-magnifier! view-node canvas event)))
