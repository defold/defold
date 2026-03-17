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
            [editor.ui.popup :as popup])
  (:import com.jogamp.opengl.GL2
           [editor.types AABB Camera]
           [java.util List]
           [javafx.scene Parent]
           [javafx.scene.control PopupControl]
           [java.nio ByteBuffer ByteOrder DoubleBuffer]
           [javax.vecmath Matrix3d Point3d Vector4d]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defonce ^List axes [:x :y :z])

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
  [^GL2 gl ^AABB aabb options]
  (let [{:keys [axes-colors active-plane]} options]
    (when-not (= active-plane :x)
      (gl/gl-color gl (or (:x axes-colors) x-axis-color))
      (gl/gl-vertex-3d gl (-> aabb types/min-p .x) 0.0 0.0)
      (gl/gl-vertex-3d gl (-> aabb types/max-p .x) 0.0 0.0))

    (when-not (= active-plane :y)
      (gl/gl-color gl (or (:y axes-colors) y-axis-color))
      (gl/gl-vertex-3d gl 0.0 (-> aabb types/min-p .y) 0.0)
      (gl/gl-vertex-3d gl 0.0 (-> aabb types/max-p .y) 0.0))

    (when-not (= active-plane :z)
      (gl/gl-color gl (or (:z axes-colors) z-axis-color))
      (gl/gl-vertex-3d gl 0.0 0.0 (-> aabb types/min-p .z))
      (gl/gl-vertex-3d gl 0.0 0.0 (-> aabb types/max-p .z)))))

(defn render-grid-sizes
  [^GL2 gl ^doubles dir grids options is-2d]
  (let [{:keys [^double opacity color auto-scale]} options]
    (doseq [grid-index (range (if auto-scale 2 1))
            :let [^double fixed-axis (:plane grids)
                  ^double ratio (nth (:ratios grids) grid-index)
                  ratio (Math/abs (* ^double (aget dir fixed-axis) ratio))
                  ratio (cond-> ratio (not is-2d) (max 0.5))
                  alpha (cond-> opacity auto-scale (* ratio))
                  size-map (nth (:sizes grids) grid-index)
                  ^double u-axis (mod (inc fixed-axis) 3)
                  ^double v-axis (mod (inc u-axis) 3)
                  u-axis-key (nth axes u-axis)
                  v-axis-key (nth axes v-axis)
                  u-size (get size-map u-axis-key)
                  v-size (get size-map v-axis-key)]]
      (doto gl
        (gl/gl-color (colors/alpha color alpha))
        (render-grid fixed-axis u-size v-size (nth (:aabbs grids) grid-index))))))

(defn- enable-fog
  [^GL2 gl camera]
  (let [max-fov (math/deg->rad (max ^double (:fov-x camera) ^double (:fov-y camera)))
        fog-start (* max-fov ^double (:z-far camera))]
    (doto gl
      (.glEnable GL2/GL_FOG)
      (.glFogi GL2/GL_FOG_MODE GL2/GL_LINEAR)
      (.glFogfv GL2/GL_FOG_COLOR (float-array colors/scene-background) 0)
      (.glFogf GL2/GL_FOG_START fog-start)
      (.glFogf GL2/GL_FOG_END (* 2 fog-start)))))

(defn render-scaled-grids
  [^GL2 gl _pass renderables _count]
  (let [renderable (first renderables)
        {:keys [camera grids options]} (:user-render-data renderable)
        view-matrix (c/camera-view-matrix camera)
        dir (double-array 4)
        is-2d (c/mode-2d? camera)
        is-perspective (= :perspective (:type camera))
        _ (.getRow view-matrix 2 dir)]
    (when is-perspective
      (enable-fog gl camera))
    (gl/gl-lines gl
      (render-grid-sizes dir grids options is-2d)
      (render-primary-axes (apply geom/aabb-union (:aabbs grids)) options))
    (when is-perspective
      (.glDisable gl GL2/GL_FOG))))

(g/defnk produce-renderable
  [camera grids merged-options]
  {pass/infinity-grid ; Grid lines stretching to infinity. Not depth-clipped to frustum.
   [{:world-transform geom/Identity4d
     :tags #{:grid}
     :render-fn render-scaled-grids
     :user-render-data {:camera camera
                        :grids grids
                        :options merged-options}}]})

(defn frustum-plane-projection
  [^Vector4d plane1 ^Vector4d plane2 plane]
  (let [nx (if (= plane 0) 1.0 0.0)
        ny (if (= plane 1) 1.0 0.0)
        nz (if (= plane 2) 1.0 0.0)
        m (Matrix3d. nx          ny          nz
                     (.x plane1) (.y plane1) (.z plane1)
                     (.x plane2) (.y plane2) (.z plane2))
        v (Point3d. 0.0 (- (.w plane1)) (- (.w plane2)))
        det (.determinant m)]
    (if (< (Math/abs det) 1e-10)
      (Point3d. 0.0 0.0 0.0)
      (do (.invert m)
          (.transform m v)
          v))))

(defn orthographic-aabb
  [planes plane]
  (-> geom/null-aabb
      (geom/aabb-incorporate (frustum-plane-projection (nth planes 0) (nth planes 2) plane))
      (geom/aabb-incorporate (frustum-plane-projection (nth planes 0) (nth planes 3) plane))
      (geom/aabb-incorporate (frustum-plane-projection (nth planes 1) (nth planes 2) plane))
      (geom/aabb-incorporate (frustum-plane-projection (nth planes 1) (nth planes 3) plane))))

