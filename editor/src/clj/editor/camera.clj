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

(ns editor.camera
  (:require [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.colors :as colors]
            [editor.geom :as geom]
            [editor.graph-util :as gu]
            [editor.input :as i]
            [editor.keymap :as keymap]
            [editor.math :as math]
            [editor.types :as types]
            [editor.ui :as ui]
            [editor.ui.popup :as popup]
            [editor.prefs :as prefs]
            [schema.core :as s]
            [util.eduction :as e])
  (:import [com.sun.javafx.util Utils]
           [editor.types AABB Camera Frustum Rect Region]
           [java.util List]
           [javafx.css PseudoClass]
           [javafx.event ActionEvent]
           [javafx.geometry HPos Point2D Pos VPos]
           [javafx.scene Cursor Node Parent]
           [javafx.scene.control Button Control CheckBox Label PopupControl Slider TextField ToggleButton ToggleGroup]
           [javafx.scene.image ImageView]
           [javafx.scene.input KeyCode KeyCodeCombination]
           [javafx.scene.layout HBox Priority StackPane VBox]
           [javafx.scene.paint Color]
           [javafx.stage PopupWindow$AnchorLocation]
           [javax.vecmath AxisAngle4d Matrix3d Matrix4d Point2d Point3d Quat4d Tuple2d Tuple3d Tuple4d Vector3d Vector4d]))

(set! *warn-on-reflection* true)

(def fov-x-35mm-full-frame 54.4)
(def fov-y-35mm-full-frame 37.8)

(s/defn camera-forward-vector :- Vector3d
  [camera :- Camera]
  (math/rotate (types/rotation camera)
               (Vector3d. 0.0 0.0 -1.0)))

(s/defn camera-up-vector :- Vector3d
  [camera :- Camera]
  (math/rotate (types/rotation camera)
               (Vector3d. 0.0 1.0 0.0)))

(s/defn camera-right-vector :- Vector3d
  [camera :- Camera]
  (math/rotate (types/rotation camera)
               (Vector3d. 1.0 0.0 0.0)))

(s/defn camera-focus-point :- Point3d
  [camera :- Camera]
  (let [^Vector4d p (:focus-point camera)]
    (Point3d. (.x p) (.y p) (.z p))))

(s/defn camera-without-depth-clipping :- Camera
  [camera :- Camera]
  (assoc camera
    :z-near Double/MIN_NORMAL
    :z-far Double/MAX_VALUE))

(s/defn camera-view-matrix :- Matrix4d
  [camera :- Camera]
  (let [pos (Vector3d. (types/position camera))
        m   (Matrix4d.)]
    (.setIdentity m)
    (.set m (types/rotation camera))
    (.transpose m)
    (.transform m pos)
    (.negate pos)
    (.setColumn m 3 (.x pos) (.y pos) (.z pos) 1.0)
    m))

(defn simple-perspective-projection-matrix
  ^Matrix4d [^double near ^double far ^double fov-deg-x ^double fov-deg-y]
  (let [aspect (/ fov-deg-x fov-deg-y)

        ymax (* near (Math/tan (/ (* fov-deg-y Math/PI) 360.0)))
        ymin (- ymax)
        xmin (* ymin aspect)
        xmax (* ymax aspect)

        x (/ (* 2.0 near) (- xmax xmin))
        y (/ (* 2.0 near) (- ymax ymin))
        a (/ (+ xmin xmax) (- xmax xmin))
        b (/ (+ ymin ymax) (- ymax ymin))
        c (/ (- (+ near far)) (- far near))
        d (/ (- (* 2.0 near far)) (- far near))
        m (Matrix4d.)]
    (set! (. m m00) x)
    (set! (. m m01) 0.0)
    (set! (. m m02) a)
    (set! (. m m03) 0.0)

    (set! (. m m10) 0.0)
    (set! (. m m11) y)
    (set! (. m m12) b)
    (set! (. m m13) 0.0)

    (set! (. m m20) 0.0)
    (set! (. m m21) 0.0)
    (set! (. m m22) c)
    (set! (. m m23) d)

    (set! (. m m30) 0.0)
    (set! (. m m31) 0.0)
    (set! (. m m32) -1.0)
    (set! (. m m33) 0.0)
    m))

(s/defn camera-perspective-projection-matrix :- Matrix4d
  [camera :- Camera]
  (simple-perspective-projection-matrix (.z-near camera) (.z-far camera) (.fov-x camera) (.fov-y camera)))

(defn region-orthographic-projection-matrix
  ^Matrix4d [^Region viewport ^double near ^double far]
  (let [left   (.left viewport)
        right  (.right viewport)
        top    (.top viewport)
        bottom (.bottom viewport)
        m      (Matrix4d.)]
    (set! (. m m00) (/ 2.0 (- right left)))
    (set! (. m m01) 0.0)
    (set! (. m m02) 0.0)
    (set! (. m m03) (/ (- (+ right left)) (- right left)))

    (set! (. m m10) 0.0)
    (set! (. m m11) (/ 2.0 (- top bottom)))
    (set! (. m m12) 0.0)
    (set! (. m m13) (/ (- (+ top bottom)) (- top bottom)))

    (set! (. m m20) 0.0)
    (set! (. m m21) 0.0)
    (set! (. m m22) (/ 2.0 (- near far)))
    (set! (. m m23) (/ (+ near far) (- near far)))

    (set! (. m m30) 0.0)
    (set! (. m m31) 0.0)
    (set! (. m m32) 0.0)
    (set! (. m m33) 1.0)
    m))

(defn simple-orthographic-projection-matrix
  ^Matrix4d [^double near ^double far ^double fov-deg-x ^double fov-deg-y]
  (let [right (/ fov-deg-x 2.0)
        left (- right)
        top (/ fov-deg-y 2.0)
        bottom (- top)
        m (Matrix4d.)]
    (set! (. m m00) (/ 2.0 (- right left)))
    (set! (. m m01) 0.0)
    (set! (. m m02) 0.0)
    (set! (. m m03) (/ (- (+ right left)) (- right left)))

    (set! (. m m10) 0.0)
    (set! (. m m11) (/ 2.0 (- top bottom)))
    (set! (. m m12) 0.0)
    (set! (. m m13) (/ (- (+ top bottom)) (- top bottom)))

    (set! (. m m20) 0.0)
    (set! (. m m21) 0.0)
    (set! (. m m22) (/ 2.0 (- near far)))
    (set! (. m m23) (/ (+ near far) (- near far)))

    (set! (. m m30) 0.0)
    (set! (. m m31) 0.0)
    (set! (. m m32) 0.0)
    (set! (. m m33) 1.0)
    m))

