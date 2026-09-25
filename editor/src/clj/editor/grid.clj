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
            [editor.gl.vertex2 :as vtx]
            [editor.math :as math]
            [editor.prefs :as prefs]
            [editor.shaders :as shaders]
            [editor.types :as types]
            [editor.ui.settings-popup :as settings-popup]
            [util.array :as array])
  (:import com.jogamp.opengl.GL2
           [editor.types AABB Camera]
           [java.util List]
           [javafx.scene Parent]
           [javax.vecmath Matrix3d Point3d Vector4d]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defonce ^List axes [:x :y :z])

(def x-axis-color colors/scene-grid-x-axis)
(def y-axis-color colors/scene-grid-y-axis)
(def z-axis-color colors/scene-grid-z-axis)

(def ^:private grid-shader shaders/infinity-grid-local-space)

(defn grid-axis-line-positions
  [fixed-axis u-axis u-min u-max u-size v-axis v-min v-max]
  (let [fixed-axis (long fixed-axis)
        u-axis (long u-axis)
        v-axis (long v-axis)]
    (into []
          (mapcat (fn [u]
                    [(-> [0.0 0.0 0.0]
                         (assoc fixed-axis 0.0)
                         (assoc u-axis u)
                         (assoc v-axis v-min))
                     (-> [0.0 0.0 0.0]
                         (assoc fixed-axis 0.0)
                         (assoc u-axis u)
                         (assoc v-axis v-max))]))
          (range u-min u-max u-size))))

(defn grid-line-positions
  [^long fixed-axis u-size v-size aabb]
  (let [min-values (geom/as-array (types/min-p aabb))
        max-values (geom/as-array (types/max-p aabb))
        u-axis (long (mod (inc fixed-axis) 3))
        u-min (nth min-values u-axis)
        u-max (nth max-values u-axis)
        v-axis (long (mod (inc u-axis) 3))
        v-min (nth min-values v-axis)
        v-max (nth max-values v-axis)]
    (into (grid-axis-line-positions fixed-axis u-axis u-min u-max u-size v-axis v-min v-max)
          (grid-axis-line-positions fixed-axis v-axis v-min v-max v-size u-axis u-min u-max))))

(defn- into-colored-vertices!
  [vertices positions color]
  (let [[r g b a] color]
    (reduce (fn [vertices [x y z]]
              (conj! vertices [x y z r g b a]))
            vertices
            positions)))

(defn- into-primary-axis-vertices!
  [vertices ^AABB aabb options]
  (let [{:keys [axes-colors active-plane]} options
        min-p (types/min-p aabb)
        max-p (types/max-p aabb)
        vertices (cond-> vertices
                   (not= active-plane :x)
                   (into-colored-vertices! [[(.x min-p) 0.0 0.0]
                                            [(.x max-p) 0.0 0.0]]
                                           (or (:x axes-colors) x-axis-color))

                   (not= active-plane :y)
                   (into-colored-vertices! [[0.0 (.y min-p) 0.0]
                                            [0.0 (.y max-p) 0.0]]
                                           (or (:y axes-colors) y-axis-color))

                   (not= active-plane :z)
                   (into-colored-vertices! [[0.0 0.0 (.z min-p)]
                                            [0.0 0.0 (.z max-p)]]
                                           (or (:z axes-colors) z-axis-color)))]
    vertices))

(defn grid-vertex-data
  [^doubles dir grids options is-2d]
  (let [{:keys [^double opacity color auto-scale]} options
        fixed-axis (long (:plane grids))
        u-axis (long (mod (inc fixed-axis) 3))
        v-axis (long (mod (inc u-axis) 3))
        grid-vertices
        (reduce
          (fn [vertices grid-index]
            (let [^double ratio (nth (:ratios grids) grid-index)
                  ratio (Math/abs (* (aget dir fixed-axis) ratio))
                  ratio (cond-> ratio (not is-2d) (max 0.5))
                  alpha (cond-> opacity auto-scale (* ratio))
                  size-map (nth (:sizes grids) grid-index)
                  u-size (get size-map (nth axes u-axis))
                  v-size (get size-map (nth axes v-axis))
                  positions (grid-line-positions fixed-axis u-size v-size (nth (:aabbs grids) grid-index))]
              (into-colored-vertices! vertices positions (colors/alpha color alpha))))
          (transient [])
          (range (if auto-scale 2 1)))
        grid-aabb (apply geom/aabb-union (:aabbs grids))]
    (persistent! (into-primary-axis-vertices! grid-vertices grid-aabb options))))

