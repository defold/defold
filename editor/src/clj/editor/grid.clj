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

(ns editor.grid
  (:require [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.camera :as c]
            [editor.colors :as colors]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.pass :as pass]
            [editor.math :as math]
            [editor.prefs :as prefs]
            [editor.scene-cache :as scene-cache]
            [editor.types :as types]
            [editor.ui :as ui]
            [editor.ui.popup :as popup])
  (:import com.jogamp.opengl.GL2
           [com.sun.javafx.util Utils]
           [editor.types AABB Camera]
           [javafx.geometry HPos Point2D Pos VPos]
           [javafx.scene Parent]
           [javafx.scene.control Label Slider TextField ToggleButton ToggleGroup PopupControl]
           [javafx.scene.layout HBox Priority Region StackPane VBox]
           [java.nio ByteBuffer ByteOrder DoubleBuffer]
           [javax.vecmath Matrix3d Point3d Vector4d]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defonce grid-options-prefs-path [:scene :grid])
(defonce opacity-prefs-path [:scene :grid :opacity])
(defonce size-prefs-path [:scene :grid :size])
(defonce active-plane-prefs-path [:scene :grid :active-plane])
(defonce color-prefs-path [:scene :grid :color])

(defonce axes [:x :y :z])

(def x-axis-color colors/scene-grid-x-axis)
(def y-axis-color colors/scene-grid-y-axis)
(def z-axis-color colors/scene-grid-z-axis)

(defn- make-grid-vertex-buffer [_1 _2]
  (-> (ByteBuffer/allocateDirect (* 3 8))
    (.order (ByteOrder/nativeOrder))
    (.asDoubleBuffer)))

(defn- ignore-grid-vertex-buffer [_1 _2 _3] nil)

(scene-cache/register-object-cache! ::grid-vertex
                                    make-grid-vertex-buffer
                                    ignore-grid-vertex-buffer
                                    ignore-grid-vertex-buffer)

(defn render-grid-axis
  [^GL2 gl ^DoubleBuffer vx uidx start stop size vidx min max]
  (doseq [u (range start stop size)]
    (.put vx ^int uidx ^double u)
    (.put vx ^int vidx ^double min)
    (gl/gl-vertex-3dv gl vx)
    (.put vx ^int vidx ^double max)
    (gl/gl-vertex-3dv gl vx)))

(defn render-grid
  [gl fixed-axis u-size v-size aabb]
  (let [min-values (geom/as-array (types/min-p aabb))
        max-values (geom/as-array (types/max-p aabb))
        u-axis ^double (mod (inc ^int fixed-axis) 3)
        u-min (nth min-values u-axis)
        u-max (nth max-values u-axis)
        v-axis ^double (mod (inc ^int u-axis) 3)
        v-min (nth min-values v-axis)
        v-max (nth max-values v-axis)
        vertex ^DoubleBuffer (scene-cache/request-object! ::grid-vertex :grid-vertex {} nil)]
    (.put vertex ^int fixed-axis 0.0)
    (render-grid-axis gl vertex u-axis u-min u-max u-size v-axis v-min v-max)
    (render-grid-axis gl vertex v-axis v-min v-max v-size u-axis u-min u-max)))

(defn render-primary-axes
  [^GL2 gl ^AABB aabb colors]
  (gl/gl-color gl (if (:x colors) (colors/hex-color->color (:x colors)) x-axis-color))
  (gl/gl-vertex-3d gl (-> aabb types/min-p .x) 0.0 0.0)
  (gl/gl-vertex-3d gl (-> aabb types/max-p .x) 0.0 0.0)
  (gl/gl-color gl (if (:y colors) (colors/hex-color->color (:y colors)) y-axis-color))
  (gl/gl-vertex-3d gl 0.0 (-> aabb types/min-p .y) 0.0)
  (gl/gl-vertex-3d gl 0.0 (-> aabb types/max-p .y) 0.0)
  (gl/gl-color gl (if (:z colors) (colors/hex-color->color (:z colors)) z-axis-color))
  (gl/gl-vertex-3d gl 0.0 0.0 (-> aabb types/min-p .z))
  (gl/gl-vertex-3d gl 0.0 0.0 (-> aabb types/max-p .z)))

(defn render-grid-sizes
  [^GL2 gl ^doubles dir grids opacity color]
  (doall
   (for [grid-index (range 2)
         :let [fixed-axis (:perp-axis grids)
               ratio ^double (nth (:ratios grids) grid-index)
               alpha (Math/abs (* ^double (aget dir fixed-axis) ratio opacity))
               size-map (nth (:sizes grids) grid-index)
               u-axis (mod (inc fixed-axis) 3)
               v-axis (mod (inc u-axis) 3)
               u-axis-key (nth axes u-axis)
               v-axis-key (nth axes v-axis)
               u-size (get size-map u-axis-key)
               v-size (get size-map v-axis-key)]]
     (do
       (gl/gl-color gl (colors/alpha (colors/hex-color->color color) alpha))
       (render-grid gl fixed-axis u-size v-size (nth (:aabbs grids) grid-index))))))

