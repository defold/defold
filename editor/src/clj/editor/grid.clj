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
  (:require [dynamo.graph :as g]
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
           [javafx.geometry HPos Point2D VPos]
           [javafx.scene Parent]
           [javafx.scene.control Label Slider TextField PopupControl]
           [javafx.scene.layout HBox Region StackPane VBox]
           [java.nio ByteBuffer ByteOrder DoubleBuffer]
           [javax.vecmath Matrix3d Point3d Vector4d]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defonce opacity-prefs-path [:scene :grid :opacity])
(defonce size-prefs-path [:scene :grid :size])

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
  [gl fixed-axis size aabb]
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
    (render-grid-axis gl vertex u-axis u-min u-max size v-axis v-min v-max)
    (render-grid-axis gl vertex v-axis v-min v-max size u-axis u-min u-max)))

(defn render-primary-axes
  [^GL2 gl ^AABB aabb]
  (gl/gl-color gl x-axis-color)
  (gl/gl-vertex-3d gl (-> aabb types/min-p .x) 0.0 0.0)
  (gl/gl-vertex-3d gl (-> aabb types/max-p .x) 0.0 0.0)
  (gl/gl-color gl y-axis-color)
  (gl/gl-vertex-3d gl 0.0 (-> aabb types/min-p .y) 0.0)
  (gl/gl-vertex-3d gl 0.0 (-> aabb types/max-p .y) 0.0)
  (gl/gl-color gl z-axis-color)
  (gl/gl-vertex-3d gl 0.0 0.0 (-> aabb types/min-p .z))
  (gl/gl-vertex-3d gl 0.0 0.0 (-> aabb types/max-p .z)))

(defn render-grid-sizes
  [^GL2 gl ^doubles dir grids opacity]
  (doall
    (for [grid-index (range 2)
          axis (range 3)
          :let [ratio ^double (nth (:ratios grids) grid-index)
                alpha (Math/abs (* ^double (aget dir axis) ratio opacity))]]
     (do
       (gl/gl-color gl (colors/alpha colors/scene-grid alpha))
       (render-grid gl axis
                    (nth (:sizes grids) grid-index)
                    (nth (:aabbs grids) grid-index))))))

(defn render-scaled-grids
  [^GL2 gl _pass renderables _count]
  (let [renderable (first renderables)
        user-render-data (:user-render-data renderable)
        camera (:camera user-render-data)
        grids (:grids user-render-data)
        view-matrix (c/camera-view-matrix camera)
        dir (double-array 4)
        _ (.getRow view-matrix 2 dir)]
    (gl/gl-lines gl
      (render-grid-sizes dir grids (-> renderable :user-render-data :opacity))
      (render-primary-axes (apply geom/aabb-union (:aabbs grids))))))

(g/defnk grid-renderable
  [camera grids prefs]
  (let [opacity (prefs/get prefs opacity-prefs-path)]
    {pass/infinity-grid ; Grid lines stretching to infinity. Not depth-clipped to frustum.
     [{:world-transform geom/Identity4d
       :tags #{:grid}
       :render-fn render-scaled-grids
       :user-render-data {:camera camera
                          :grids grids
                          :opacity opacity}}]
     pass/transparent ; Grid lines potentially intersecting scene geometry.
     [{:world-transform geom/Identity4d
       :tags #{:grid}
       :render-fn render-scaled-grids
       :user-render-data {:camera camera
                          :grids grids
                          :opacity opacity}}]}))

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

(defn small-grid-size [extent] (Math/pow 10.0 (dec (Math/floor (Math/log10 extent)))))
(defn large-grid-size [extent] (Math/pow 10.0      (Math/floor (Math/log10 extent))))

(defn grid-snap-down [^double a ^double sz] (* sz (Math/floor (/ a sz))))
(defn grid-snap-up   [^double a ^double sz] (* sz (Math/ceil  (/ a sz))))

(defn snap-out-to-grid
  [^AABB aabb grid-size]
  (types/->AABB (Point3d. (grid-snap-down (-> aabb types/min-p .x) grid-size)
                          (grid-snap-down (-> aabb types/min-p .y) grid-size)
                          (grid-snap-down (-> aabb types/min-p .z) grid-size))
                (Point3d. (grid-snap-up (-> aabb types/max-p .x) grid-size)
                          (grid-snap-up (-> aabb types/max-p .y) grid-size)
                          (grid-snap-up (-> aabb types/max-p .z) grid-size))))

(g/defnk update-grids
  [camera]
  (let [frustum-planes (c/viewproj-frustum-planes camera)
        aabb (frustum-projection-aabb frustum-planes)
        extent (geom/as-array (geom/aabb-extent aabb))
        perp-axis 2
        _ (aset-double extent perp-axis Double/POSITIVE_INFINITY)
        smallest-extent (reduce min extent)
        first-grid-ratio (grid-ratio smallest-extent)
        grid-size-small (small-grid-size smallest-extent)
        grid-size-large (large-grid-size smallest-extent)]
    {:ratios [first-grid-ratio (- 1.0 ^double first-grid-ratio)]
     :sizes [grid-size-small grid-size-large]
     :aabbs [(snap-out-to-grid aabb grid-size-small)
             (snap-out-to-grid aabb grid-size-large)]}))

(g/defnk produce-snapping-points
  [grids]
  {})

(g/defnode Grid
  (property active-plane g/Keyword)
  (property prefs g/Any)

  (input camera Camera)

  (output grids g/Any :cached update-grids)
  (output snapping-points g/Any :cached produce-snapping-points)
  (output renderable pass/RenderData :cached grid-renderable))

(defn- pref-popup-position
  ^Point2D [^Parent container]
  (Utils/pointRelativeTo container 0 0 HPos/CENTER VPos/BOTTOM 0.0 10.0 true))

(defn- opacity-slider [app-view prefs]
  (let [value (prefs/get prefs opacity-prefs-path)
        slider (Slider. 0.0 0.5 value)]
    (ui/observe
     (.valueProperty slider)
     (fn [_observable _old-val new-val]
       (let [val (math/round-with-precision new-val 0.05)]
         (prefs/set! prefs opacity-prefs-path val)
         (let [scene-view-id (g/node-value app-view :active-view)
               grid-id (g/node-value scene-view-id :grid)]
           (g/transact [(g/invalidate-output grid-id :renderable)])))))
    (VBox. 5 (ui/node-array [(Label. "Opacity") slider]))))

(defn show-settings! [app-view ^Parent owner prefs]
  (if-let [popup ^PopupControl (ui/user-data owner ::popup)]
    (.hide popup)
    (let [region (StackPane.)
          size-row (HBox.)
          popup (popup/make-popup owner region)
          anchor ^Point2D (pref-popup-position (.getParent owner))]
      (ui/children! size-row [(TextField.) (TextField.) (TextField.)])
      (ui/children! region [(doto (Region.)
                              (ui/add-style! "popup-shadow"))
                            (doto (VBox.)
                              (ui/add-style! "grid-settings")
                              (ui/add-child! size-row)
                              (ui/add-child! (opacity-slider app-view prefs)))])
      (ui/user-data! owner ::popup popup)
      (ui/on-closed! popup (fn [_] (ui/user-data! owner ::popup nil)))
      (.show popup owner (.getX anchor) (.getY anchor)))))
