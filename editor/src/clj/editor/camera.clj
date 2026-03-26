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
  (:require [dynamo.graph :as g]
            [editor.geom :as geom]
            [editor.graph-util :as gu]
            [editor.input :as i]
            [editor.keymap :as keymap]
            [editor.math :as math]
            [editor.types :as types]
            [editor.ui :as ui]
            [editor.ui.popup :as popup]
            [editor.prefs :as prefs])
  (:import [editor.types AABB Camera Frustum Rect Region]
           [javafx.css PseudoClass]
           [javafx.scene Cursor Node Parent]
           [javafx.scene.image ImageView]
           [javafx.scene.input KeyCode]
           [javax.vecmath AxisAngle4d Matrix3d Matrix4d Point2d Point3d Quat4d Tuple2d Tuple3d Tuple4d Vector3d Vector4d]))

(set! *warn-on-reflection* true)

(def fov-x-35mm-full-frame 54.4)
(def fov-y-35mm-full-frame 37.8)

(def vector3-up (Vector3d. 0.0 1.0 0.0))

(defn camera-forward-vector
  ^Vector3d [^Camera camera]
  (math/rotate (types/rotation camera)
               (Vector3d. 0.0 0.0 -1.0)))

(defn camera-up-vector
  ^Vector3d [^Camera camera]
  (math/rotate (types/rotation camera) vector3-up))

(defn camera-right-vector
  ^Vector3d [^Camera camera]
  (math/rotate (types/rotation camera)
               (Vector3d. 1.0 0.0 0.0)))

(defn camera-focus-point
  ^Point3d [^Camera camera]
  (let [^Vector4d p (:focus-point camera)]
    (Point3d. (.x p) (.y p) (.z p))))

(defn camera-without-depth-clipping
  ^Camera [^Camera camera]
  (assoc camera
    :z-near Double/MIN_NORMAL
    :z-far Double/MAX_VALUE))

