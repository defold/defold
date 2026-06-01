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
  (:require [editor.ui :as ui])
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

(defrecord ColorDropper [^Color color
                         ^StackPane dropper-area
                         ^ChangeListener size-listener
                         ^WritableImage image])

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
  [color-dropper ^Canvas canvas]
  (let [graphics-context ^GraphicsContext (.getGraphicsContext2D canvas)]
    (.clearRect graphics-context 0 0 (.getWidth canvas) (.getHeight canvas))
    (ui/refresh (ui/main-scene))
    (swap! color-dropper assoc :image (.snapshot (ui/main-root) nil nil))
    nil))

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
    [color-dropper ^Canvas canvas ^MouseEvent e]
    (.consume e)
    (when-let [image ^WritableImage (:image @color-dropper)]
      (let [graphics-context ^GraphicsContext (.getGraphicsContext2D canvas)
            pixel-reader ^PixelReader (.getPixelReader image)
            mouse-x (.getSceneX e)
            mouse-y (.getSceneY e)]
        (when (in-bounds? image mouse-x mouse-y)
          (swap! color-dropper assoc :color (.getColor pixel-reader mouse-x mouse-y)))
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

(defn deactivate!
  [color-dropper]
  (let [{:keys [^StackPane dropper-area ^ChangeListener size-listener]} @color-dropper]
    (when dropper-area
      (let [main-view ^StackPane (ui/main-root)]
        (when size-listener
          (.removeListener (.widthProperty main-view) size-listener)
          (.removeListener (.heightProperty main-view) size-listener))
        (swap! color-dropper assoc
               :dropper-area nil
               :size-listener nil
               :image nil)
        (-> (.getChildren main-view)
            (.remove dropper-area)))))
  nil)

(defn- apply-and-deactivate!
  [color-dropper pick-fn]
  (let [color (:color @color-dropper)]
    (deactivate! color-dropper)
    (pick-fn color)))

(defn- key-pressed-handler!
  [color-dropper pick-fn ^KeyEvent event]
  (condp = (.getCode event)
    KeyCode/ENTER (apply-and-deactivate! color-dropper pick-fn)
    KeyCode/ESCAPE (deactivate! color-dropper)
    nil))

(defn make-color-dropper! []
  (atom (->ColorDropper nil nil nil nil)))

(defn activate!
  [color-dropper pick-fn ^MouseEvent event]
  (deactivate! color-dropper)
  (let [main-view ^StackPane (ui/main-root)
        canvas (Canvas. (.getWidth main-view) (.getHeight main-view))
        size-listener (reify ChangeListener
                        (changed [_ _ _ _]
                          (capture! color-dropper canvas)))
        dropper-area (doto (StackPane.)
                       (.setCursor Cursor/NONE)
                       (ui/add-child! canvas)
                       (.setStyle "-fx-background-color: transparent;"))]
    (ui/add-child! main-view dropper-area)
    (swap! color-dropper assoc
           :color nil
           :image nil
           :dropper-area dropper-area
           :size-listener size-listener)

    (.bind (.widthProperty canvas) (.widthProperty dropper-area))
    (.bind (.heightProperty canvas) (.heightProperty dropper-area))

    (.addListener (.widthProperty main-view) size-listener)
    (.addListener (.heightProperty main-view) size-listener)

    (doto dropper-area
      (.addEventHandler KeyEvent/ANY (ui/event-handler event (key-pressed-handler! color-dropper pick-fn event)))
      (.addEventHandler MouseEvent/MOUSE_MOVED (ui/event-handler event (paint-magnifier! color-dropper canvas event)))
      (.addEventHandler MouseEvent/MOUSE_PRESSED (ui/event-handler event (apply-and-deactivate! color-dropper pick-fn)))
      (.requestFocus))
    
    (capture! color-dropper canvas)
    (paint-magnifier! color-dropper canvas event)))
