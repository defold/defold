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
            [editor.ui.popup :as popup]
            [util.eduction :as e])
  (:import com.jogamp.opengl.GL2
           [com.sun.javafx.util Utils]
           [editor.types AABB Camera]
           [java.util List]
           [javafx.event ActionEvent]
           [javafx.geometry HPos Point2D Pos VPos]
           [javafx.scene Node Parent]
           [javafx.scene.control Button Control Label Slider TextField ToggleButton ToggleGroup PopupControl]
           [javafx.scene.layout HBox Region StackPane VBox]
           [javafx.scene.paint Color]
           [javafx.stage PopupWindow$AnchorLocation]
           [java.nio ByteBuffer ByteOrder DoubleBuffer]
           [javax.vecmath Matrix3d Point3d Vector4d]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defonce grid-prefs-path [:scene :grid])

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
  (cond-> (if prefs (prefs/get prefs grid-prefs-path) {})
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

(defmulti settings-row (fn [_app-view _prefs _popup option] option))

(defn- ensure-focus-traversable!
  [^Control control]
  (ui/observe (.focusTraversableProperty control)
    (fn [_ _ _]
      (.setFocusTraversable control true))))

(defmethod settings-row :opacity
  [app-view prefs ^PopupControl popup option]
  (let [prefs-path (conj grid-prefs-path option)
        value (prefs/get prefs prefs-path)
        slider (Slider. 0.0 1.0 value)
        label (Label. "Opacity")]
    (doto slider
      (ensure-focus-traversable!)
      (.setBlockIncrement 0.1)
      ;; Hacky way to fix a Linux specific issue that interferes with mouse events,
      ;; when autoHide is set to true.
      (.setOnMouseEntered (ui/event-handler e (.setAutoHide popup false)))
      (.setOnMouseExited (ui/event-handler e (.setAutoHide popup true))))

    (ui/observe
      (.valueProperty slider)
      (fn [_observable _old-val new-val]
        (let [val (math/round-with-precision new-val 0.01)]
          (prefs/set! prefs prefs-path val)
          (invalidate-grids! app-view))))
    [label slider]))

(defn plane-toggle-button
  [prefs plane-group prefs-path plane]
  (let [active-plane (prefs/get prefs prefs-path)]
    (doto (ToggleButton. (string/upper-case (name plane)))
      (ensure-focus-traversable!)
      (.setToggleGroup plane-group)
      (.setSelected (= plane active-plane))
      (ui/add-style! "plane-toggle"))))

(defmethod settings-row :active-plane
  [app-view prefs _popup option]
  (let [prefs-path (conj grid-prefs-path option)
        plane-group (ToggleGroup.)
        buttons (mapv (partial plane-toggle-button prefs plane-group prefs-path) axes)
        label (Label. "Plane")]
    (ui/observe (.selectedToggleProperty plane-group)
                (fn [_ ^ToggleButton old-value ^ToggleButton new-value]
                  (if new-value
                    (do (let [active-plane (-> (.getText new-value)
                                               string/lower-case
                                               keyword)]
                          (prefs/set! prefs prefs-path active-plane))
                        (invalidate-grids! app-view))
                    (.setSelected old-value true))))
    (concat [label] buttons)))

(defmethod settings-row :color
  [app-view prefs _popup option]
  (let [prefs-path (conj grid-prefs-path option)
        text-field (TextField.)
        [r g b a] (prefs/get prefs prefs-path)
        color (->> (Color. r g b a) (.toString) nnext (drop-last 2) (apply str "#"))
        label (Label. "Color")
        cancel-fn (fn [_] (ui/text! text-field color))
        update-fn (fn [_] (try
                            (if-let [value (some-> (.getText text-field) colors/hex-color->color)]
                              (do (prefs/set! prefs prefs-path value)
                                  (invalidate-grids! app-view))
                              (cancel-fn nil))
                            (catch Exception _e
                              (cancel-fn nil))))]
    (doto text-field
      (ui/text! color)
      (ui/customize! update-fn cancel-fn)
      (ensure-focus-traversable!))
    [label text-field]))

(defn- axis-group
  [app-view prefs prefs-path axis]
  (let [text-field (TextField.)
        label (Label. (string/upper-case (name axis)))
        size-val (str (get (prefs/get prefs prefs-path) axis))
        cancel-fn (fn [_] (ui/text! text-field size-val))
        update-fn (fn [_] (try
                            (let [value (Float/parseFloat (.getText text-field))]
                              (if (pos? value)
                                (do (prefs/set! prefs (conj prefs-path axis) value)
                                    (ui/text! text-field (str value))
                                    (invalidate-grids! app-view))
                                (cancel-fn nil)))
                            (catch Exception _e
                              (cancel-fn nil))))]
    (doto text-field
      (ui/text! size-val)
      (ui/customize! update-fn cancel-fn)
      (ensure-focus-traversable!))
    [label text-field]))

(defmethod settings-row :size
  [app-view prefs _popup option]
  (let [prefs-path (conj grid-prefs-path option)]
    (into []
          (comp (map (partial axis-group app-view prefs prefs-path))
                (mapcat identity))
          axes)))

(declare settings)

(defn- reset-button
  [app-view prefs ^PopupControl popup]
  (let [button (doto (Button. "Reset to Defaults")
                 (.setPrefWidth Double/MAX_VALUE))
        reset-fn (fn [^ActionEvent event]
                   (let [target ^Node (.getTarget event)
                         parent (.getParent target)]
                     (doseq [path [[:size :x]
                                   [:size :y]
                                   [:size :z]
                                   [:active-plane]
                                   [:opacity]
                                   [:color]]]
                       (let [path (into grid-prefs-path path)]
                         (prefs/set! prefs path (:default (prefs/schema prefs path)))))
                     (invalidate-grids! app-view)
                     (doto parent
                       (ui/children! (ui/node-array (settings app-view prefs popup)))
                       (.requestFocus))))]
    (doto button
      (ui/on-action! reset-fn)
      (ensure-focus-traversable!))
    button))

(defn- settings
  [app-view prefs popup]
  (let [scene-view-id (g/node-value app-view :active-view)
        grid (g/node-value scene-view-id :grid)
        options (g/node-value grid :options)
        reset-btn (reset-button app-view prefs popup)]
    (->> [:size :active-plane :color :opacity]
         (e/remove (partial contains? options))
         (reduce (fn [rows option]
                   (conj rows (doto (HBox. 5 (ui/node-array (settings-row app-view prefs popup option)))
                                (.setAlignment Pos/CENTER))))
                 [reset-btn]))))

(defn- pref-popup-position
  ^Point2D [^Parent container]
  (Utils/pointRelativeTo container 0 0 HPos/RIGHT VPos/BOTTOM 0.0 10.0 true))

(defn show-settings! [app-view ^Parent owner prefs]
  (if-let [popup ^PopupControl (ui/user-data owner ::popup)]
    (.hide popup)
    (let [region (StackPane.)
          popup (popup/make-popup owner region)
          anchor ^Point2D (pref-popup-position (.getParent owner))]
      (ui/children! region [(doto (Region.)
                              (ui/add-style! "popup-shadow"))
                            (doto (VBox. 10 (ui/node-array (settings app-view prefs popup)))
                              (.setFocusTraversable true)
                              (ensure-focus-traversable!)
                              (ui/add-style! "grid-settings"))])
      (ui/user-data! owner ::popup popup)
      (doto popup
        (.setAnchorLocation PopupWindow$AnchorLocation/CONTENT_TOP_RIGHT)
        (ui/on-closed! (fn [_] (ui/user-data! owner ::popup nil)))
        (.show owner (.getX anchor) (.getY anchor))))))