(defn grid-ratio [extent]
  (let [exp (Math/log10 extent)]
    (- 1.0 (- exp (Math/floor exp)))))

(defn- normalize ^double
  [^double v ^double scale] ^double
  (Math/pow 10 (- (Math/floor (Math/log10 v)) scale)))

(defn small-grid-axis-size [^double extent ^double size] (* (/ size (normalize size 1)) (normalize extent 2)))
(defn large-grid-axis-size [^double extent ^double size] (* (/ size (normalize size 1)) (normalize extent 1)))

(defn grid-snap-down [^double a ^double sz] (* sz (Math/floor (/ a sz))))
(defn grid-snap-up   [^double a ^double sz] (* sz (Math/ceil  (/ a sz))))

(defn grid-size
  [size auto-scale extent grid-axis-size-fn]
  (into {}
        (map (fn [[k ^double v]]
               [k (max v (cond->> v auto-scale ^double (grid-axis-size-fn extent)))])
             size)))

(defn snap-out-to-grid
  [^AABB aabb size]
  (types/->AABB (Point3d. (grid-snap-down (-> aabb types/min-p .x) (:x size))
                          (grid-snap-down (-> aabb types/min-p .y) (:y size))
                          (grid-snap-down (-> aabb types/min-p .z) (:z size)))
                (Point3d. (grid-snap-up (-> aabb types/max-p .x) (:x size))
                          (grid-snap-up (-> aabb types/max-p .y) (:y size))
                          (grid-snap-up (-> aabb types/max-p .z) (:z size)))))

(defn fov->grid-size
  ^double [^double fov]
  (-> fov
      (min 175.0)
      (* 0.5)
      (math/deg->rad)
      (Math/tan)))

(defn perspective-aabb
  [camera]
  (let [focus ^Vector4d (:focus-point camera)
        focus-pos (Point3d. (.x focus) (.y focus) (.z focus))
        focus-distance (.distance focus-pos (:position camera))
        [^double width ^double height] (->> [(:fov-x camera) (:fov-y camera)]
                                            (mapv #(-> ^double % fov->grid-size (* focus-distance 2))))
        [^double x ^double y ^double z] (types/Point3d->Vec3 focus-pos)]
    (types/->AABB (Point3d. (- x width) (- y height) (- z width))
                  (Point3d. (+ x width) (+ y height) (+ z width)))))

(g/defnk produce-grids
  [camera merged-options]
  (let [{:keys [size active-plane auto-scale]} merged-options
        plane (.indexOf axes active-plane)
        aabb (if (= :perspective (:type camera))
               (perspective-aabb camera)
               (orthographic-aabb (c/viewproj-frustum-planes camera) plane))
        extent (geom/as-array (geom/aabb-extent aabb))
        _ (aset-double extent plane Double/POSITIVE_INFINITY)
        smallest-extent (reduce min extent)
        first-grid-ratio (grid-ratio smallest-extent)
        grid-size-small (grid-size size auto-scale smallest-extent small-grid-axis-size)
        grid-size-large (when auto-scale (grid-size size auto-scale smallest-extent large-grid-axis-size))]
    {:ratios [first-grid-ratio (- 1.0 ^double first-grid-ratio)]
     :sizes [grid-size-small grid-size-large]
     :aabbs (cond-> [(snap-out-to-grid aabb grid-size-small)]
                    grid-size-large
                    (conj (snap-out-to-grid aabb grid-size-large)))
     :plane plane}))

(g/defnk produce-merged-options
  [prefs camera options]
  (cond-> (if prefs (prefs/get prefs [:scene :grid]) {})
          :always
          (assoc :auto-scale true)

          options
          (merge options)

          (c/mode-2d? camera)
          (assoc :active-plane :z)))

(g/defnode Grid
  (property prefs g/Any)

  (input camera Camera)

  (output options g/Any (g/constantly nil))
  (output merged-options g/Any produce-merged-options)
  (output grids g/Any :cached produce-grids)
  (output renderable pass/RenderData :cached produce-renderable))

(defn- invalidate-grids! [app-view]
  (let [scene-view-id (g/node-value app-view :active-view)
        grid-id (g/node-value scene-view-id :grid)]
    (g/transact [(g/invalidate-output grid-id :grids)])))

(defmethod popup/settings-row [:grid :opacity]
  [app-view prefs prefs-path ^PopupControl popup [_ option]]
  (popup/slider-setting prefs popup option prefs-path "Opacity" 0.0 1.0 #(invalidate-grids! app-view)))

(defmethod popup/settings-row [:grid :size]
  [app-view prefs prefs-path _popup [_ option]]
  (popup/vec3-floats-setting prefs prefs-path _popup option #(invalidate-grids! app-view)))

(defmethod popup/settings-row [:grid :color]
  [app-view prefs prefs-path _popup [_ option]]
  (popup/color-setting prefs prefs-path _popup option #(invalidate-grids! app-view)))

(defmethod popup/settings-row [:grid :active-plane]
  [app-view prefs prefs-path _popup [_ option]]
  (popup/vec3-toggle-setting prefs prefs-path _popup option "Plane" #(invalidate-grids! app-view)))

(defn show-settings! [app-view ^Parent owner prefs]
  (let [scene-view-id (g/node-value app-view :active-view)
        grid (g/node-value scene-view-id :grid)
        ignore-options (g/node-value grid :options)]
    (popup/show-settings! owner prefs 220 [:scene :grid]
                          [[:size :x] [:size :y] [:size :z] [:active-plane] [:color] [:opacity]]
                          ignore-options
                          #(invalidate-grids! app-view))))