(s/defn camera-orthographic-projection-matrix :- Matrix4d
  [camera :- Camera]
  (simple-orthographic-projection-matrix (.z-near camera) (.z-far camera) (.fov-x camera) (.fov-y camera)))

(s/defn camera-projection-matrix :- Matrix4d
  [camera :- Camera]
  (case (:type camera)
    :perspective  (camera-perspective-projection-matrix camera)
    :orthographic (camera-orthographic-projection-matrix camera)))

(s/defn make-camera :- Camera
  ([] (make-camera :perspective))
  ([t :- (s/enum :perspective :orthographic)]
    (make-camera t identity))
  ([t :- (s/enum :perspective :orthographic) filter-fn :- Runnable]
   (make-camera t filter-fn nil))
  ([t :- (s/enum :perspective :orthographic) filter-fn :- Runnable opts]
   (let [distance 5000.0
         position (doto (Point3d.) (.set 0.0 0.0 1.0) (.scale distance))
         rotation (doto (Quat4d.) (.set 0.0 0.0 0.0 1.0))]
     (types/->Camera t position rotation
                     1.0 10000.0
                     (get opts :fov-x fov-x-35mm-full-frame) (get opts :fov-y fov-y-35mm-full-frame)
                     (Vector4d. 0.0 0.0 0.0 1.0)
                     0.0
                     filter-fn))))

(s/defn set-extents :- Camera
  [camera :- Camera fov-x :- s/Num fov-y :- s/Num z-near :- s/Num z-far :- s/Num]
  (assoc camera
         :fov-x fov-x
         :fov-y fov-y
         :z-near z-near
         :z-far z-far))

(s/defn camera-rotate :- Camera
  [camera :- Camera q :- Quat4d]
  (assoc camera :rotation (doto (Quat4d. (types/rotation camera)) (.mul (doto (Quat4d. q) (.normalize))))))

(s/defn camera-move :- Camera
  [camera :- Camera x :- s/Num y :- s/Num z :- s/Num]
  (assoc camera :position (doto (Point3d. x y z) (.add (types/position camera)))))

(s/defn camera-set-position :- Camera
  [camera :- Camera x :- s/Num y :- s/Num z :- s/Num]
  (assoc camera :position (Point3d. x y z)))

(s/defn camera-set-center :- Camera
  [camera :- Camera bounds :- AABB]
  (let [center (geom/aabb-center bounds)
        view-matrix (camera-view-matrix camera)
        position (Point3d. center)]
    (.transform view-matrix position)
    (.setZ position 0.0)
    (.transform ^Matrix4d (geom/invert view-matrix) position)
    (assoc camera
      :position position
      :focus-point (Vector4d. (.x center) (.y center) (.z center) 1.0))))

(s/defn camera-project :- Point3d
  "Returns a point in device space (i.e., corresponding to pixels on screen)
   that the given point projects onto. The input point should be in world space."
  [camera :- Camera viewport :- Region point :- Tuple3d]
  (let [proj  (camera-projection-matrix camera)
        model (camera-view-matrix camera)
        in    (Vector4d. (.x point) (.y point) (.z point) 1.0)
        out   (Vector4d.)]
    (.transform model in out)
    (.transform proj out in)

    (assert (not= 0.0 (.w in)))
    (let [w (.w in)
          x (/ (.x in) w)
          y (/ (.y in) w)
          z (/ (.z in) w)

          rx (+ (.left viewport) (/ (* (+ 1 x) (.right  viewport)) 2))
          ry (+ (.top  viewport) (/ (* (+ 1 y) (.bottom viewport)) 2))
          rz (/ (+ 1 z) 2)

          device-y (- (.bottom viewport) (.top viewport) ry 1)]
      (Point3d. rx device-y rz))))

(defn camera-view-proj-matrix ^Matrix4d [camera]
  (doto (camera-projection-matrix camera)
    (.mul (camera-view-matrix camera))))

(defn pick-matrix
  "Create a picking matrix as used by gluPickMatrix.

  `viewport` is the extents of the viewport

  `pick-rect` is the picking rectangle, where .x .y is the center of
  the picking region and .width .height the corresponding dimensions
  picking region.

  See:
    * editor.scene-selection/calc-picking-rect
    * https://www.khronos.org/registry/OpenGL-Refpages/gl2.1/xhtml/gluPickMatrix.xml"
  ^Matrix4d [^Region viewport ^Rect pick-rect]
  (let [sx (/ (.right viewport)
              (.width pick-rect))
        sy (/ (.bottom viewport)
              (.height pick-rect))
        tx (/ (+ (.right viewport)
                 (* 2.0 (- (.left viewport)
                           (.x pick-rect))))
              (.width pick-rect))
        ty (/ (+ (.bottom viewport)
                 (* 2.0 (- (.top viewport) (- (.bottom viewport) (.y pick-rect)))))
              (.height pick-rect))]
  (doto (Matrix4d.)
    (.set (double-array [sx  0.0 0.0 tx
                         0.0 sy  0.0 ty
                         0.0 0.0 1.0 0.0
                         0.0 0.0 0.0 1.0])))))


(defmacro normalize-to-bipolar [x x-min x-max]
  `(- (/ (* (- ~x ~x-min) 2) ~x-max) 1.0))

(defmacro perspective-divide [v]
  `(do
     (if (= (.w ~v) 0.0) (throw (ArithmeticException.)))
     (Vector4d. (/ (.x ~v) (.w ~v))
                (/ (.y ~v) (.w ~v))
                (/ (.z ~v) (.w ~v))
                1.0)))

(s/defn camera-unproject :- Vector4d
  [camera :- Camera viewport :- Region win-x :- s/Num win-y :- s/Num win-z :- s/Num]
  (let [win-y    (- (.bottom viewport) (.top viewport) win-y 1.0)
        in       (Vector4d. (normalize-to-bipolar win-x (.left viewport) (.right viewport))
                            (normalize-to-bipolar win-y (.top viewport) (.bottom viewport))
                            (- (* 2 win-z) 1.0)
                            1.0)
        proj     (camera-projection-matrix camera)
        model    (camera-view-matrix camera)
        a        (Matrix4d.)
        out      (Vector4d.)]
    (.mul a proj model)
    (.invert a)
    (.transform a in out)
    (perspective-divide out)))