(defn render-scaled-grids
  [^GL2 gl _pass renderables _count]
  (let [renderable (first renderables)
        {:keys [camera grids options]} (:user-render-data renderable)
        {:keys [opacity color axes-colors]} options
        view-matrix (c/camera-view-matrix camera)
        dir (double-array 4)
        _ (.getRow view-matrix 2 dir)]
    (gl/gl-lines gl
      (render-grid-sizes dir grids opacity color)
      (render-primary-axes (apply geom/aabb-union (:aabbs grids)) axes-colors))))

(g/defnk grid-renderable
  [camera grids merged-options]
  {pass/infinity-grid ; Grid lines stretching to infinity. Not depth-clipped to frustum.
   [{:world-transform geom/Identity4d
     :tags #{:grid}
     :render-fn render-scaled-grids
     :user-render-data {:camera camera
                        :grids grids
                        :options merged-options}}]
   pass/transparent ; Grid lines potentially intersecting scene geometry.
   [{:world-transform geom/Identity4d
     :tags #{:grid}
     :render-fn render-scaled-grids
     :user-render-data {:camera camera
                        :grids grids
                        :options merged-options}}]})

(defn frustum-plane-projection
  [^Vector4d plane1 ^Vector4d plane2]
  (let [m (Matrix3d. 0.0         0.0         1.0
                     (.x plane1) (.y plane1) (.z plane1)
                     (.x plane2) (.y plane2) (.z plane2))
        v (Point3d. 0.0 (- (.w plane1)) (- (.w plane2)))]
    (.invert m)
    (.transform m v)
    v))

(defn frustum-projection-aabb
  [planes]
  (-> geom/null-aabb
      (geom/aabb-incorporate (frustum-plane-projection (nth planes 0) (nth planes 2)))
      (geom/aabb-incorporate (frustum-plane-projection (nth planes 0) (nth planes 3)))
      (geom/aabb-incorporate (frustum-plane-projection (nth planes 1) (nth planes 2)))
      (geom/aabb-incorporate (frustum-plane-projection (nth planes 1) (nth planes 3)))))

(defn grid-ratio [extent]
  (let [exp (Math/log10 extent)]
    (- 1.0 (- exp (Math/floor exp)))))

(defn grid-snap-down [^double a ^double sz] (* sz (Math/floor (/ a sz))))
(defn grid-snap-up   [^double a ^double sz] (* sz (Math/ceil  (/ a sz))))

(defn snap-out-to-grid
  [^AABB aabb size]
  (types/->AABB (Point3d. (grid-snap-down (-> aabb types/min-p .x) (:x size))
                          (grid-snap-down (-> aabb types/min-p .y) (:y size))
                          (grid-snap-down (-> aabb types/min-p .z) (:z size)))
                (Point3d. (grid-snap-up (-> aabb types/max-p .x) (:x size))
                          (grid-snap-up (-> aabb types/max-p .y) (:y size))
                          (grid-snap-up (-> aabb types/max-p .z) (:z size)))))

(g/defnk update-grids
  [camera merged-options]
  (let [{:keys [size active-plane]} merged-options
        frustum-planes (c/viewproj-frustum-planes camera)
        aabb (frustum-projection-aabb frustum-planes)
        extent (geom/as-array (geom/aabb-extent aabb))
        perp-axis (.indexOf axes active-plane)
        _ (aset-double extent perp-axis Double/POSITIVE_INFINITY)
        smallest-extent (reduce min extent)
        first-grid-ratio (grid-ratio smallest-extent)
        large-size (into {} (map (fn [[k v]] [k (* v 10)]) size))]
    {:ratios [first-grid-ratio (- 1.0 ^double first-grid-ratio)]
     :sizes [size large-size]
     :aabbs [(snap-out-to-grid aabb size)
             (snap-out-to-grid aabb large-size)]
     :perp-axis perp-axis}))

(g/defnode Grid
  (property prefs g/Any)

  (input camera Camera)

  (output options g/Any (g/constantly nil))
  (output merged-options g/Any (g/fnk [prefs options] 
                                 (cond-> (if prefs (prefs/get prefs grid-options-prefs-path) {})
                                   options (merge options))))
  (output grids g/Any :cached update-grids)
  (output renderable pass/RenderData :cached grid-renderable))

