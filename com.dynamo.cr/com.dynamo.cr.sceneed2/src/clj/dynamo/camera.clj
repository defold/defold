(ns dynamo.camera
  (:require [schema.macros :as sm]
            [schema.core :as s]
            [plumbing.core :refer [defnk]]
            [dynamo.types :as t]
            [dynamo.node :as n]
            [dynamo.project :as p]
            [dynamo.ui :as ui]
            [internal.cache :refer [caching]]
            [dynamo.geom :as g])
  (:import [javax.vecmath Point3d Quat4d Matrix4d Vector3d Vector4d]
           [org.eclipse.swt SWT]
           [dynamo.types Camera Region AABB]))

(set! *warn-on-reflection* true)

(sm/defn camera-view-matrix :- Matrix4d
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

(sm/defn camera-perspective-projection-matrix :- Matrix4d
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

(sm/defn camera-orthographic-projection-matrix :- Matrix4d
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

(sm/defn camera-projection-matrix :- Matrix4d
  [camera :- Camera]
  (case (:type camera)
    :perspective  (camera-perspective-projection-matrix camera)
    :orthographic (camera-orthographic-projection-matrix camera)))



(sm/defn make-camera :- Camera
  ([] (make-camera :perspective))
  ([t :- (s/enum :perspective :orthographic)]
    (let [distance 10000.0
          position (doto (Point3d.) (.set 0.0 0.0 1.0) (.scale distance))
          rotation (doto (Quat4d.)   (.set 0.0 0.0 0.0 1.0))]
      (t/->Camera t position rotation 1 2000 1 30 (Vector4d. 0 0 0 1.0) (t/->Region 0 0 0 0)))))

(sm/defn set-orthographic :- Camera
  [camera :- Camera fov :- s/Num aspect :- s/Num z-near :- s/Num z-far :- s/Num]
  (assoc camera
         :fov fov
         :aspect aspect
         :z-near z-near
         :z-far z-far))

(sm/defn camera-rotate :- Camera
  [camera :- Camera q :- Quat4d]
  (assoc camera :rotation (.mul (t/rotation camera) (.normalize (Quat4d. q)))))

(sm/defn camera-move :- Camera
  [camera :- Camera x :- s/Num y :- s/Num z :- s/Num]
  (let [p (t/position camera)]
    (set! (. p x) (+ x (. p x)))
    (set! (. p y) (+ y (. p y)))
    (set! (. p z) (+ z (. p z)))
    camera))

(sm/defn camera-set-position :- Camera
  [camera :- Camera x :- s/Num y :- s/Num z :- s/Num]
  (let [p (t/position camera)]
    (set! (. p x) x)
    (set! (. p y) y)
    (set! (. p z) z)
    camera))

(sm/defn camera-set-center :- Camera
  [camera :- Camera bounds :- AABB]
  (let [center (g/aabb-center bounds)
        view-matrix (camera-view-matrix camera)]
    (.transform view-matrix center)
    (set! (. center z) 0)
    (.transform ^Matrix4d (g/invert view-matrix) center)
    (camera-set-position camera (.x center) (.y center) (.z center))))

(sm/defn camera-project :- Point3d
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

(sm/defn camera-unproject :- Vector4d
  [camera :- Camera win-x :- s/Num win-y :- s/Num win-z :- s/Num]
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

(sm/defn viewproj-frustum-planes :- [Vector4d]
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

(n/defnode CameraNode
  (property camera      {:schema Camera})
  (output   camera Camera [this _] (:camera this)))

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

(def ^:private button-interpretation
  {[:one-button 1 SWT/ALT]                   :rotate
   [:one-button 1 (bit-or SWT/ALT SWT/CTRL)] :track
   [:one-button 1 SWT/CTRL]                  :dolly
   [:three-button 1 SWT/ALT]                 :rotate
   [:three-button 2 SWT/ALT]                 :track
   [:three-button 3 SWT/ALT]                 :dolly})

(defn camera-movement
  ([event]
    (camera-movement (ui/mouse-type) (:button event) (:state-mask event)))
  ([mouse-type button mods]
    (button-interpretation [mouse-type button mods] :idle)))


(sm/defn camera-fov-from-aabb
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

(sm/defn camera-ortho-frame-aabb-fn :- Camera
  [camera :- Camera ^AABB aabb :- AABB]
  (assert (= :orthographic (:type camera)))
  (let [fov (camera-fov-from-aabb camera aabb)]
    (fn [old-cam]
      (-> old-cam
        (set-orthographic fov (:aspect camera) (:z-near camera) (:z-far camera))
        (camera-set-center aabb)))))

(n/defnode CameraController
  (input camera [CameraNode])

  (property movement {:schema s/Any :default :idle})

  (output self CameraController [this _] this)

  (on :mouse-down
      (set-property self
                    :last-x (:x event)
                    :last-y (:y event)
                    :movement (camera-movement event)))

  (on :mouse-move
      (when (not (= :idle (:movement self)))
        (let [camera-node (p/node-feeding-into self :camera)
              x (:x event)
              y (:y event)]
          (case (:movement self)
            :dolly  (do (update-property camera-node :camera dolly (* -0.002 (- y (:last-y self)))) (repaint))
            :track  (do (update-property camera-node :camera track (:last-x self) (:last-y self) x y) (repaint))
            nil)
          (set-property self
                        :last-x x
                        :last-y y))))

  (on :mouse-up
      (set-property self
                    :last-x nil
                    :last-y nil
                    :movement :idle))

  (on :mouse-wheel
      (let [camera-node (p/node-feeding-into self :camera)]
        (update-property camera-node :camera dolly (* -0.02 (:count event)))
        (repaint))))