(s/defn viewproj-frustum-planes :- [Vector4d]
  [camera :- Camera]
  (let [view-proj   (doto (camera-projection-matrix camera)
                      (.mul (camera-view-matrix camera)))
        persp-vec   (let [x (Vector4d.)] (.getRow view-proj 3 x) x)
        rows        (mapcat #(repeat 2 %) (range 3))
        scales      (take 6 (cycle [-1 1]))]
    (map (fn [row scale]
           (let [temp ^Vector4d (Vector4d.)]
             (.getRow view-proj ^int row temp)
             (.scale  temp scale)
             (.add    temp persp-vec)
             temp))
         rows scales)))

(defn screen->clip
  ^Point2d [^Region viewport ^double x ^double y]
  (Point2d. (normalize-to-bipolar x (.left viewport) (.right viewport))
            (- (normalize-to-bipolar y (.top viewport) (.bottom viewport)))))

(defn clip->world
  ^Point3d [^Matrix4d inv-view-proj ^Tuple2d clip ^double clip-z]
  (let [undivided (Vector4d. (.x clip) (.y clip) clip-z 1.0)
        _ (.transform inv-view-proj undivided)
        world (perspective-divide undivided)]
    (Point3d. (.x world) (.y world) (.z world))))

(s/defn screen-rect-frustum :- Frustum
  [camera :- Camera viewport :- Region rect :- Rect]
  (let [inv-view-proj (doto (camera-projection-matrix camera)
                        (.mul (camera-view-matrix camera))
                        (.invert))
        clip-tl (screen->clip viewport (.x rect) (.y rect))
        clip-tr (screen->clip viewport (+ (.x rect) (.width rect)) (.y rect))
        clip-bl (screen->clip viewport (.x rect) (+ (.y rect) (.height rect)))
        clip-br (screen->clip viewport (+ (.x rect) (.width rect)) (+ (.y rect) (.height rect)))
        near-tl (clip->world inv-view-proj clip-tl -1.0)
        near-tr (clip->world inv-view-proj clip-tr -1.0)
        near-bl (clip->world inv-view-proj clip-bl -1.0)
        near-br (clip->world inv-view-proj clip-br -1.0)
        far-tl (clip->world inv-view-proj clip-tl 1.0)
        far-tr (clip->world inv-view-proj clip-tr 1.0)
        far-bl (clip->world inv-view-proj clip-bl 1.0)
        far-br (clip->world inv-view-proj clip-br 1.0)]
    (geom/corners->frustum near-tl near-tr near-bl near-br far-tl far-tr far-bl far-br)))

(defn- dolly-orthographic [camera delta]
  (let [dolly-fn (fn [fov]
                   (min 1000000.0
                        (max 0.01 (+ (or fov 0)
                                     (* (or fov 1)
                                        delta)))))]
    (-> camera
        (update :fov-x dolly-fn)
        (update :fov-y dolly-fn))))

(defn- dolly-perspective [camera delta]
  (let [forward (camera-forward-vector camera)
        position (types/position camera)
        focus ^Vector4d (:focus-point camera)
        camera-to-focus-point (doto (Vector3d. (.x focus) (.y focus) (.z focus))
                                (.sub position))
        distance-to-focus-point (.length camera-to-focus-point)
        offset-distance (if (pos? delta)
                          (max 0.01 (* delta distance-to-focus-point))
                          (min -0.01 (* delta distance-to-focus-point)))]
    (update camera :position math/offset-scaled forward (- offset-distance))))

(defn dolly [camera delta]
  (case (:type camera)
    :orthographic (dolly-orthographic camera delta)
    :perspective (dolly-perspective camera delta)))

(def ^:private dolly-friction 8.0)
(def ^:private dolly-sensitivity 0.5)

(def ^:private dolly-smooth-speed 10.0)
(def dolly-velocity (atom 0.0))
(def dolly-target (atom 0.0))

(defn- apply-dolly-easing [self camera dt]
  (let [remaining @dolly-target]
    (if (< (Math/abs (double remaining)) 0.01)
      (do
        (reset! dolly-target 0.0)
        camera)
      (let [step (* (Math/signum (double remaining))
                    (min (Math/abs (double remaining))
                         (* dolly-smooth-speed (Math/abs (double remaining)) (double dt))))]
        (reset! dolly-target (- remaining step))
        (dolly camera step)))))

(defn- add-dolly-impulse! [self delta]
  (swap! dolly-target + (* delta dolly-sensitivity)))

(defn mode-2d? [camera]
  (and (= 1.0 (some-> camera camera-view-matrix (.getElement 2 2)))
       (= :orthographic (:type camera))))

(defn track [^Camera camera ^Region viewport last-x last-y evt-x evt-y]
  (let [focus ^Vector4d (:focus-point camera)
        point (camera-project camera viewport (Point3d. (.x focus) (.y focus) (.z focus)))
        screen-z (.z point)
        world (camera-unproject camera viewport evt-x evt-y screen-z)
        delta (camera-unproject camera viewport last-x last-y screen-z)]
    (.sub delta world)
    (assoc (camera-move camera (.x delta) (.y delta) (.z delta))
           :focus-point (doto focus (.add delta)))))

(defn pan-at-pointer-position
  "Pans the camera so that the focus point is at the same position as it was before `dolly`."
  [^Camera camera ^Camera prev-camera ^Region viewport [^double x ^double y]]
  (let [focus ^Vector4d (:focus-point camera)
        focus-point-3d (Point3d. (.x focus) (.y focus) (.z focus))
        point (camera-project camera viewport focus-point-3d)
        prev-point (camera-project prev-camera viewport focus-point-3d)
        world (camera-unproject camera viewport x y (.z point))
        delta (camera-unproject prev-camera viewport x y (.z prev-point))]
    (.sub delta world)
    (assoc (camera-move camera (.x delta) (.y delta) (.z delta))
           :focus-point (doto focus (.add delta)))))

(defn tumble [^Camera camera dx dy]
  (let [rate 0.005
        focus ^Vector4d (:focus-point camera)
        delta ^Vector4d (doto (Vector4d. ^Point3d (:position camera))
                          (.sub focus))
        q-delta ^Quat4d (doto (Quat4d.)
                          (.set delta))
        r ^Quat4d (Quat4d. ^Quat4d (:rotation camera))
        inv-r ^Quat4d (doto (Quat4d. r)
                     (.conjugate))
        m ^Matrix4d (doto (Matrix4d.)
                    (.setIdentity)
                    (.set r)
                    (.transpose))
        q2 ^Quat4d (doto (Quat4d.)
                     (.set (AxisAngle4d. 1.0 0.0 0.0 (* dy rate))))
        y-axis ^Vector4d (doto (Vector4d.))
        q1 ^Quat4d (doto (Quat4d.))]
    (.mul q-delta inv-r q-delta)
    (.mul q-delta r)
    (.getColumn m 1 y-axis)
    (.set q1 (AxisAngle4d. (.x y-axis) (.y y-axis) (.z y-axis) (* dx rate)))
    (.mul q1 q2)
    (.normalize q1)
    (.mul r q1)
    ; rotation is now in r
    (.conjugate inv-r r)

    (.mul q-delta r q-delta)
    (.mul q-delta inv-r)
    (.set delta q-delta)
    (.add delta focus)
    ; position is now in delta
    (assoc camera
           :position (Point3d. (.x delta) (.y delta) (.z delta))
           :rotation r)))

(def ^:private button-interpretation
  ;; button    shift ctrl  alt   meta => movement
  {[:primary   false true  false false] :tumble
   [:primary   false false true  false] :track
   [:primary   false true  true  false] :dolly
   [:secondary false false false false] :look
   [:secondary true  false false false] :look
   [:secondary false false true  false] :look
   [:middle    false false false false] :track})

(defn camera-movement
  ([action]
    (camera-movement (:button action) (:shift action) (:control action) (:alt action) (:meta action)))
  ([button shift ctrl alt meta]
    (let [key [button shift ctrl alt meta]]
      (button-interpretation key :idle))))

(s/defn camera-orthographic-fov-from-aabb :- s/Num
  [camera :- Camera viewport :- Region ^AABB aabb :- AABB]
  (assert camera "no camera?")
  (assert aabb   "no aabb?")
  (if (geom/null-aabb? aabb)
    [(:fov-x camera) (:fov-y camera)]
    (let [min-proj    (camera-project camera viewport (.. aabb min))
          max-proj    (camera-project camera viewport (.. aabb max))
          proj-width  (Math/abs (- (.x max-proj) (.x min-proj)))
          proj-height (Math/abs (- (.y max-proj) (.y min-proj)))]
      (if (or (< proj-width math/epsilon) (< proj-height math/epsilon))
        [(:fov-x camera) (:fov-y camera)]
        (let [w (- (.right viewport) (.left viewport))
              h (- (.bottom viewport) (.top viewport))
              factor-x    (/ proj-width w)
              factor-y    (/ proj-height h)
              fov-x-prim  (* factor-x (:fov-x camera))
              fov-y-prim  (* factor-y (:fov-y camera))]
          [(* 1.1 fov-x-prim) (* 1.1 fov-y-prim)])))))

(s/defn camera-orthographic-frame-aabb :- Camera
  [camera :- Camera viewport :- Region ^AABB aabb :- AABB]
  (assert (= :orthographic (:type camera)))
  (let [aspect (/ (:fov-x camera) (:fov-y camera))
        [^double fov-x ^double fov-y] (camera-orthographic-fov-from-aabb camera viewport aabb)
        [fov-x fov-y] (if (> (/ fov-x aspect) (* aspect fov-y))
                        [fov-x (/ fov-x aspect)]
                        [(* aspect fov-y) fov-y])
        filter-fn (or (:filter-fn camera) identity)]
    (-> camera
        (camera-set-center aabb)
        (set-extents fov-x fov-y (:z-near camera) (:z-far camera))
        filter-fn)))

(defn find-perspective-frame-distance
  ^double [points point->coord ^double fov-deg]
  (let [half-fov-rad (math/deg->rad (* fov-deg 0.5))
        comp-half-fov-rad (- ^double math/half-pi half-fov-rad)
        tan-comp-half-fov-rad (Math/tan comp-half-fov-rad)]
    (reduce (fn [^double max-distance ^Point3d point]
              (max max-distance
                   (+ (.z point)
                      (* tan-comp-half-fov-rad
                         (Math/abs ^double (point->coord point))))))
            Double/NEGATIVE_INFINITY
            points)))

(s/defn camera-perspective-frame-aabb :- Camera
  [camera :- Camera ^AABB aabb :- AABB]
  (assert (= :perspective (:type camera)))
  (let [^double fov-x-deg (:fov-x camera)
        ^double fov-y-deg (:fov-y camera)
        filter-fn (or (:filter-fn camera) identity)
        new-camera (camera-set-center camera aabb)
        view-matrix (camera-view-matrix new-camera)
        corners (mapv (fn [^Point3d corner]
                        (.transform view-matrix corner)
                        corner)
                      (geom/aabb->corners aabb))
        distance (max (find-perspective-frame-distance corners #(.x ^Point3d %) fov-x-deg)
                      (find-perspective-frame-distance corners #(.y ^Point3d %) fov-y-deg))
        cam-forward (camera-forward-vector new-camera)]
    (-> new-camera
        (update :position math/offset-scaled cam-forward (- distance))
        filter-fn)))

(s/defn camera-frame-aabb :- Camera
  [camera :- Camera viewport :- Region ^AABB aabb :- AABB]
  (case (:type camera)
    :orthographic (camera-orthographic-frame-aabb camera viewport aabb)
    :perspective (camera-perspective-frame-aabb camera aabb)))

(defn camera-orthographic-realign ^Camera
  [^Camera camera]
  (assert (= :orthographic (:type camera)))
  (let [focus ^Vector4d (:focus-point camera)
        delta ^Vector4d (doto (Vector4d. ^Point3d (:position camera))
                          (.sub focus))
        dist (.length delta)]
    (assoc camera
      :position (Point3d. (.x focus) (.y focus) dist)
      :rotation (Quat4d. 0.0 0.0 0.0 1.0))))

(s/defn camera-orthographic-frame-aabb-y :- Camera
  [camera :- Camera viewport :- Region ^AABB aabb :- AABB]
  (assert (= :orthographic (:type camera)))
  (let [fov-x (:fov-x camera)
        [_ ^double fov-y] (camera-orthographic-fov-from-aabb camera viewport aabb)
        filter-fn (or (:filter-fn camera) identity)]
    (-> camera
      (camera-set-center aabb)
      (set-extents fov-x fov-y (:z-near camera) (:z-far camera))
      filter-fn)))

(s/defn camera-orthographic->perspective :- Camera
  [camera :- Camera ^double fov-y-deg]
  ;; Orthographic fov is specified as distance between the frustum edges.
  ;; Perspective fov is specified as degrees between the frustum edges.
  (assert (= :orthographic (:type camera)))
  (let [^double fov-x-distance (:fov-x camera)
        ^double fov-y-distance (:fov-y camera)
        half-fov-y-rad (math/deg->rad (* fov-y-deg 0.5))
        aspect (/ fov-x-distance fov-y-distance)
        focus-distance (/ fov-y-distance 2.0 (Math/tan half-fov-y-rad))
        focus-pos (camera-focus-point camera)
        cam-forward (camera-forward-vector camera)
        cam-pos (math/offset-scaled focus-pos cam-forward (- focus-distance))]
    (assoc camera
      :type :perspective
      :position cam-pos
      :fov-x (* fov-y-deg aspect)
      :fov-y fov-y-deg)))

(s/defn camera-perspective->orthographic :- Camera
  [camera :- Camera]
  ;; Orthographic fov is specified as distance between the frustum edges.
  ;; Perspective fov is specified as degrees between the frustum edges.
  (assert (= :perspective (:type camera)))
  (let [focus-pos (camera-focus-point camera)
        focus-distance (.length (math/subtract-vector focus-pos (:position camera)))
        ^double fov-x-deg (:fov-x camera)
        ^double fov-y-deg (:fov-y camera)
        half-fov-x-rad (math/deg->rad (* fov-x-deg 0.5))
        half-fov-y-rad (math/deg->rad (* fov-y-deg 0.5))
        fov-x-distance (* focus-distance 2.0 (Math/tan half-fov-x-rad))
        fov-y-distance (* focus-distance 2.0 (Math/tan half-fov-y-rad))
        cam-forward (camera-forward-vector camera)
        cam-pos (math/offset-scaled focus-pos cam-forward (- 5000))]
    (assoc camera
      :type :orthographic
      :position cam-pos
      :fov-x fov-x-distance
      :fov-y fov-y-distance)))

(s/defn camera-ensure-orthographic :- Camera
  [camera :- Camera]
  (case (:type camera)
    :orthographic camera
    :perspective (camera-perspective->orthographic camera)))

(s/defn find-z-extents :- [s/Num s/Num]
  [cam-pos :- Vector3d cam-dir :- Vector3d aabb :- AABB]
  (loop [corners (geom/aabb->corners aabb)
         near Double/MAX_VALUE
         far Double/MIN_VALUE]
    (if-some [corner (first corners)]
      (let [cam->corner (math/subtract-vector corner cam-pos)
            distance (.dot cam-dir cam->corner)]
        (recur (next corners)
               (min distance near)
               (max distance far)))
      [(max 1.0 (- near 0.001))
       (max 10.0 (+ far 0.001))])))

(g/defnk produce-camera [_node-id local-camera scene-aabb viewport]
  (let [filter-fn (or (:filter-fn local-camera) identity)
        w (- (:right viewport) (:left viewport))
        h (- (:bottom viewport) (:top viewport))
        aspect (if (and (pos? w) (pos? h))
                 (/ (double w) h)
                 1.0)
        fov-y (:fov-y local-camera)
        fov-x (* aspect fov-y)
        [z-near z-far] (if (nil? scene-aabb)
                         [1.0 10000.0]
                         (find-z-extents (types/position local-camera)
                                         (camera-forward-vector local-camera)
                                         scene-aabb))]
    (-> local-camera
        (set-extents fov-x fov-y z-near z-far)
        filter-fn)))

(defn significant-drag?
  [current-position previous-position]
  (let [threshold 2]
    (->> (map (comp abs -) current-position previous-position)
         (apply max)
         (< threshold))))

(defn- lerp [a b t]
  (let [d (- b a)]
    (+ a (* t d))))

(defn scale-factor [camera viewport]
  (let [inv-view (doto (Matrix4d. (camera-view-matrix camera)) (.invert))
        x-axis   (Vector4d.)
        _        (.getColumn inv-view 0 x-axis)
        y-axis   (Vector4d.)
        _        (.getColumn inv-view 1 y-axis)
        cp0      (camera-project camera viewport (Point3d.))
        cpx      (camera-project camera viewport (Point3d. (.x x-axis) (.y x-axis) (.z x-axis)))
        cpy      (camera-project camera viewport (Point3d. (.x y-axis) (.y y-axis) (.z y-axis)))]
    [(/ 1.0 (Math/abs (- (.x cp0) (.x cpx)))) (/ 1.0 (Math/abs (- (.y cp0) (.y cpy)))) 1.0]))

(defn interpolate ^Camera [^Camera from ^Camera to ^double t]
  (let [filter-fn (or (:filter-fn from) identity)
        ^Camera to (filter-fn to)]
    (let [p (doto (Point3d.) (.interpolate ^Tuple3d (:position from) ^Tuple3d (:position to) t))
          fp (doto (Vector4d.) (.interpolate ^Tuple4d (:focus-point from) ^Tuple4d (:focus-point to) t))
          at (doto (Vector3d. (.x fp) (.y fp) (.z fp))
               (.sub p)
               (.negate)
               (.normalize))
          up ^Vector3d (math/rotate ^Quat4d (:rotation from) (Vector3d. 0.0 1.0 0.0))
          to-up ^Vector3d (math/rotate ^Quat4d (:rotation to) (Vector3d. 0.0 1.0 0.0))
          up (doto up
               (.interpolate ^Tuple3d to-up t)
               (.normalize))
          up (if (< 0.9999 (Math/abs (.dot up at)))
               (Vector3d. 0.0 0.0 1.0)
               up)
          right (doto (Vector3d.)
                  (.cross up at)
                  (.normalize))
          up (doto up
               (.cross at right)
               (.normalize))
          r (doto (Quat4d.)
              (.set (doto (Matrix3d.)
                      (.setColumn 0 right)
                      (.setColumn 1 up)
                      (.setColumn 2 at))))]
      (types/->Camera (:type to) p r
                      (lerp (:z-near from) (:z-near to) t)
                      (lerp (:z-far from) (:z-far to) t)
                      (lerp (:fov-x from) (:fov-x to) t)
                      (lerp (:fov-y from) (:fov-y to) t)
                      fp
                      (.distance p (Point3d. (.x fp) (.y fp) (.z fp)))
                      filter-fn))))

(defn set-camera!
  ([camera-node start-camera end-camera animate?]
   (set-camera! camera-node start-camera end-camera animate? nil))
  ([camera-node start-camera end-camera animate? on-animation-end]
   (if animate?
     (let [duration 0.5]
       (g/transact (g/set-property camera-node :animating true))
       (ui/anim! duration
                 (fn [^double t]
                   (let [t (- (* t t 3) (* t t t 2))
                         cam (interpolate start-camera end-camera t)]
                     (g/transact
                       (g/set-property camera-node :local-camera cam))))
                 (fn []
                   (g/transact
                     [(g/set-property camera-node :local-camera end-camera)
                      (g/set-property camera-node :animating false)])
                   ;; TODO: This doesn't belong here
                   (ui/user-data! (ui/main-scene) :editor.scene/ui/refresh-requested? true)
                   (when on-animation-end (on-animation-end)))))
     (g/transact
       (g/set-property camera-node :local-camera end-camera)))
   nil))

(defn- toggle-free-cam-css [image-view active]
  (when-let [tab-content (loop [current (.getParent ^Node image-view)]
                           (when current
                             (if (.contains (.getStyleClass current) "tab-content-area")
                               current
                               (recur (.getParent current)))))]
    (.pseudoClassStateChanged ^Node tab-content (PseudoClass/getPseudoClass "free-cam-mode-active") active)))

(defn look-delta [camera-node current-camera free-camera dx dy look-sensitivity dt]
  (let [{:keys [pitch yaw smoothed-look-delta]} free-camera
        [prev-dx prev-dy] smoothed-look-delta
        look-smoothing (prefs/get (g/node-value camera-node :prefs) [:scene :perspective-camera :mouse-smoothing])
        alpha (- 1.0 (Math/exp (* -25.0 look-smoothing dt)))
        smooth-dx (+ prev-dx (* alpha (- dx prev-dx)))
        smooth-dy (+ prev-dy (* alpha (- dy prev-dy)))]
    (if (or (not= smooth-dx 0.0) (not= smooth-dy 0.0))
      (let [new-yaw (+ yaw (* smooth-dx look-sensitivity))
            new-pitch (max -86.0 (min 86.0 (+ pitch (* smooth-dy look-sensitivity))))
            new-rotation (math/euler->quat [new-pitch new-yaw 0.0])
            focus-distance (:focus-distance current-camera)
            new-camera (assoc current-camera :rotation new-rotation)
            new-focus ^Point3d (math/offset-scaled (:position new-camera) (camera-forward-vector new-camera) focus-distance)
            new-focus (Vector4d. (.x new-focus) (.y new-focus) (.z new-focus) 1.0)]
        [(assoc new-camera :focus-point new-focus)
         (assoc free-camera :pitch new-pitch :yaw new-yaw :smoothed-look-delta [smooth-dx smooth-dy])])
      [current-camera free-camera])))

(defn wasd-move
  [camera-node camera free-camera ^Vector3d target-dir speed dt]
  (when (not= (.length target-dir) 0.0)
    (.normalize target-dir))

  (let [focus-distance (:focus-distance camera)
        final-speed (* speed focus-distance 0.1)]
    (.scale target-dir final-speed)

    (let [vel ^Vector3d (:velocity free-camera)
          diff (doto (Vector3d. target-dir) (.sub vel))
          damping (prefs/get (g/node-value camera-node :prefs) [:scene :perspective-camera :move-damping])]
      (.scale diff (* damping dt))
      (.add vel diff)
      (when (= (.length target-dir) 0.0)
        (.scale vel (Math/exp (* (- damping) dt))))

      (if (> (.length vel) 0.001)
        (let [offset (doto (Vector3d. vel) (.scale dt))
              new-camera (camera-move camera (.x offset) (.y offset) (.z offset))
              new-focus ^Point3d (math/offset-scaled (:position new-camera) (camera-forward-vector new-camera) focus-distance)]
          (assoc new-camera :focus-point (Vector4d. (.x new-focus) (.y new-focus) (.z new-focus) 1.0)))
        camera))))

(defn set-camera-type! [camera-controller projection-type]
  (let [old-camera (g/node-value camera-controller :local-camera)
        current-type (:type old-camera)]
    (when (not= current-type projection-type)
      (let [new-camera (case projection-type
                         :orthographic (camera-perspective->orthographic old-camera)
                         :perspective (camera-orthographic->perspective old-camera fov-y-35mm-full-frame))]
        (set-camera! camera-controller old-camera new-camera false)))))

(defn camera-2d?
  [camera-node evaluation-context]
  (mode-2d? (g/node-value camera-node :local-camera evaluation-context)))

(defn- sync-camera-position
  [^Camera camera-a ^Camera camera-b viewport]
  (let [focus ^Vector4d (:focus-point camera-b)
        point (camera-project camera-b viewport (Point3d. (.x focus) (.y focus) (.z focus)))
        world (camera-unproject camera-a viewport (.x point) (.y point) (.z point))
        delta (camera-unproject camera-b viewport (.x point) (.y point) (.z point))]
    (.sub delta world)
    (cond-> camera-a
      :always
      (-> (camera-ensure-orthographic)
          (camera-move (.x delta) (.y delta) (.z delta))
          (assoc :fov-x (:fov-x camera-b)
                 :fov-y (:fov-y camera-b)))

      (= (:type camera-a) :perspective)
      (camera-orthographic->perspective fov-y-35mm-full-frame))))

(defmulti realign-camera
  (fn [camera-node _animate?]
    (g/with-auto-evaluation-context evaluation-context
      (if (camera-2d? camera-node evaluation-context) :2d :3d))))

(defmethod realign-camera :2d
  [camera-node animate?]
  (let [local-cam (g/node-value camera-node :local-camera)
        viewport (g/node-value camera-node :viewport)
        camera-3d (-> (or (g/node-value camera-node :cached-3d-camera)
                          (tumble (g/node-value camera-node :local-camera) 200.0 -100.0))
                      (sync-camera-position local-cam viewport))
        local-cam (cond-> local-cam
                    (= (:type camera-3d) :perspective)
                    (camera-orthographic->perspective fov-y-35mm-full-frame))]
    (set-camera! camera-node local-cam camera-3d animate?)))

(defmethod realign-camera :3d
  [camera-node animate?]
  (let [local-cam (g/node-value camera-node :local-camera)
        is-perspective (= (:type local-cam) :perspective)]
    (g/transact (g/set-property camera-node :cached-3d-camera local-cam))
    (let [end-camera (cond-> local-cam
                       is-perspective camera-perspective->orthographic
                       :always camera-orthographic-realign
                       is-perspective (camera-orthographic->perspective fov-y-35mm-full-frame))]
      (set-camera! camera-node local-cam end-camera animate? #(set-camera-type! camera-node :orthographic)))))

(defn start-free-camera-mode! [camera-id]
  (let [current-camera (g/node-value camera-id :local-camera)
        [pitch yaw _] (math/quat->euler (:rotation current-camera))
        focus-distance (.distance ^Point3d (:position current-camera) (camera-focus-point current-camera))
        current-camera (if (= (:type current-camera) :orthographic)
                         (camera-orthographic->perspective current-camera fov-y-35mm-full-frame)
                         current-camera)]
    (i/start-mouse-capture)
    (g/set-property! camera-id :free-camera-mode true)
    (g/set-property! camera-id :local-camera (assoc current-camera :focus-distance focus-distance))
    (g/set-property! camera-id :cursor-type :none)
    (g/set-property! camera-id :free-camera {:velocity (Vector3d. 0.0 0.0 0.0)
                                             :pitch pitch
                                             :yaw yaw
                                             :smoothed-look-delta [0.0 0.0]})))
(defn- free-camera-movement-keys [prefs]
  (let [key-for-command (fn [cmd]
                          (some-> ^KeyCodeCombination
                                  (first (keymap/shortcuts (keymap/from-prefs prefs) cmd))
                                  (.getCode)))]
    (into #{} (keep key-for-command)
          [:scene.camera-move-forward
           :scene.camera-move-backward
           :scene.camera-move-left
           :scene.camera-move-right
           :scene.camera-move-up
           :scene.camera-move-down])))

(defn handle-input [self input-state action _user-data]
  (let [image-view (g/node-value self :image-view)
        movements-enabled (g/node-value self :movements-enabled)
        free-camera-mode (g/node-value self :free-camera-mode)
        ui-state (or (g/user-data self ::ui-state) {:movement :idle})
        {:keys [initial-x initial-y]} ui-state
        {:keys [x y type button key-code]} action
        local-cam (g/node-value self :local-camera)
        is-secondary (= button :secondary)
        is-perspective? (= :perspective (:type local-cam))
        movement (if (= type :mouse-pressed)
                   (get movements-enabled (camera-movement action) :idle)
                   (:movement ui-state))
        is-significant-drag (and initial-x
                                 initial-y
                                 x
                                 y
                                 (significant-drag? [x y] [initial-x initial-y]))]
    (case type
      :scroll (if (contains? movements-enabled :dolly)
                (do
                  (add-dolly-impulse! self (* -0.002 (:delta-y action)))
                  nil)
                action)
      :mouse-pressed
      (do
        (g/user-data-swap! self ::ui-state assoc
                           :last-x x
                           :last-y y
                           :initial-x x
                           :initial-y y
                           :movement movement)
        (when (and (= movement :idle)
                   (not free-camera-mode))
          action))

      :drag-detected
      (if (and is-secondary)
        (do
          (toggle-free-cam-css image-view true)
          (start-free-camera-mode! self)
          nil)
        action)

      :mouse-released
      (let [dragging (:is-dragging (g/user-data self ::ui-state))]
        (g/user-data-swap! self ::ui-state assoc
                           :last-x nil
                           :last-y nil
                           :initial-x nil
                           :initial-y nil
                           :is-dragging false
                           :movement :idle)
        (g/set-property! self :cursor-type :default)
        (cond
          free-camera-mode
          (do
            (g/set-property! self :free-camera-mode false)
            (g/set-property! self :free-camera {:velocity (Vector3d. 0.0 0.0 0.0)
                                                :pitch 0.0
                                                :yaw 0.0
                                                :smoothed-look-delta [0.0 0.0]})
            (i/stop-mouse-capture)
            (g/set-property! self :cursor-type :default)
            (toggle-free-cam-css image-view false))

          ;; Allow right click for context menu
          (or (= movement :idle)
              (and is-secondary
                   (not dragging)
                   (not is-significant-drag)))
          action

          :else
          nil))

      :key-pressed
      (cond
        (and (= key-code KeyCode/ESCAPE)
             (not (contains? (:mouse-buttons input-state) :secondary))
             free-camera-mode)
        ;; TODO: We probably want a counterpart to start-free-camera-mode! to call this
        (do
          (g/set-property! self :free-camera-mode false)
          (g/set-property! self :cursor-type :default)
          (i/stop-mouse-capture)
          nil)

        (and (contains? (:mouse-buttons input-state) :secondary)
             (not free-camera-mode)
             (contains? (free-camera-movement-keys (g/node-value self :prefs)) key-code))
        (do
          (toggle-free-cam-css image-view true)
          (start-free-camera-mode! self)))

      ;; NOTE: Don't let other handlers receive input if we're in free camera mode
      free-camera-mode
      nil

      action)))

(def ^:private camera-speed 5.0)
(def ^:private camera-speed-boost 3.0)
(def ^:private camera-speed-precision 0.35)

(defn- warp-mouse-around-edges [^ImageView image-view cursor-x cursor-y last-x last-y]
  (if (and cursor-x last-x)
    (let [screen-w (.getFitWidth image-view)
          screen-h (.getFitHeight image-view)
          padding 1
          [warp-x warp-y] (cond
                            (< cursor-x 0)        [(- screen-w padding) cursor-y]
                            (> cursor-x screen-w) [padding cursor-y]
                            (< cursor-y 0)        [cursor-x (- screen-h padding)]
                            (> cursor-y screen-h) [cursor-x padding]
                            :else nil)]
      (if warp-x
        (let [origin (.localToScreen image-view 0.0 0.0)
              ;; NOTE: make-gl-pane! flips the ImageView's Y axis, so we have to flip back for the localToScreen to work
              screen-abs-pos (.localToScreen image-view warp-x (- screen-h warp-y))]
          (i/warp-cursor (.getX screen-abs-pos) (.getY screen-abs-pos))
          [(double warp-x) (double warp-y) (double warp-x) (double warp-y)])
        [cursor-x cursor-y last-x last-y]))
    [cursor-x cursor-y last-x last-y]))

(defn- handle-update-tick [self input-state dt]
  (if (g/node-value self :free-camera-mode)
    (let [current-camera (g/node-value self :local-camera)
          prefs (g/node-value self :prefs)
          {:keys [mouse-buttons modifiers pressed-keys cursor-pos]} input-state
          is-secondary-button (or (contains? mouse-buttons :secondary)
                                  (g/node-value self :free-camera-mode))
          shift (contains? modifiers :shift)
          alt (contains? modifiers :alt)
          speed (* camera-speed
                   (cond shift camera-speed-boost alt camera-speed-precision :else 1.0)
                   (prefs/get prefs [:scene :perspective-camera :speed]))
          free-camera (g/node-value self :free-camera)
          walking-mode (prefs/get prefs [:scene :perspective-camera :walking-mode])
          forward (let [f (camera-forward-vector current-camera)]
                    (if walking-mode
                      (Vector3d. (.x f) 0.0 (.z f))
                      f))
          right (camera-right-vector current-camera)
          up (if walking-mode
               (Vector3d. 0.0 1.0 0.0)
               (camera-up-vector current-camera))
          [camera-after-look free-camera]
          (if is-secondary-button
            (let [invert-y? (prefs/get prefs [:scene :perspective-camera :invert-y])
                  look-sensitivity (prefs/get prefs [:scene :perspective-camera :look-sensitivity])
                  mouse-delta (i/poll-mouse-delta)
                  dx (- (if mouse-delta (.dx mouse-delta) 0.0))
                  dy (if mouse-delta (.dy mouse-delta) 0.0)
                  dy (if invert-y? dy (- dy))] ;; It's already inverted
              (look-delta self current-camera free-camera dx dy look-sensitivity dt))
            [current-camera free-camera])
          key-for-command (fn [cmd] (some-> ^KeyCodeCombination (first (keymap/shortcuts (keymap/from-prefs prefs) cmd))
                                            (.getCode)))
          w-key (key-for-command :scene.camera-move-forward)
          a-key (key-for-command :scene.camera-move-left)
          s-key (key-for-command :scene.camera-move-backward)
          d-key (key-for-command :scene.camera-move-right)
          q-key (key-for-command :scene.camera-move-down)
          e-key (key-for-command :scene.camera-move-up)
          target-dir (Vector3d. 0.0 0.0 0.0)
          _ (doseq [key pressed-keys]
              (cond
                (= key w-key) (.add target-dir forward)
                (= key s-key) (.sub target-dir forward)
                (= key d-key) (.add target-dir right)
                (= key a-key) (.sub target-dir right)
                (= key q-key) (.sub target-dir up)
                (= key e-key) (.add target-dir up)))
          final-camera (wasd-move self camera-after-look free-camera target-dir speed dt)]
      (g/set-property! self :free-camera free-camera)
      (when (not= final-camera current-camera)
        (set-camera! self current-camera final-camera false)))
    (let [viewport (g/node-value self :viewport)
          ui-state (or (g/user-data self ::ui-state) {:movement :idle})
          {:keys [last-x last-y initial-x initial-y movement]} ui-state
          camera (g/node-value self :camera)
          is-mode-2d (mode-2d? camera)
          filter-fn (:filter-fn camera)
          {:keys [mouse-buttons modifiers pressed-keys cursor-pos]} input-state
          [cursor-x cursor-y] cursor-pos
          is-primary (contains? mouse-buttons :primary)
          scroll-delta-y (second (:scroll-delta input-state [0.0 0.0]))
          [mouse-x mouse-y] (:view-pos input-state)
          movements-enabled (g/node-value self :movements-enabled)
          alt (contains? (:modifiers input-state) :alt)
          has-mouse-moved (and mouse-x mouse-y last-x last-y (not= :idle movement))
          image-view (g/node-value self :image-view)
          [mouse-x mouse-y last-x last-y] (warp-mouse-around-edges image-view mouse-x mouse-y last-x last-y)
          is-significant-drag (and initial-x
                                   initial-y
                                   mouse-x
                                   mouse-y
                                   (significant-drag? [mouse-x mouse-y] [initial-x initial-y]))
          camera (cond-> camera
                   (and (not (zero? scroll-delta-y))
                        (contains? movements-enabled :dolly))
                   (cond->
                     :always
                     (dolly (* -0.002 scroll-delta-y))

                     (or (and is-mode-2d (not alt))
                         (and (not is-mode-2d) alt))
                     (pan-at-pointer-position camera viewport [mouse-x mouse-y]))

                   has-mouse-moved
                   (cond->
                       (= :dolly movement)
                     (dolly (* -0.002 (- mouse-y last-y)))
                     (and (= :track movement)
                          #_is-significant-drag)
                     (track viewport last-x last-y mouse-x mouse-y)
                     (= :tumble movement)
                     (tumble (- last-x mouse-x) (- last-y mouse-y)))

                   filter-fn
                   filter-fn)
          camera (apply-dolly-easing self camera dt)]
      (g/set-property! self :local-camera camera)
      (when has-mouse-moved
        (g/user-data-swap! self ::ui-state assoc
                           :last-x mouse-x
                           :last-y mouse-y
                           :is-dragging (or (:is-dragging ui-state) is-significant-drag))
        (when is-significant-drag
          (g/set-property! self :cursor-type
                           (if (or (not= :perspective (:type (g/node-value self :local-camera)))
                                   is-primary)
                             :pan
                             :none)))))))

;; TODO: Merge this back in or use an fnk thingie?
(defn- handle-input-fnk [self input-state action user-data]
  (handle-input self input-state action user-data))

(defn- handle-update-tick-fnk [self input-state dt]
  (handle-update-tick self input-state dt))

(g/defnode CameraController
  (property prefs g/Any)
  (property image-view ImageView)
  (property name g/Keyword (default :local-camera))
  (property local-camera Camera)
  (property cached-3d-camera Camera)
  (property animating g/Bool)
  (property movements-enabled g/Any (default #{:dolly :track :tumble :look}))
  (property cursor-type g/Keyword)
  (property free-camera-mode g/Bool)
  (property free-camera g/Any (default {:velocity (Vector3d. 0.0 0.0 0.0)
                                        :pitch 0.0
                                        :yaw 0.0
                                        :smoothed-look-delta [0.0 0.0]}))

  (input scene-aabb AABB)
  (input viewport Region)

  (output viewport Region (gu/passthrough viewport))
  (output camera Camera :cached produce-camera)
  (output cursor-type g/Keyword (gu/passthrough cursor-type))

  (output input-handler Runnable :cached (g/constantly handle-input-fnk))
  (output update-tick-handler Runnable :cached (g/constantly handle-update-tick-fnk)))

(defmethod popup/settings-row [:perspective-camera :speed]
  [app-view prefs prefs-path ^PopupControl popup [_ option]]
  (popup/slider-setting app-view prefs popup option prefs-path "Move Speed" 1.0 3.0))

(defmethod popup/settings-row [:perspective-camera :move-damping]
  [app-view prefs prefs-path ^PopupControl popup [_ option]]
  (popup/slider-setting app-view prefs popup option prefs-path "Move Damping" 5.0 20.0))

(defmethod popup/settings-row [:perspective-camera :look-sensitivity]
  [app-view prefs prefs-path ^PopupControl popup [_ option]]
  (popup/slider-setting app-view prefs popup option prefs-path "Look Sensitivity" 0.02 0.5))

(defmethod popup/settings-row [:perspective-camera :mouse-smoothing]
  [app-view prefs prefs-path ^PopupControl popup [_ option]]
  (popup/slider-setting app-view prefs popup option prefs-path "Mouse Smoothing" 0.3 0.8))

(defmethod popup/settings-row [:perspective-camera :invert-y]
  [app-view prefs prefs-path _popup [_ option]]
  (popup/toggle-setting app-view prefs _popup option prefs-path "Invert Y" nil))

(defmethod popup/settings-row [:perspective-camera :walking-mode]
  [app-view prefs prefs-path _popup [_ option]]
  (popup/toggle-setting app-view prefs _popup option prefs-path "Walking Mode" nil))

(defn show-settings! [app-view ^Parent owner prefs]
  (popup/show-settings! app-view owner prefs 260 [:scene :perspective-camera]
                        [[:speed] [:move-damping] [:mouse-smoothing] [:look-sensitivity] [:invert-y] [:walking-mode]]))