(defn camera-view-matrix
  ^Matrix4d [^Camera camera]
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

(defn camera-perspective-projection-matrix
  ^Matrix4d [^Camera camera]
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

(defn- camera-orthographic-projection-matrix
  ^Matrix4d [^Camera camera]
  (simple-orthographic-projection-matrix (.z-near camera) (.z-far camera) (.fov-x camera) (.fov-y camera)))

(defn camera-projection-matrix
  ^Matrix4d [^Camera camera]
  (case (:type camera)
    :perspective  (camera-perspective-projection-matrix camera)
    :orthographic (camera-orthographic-projection-matrix camera)))

(defn make-camera
  (^Camera [] (make-camera :perspective))
  ([t]
   (make-camera t identity))
  (^Camera [t filter-fn]
   (make-camera t filter-fn nil))
  (^Camera [t filter-fn opts]
   (let [distance 5000.0
         position (doto (Point3d.) (.set 0.0 0.0 1.0) (.scale distance))
         rotation (doto (Quat4d.) (.set 0.0 0.0 0.0 1.0))]
     (Camera. t position rotation
              1.0 10000.0
              (get opts :fov-x fov-x-35mm-full-frame) (get opts :fov-y fov-y-35mm-full-frame)
              (Vector4d. 0.0 0.0 0.0 1.0)
              0.0
              filter-fn))))

(defn- set-extents
  ^Camera [^Camera camera fov-x fov-y z-near z-far]
  (assoc camera
         :fov-x fov-x
         :fov-y fov-y
         :z-near z-near
         :z-far z-far))

(defn- camera-rotate
  ^Camera [^Camera camera ^Quat4d q]
  (assoc camera :rotation (doto (Quat4d. (types/rotation camera)) (.mul (doto (Quat4d. q) (.normalize))))))

(defn camera-move
  ^Camera [^Camera camera x y z]
  (assoc camera :position (doto (Point3d. x y z) (.add (types/position camera)))))

(defn camera-set-position
  ^Camera [^Camera camera x y z]
  (assoc camera :position (Point3d. x y z)))

(defn camera-set-center
  ^Camera [^Camera camera ^AABB bounds]
  (let [center (geom/aabb-center bounds)
        view-matrix (camera-view-matrix camera)
        position (Point3d. center)]
    (.transform view-matrix position)
    (.setZ position 0.0)
    (.transform ^Matrix4d (geom/invert view-matrix) position)
    (assoc camera
      :position position
      :focus-point (Vector4d. (.x center) (.y center) (.z center) 1.0))))

(defn camera-project
  "Returns a point in device space (i.e., corresponding to pixels on screen)
   that the given point projects onto. The input point should be in world space."
  ^Point3d [camera ^Region viewport ^Tuple3d point]
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
              (double (.width pick-rect)))
        sy (/ (.bottom viewport)
              (double (.height pick-rect)))
        tx (/ (+ (.right viewport)
                 (* 2.0 (- (.left viewport)
                           (double (.x pick-rect)))))
              (double (.width pick-rect)))
        ty (/ (+ (.bottom viewport)
                 (* 2.0 (- (.top viewport) (- (.bottom viewport) (double (.y pick-rect))))))
              (double (.height pick-rect)))]
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

(defn camera-unproject
  ^Vector4d [^Camera camera ^Region viewport ^Tuple3d win-coords]
  (let [win-y    (- (.bottom viewport) (.top viewport) (.y win-coords) 1.0)
        in       (Vector4d. (normalize-to-bipolar (.x win-coords) (.left viewport) (.right viewport))
                            (normalize-to-bipolar win-y (.top viewport) (.bottom viewport))
                            (- (* 2 (.z win-coords)) 1.0)
                            1.0)
        proj     (camera-projection-matrix camera)
        model    (camera-view-matrix camera)
        a        (Matrix4d.)
        out      (Vector4d.)]
    (.mul a proj model)
    (.invert a)
    (.transform a in out)
    (perspective-divide out)))

(defn viewproj-frustum-planes [^Camera camera]
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

(defn screen-rect-frustum
  ^Frustum [camera ^Region viewport ^Rect rect]
  (let [inv-view-proj (doto (camera-projection-matrix camera)
                        (.mul (camera-view-matrix camera))
                        (.invert))
        w (double (.width rect))
        h (double (.height rect))
        clip-tl (screen->clip viewport (.x rect) (.y rect))
        clip-tr (screen->clip viewport (+ (.x rect) w) (.y rect))
        clip-bl (screen->clip viewport (.x rect) (+ (.y rect) h))
        clip-br (screen->clip viewport (+ (.x rect) w) (+ (.y rect) h))
        near-tl (clip->world inv-view-proj clip-tl -1.0)
        near-tr (clip->world inv-view-proj clip-tr -1.0)
        near-bl (clip->world inv-view-proj clip-bl -1.0)
        near-br (clip->world inv-view-proj clip-br -1.0)
        far-tl (clip->world inv-view-proj clip-tl 1.0)
        far-tr (clip->world inv-view-proj clip-tr 1.0)
        far-bl (clip->world inv-view-proj clip-bl 1.0)
        far-br (clip->world inv-view-proj clip-br 1.0)]
    (geom/corners->frustum near-tl near-tr near-bl near-br far-tl far-tr far-bl far-br)))

(def ^:private dolly-delta-scale 0.0015)

(defn- dolly-orthographic [camera ^double delta]
  (let [dolly-fn (fn [^double fov]
                   (min 1000000.0
                        (max 0.01 (+ (or fov 0.0)
                                     (* (or fov 1.0)
                                        delta)))))]
    (-> camera
        (update :fov-x dolly-fn)
        (update :fov-y dolly-fn))))

(defn- dolly-perspective [camera ^double delta]
  (let [forward (camera-forward-vector camera)
        position (types/position camera)
        focus ^Vector4d (:focus-point camera)
        camera-to-focus-point (doto (Vector3d. (.x focus) (.y focus) (.z focus))
                                (.sub position))
        distance-to-focus-point (.length camera-to-focus-point)
        offset-distance (if (pos? delta)
                          (max 0.01 (* delta distance-to-focus-point))
                          (min -0.01 (* delta distance-to-focus-point)))]
    (update camera :position math/offset-scaled forward offset-distance)))

(defn dolly [camera ^double delta]
  (case (:type camera)
    :orthographic (dolly-orthographic camera (- delta))
    :perspective (dolly-perspective camera delta)))

(def ^:private zoom-inertia 0.04)

(defn- interpolate-position-and-focus-point [camera target-camera ^double factor]
  (let [current-pos (types/position camera)
        target-pos (types/position target-camera)
        new-pos (Point3d. current-pos)
        current-fp ^Vector4d (:focus-point camera)
        target-fp (:focus-point target-camera)
        new-fp (Vector4d. (.x current-fp) (.y current-fp) (.z current-fp) (.w current-fp))]
    (.interpolate new-pos ^Tuple3d target-pos factor)
    (.interpolate new-fp ^Tuple4d target-fp factor)
    [new-pos new-fp]))

(defn- apply-dolly-interpolation [camera-node camera ^double dt]
  (let [target-camera (:dolly-target-camera (g/user-data camera-node ::camera-state))]
    (if (nil? target-camera)
      camera
      (let [factor (- 1.0 (Math/exp (- (/ dt ^double zoom-inertia))))
            [new-pos ^Vector4d new-fp] (interpolate-position-and-focus-point camera target-camera factor)]
        (case (:type camera)
          :perspective
          (let [distance (.distance ^Point3d new-pos (types/position target-camera))
                fp-point (Point3d. (.x new-fp) (.y new-fp) (.z new-fp))
                focus-point-distance (.distance fp-point new-pos)]
            (if (< distance (* focus-point-distance 0.002))
              (do
                (g/user-data-swap! camera-node ::camera-state dissoc :dolly-target-camera)
                target-camera)
              (assoc camera :position new-pos :focus-point new-fp)))
          :orthographic
          (let [new-fov-x (+ (* (- 1.0 factor) (double (:fov-x camera)))
                             (* factor (double (:fov-x target-camera))))
                new-fov-y (+ (* (- 1.0 factor) (double (:fov-y camera)))
                             (* factor (double (:fov-y target-camera))))
                threshold (* ^double (:fov-y camera) 0.008)]
            (if (< (Math/abs (- new-fov-y (double (:fov-y target-camera)))) threshold)
              (do
                (g/user-data-swap! camera-node ::camera-state dissoc :dolly-target-camera)
                target-camera)
              (assoc camera :fov-x new-fov-x :fov-y new-fov-y :position new-pos :focus-point new-fp))))))))

(defn pan-at-pointer-position
  "Pans the camera so that the focus point is at the same position as it was before `dolly`."
  [^Camera camera ^Camera prev-camera ^Region viewport [^double x ^double y]]
  (let [focus ^Vector4d (:focus-point camera)
        focus-point-3d (Point3d. (.x focus) (.y focus) (.z focus))
        point (camera-project camera viewport focus-point-3d)
        prev-point (camera-project prev-camera viewport focus-point-3d)
        world (camera-unproject camera viewport (Point3d. x y (.z point)))
        delta (camera-unproject prev-camera viewport (Point3d. x y (.z prev-point)))]
    (.sub delta world)
    (assoc (camera-move camera (.x delta) (.y delta) (.z delta))
           :focus-point (doto focus (.add delta)))))

(defn- set-dolly-target!
  ([camera-node ^double delta]
   (set-dolly-target! camera-node delta nil [nil nil]))
  ([camera-node ^double delta viewport [mouse-x mouse-y]]
   (let [current-target (or (:dolly-target-camera (g/user-data camera-node ::camera-state))
                            (g/node-value camera-node :local-camera))
         new-target (dolly current-target delta)
         new-target (if (and viewport mouse-x mouse-y)
                      (pan-at-pointer-position new-target current-target viewport [mouse-x mouse-y])
                      new-target)]
     (g/user-data-swap! camera-node ::camera-state assoc :dolly-target-camera new-target))))

(defn- reset-dolly! [camera-node]
  (when-let [target-camera (:dolly-target-camera (g/user-data camera-node ::camera-state))]
    (g/user-data-swap! camera-node ::camera-state assoc :dolly-target-camera nil)
    (g/set-property! camera-node :local-camera target-camera)))

(defn track [^Camera camera ^Region viewport last-x last-y evt-x evt-y]
  (let [focus ^Vector4d (:focus-point camera)
        point (camera-project camera viewport (Point3d. (.x focus) (.y focus) (.z focus)))
        screen-z (.z point)
        world ^Vector4d (camera-unproject camera viewport (Point3d. evt-x evt-y screen-z))
        delta ^Vector4d (camera-unproject camera viewport (Point3d. last-x last-y screen-z))]
    (.sub delta world)
    (assoc (camera-move camera (.x delta) (.y delta) (.z delta))
           :focus-point (doto focus (.add delta)))))

(defn tumble [^Camera camera ^double dx ^double dy]
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
    ;; rotation is now in r
    (.conjugate inv-r r)

    (.mul q-delta r q-delta)
    (.mul q-delta inv-r)
    (.set delta q-delta)
    (.add delta focus)
    ;; position is now in delta
    (assoc camera
           :position (Point3d. (.x delta) (.y delta) (.z delta))
           :rotation r)))

(def ^:private button-interpretation
  ;; button    shift ctrl  alt   meta => movement
  {[:primary   false true  false false] :tumble
   [:primary   false false true  false] :track
   [:primary   false true  true  false] :dolly
   [:middle    false false false false] :track})

(defn camera-movement
  ([action]
   (camera-movement (:button action) (:shift action) (:control action) (:alt action) (:meta action)))
  ([button shift ctrl alt meta]
   (let [key [button shift ctrl alt meta]]
     (button-interpretation key :idle))))

(defn camera-orthographic-fov-from-aabb [^Camera camera ^Region viewport ^AABB aabb]
  {:pre [camera aabb]}
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
              factor-x (/ proj-width w)
              factor-y (/ proj-height h)
              fov-x-prim  (* factor-x (.fov-x camera))
              fov-y-prim  (* factor-y (.fov-y camera))]
          [(* 1.1 fov-x-prim) (* 1.1 fov-y-prim)])))))

