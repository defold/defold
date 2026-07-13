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
  (:require [editor.ui :as ui]
            [util.defonce :as defonce])
  (:import [javafx.beans.value ChangeListener]
           [javafx.geometry Point2D]
           [javafx.scene Cursor Node]
           [javafx.scene.canvas Canvas GraphicsContext]
           [javafx.scene.image PixelReader WritableImage]
           [javafx.scene.input KeyCode KeyEvent MouseEvent]
           [javafx.scene.layout StackPane]
           [javafx.scene.paint Color]
           [javafx.scene.shape Circle]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defonce/record ColorDropper [^Color color
                              ^StackPane dropper-area
                              ^ChangeListener size-listener
                              ^WritableImage image
                              prev-focus-owner
                              ;; 0-arg callback run when the pick ends, or nil. See `activate!`.
                              on-deactivated])

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
    [color-dropper ^Canvas canvas ^double mouse-x ^double mouse-y]
    (when-let [image ^WritableImage (:image @color-dropper)]
      (let [graphics-context ^GraphicsContext (.getGraphicsContext2D canvas)
            pixel-reader ^PixelReader (.getPixelReader image)]
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
  (let [{:keys [^StackPane dropper-area ^ChangeListener size-listener on-deactivated ^Node prev-focus-owner]} @color-dropper]
    (when dropper-area
      (let [main-view ^StackPane (ui/main-root)]
        (when size-listener
          (.removeListener (.widthProperty main-view) size-listener)
          (.removeListener (.heightProperty main-view) size-listener))
        (swap! color-dropper assoc
               :dropper-area nil
               :size-listener nil
               :image nil
               :prev-focus-owner nil
               :on-deactivated nil)
        (-> (.getChildren main-view)
            (.remove dropper-area))
        (when prev-focus-owner (.requestFocus prev-focus-owner))
        (when on-deactivated (on-deactivated)))))
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
  (atom (->ColorDropper nil nil nil nil nil nil)))

(defn activate! [color-dropper pick-fn on-activated on-deactivated ^MouseEvent event]
  (let [prev-focus-owner (ui/focus-owner (ui/main-scene))]
    (deactivate! color-dropper)
    (when on-activated (on-activated))
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
      ;; NOTE: Here we check if we are within the :workbench context, if we are,
      ;; grab the whole context and pretend the color dropper overlay is under
      ;; it. If we don't, when the overlay grabs focus, the scene-view toolbar
      ;; will disappear and if the color dropper was started from somewhere with
      ;; the toolbar (like the grid popup), the owner of the popup gets rebuilt
      ;; and calling on-deactivated ends up throwing an exception
      (when-let [ctx-node (ui/closest-node-where
                            (fn [^Node n] (= :workbench (:name (ui/user-data n ::ui/context))))
                            prev-focus-owner)]
        (ui/user-data! dropper-area ::ui/context (ui/user-data ctx-node ::ui/context)))
      (swap! color-dropper assoc
             :color nil
             :image nil
             :dropper-area dropper-area
             :size-listener size-listener
             :prev-focus-owner prev-focus-owner
             :on-deactivated on-deactivated)

      (.bind (.widthProperty canvas) (.widthProperty dropper-area))
      (.bind (.heightProperty canvas) (.heightProperty dropper-area))

      (.addListener (.widthProperty main-view) size-listener)
      (.addListener (.heightProperty main-view) size-listener)

      (doto dropper-area
        (.addEventHandler KeyEvent/ANY (ui/event-handler event (key-pressed-handler! color-dropper pick-fn event)))
        (.addEventHandler MouseEvent/MOUSE_MOVED (ui/event-handler event
                                                                    (.consume ^MouseEvent event)
                                                                    (paint-magnifier! color-dropper canvas (.getSceneX ^MouseEvent event) (.getSceneY ^MouseEvent event))))
        (.addEventHandler MouseEvent/MOUSE_PRESSED (ui/event-handler event (apply-and-deactivate! color-dropper pick-fn)))
        (.requestFocus))

      (capture! color-dropper canvas)
      (let [point ^Point2D (.screenToLocal main-view (.getScreenX event) (.getScreenY event))]
        (paint-magnifier! color-dropper canvas (.getX point) (.getY point))))))