(defn- make-grid-vertex-buffer
  [vertex-data]
  (let [vertex-description (shaders/vertex-description grid-shader)
        vertex-buffer (vtx/make-vertex-buffer vertex-description :stream (count vertex-data))
        byte-buffer (vtx/buf vertex-buffer)
        float-buffer (.asFloatBuffer byte-buffer)]
    (doseq [vertex vertex-data]
      (.put float-buffer (float-array vertex)))
    (.position byte-buffer (* (.position float-buffer) Float/BYTES))
    (vtx/flip! vertex-buffer)))

(defn grid-fog-parameters
  ^floats [camera]
  (if (= :perspective (:type camera))
    (let [max-fov (math/deg->rad (max ^double (:fov-x camera) ^double (:fov-y camera)))
          fog-start (* max-fov ^double (:z-far camera))]
      (array/of-floats fog-start (* 2.0 fog-start) 1.0 0.0))
    (array/of-floats 0.0 1.0 0.0 0.0)))

(defn render-scaled-grids
  [^GL2 gl render-args renderables _count]
  (let [renderable (first renderables)
        {:keys [camera grids options]} (:user-render-data renderable)
        view-matrix (c/camera-view-matrix camera)
        dir (double-array 4)
        is-2d (c/mode-2d? camera)
        _ (.getRow view-matrix 2 dir)
        vertex-buffer (make-grid-vertex-buffer (grid-vertex-data dir grids options is-2d))
        vertex-binding (vtx/use-with ::grid vertex-buffer grid-shader)
        render-args (assoc render-args
                      :fog-color (float-array colors/scene-background)
                      :fog-parameters (grid-fog-parameters camera))]
    (gl/with-gl-bindings gl render-args [grid-shader vertex-binding]
      (gl/gl-draw-arrays gl GL2/GL_LINES 0 (count vertex-buffer)))))

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

(defn- grid-mode
  "Returns :grid-2d or :grid-3d depending on camera projection mode"
  [camera]
  (if (c/mode-2d? camera) :grid-2d :grid-3d))

(defn- get-grid-pref [prefs camera path]
  (prefs/get prefs (into [:scene (grid-mode camera)] path)))

(defn- set-grid-pref! [prefs camera path value]
  (prefs/set! prefs (into [:scene (grid-mode camera)] path) value))

(g/defnk produce-merged-options
  [prefs camera options]
  (merge (if prefs (get-grid-pref prefs camera []) {})
         {:auto-scale true}
         options))

(g/defnode Grid
  (property prefs g/Any)

  (input camera Camera)

  (output options g/Any (g/constantly nil))
  (output merged-options g/Any produce-merged-options)
  (output grids g/Any :cached produce-grids)
  (output renderable pass/RenderData :cached produce-renderable))

(defn- invalidate-grids! [app-view]
  (g/let-ec [scene-view-id (g/node-value app-view :active-view evaluation-context)
             grid-id (g/node-value scene-view-id :grid evaluation-context)]
    (g/transact
      {:undoable false}
      (g/invalidate-output grid-id :grids))))

(defn show-settings! [^Parent owner app-view prefs keymap localization]
  (g/let-ec [scene-view-id (g/node-value app-view :active-view evaluation-context)
             grid (g/node-value scene-view-id :grid evaluation-context)
             camera (g/node-value grid :camera evaluation-context)
             ignored-keys (set (keys (g/node-value grid :options evaluation-context)))]
    (let [value-changed-fn (fn [k v]
                             (set-grid-pref! prefs camera [k] v)
                             (invalidate-grids! app-view))

          all-descriptors
          [{:type :reset-all
            :on-reset (fn [swap-state]
                        (prefs/reset-path! prefs [:scene (grid-mode camera)])
                        (swap-state merge (get-grid-pref prefs camera []))
                        (invalidate-grids! app-view))}
           {:key :size :type :vec3-floats
            :value (get-grid-pref prefs camera [:size])
            :on-value-changed (partial value-changed-fn :size)}
           {:key :active-plane :type :vec3-toggle :label "scene-popup.grid.plane"
            :value (get-grid-pref prefs camera [:active-plane])
            :on-value-changed (partial value-changed-fn :active-plane)}
           {:key :color :type :color :label "scene-popup.grid.color"
            :value (get-grid-pref prefs camera [:color])
            :on-value-changed (partial value-changed-fn :color)}
           {:key :opacity :type :slider :label "scene-popup.grid.opacity" :min 0.0 :max 1.0
            :value (get-grid-pref prefs camera [:opacity])
            :on-value-changed (partial value-changed-fn :opacity)
            :slider-value->string (fn [^double v]
                                    (str (Math/round (* v 100)) "%"))}]

          descriptors (filterv #(not (contains? ignored-keys (:key %))) all-descriptors)

          initial-state (into {} (keep #(when-let [k (:key %)] [k (:value %)])) descriptors)]
      (settings-popup/show! owner keymap localization initial-state 240 descriptors))))
