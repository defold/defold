(ns dynamo.camera
  (:require [dynamo.geom :as geom]
            [dynamo.graph :as g]
            [dynamo.types :as t]
            [dynamo.ui :as ui]
            [schema.core :as s])
  (:import [dynamo.types Camera Region AABB]
           [javax.vecmath Point3d Quat4d Matrix4d Vector3d Vector4d AxisAngle4d]))

(s/defn camera-view-matrix :- Matrix4d
  [camera :- Camera]
  (let [pos (Vector3d. (t/position camera))
        m   (Matrix4d.)]
    (.setIdentity m)
    (.set m (t/rotation camera))
    (.transpose m)
    (.transform m pos)
    (.negate pos)
    (.setColumn m 3 (.x pos) (.y pos) (.z pos) 1.0)
    m))

(s/defn camera-perspective-projection-matrix :- Matrix4d
  [camera :- Camera]
  (let [near   (.z-near camera)
        far    (.z-far camera)
        aspect (.aspect camera)
        fov    (.fov camera)

        ymax (* near (Math/tan (/ (* fov Math/PI) 360.0)))
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

(s/defn camera-orthographic-projection-matrix :- Matrix4d
  [camera :- Camera]
  (let [near   (.z-near camera)
        far    (.z-far camera)
        right  (/ (.fov camera) 2.0)
        left   (- right)
        bottom (/ left (.aspect camera))
        top    (/ right (.aspect camera))

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
    (set! (. m m22) (/ -2.0 (- far near)))
    (set! (. m m23) (/ (- (+ far near) (- far near))))

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
  ([t :- (t/enum :perspective :orthographic)]
    (let [distance 10000.0
          position (doto (Point3d.) (.set 0.0 0.0 1.0) (.scale distance))
          rotation (doto (Quat4d.)   (.set 0.0 0.0 0.0 1.0))]
      (t/->Camera t position rotation 1 2000 1 30 (Vector4d. 0 0 0 1.0) (t/->Region 0 0 0 0)))))

(s/defn set-orthographic :- Camera
  [camera :- Camera fov :- t/Num aspect :- t/Num z-near :- t/Num z-far :- t/Num]
  (assoc camera
         :fov fov
         :aspect aspect
         :z-near z-near
         :z-far z-far))

(s/defn camera-rotate :- Camera
  [camera :- Camera q :- Quat4d]
  (assoc camera :rotation (doto (Quat4d. (t/rotation camera)) (.mul (doto (Quat4d. q) (.normalize))))))

(s/defn camera-move :- Camera
  [camera :- Camera x :- t/Num y :- t/Num z :- t/Num]
  (assoc camera :position (doto (Point3d. x y z) (.add (t/position camera)))))

(s/defn camera-set-position :- Camera
  [camera :- Camera x :- t/Num y :- t/Num z :- t/Num]
  (assoc camera :position (Point3d. x y z)))

(s/defn camera-set-center :- Camera
  [camera :- Camera bounds :- AABB]
  (let [center (geom/aabb-center bounds)
        view-matrix (camera-view-matrix camera)]
    (.transform view-matrix center)
    (set! (. center z) 0)
    (.transform ^Matrix4d (geom/invert view-matrix) center)
    (camera-set-position camera (.x center) (.y center) (.z center))))

(s/defn camera-project :- Point3d
  "Returns a point in device space (i.e., corresponding to pixels on screen)
   that the given point projects onto. The input point should be in world space."
  [camera :- Camera region :- Region point :- Point3d]
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

          rx (+ (.left region) (/ (* (+ 1 x) (.right  region)) 2))
          ry (+ (.top  region) (/ (* (+ 1 y) (.bottom region)) 2))
          rz (/ (+ 1 z) 2)

          device-y (- (.bottom region) (.top region) ry 1)]
      (Point3d. rx device-y rz))))

(defmacro scale-to-doubleunit [x x-min x-max]
  `(- (/ (* (- ~x ~x-min) 2) ~x-max) 1.0))

(defmacro normalize-vector [v]
  `(do
     (if (= (.w ~v) 0.0) (throw (ArithmeticException.)))
     (Vector4d. (/ (.x ~v) (.w ~v))
                (/ (.y ~v) (.w ~v))
                (/ (.z ~v) (.w ~v))
                1.0)))

(s/defn camera-unproject :- Vector4d
  [camera :- Camera win-x :- t/Num win-y :- t/Num win-z :- t/Num]
  (let [viewport (t/viewport camera)
        win-y    (- (.bottom viewport) (.top viewport) win-y 1.0)
        in       (Vector4d. (scale-to-doubleunit win-x (.left viewport) (.right viewport))
                            (scale-to-doubleunit win-y (.top viewport)  (.bottom viewport))
                            (- (* 2 win-z) 1.0)
                            1.0)
        proj     (camera-projection-matrix camera)
        model    (camera-view-matrix camera)
        a        (Matrix4d.)
        out      (Vector4d.)]
    (.mul a proj model)
    (.invert a)
    (.transform a in out)

    (normalize-vector out)))

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

(defn dolly
  [camera delta]
  (update-in camera [:fov]
             (fn [fov]
               (max 0.01 (+ (or fov 0) (* (or fov 1) delta))))))

(defn track
  [camera last-x last-y evt-x evt-y]
  (let [focus ^Vector4d (:focus-point camera)
        point (camera-project camera (:viewport camera) (Point3d. (.x focus) (.y focus) (.z focus)))
        world (camera-unproject camera evt-x evt-y (.z point))
        delta (camera-unproject camera last-x last-y (.z point))]
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
  {[:one-button 1 false true false]          :tumble
   [:one-button 1 true  true false]          :track
   [:one-button 1 true false false]          :dolly
   [:three-button 1 false true false]        :tumble
   [:three-button 2 false true false]        :track
   [:three-button 3 false true false]        :dolly})

(defn camera-movement
  ([event]
    (camera-movement (ui/mouse-type) (:button event) (:ctrl-down event) (:alt-down event) (:shift-down event)))
  ([mouse-type button control alt shift]
    (button-interpretation [mouse-type button control alt shift] :idle)))


(s/defn camera-fov-from-aabb :- t/Num
  [camera :- Camera ^AABB aabb :- AABB]
  (assert camera "no camera?")
  (assert aabb   "no aabb?")
  (let [^Region viewport (:viewport camera)
        min-proj    (camera-project camera viewport (.. aabb min))
        max-proj    (camera-project camera viewport (.. aabb max))
        proj-width  (Math/abs (- (.x max-proj) (.x min-proj)))
        proj-height (Math/abs (- (.y max-proj) (.y min-proj)))
        factor-x    (Math/abs (/ proj-width  (- (.right viewport) (.left viewport))))
        factor-y    (Math/abs (/ proj-height (- (.top viewport) (.bottom viewport))))
        factor-y    (* factor-y (:aspect camera))
        fov-x-prim  (* factor-x (:fov camera))
        fov-y-prim  (* factor-y (:fov camera))
        y-aspect    (/ fov-y-prim (:aspect camera))]
    (* 1.1 (Math/max y-aspect fov-x-prim))))

(s/defn camera-orthographic-frame-aabb :- Camera
  [camera :- Camera ^AABB aabb :- AABB]
  (assert (= :orthographic (:type camera)))
  (-> camera
    (set-orthographic (camera-fov-from-aabb camera aabb) (:aspect camera) (:z-near camera) (:z-far camera))
    (camera-set-center aabb)))

(g/defnode CameraController
  (property camera Camera)

  (property ui-state t/Any (default (constantly (atom {:movement :idle}))))
  (property movements-enabled t/Any (default #{:dolly :track :tumble}))

  (on :mouse-down
    (swap! (:ui-state self) assoc
      :last-x (:x event)
      :last-y (:y event)
      :movement (get (:movements-enabled self) (camera-movement event) :idle)))

  (on :mouse-move
    (let [{:keys [movement last-x last-y]} @(:ui-state self)
          {:keys [x y]} event]
      (when (not (= :idle movement))
        (case movement
          :dolly  (g/update-property self :camera dolly  (* -0.002 (- y last-y)))
          :track  (g/update-property self :camera track  last-x last-y x y)
          :tumble (g/update-property self :camera tumble last-x last-y x y)
          nil)
        (swap! (:ui-state self) assoc
          :last-x x
          :last-y y))))

  (on :mouse-up
    (swap! (:ui-state self) assoc
      :last-x nil
      :last-y nil
      :movement :idle))

  (on :mouse-wheel
    (when (contains? (:movements-enabled self) :dolly)
      (g/update-property self :camera dolly (* -0.02 (:count event))))))