(defn- invalidate-grids! [app-view]
  (let [scene-view-id (g/node-value app-view :active-view)
        grid-id (g/node-value scene-view-id :grid)]
    (g/transact [(g/invalidate-output grid-id :grids)])))

(defn- opacity-row [app-view prefs]
  (let [value (prefs/get prefs opacity-prefs-path)
        slider (Slider. 0.0 0.5 value)]
    (ui/observe
     (.valueProperty slider)
     (fn [_observable _old-val new-val]
       (let [val (math/round-with-precision new-val 0.01)]
         (prefs/set! prefs opacity-prefs-path val)
         (invalidate-grids! app-view))))
    (HBox. 5 (ui/node-array [(doto (Label. "Opacity")
                               (.setPrefWidth 70)) slider]))))

(defn plane-toggle-button
  [prefs plane-group plane]
  (let [active-plane (prefs/get prefs active-plane-prefs-path)]
    (doto (ToggleButton. (string/upper-case (name plane)))
      (.setToggleGroup plane-group)
      (.setSelected (= plane active-plane))
      (ui/add-style! "plane-toggle"))))

(defn- plane-row
  [app-view prefs]
  (let [plane-group (ToggleGroup.)
        buttons (mapv (partial plane-toggle-button prefs plane-group) axes)
        label (doto (Label. "Plane")
                (HBox/setHgrow Priority/ALWAYS)
                (.setPrefWidth 62))]
    (ui/observe (.selectedToggleProperty plane-group)
                (fn [_ _ active-toggle]
                  (let [active-plane (-> (.getText active-toggle) string/lower-case keyword)]
                    (prefs/set! prefs active-plane-prefs-path active-plane))
                  (invalidate-grids! app-view)))
    (doto (HBox. 0 (ui/node-array (concat [label] buttons)))
      (.setAlignment Pos/CENTER))))

(defn- color-row
  [app-view prefs]
  (let [text-field (TextField.)
        color (prefs/get prefs color-prefs-path)
        label (doto (Label. "Color")
                (HBox/setHgrow Priority/ALWAYS)
                (.setPrefWidth 70))]
    (doto text-field
      (ui/text! color)
      (ui/on-action! (fn [_] (when-let [value (.getText text-field)]
                               (prefs/set! prefs color-prefs-path value)
                               (invalidate-grids! app-view)))))
    (doto (HBox. 0 (ui/node-array [label text-field]))
      (.setAlignment Pos/CENTER))))

(defn- axis-group
  [app-view prefs axis]
  (let [text-field (TextField.)
        label (Label. (string/upper-case (name axis)))
        size-val (get (prefs/get prefs size-prefs-path) axis)]
    (doto text-field
      (ui/text! (str size-val))
      (ui/on-action! (fn [_] (when-let [value (some-> (.getText text-field) Integer/parseInt)]
                               (prefs/set! prefs (conj size-prefs-path axis) value)
                               (invalidate-grids! app-view)))))
    [label text-field]))

(defn- size-row
  [app-view prefs]
  (let [axis-groups (vec (flatten (map (partial axis-group app-view prefs) axes)))]
    (doto (HBox. 5 (ui/node-array axis-groups))
      (.setAlignment Pos/CENTER))))

(defn- settings-rows
  [app-view prefs]
  (let [scene-view-id (g/node-value app-view :active-view)
        grid (g/node-value scene-view-id :grid)
        options (g/node-value grid :options)]
    ;; Grid options override the preferences,
    ;; so we hide the row of overridden prefs.
    (cond-> []
      (not (:color options))
      (conj (color-row app-view prefs))

      (not (:active-plane options))
      (conj (plane-row app-view prefs))

      (not (:size options))
      (conj (size-row app-view prefs))

      (not (:opacity options))
      (conj (opacity-row app-view prefs)))))

(defn- pref-popup-position
  ^Point2D [^Parent container]
  (Utils/pointRelativeTo container 0 0 HPos/RIGHT VPos/BOTTOM -190.0 10.0 true))

(defn show-settings! [app-view ^Parent owner prefs]
  (if-let [popup ^PopupControl (ui/user-data owner ::popup)]
    (.hide popup)
    (let [region (StackPane.)
          popup (popup/make-popup owner region)
          anchor ^Point2D (pref-popup-position (.getParent owner))]
      (ui/children! region [(doto (Region.)
                              (ui/add-style! "popup-shadow"))
                            (doto (VBox. 10 (ui/node-array (settings-rows app-view prefs)))
                              (ui/add-style! "grid-settings"))])
      (ui/user-data! owner ::popup popup)
      (ui/on-closed! popup (fn [_] (ui/user-data! owner ::popup nil)))
      (.show popup owner (.getX anchor) (.getY anchor)))))