(defn camera-orthographic-frame-aabb
  ^Camera [^Camera camera ^Region viewport ^AABB aabb]
  {:pre [(= :orthographic (:type camera))]}
  (let [aspect (/ ^double (:fov-x camera) ^double (:fov-y camera))
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

(defn camera-perspective-frame-aabb
  ^Camera [^Camera camera ^AABB aabb]
  {:pre [(= :perspective (:type camera))]}
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

(defn camera-frame-aabb
  ^Camera [^Camera camera ^Region viewport ^AABB aabb]
  (case (:type camera)
    :orthographic (camera-orthographic-frame-aabb camera viewport aabb)
    :perspective (camera-perspective-frame-aabb camera aabb)))

(defn camera-orthographic-realign ^Camera
  [^Camera camera]
  {:pre [(= :orthographic (:type camera))]}
  (let [focus ^Vector4d (:focus-point camera)
        delta ^Vector4d (doto (Vector4d. ^Point3d (:position camera))
                          (.sub focus))
        dist (.length delta)]
    (assoc camera
      :position (Point3d. (.x focus) (.y focus) dist)
      :rotation (Quat4d. 0.0 0.0 0.0 1.0))))

(defn camera-orthographic-frame-aabb-y
  ^Camera [^Camera camera ^Region viewport ^AABB aabb]
  {:pre [(= :orthographic (:type camera))]}
  (let [fov-x (:fov-x camera)
        [_ ^double fov-y] (camera-orthographic-fov-from-aabb camera viewport aabb)
        filter-fn (or (:filter-fn camera) identity)]
    (-> camera
        (camera-set-center aabb)
        (set-extents fov-x fov-y (:z-near camera) (:z-far camera))
        filter-fn)))

(defn camera-orthographic->perspective
  ^Camera [^Camera camera ^double fov-y-deg]
  ;; Orthographic fov is specified as distance between the frustum edges.
  ;; Perspective fov is specified as degrees between the frustum edges.
  {:pre [(= :orthographic (:type camera))]}
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

(defn camera-perspective->orthographic
  ^Camera [^Camera camera]
  ;; Orthographic fov is specified as distance between the frustum edges.
  ;; Perspective fov is specified as degrees between the frustum edges.
  {:pre [(= :perspective (:type camera))]}
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

(defn camera-ensure-orthographic
  ^Camera [^Camera camera]
  (case (:type camera)
    :orthographic camera
    :perspective (camera-perspective->orthographic camera)))

(defn find-z-extents
  ^doubles [^Vector3d cam-pos ^Vector3d cam-dir ^AABB aabb]
  (loop [corners (geom/aabb->corners aabb)
         near Double/MAX_VALUE
         far Double/MIN_VALUE]
    (if-some [corner (first corners)]
      (let [cam->corner (math/subtract-vector corner cam-pos)
            distance (.dot cam-dir cam->corner)]
        (recur (next corners)
               (min distance near)
               (max distance far)))
      [(max 0.1 (- near 0.001))
       (max 100.0 (+ far 0.001))])))

(g/defnk produce-camera [_node-id local-camera scene-aabb ^Region viewport]
  (let [filter-fn (or (:filter-fn local-camera) identity)
        w (- (.right viewport) (.left viewport))
        h (- (.bottom viewport) (.top viewport))
        aspect (if (and (pos? w) (pos? h))
                 (/ (double w) h)
                 1.0)
        fov-y (double (:fov-y local-camera))
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
  [[^double cx ^double cy] [^double px ^double py]]
  (< 2.0 (max (Math/abs (- cx px))
              (Math/abs (- cy py)))))

(defn- lerp [^double a ^double b ^double t]
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
        ^Camera to (filter-fn to)
        p (doto (Point3d.) (.interpolate ^Tuple3d (:position from) ^Tuple3d (:position to) t))
        fp (doto (Vector4d.) (.interpolate ^Tuple4d (:focus-point from) ^Tuple4d (:focus-point to) t))
        at (doto (Vector3d. (.x fp) (.y fp) (.z fp))
             (.sub p)
             (.negate)
             (.normalize))
        up ^Vector3d (math/rotate ^Quat4d (:rotation from) vector3-up)
        to-up ^Vector3d (math/rotate ^Quat4d (:rotation to) vector3-up)
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
    (Camera. (:type to) p r
             (lerp (:z-near from) (:z-near to) t)
             (lerp (:z-far from) (:z-far to) t)
             (lerp (:fov-x from) (:fov-x to) t)
             (lerp (:fov-y from) (:fov-y to) t)
             fp
             (.distance p (Point3d. (.x fp) (.y fp) (.z fp)))
             filter-fn)))

(defn set-camera!
  ([camera-node start-camera end-camera animate?]
   (set-camera! camera-node start-camera end-camera animate? nil))
  ([camera-node start-camera end-camera animate? on-animation-end]
   (if animate?
     (let [duration 0.5]
       (g/transact (g/set-property camera-node :animating true))
       ;; NOTE: If the user was dollying during an animation, cancel the dolly
       (when (:dolly-target-camera (g/user-data camera-node ::camera-state))
         (g/user-data-swap! camera-node ::camera-state assoc :dolly-target-camera nil))
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
                   (ui/user-data! (ui/main-scene) ::ui/refresh-requested? true)
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

(defn- look-rotation [camera-state current-camera dx dy look-sensitivity dt]
  (let [max-pitch 86.0
        smoothing-rate -15.0
        {:keys [^double free-cam-pitch ^double free-cam-yaw ^double free-cam-smoothed-pitch ^double free-cam-smoothed-yaw]} camera-state
        smooth-yaw (or free-cam-smoothed-yaw free-cam-yaw)
        smooth-pitch (or free-cam-smoothed-pitch free-cam-pitch)
        target-yaw (+ free-cam-yaw (* ^double dx ^double look-sensitivity))
        target-pitch (max (- max-pitch) (min max-pitch (+ free-cam-pitch (* ^double dy ^double look-sensitivity))))
        smoothing (- 1.0 (Math/pow 10.0 (* smoothing-rate ^double dt)))
        new-smooth-yaw (+ smooth-yaw (* smoothing (- target-yaw smooth-yaw)))
        new-smooth-pitch (+ smooth-pitch (* smoothing (- target-pitch smooth-pitch)))
        new-rotation (math/euler->quat [new-smooth-pitch new-smooth-yaw 0.0])
        focus-distance (:focus-distance current-camera)
        new-camera (assoc current-camera :rotation new-rotation)
        new-focus ^Point3d (math/offset-scaled (:position new-camera) (camera-forward-vector new-camera) focus-distance)
        new-focus (Vector4d. (.x new-focus) (.y new-focus) (.z new-focus) 1.0)]
    [(assoc new-camera :focus-point new-focus)
     (assoc camera-state
       :free-cam-pitch target-pitch
       :free-cam-yaw target-yaw
       :free-cam-smoothed-pitch new-smooth-pitch
       :free-cam-smoothed-yaw new-smooth-yaw)]))

(defn- wasd-move [^Vector3d free-cam-velocity camera ^Vector3d target-dir speed dt]
  (when (not= (.length target-dir) 0.0)
    (.normalize target-dir))

  (let [focus-distance (:focus-distance camera)
        final-speed (* ^double speed ^double focus-distance 0.1)]
    (.scale target-dir final-speed)

    (let [diff (doto (Vector3d. target-dir) (.sub free-cam-velocity))
          damping 20.0
          factor (- 1.0 (Math/exp (* (- damping) ^double dt)))]
      (.scale diff factor)
      (.add free-cam-velocity diff)
      (when (= (.length target-dir) 0.0)
        (.scale free-cam-velocity (Math/exp (* (- damping) ^double dt))))

      (if (> (.length free-cam-velocity) 0.001)
        (let [offset (doto (Vector3d. free-cam-velocity) (.scale dt))
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

(defn mode-2d? [camera]
  (and (= 1.0 (some-> camera camera-view-matrix (.getElement 2 2)))
       (= :orthographic (:type camera))))

(defn camera-2d?
  [camera-node evaluation-context]
  (mode-2d? (g/node-value camera-node :local-camera evaluation-context)))

(defn- sync-camera-position
  [^Camera camera-a ^Camera camera-b viewport]
  (let [focus ^Vector4d (:focus-point camera-b)
        point (camera-project camera-b viewport (Point3d. (.x focus) (.y focus) (.z focus)))
        world (camera-unproject camera-a viewport point)
        delta (camera-unproject camera-b viewport point)]
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

(defn- contains-key-code? [pressed-keys key-codes] (some #(contains? pressed-keys %) key-codes))

(defn free-cam-mode-active? [camera-node]
  (some-> camera-node (g/user-data ::camera-state) :free-cam-mode))

(defn start-free-cam-mode! [^ImageView image-view camera-node current-cursor-pos]
  (when (and (not (:free-cam-mode (g/user-data camera-node ::camera-state)))
             (not (g/node-value camera-node :animating)))
    ;; NOTE: If the dolly is animating, this prevents funkiness
    (reset-dolly! camera-node)
    (.requestFocus image-view)
    (let [local-camera (g/node-value camera-node :local-camera)
          current-camera (cond-> local-camera
                           (= :orthographic (:type local-camera))
                           (camera-orthographic->perspective fov-y-35mm-full-frame))
          [pitch yaw _] (math/quat->euler (:rotation current-camera))
          focus-distance (.distance ^Point3d (:position current-camera) (camera-focus-point current-camera))
          bounds (.localToScreen image-view (.getBoundsInLocal image-view))
          center-x (int (+ (.getMinX bounds) (/ (.getWidth bounds) 2.0)))
          center-y (int (+ (.getMinY bounds) (/ (.getHeight bounds) 2.0)))]
      (toggle-free-cam-css image-view true)
      ;; NOTE: If we don't move the camera to the center, then fast mouse movements might mouse over other nodes, and JavaFX will
      ;; reset our cursor visibility
      (i/start-mouse-capture center-x center-y)
      (g/transact
        (concat
          (g/set-property camera-node :local-camera (assoc current-camera :focus-distance focus-distance))
          (g/set-property camera-node :cursor-type :none)))
      (g/user-data-swap! camera-node ::camera-state assoc
        :free-cam-mode true
        :free-cam-cursor-start-pos current-cursor-pos
        :free-cam-velocity (Vector3d. 0.0 0.0 0.0)
        :free-cam-pitch pitch
        :free-cam-yaw yaw
        :free-cam-smoothed-pitch pitch
        :free-cam-smoothed-yaw yaw))))

(defn stop-free-cam-mode! [image-view camera-node]
  (ui/set-cursor image-view Cursor/DEFAULT)
  (toggle-free-cam-css image-view false)
  (i/stop-mouse-capture)
  (let [[x y] (:free-cam-cursor-start-pos (g/user-data camera-node ::camera-state))]
    (when (and x y)
      (i/warp-cursor x y)))
  (g/set-property! camera-node :cursor-type :default)
  (g/user-data-swap! camera-node ::camera-state update :free-cam-mode (constantly false)))

(defn handle-input [self input-state action _user-data]
  (let [image-view (g/node-value self :image-view)
        camera-state (or (g/user-data self ::camera-state) {:movement :idle})
        movements-enabled (g/node-value self :movements-enabled)
        free-cam-mode (:free-cam-mode camera-state)
        {:keys [initial-x initial-y]} camera-state
        {:keys [x y type button key-code]} action
        local-cam (g/node-value self :local-camera)
        is-secondary (= button :secondary)
        movement (if (= type :mouse-pressed)
                   (get movements-enabled (camera-movement action) :idle)
                   (:movement camera-state))]
    (case type
      :scroll (if (and (contains? movements-enabled :dolly)
                       (not free-cam-mode))
                (let [is-mode-2d (mode-2d? local-cam)
                      alt (contains? (:modifiers input-state) :alt)
                      pan (or (and is-mode-2d (not alt))
                              (and (not is-mode-2d) alt))
                      delta (* ^double dolly-delta-scale (double (:delta-y action)))]
                  (if pan
                    (set-dolly-target! self delta (g/node-value self :viewport) (:view-pos input-state))
                    (set-dolly-target! self delta))
                  nil)
                action)
      :mouse-pressed
      (do
        ;; NOTE: The user might be trying to track/tumble. In case we're still interpolating the dolly, just reset it
        (reset-dolly! self)
        (g/user-data-swap! self ::camera-state assoc
                           :last-x x
                           :last-y y
                           :initial-x x
                           :initial-y y
                           :movement movement)
        (when (and (= movement :idle)
                   (not free-cam-mode))
          action))

      :drag-detected
      (do
        (g/user-data-swap! self ::camera-state assoc :is-dragging true)
        (if (and is-secondary
                 (movements-enabled :look))
          (start-free-cam-mode! image-view self (:cursor-pos input-state))
          (do
            (println "Movement" movement)
            (when (and (= :idle movement)
                       (not (g/node-value self :animating)))
              (println (not= :perspective (:type local-cam)))
              (g/set-property! self :cursor-type
                               (if (not= :perspective (:type local-cam))
                                 :pan
                                 :none)))
            action)))

      :mouse-released
      (let [dragging (:is-dragging (g/user-data self ::camera-state))]
        (g/user-data-swap! self ::camera-state assoc
                           :last-x nil
                           :last-y nil
                           :initial-x nil
                           :initial-y nil
                           :is-dragging false
                           :movement :idle)
        (g/set-property! self :cursor-type :default)
        (cond
          free-cam-mode
          (stop-free-cam-mode! image-view self)

          (or (= movement :idle)
              ;; Allow right click for context menu
              (and is-secondary
                   (not dragging)))
          action

          :else
          nil))

      :key-pressed
      (cond
        (and (= key-code KeyCode/ESCAPE)
             free-cam-mode
             (not (contains? (:mouse-buttons input-state) :secondary)))
        (stop-free-cam-mode! image-view self)

        (and (contains? (:mouse-buttons input-state) :secondary)
             (not free-cam-mode)
             (contains-key-code? (:pressed-keys input-state) (:all (g/node-value self :free-cam-shortcuts))))
        (start-free-cam-mode! image-view self (:cursor-pos input-state)))

      ;; NOTE: Don't let other handlers receive input if we're in free camera mode
      (if free-cam-mode
        nil
        action))))

(def ^:private camera-speed 5.0)
(def ^:private camera-speed-boost 3.0)
(def ^:private camera-speed-precision 0.35)

(defn- warp-mouse-around-edges
  [^ImageView image-view [^double cursor-x ^double cursor-y] [last-x last-y]]
  ;; NOTE: Unfortunately, Wayland doesn't support XWarpPointer, so we can't support this feature on Wayland just yet
  ;; TODO: We shouldn't have to check for image-view here, we shold be doing it before
  (if (and (not i/is-wayland) image-view cursor-x last-x)
    (let [screen-w (.getFitWidth image-view)
          screen-h (.getFitHeight image-view)
          padding 1
          [warp-x ^double warp-y]
          (cond
            (< cursor-x 0)        [(- screen-w padding) cursor-y]
            (> cursor-x screen-w) [padding cursor-y]
            (< cursor-y 0)        [cursor-x (- screen-h padding)]
            (> cursor-y screen-h) [cursor-x padding]
            :else nil)]
      (if warp-x
        ;; NOTE: make-gl-pane! flips the ImageView's Y axis, so we have to flip back for the localToScreen to work
        (let [screen-abs-pos (.localToScreen image-view warp-x (- screen-h warp-y))]
          (i/warp-cursor (.getX screen-abs-pos) (.getY screen-abs-pos))
          [(double warp-x) (double warp-y) (double warp-x) (double warp-y)])
        [cursor-x cursor-y last-x last-y]))
    [cursor-x cursor-y last-x last-y]))

(defn- compute-target-dir [pressed-keys free-cam-shortcuts camera-forward camera-right camera-up]
  (let [target-dir (Vector3d.)
        {:keys [forward left backward right down up]} free-cam-shortcuts]
    (when (contains-key-code? pressed-keys forward)  (.add target-dir camera-forward))
    (when (contains-key-code? pressed-keys backward) (.sub target-dir camera-forward))
    (when (contains-key-code? pressed-keys right)    (.add target-dir camera-right))
    (when (contains-key-code? pressed-keys left)     (.sub target-dir camera-right))
    (when (contains-key-code? pressed-keys down)     (.sub target-dir camera-up))
    (when (contains-key-code? pressed-keys up)       (.add target-dir camera-up))
    target-dir))

(defn- handle-update-tick [self input-state dt]
  (let [{:keys [free-cam-mode] :as camera-state} (g/user-data self ::camera-state)]
    (if free-cam-mode
      (let [current-camera (g/node-value self :local-camera)
            prefs (g/node-value self :prefs)
            {:keys [modifiers pressed-keys]} input-state
            shift (contains? modifiers :shift)
            alt (contains? modifiers :alt)
            speed (* ^double camera-speed
                     (double (cond shift camera-speed-boost
                                   alt camera-speed-precision
                                   :else 1.0))
                     (double (prefs/get prefs [:scene :perspective-camera :speed])))
            walking-mode (prefs/get prefs [:scene :perspective-camera :walking-mode])
            camera-forward (camera-forward-vector current-camera)
            camera-forward (if walking-mode
                             (Vector3d. (.x camera-forward) 0.0 (.z camera-forward))
                             camera-forward)
            camera-right (camera-right-vector current-camera)
            camera-up (if walking-mode
                        vector3-up
                        (camera-up-vector current-camera))
            look-sensitivity (double (prefs/get prefs [:scene :perspective-camera :look-sensitivity]))
            mouse-delta (i/poll-mouse-delta)
            dx (- (if mouse-delta (.dx mouse-delta) 0.0))
            dy (if mouse-delta (.dy mouse-delta) 0.0)
            dy (if (prefs/get prefs [:scene :perspective-camera :invert-y]) dy (- dy))
            [camera camera-state] (look-rotation camera-state current-camera dx dy look-sensitivity dt)
            free-cam-shortcuts (g/node-value self :free-cam-shortcuts)
            target-dir (compute-target-dir pressed-keys free-cam-shortcuts camera-forward camera-right camera-up)
            final-camera (wasd-move (:free-cam-velocity camera-state) camera target-dir speed dt)]
        (g/user-data-swap! self ::camera-state merge camera-state)
        (when (not= final-camera current-camera)
          (set-camera! self current-camera final-camera false)))
      (let [viewport (g/node-value self :viewport)
            {:keys [^double last-x ^double last-y ^double initial-x ^double initial-y movement dragging]} camera-state
            camera (g/node-value self :camera)
            filter-fn (:filter-fn camera)
            {:keys [mouse-buttons]} input-state
            is-primary (contains? mouse-buttons :primary)
            [mouse-x mouse-y] (:view-pos input-state)
            has-mouse-moved (and mouse-x mouse-y last-x last-y
                                 (not= :idle movement)
                                 (or (not= mouse-x last-x)
                                     (not= mouse-y last-y)))
            image-view (g/node-value self :image-view)
            [^double mouse-x ^double mouse-y ^double last-x ^double last-y]
            (warp-mouse-around-edges image-view [mouse-x mouse-y] [last-x last-y])
            camera (cond-> camera
                     has-mouse-moved
                     (cond->
                       (= :track movement)
                       (track viewport last-x last-y mouse-x mouse-y)

                       (= :tumble movement)
                       (tumble (- last-x mouse-x) (- last-y mouse-y)))

                     filter-fn
                     filter-fn)
            camera (apply-dolly-interpolation self camera dt)]
        (when (and has-mouse-moved (= :dolly movement))
          (set-dolly-target! self (* ^double dolly-delta-scale (- mouse-y last-y))))
        (g/set-property! self :local-camera camera)
        (when has-mouse-moved
          (g/user-data-swap! self ::camera-state assoc
                             :last-x mouse-x
                             :last-y mouse-y))))))

(g/defnode CameraController
  (property prefs g/Any)
  (property image-view ImageView)
  (property name g/Keyword (default :local-camera))
  (property local-camera Camera)
  (property cached-3d-camera Camera)
  (property animating g/Bool)
  (property movements-enabled g/Any (default #{:dolly :track :tumble :look}))
  (property cursor-type g/Keyword)

  (input scene-aabb AABB)
  (input viewport Region)
  (input keymap g/Any)

  (output viewport Region (gu/passthrough viewport))
  (output camera Camera :cached produce-camera)
  (output cursor-type g/Keyword (gu/passthrough cursor-type))
  (output free-cam-shortcuts g/Any :cached
          (g/fnk [keymap]
            (let [forward  (keymap/shortcut-key-codes keymap (keymap/shortcuts keymap :scene.free-camera.forward))
                  left     (keymap/shortcut-key-codes keymap (keymap/shortcuts keymap :scene.free-camera.left))
                  backward (keymap/shortcut-key-codes keymap (keymap/shortcuts keymap :scene.free-camera.backward))
                  right    (keymap/shortcut-key-codes keymap (keymap/shortcuts keymap :scene.free-camera.right))
                  down     (keymap/shortcut-key-codes keymap (keymap/shortcuts keymap :scene.free-camera.down))
                  up       (keymap/shortcut-key-codes keymap (keymap/shortcuts keymap :scene.free-camera.up))]
              {:forward forward :left left :backward backward :right right :down down :up up
               :all (into [] cat [forward left backward right down up])})))

  (output input-handler Runnable :cached (g/constantly handle-input))
  (output update-tick-handler Runnable :cached (g/constantly handle-update-tick)))

(defn show-settings! [^Parent owner prefs localization]
  (popup/show-settings! owner prefs localization 260 [:scene :perspective-camera]
                        [{:key :speed :type :slider :label "scene-popup.move-speed" :min 1.0 :max 3.0}
                         {:key :look-sensitivity :type :slider :label "scene-popup.look-sensitivity" :min 0.02 :max 0.5}
                         {:key :invert-y :type :toggle :label "scene-popup.invert-y"}
                         {:key :walking-mode :type :toggle :label "scene-popup.walking-mode"}]))
