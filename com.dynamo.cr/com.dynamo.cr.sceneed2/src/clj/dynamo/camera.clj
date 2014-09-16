(ns dynamo.camera
  (:require [schema.macros :as sm]
            [schema.core :as s]
            [plumbing.core :refer [defnk]]
            [dynamo.types :as t]
            [dynamo.node :as n]
            [dynamo.project :as p]
            [internal.cache :refer [caching]]
            [dynamo.geom :as g])
  (:import [javax.vecmath Point3d Quat4d Matrix4d Vector3d Vector4d]
           [dynamo.types Camera Region AABB]))

(defn camera-view-matrix
  [camera]
  (let [pos (Vector3d. (.position camera))
        m   (Matrix4d.)]
    (.setIdentity m)
    (.set m (.rotation camera))
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

(defn camera-projection-matrix
  [camera]
  (case (:type camera)
    :perspective  (camera-perspective-projection-matrix camera)
    :orthographic (camera-orthographic-projection-matrix camera)))

(sm/defn make-camera :- Camera
  ([] (make-camera :perspective))
  ([t :- (s/enum :perspective :orthographic)]
    (let [distance 10000.0
          position (doto (Vector3d.) (.set 0.0 0.0 1.0) (.scale distance))
          rotation (doto (Quat4d.)   (.set 0.0 0.0 0.0 1.0))]
      (t/->Camera t position rotation 1 2000 1 30))))

(sm/defn set-orthographic :- Camera
  [camera :- Camera fov :- s/Num aspect :- s/Num z-near :- s/Num z-far :- s/Num]
  (assoc camera
         :fov fov
         :aspect aspect
         :z-near z-near
         :z-far z-far))

(sm/defn camera-rotate :- Camera
  [camera :- Camera q :- Quat4d]
  (assoc camera :rotation (.mul (.rotation camera) (.normalize (Quat4d. q)))))

(sm/defn camera-move :- Camera
  [camera :- Camera x :- s/Num y :- s/Num z :- s/Num]
  (let [p (.position camera)]
    (set! (. p x) (+ x (. p x)))
    (set! (. p y) (+ y (. p y)))
    (set! (. p z) (+ z (. p z)))
    camera))

(sm/defn camera-set-position :- Camera
  [camera :- Camera x :- s/Num y :- s/Num z :- s/Num]
  (let [p (.position camera)]
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
    (.transform (g/invert view-matrix) center)
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

(sm/defn viewproj-frustum-planes
  [camera :- Camera]
  (let [view-proj   (doto (camera-projection-matrix camera)
                      (.mul (camera-view-matrix camera)))
        persp-vec   (let [x (Vector4d.)] (.getRow view-proj 3 x) x)
        rows        (mapcat #(repeat 2 %) (range 3))
        scales      (take 6 (cycle [-1 1]))]
    (map (fn [row scale]
           (let [temp (Vector4d.)]
             (.getRow view-proj row temp)
             (.scale  temp scale)
             (.add    temp persp-vec)
             temp))
         rows scales)))

(n/defnode CameraNode
  (property camera {:schema Camera})
  (output   camera Camera [this _] (:camera this)))

(defn dolly [camera delta]
  (update-in camera [:camera :fov] #(if % (+ % delta) 0)))

(n/defnode CameraController
  (input camera [CameraNode])
  (output self CameraController [this _] this)

  (on :mouse-wheel
      (let [camera-node (p/resource-feeding-into project-state self :camera)
            delta       (.count event)]
        (update-property camera-node :camera dolly (* -0.02 delta)))))