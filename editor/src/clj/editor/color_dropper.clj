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
(set! *unchecked-math* :warn-on-boxed)

(g/defnode ColorDropper
  (property color Color)
  (property bounds g/Any)
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
  [view-node]
  (let [{:keys [min-x min-y width height]} (g/node-value view-node :bounds)
        capture-rect (Rectangle2D. min-x min-y width height)
        writable-image (WritableImage. width height)
        image (.getScreenCapture (Robot.) writable-image capture-rect)]
    (g/set-property! view-node :image image)
    (when-not image (g/set-property! view-node :image image))))

(defn- in-bounds?
  [view-node x y]
  (let [{:keys [min-x min-y max-x max-y]} (g/node-value view-node :bounds)]
    (and (<= min-x x max-x)
         (<= min-y y max-y))))

(let [pixel-size 12
      pixel-center ^double (/ pixel-size 2)
      pixel-seq (range -4 5)
      diameter (* pixel-size (count pixel-seq))
      radius ^double (/ diameter 2)]
  (defn- mouse-move-handler!
    [view-node ^Canvas canvas ^MouseEvent e]
    (.consume e)
    (when-let [image ^WritableImage (g/node-value view-node :image)]
      (let [graphics-context ^GraphicsContext (.getGraphicsContext2D canvas)
            pixel-reader ^PixelReader (.getPixelReader image)
            mouse-x ^double (.getX e)
            mouse-y ^double (.getY e)
            delta-x (- ^double (.getScreenX e) mouse-x)
            delta-y (- ^double (.getScreenY e) mouse-y)
            adjusted-x (+ mouse-x delta-x)
            adjusted-y (+ mouse-y delta-y)
            color (.getColor pixel-reader adjusted-x adjusted-y)]
        (g/set-property! view-node :color color)
        (.clearRect graphics-context (- mouse-x radius) (- mouse-y radius) diameter diameter)
        (.setClip canvas (create-mask! mouse-x mouse-y radius))
        (.setStroke graphics-context Color/GRAY)

        (doseq [^int x-step pixel-seq
                ^int y-step pixel-seq
                :let [x (+ mouse-x (* x-step pixel-size) (- pixel-center))
                      y (+ mouse-y (* y-step pixel-size) (- pixel-center))]]
          (when (in-bounds? view-node (+ x delta-x) (+ y delta-y))
            (->> (.getColor pixel-reader (+ adjusted-x x-step) (+ adjusted-y y-step))
                 (paint-pixel! graphics-context x y pixel-size))))

        (doto graphics-context
          (.strokeOval (- mouse-x radius) (- mouse-y radius) diameter diameter)
          (highlight-pixel! (- mouse-x pixel-center) (- mouse-y pixel-center) pixel-size))))))

(defn- apply-and-deactivate!
  [view-node ^Popup popup pick-fn]
  (.hide popup)
  (pick-fn (g/node-value view-node :color))
  (ui/user-data! (ui/main-scene) ::ui/refresh-requested? true))

(defn- get-bounds []
  (let [bounds (reduce (fn [bounds ^Screen screen]
                         (let [{:keys [^double min-x ^double min-y ^double max-x ^double max-y]} bounds
                               screen-bounds (.getBounds screen)]
                           (assoc bounds
                                  :min-x (cond-> ^double (.getMinX screen-bounds) min-x (min min-x))
                                  :min-y (cond-> ^double (.getMinY screen-bounds) min-y (min min-y))
                                  :max-x (cond-> ^double (.getMaxX screen-bounds) max-x (max max-x))
                                  :max-y (cond-> ^double (.getMaxY screen-bounds) max-y (max max-y)))))
                       {}
                       (Screen/getScreens))
        {:keys [^double min-x ^double min-y ^double max-x ^double max-y]} bounds]
    (assoc bounds
           :width (- max-x min-x)
           :height (- max-y min-y))))

(defn make-color-dropper! [graph]
  (g/make-node! graph ColorDropper))

(defn- create-popup! ^Popup
  [x y width height]
   (doto (Popup.)
    (.setX x)
    (.setY y)
    (.setWidth width)
    (.setHeight height)))

(defn- activate!
  [view-node pick-fn]
  (let [bounds (get-bounds)
        {:keys [min-x min-y width height]} bounds
        popup (create-popup! min-x min-y width height)
        canvas (Canvas. width height)
        pane (doto (StackPane.)
               (.setCursor Cursor/NONE)
               (.setStyle "-fx-background-color: transparent;")
               (ui/add-child! canvas))]
    (g/transact (concat (g/set-property view-node :bounds bounds)
                        (g/set-property view-node :image nil)))
    
    (.add (.getContent popup) pane)
    (.addListener (.focusedProperty popup) (ui/change-listener _ _ v (when-not v (.hide popup))))
    
    (doto popup
      (.addEventHandler KeyEvent/ANY (ui/event-handler event (.hide popup)))
      (.addEventHandler MouseEvent/MOUSE_MOVED (ui/event-handler event (mouse-move-handler! view-node canvas event)))
      (.addEventHandler MouseEvent/MOUSE_PRESSED (ui/event-handler event (apply-and-deactivate! view-node popup pick-fn)))
      (.show (ui/main-stage))
      (.requestFocus))
    
    (capture! view-node)))

(handler/defhandler :color-dropper :global
  (active? [color-dropper evaluation-context user-data] true)
  (run [color-dropper user-data]
       (when-let [{:keys [pick-fn]} user-data]
         (activate! color-dropper pick-fn))))
