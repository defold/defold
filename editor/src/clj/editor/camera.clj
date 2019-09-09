(ns editor.camera
  (:require [dynamo.graph :as g]
            [editor.geom :as geom]
            [editor.graph-util :as gu]
            [editor.math :as math]
            [editor.types :as types]
            [schema.core :as s])
  (:import [editor.types AABB Camera Frustum Rect Region]
           [javax.vecmath AxisAngle4d Matrix3d Matrix4d Point2d Point3d Quat4d Tuple2d Tuple3d Tuple4d Vector3d Vector4d]))

(set! *warn-on-reflection* true)

(def fov-x-35mm-full-frame 54.4)
(def fov-y-35mm-full-frame 37.8)

(s/defn camera-forward-vector :- Vector3d
  [camera :- Camera]
  (math/rotate (types/rotation camera)
               (Vector3d. 0.0 0.0 -1.0)))

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

(s/defn camera-perspective-projection-matrix :- Matrix4d
  [camera :- Camera]
  (let [near   (.z-near camera)
        far    (.z-far camera)
        fov-x  (.fov-x camera)
        fov-y  (.fov-y camera)
        aspect (/ fov-x fov-y)

        ymax (* near (Math/tan (/ (* fov-y Math/PI) 360.0)))
        ymin (- ymax)
        xmin (* ymin aspect)
        xmax (* ymax aspect)

        x    (/ (* 2.0 near) (- xmax xmin))
        y    (/ (* 2.0 near) (- ymax ymin))
        a    (/ (+ xmin xmax) (- xmax xmin))
        b    (/ (+ ymin ymax) (- ymax ymin))
        c    (/ (- (+     near far)) (- far near))
        d    (/ (- (* 2.0 near far)) (- far near))
        m    (Matrix4d.)]
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

(s/defn camera-orthographic-projection-matrix :- Matrix4d
  [camera :- Camera]
  (let [near   (.z-near camera)
        far    (.z-far camera)
        right  (/ (.fov-x camera) 2.0)
        left   (- right)
        top    (/ (.fov-y camera) 2.0)
        bottom (- top)
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
                   (max 0.01 (+ (or fov 0)
                                (* (or fov 1)
                                   delta))))]
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

(defn track
  [^Camera camera ^Region viewport last-x last-y evt-x evt-y]
  (let [focus ^Vector4d (:focus-point camera)
        point (camera-project camera viewport (Point3d. (.x focus) (.y focus) (.z focus)))
        screen-z (.z point)
        world (camera-unproject camera viewport evt-x evt-y screen-z)
        delta (camera-unproject camera viewport last-x last-y screen-z)]
    (.sub delta world)
    (assoc (camera-move camera (.x delta) (.y delta) (.z delta))
      :focus-point (doto focus (.add delta)))))

(defn tumble
  [^Camera camera last-x last-y evt-x evt-y]
  (let [rate 0.005
        dx (- last-x evt-x)
        dy (- last-y evt-y)
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
                     (.set (AxisAngle4d. 1.0  0.0  0.0 (* dy rate))))
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
   [:secondary false false true  false] :dolly
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
  (let [^double half-fov-rad (math/deg->rad (* fov-deg 0.5))
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
  [^Camera camera ^Region viewport ^AABB aabb]
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
        ^double half-fov-y-rad (math/deg->rad (* fov-y-deg 0.5))
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
        ^double half-fov-x-rad (math/deg->rad (* fov-x-deg 0.5))
        ^double half-fov-y-rad (math/deg->rad (* fov-y-deg 0.5))
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
      [(max 1.0 (- near 0.0001))
       (max 10.0 (+ far 0.0001))])))

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

(defn handle-input [self action user-data]
  (let [viewport                   (g/node-value self :viewport)
        movements-enabled          (g/node-value self :movements-enabled)
        ui-state                   (or (g/user-data self ::ui-state) {:movement :idle})
        {:keys [last-x last-y]}    ui-state
        {:keys [x y type delta-y]} action
        movement                   (if (= type :mouse-pressed)
                                     (get movements-enabled (camera-movement action) :idle)
                                     (:movement ui-state))
        camera                     (g/node-value self :camera)
        filter-fn                  (or (:filter-fn camera) identity)
        camera                     (cond-> camera
                                     (and (= type :scroll)
                                          (contains? movements-enabled :dolly))
                                     (dolly (* -0.002 delta-y))

                                     (and (= type :mouse-moved)
                                          (not (= :idle movement)))
                                     (cond->
                                       (= :dolly movement)
                                       (dolly (* -0.002 (- y last-y)))
                                       (= :track movement)
                                       (track viewport last-x last-y x y)
                                       (= :tumble movement)
                                       (tumble last-x last-y x y))

                                     true
                                     filter-fn)]
    (g/set-property! self :local-camera camera)
    (case type
      :scroll (if (contains? movements-enabled :dolly) nil action)
      :mouse-pressed (do
                       (g/user-data-swap! self ::ui-state assoc :last-x x :last-y y :movement movement)
                       (if (= movement :idle) action nil))
      :mouse-released (do
                        (g/user-data-swap! self ::ui-state assoc :last-x nil :last-y nil :movement :idle)
                        (if (= movement :idle) action nil))
      :mouse-moved (if (not (= :idle movement))
                     (do
                       (g/user-data-swap! self ::ui-state assoc :last-x x :last-y y)
                       nil)
                     action)
      action)))

(g/defnode CameraController
  (property name g/Keyword (default :local-camera))
  (property local-camera Camera)
  (property movements-enabled g/Any (default #{:dolly :track :tumble}))

  (input scene-aabb AABB)
  (input viewport Region)

  (output viewport Region (gu/passthrough viewport))
  (output camera Camera :cached produce-camera)

  (output input-handler Runnable :cached (g/constantly handle-input)))

(defn- lerp [a b t]
  (let [d (- b a)]
    (+ a (* t d))))

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
                      filter-fn))))

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
