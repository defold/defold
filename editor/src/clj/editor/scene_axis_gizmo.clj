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

(ns editor.scene-axis-gizmo
  (:require [dynamo.graph :as g]
            [editor.camera :as c]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.pass :as pass]
            [editor.gl.shader :as shader]
            [editor.gl.texture :as texture]
            [editor.gl.vertex2 :as vtx]
            [editor.math :as math]
            [editor.scene-picking :as scene-picking]
            [editor.shaders :as shaders]
            [util.coll :as coll])
  (:import [com.jogamp.opengl GL2]
           [editor.types Camera Region]
           [java.awt BasicStroke Color Font RenderingHints]
           [java.awt.geom Ellipse2D$Double]
           [java.awt.image BufferedImage]
           [javax.vecmath Matrix4d Quat4d Vector3d Vector4d]))

(set! *warn-on-reflection* true)

(def ^:private gizmo-batch-key ::axis-gizmo)
(def ^:private gizmo-margin 21.0)
(def ^:private gizmo-scale 30.0)
(def ^:private gizmo-depth (* gizmo-scale 4.0))

(def ^:private axis-order [:+x :-x :+y :-y :+z :-z])

(def ^:private axis->normal
  {:+x (Vector3d. 1.0 0.0 0.0)
   :-x (Vector3d. -1.0 0.0 0.0)
   :+y (Vector3d. 0.0 1.0 0.0)
   :-y (Vector3d. 0.0 -1.0 0.0)
   :+z (Vector3d. 0.0 0.0 1.0)
   :-z (Vector3d. 0.0 0.0 -1.0)})

(defn- gizmo-model-matrix
  ^Matrix4d [^Camera camera ^Region viewport]
  ;; Screen Y points down. Apply the Y flip after the inverse camera rotation
  ;; so it changes screen orientation without changing the camera's axes.
  (let [x (- (.right viewport) gizmo-margin gizmo-scale)
        y (- (.bottom viewport) gizmo-margin gizmo-scale)
        rotation (doto (Quat4d. ^Quat4d (:rotation camera)) (.conjugate))
        rs ^Matrix4d (math/->mat4-uniform (Vector3d. 0.0 0.0 0.0) rotation gizmo-scale)
        reflect-y ^Matrix4d (doto ^Matrix4d (math/->mat4)
                              (.setElement 1 1 -1.0))
        translation ^Matrix4d (doto ^Matrix4d (math/->mat4)
                                (.setTranslation (Vector3d. x y 0.0)))]
    (doto translation
      (.mul reflect-y)
      (.mul rs))))

(defn- gizmo-projection-matrix
  "Screen-space orthographic projection, restricted to the picking rectangle
  during selection."
  ^Matrix4d [render-args ^Region viewport]
  (let [base ^Matrix4d (c/region-orthographic-projection-matrix viewport (- gizmo-depth) gizmo-depth)]
    (if-let [^Matrix4d picking-matrix (:picking-matrix render-args)]
      ;; Replacing the scene projection discards its picking transform.
      ;; Restore it so the gizmo is picked only beneath the pointer.
      (doto (Matrix4d. picking-matrix) (.mul base))
      base)))

(def ^:private stub-colors
  {0 [0.91 0.27 0.235] ; X
   1 [0.357 0.761 0.212] ; Y
   2 [0.235 0.482 0.91]}) ; Z

(def ^:private line-half-width 0.04)
(def ^:private ball-distance 1.1)
(def ^:private ball-radius 0.3)

(defn- axis-index
  ^long [axis]
  (let [^Vector3d n (axis->normal axis)]
    (cond (not (zero? (.x n))) 0
          (not (zero? (.y n))) 1
          :else 2)))

(defn- positive-axis? [axis]
  (contains? #{:+x :+y :+z} axis))

(defn- axis-color [axis]
  (let [[r g b :as rgb] (stub-colors (axis-index axis))
        ;; Preserve the weighted brightness when desaturating.
        gray (+ (* 0.299 (double r)) (* 0.587 (double g)) (* 0.114 (double b)))
        desaturated (mapv #(+ gray (* 0.75 (- (double %) gray))) rgb)]
    (cond->> desaturated
      (not (positive-axis? axis)) (mapv #(* 0.55 (double %))))))

(defn- line-quads
  "Picking box from the center to the ball on a positive axis."
  [axis]
  (let [i (axis-index axis)
        [u v] (into [] (remove #{i}) [0 1 2])
        point (fn [^double along ^double du ^double dv]
                (-> [0.0 0.0 0.0]
                    (assoc i along)
                    (assoc u (* du ^double line-half-width))
                    (assoc v (* dv ^double line-half-width))))
        corners [[-1.0 -1.0] [1.0 -1.0] [1.0 1.0] [-1.0 1.0]]
        side (fn [[du0 dv0] [du1 dv1]]
               [(point 0.0 du0 dv0) (point ball-distance du0 dv0) (point ball-distance du1 dv1) (point 0.0 du1 dv1)])]
    (mapv side corners (conj (subvec corners 1) (first corners)))))

(defn- sphere-quads [center ^double radius]
  (let [n 12
        point (fn [^long lat ^long lon]
                (let [theta (* Math/PI (/ (double lat) n))
                      phi (* 2.0 Math/PI (/ (double lon) n))
                      r (* radius (Math/sin theta))]
                  (mapv + center [(* r (Math/cos phi)) (* radius (Math/cos theta)) (* r (Math/sin phi))])))]
    (into []
          (mapcat (fn [^long lat]
                    (mapv (fn [^long lon]
                            [(point lat lon) (point lat (inc lon)) (point (inc lat) (inc lon)) (point (inc lat) lon)])
                          (range n))))
          (range n))))

(def ^:private pick-shader shaders/uniform-color-local-space)

(defn- make-pick-vertex-buffer [quads]
  (let [vertex-buffer (vtx/make-vertex-buffer (shaders/vertex-description pick-shader) :static (* 6 (count quads)))
        byte-buffer (vtx/buf vertex-buffer)
        float-buffer (.asFloatBuffer byte-buffer)]
    (doseq [positions quads
            i [0 1 2 2 3 0]]
      (let [[x y z] (positions i)]
        (doto float-buffer
          (.put (float x)) (.put (float y)) (.put (float z)))))
    (.position byte-buffer (* (.position float-buffer) Float/BYTES))
    (vtx/flip! vertex-buffer)))

(def ^:private backdrop-radius (+ ^double ball-distance ^double ball-radius 0.1))

(def ^:private pick-vertex-buffers
  (delay (assoc (into {}
                      (map (fn [axis]
                             (let [^Vector3d n (axis->normal axis)
                                   center (mapv #(* ^double ball-distance ^double %) [(.x n) (.y n) (.z n)])]
                               [axis (make-pick-vertex-buffer
                                       (cond-> (sphere-quads center ball-radius)
                                         (positive-axis? axis) (into (line-quads axis))))])))
                      axis-order)
           :backdrop (make-pick-vertex-buffer (sphere-quads [0.0 0.0 0.0] backdrop-radius)))))

;; Antialiased textures keep handle edges smooth without multisampling.

(def ^:private ball-outline-width 6.0)

(defn- ->awt-color
  (^Color [rgb] (->awt-color rgb 1.0))
  (^Color [[r g b] alpha] (Color. (float r) (float g) (float b) (float alpha))))

(defn- make-ball-image
  ^BufferedImage [axis]
  (let [size 64
        image (BufferedImage. size size BufferedImage/TYPE_INT_ARGB)
        gfx (.createGraphics image)
        ^String letter ({:+x "X" :+y "Y" :+z "Z"} axis)]
    (doto gfx
      (.setRenderingHint RenderingHints/KEY_ANTIALIASING RenderingHints/VALUE_ANTIALIAS_ON)
      (.setRenderingHint RenderingHints/KEY_TEXT_ANTIALIASING RenderingHints/VALUE_TEXT_ANTIALIAS_ON)
      (.setColor (->awt-color (axis-color axis) (if letter 1.0 0.3)))
      (.fillOval 1 1 (- size 2) (- size 2)))
    (let [inset (+ 1.0 (* 0.5 ^double ball-outline-width))
          diameter (- size (* 2.0 inset))]
      (doto gfx
        (.setColor (->awt-color (axis-color axis)))
        (.setStroke (BasicStroke. (float ball-outline-width)))
        (.draw (Ellipse2D$Double. inset inset diameter diameter))))
    (when letter
      (doto gfx
        (.setColor Color/BLACK)
        (.setFont (Font. Font/SANS_SERIF Font/BOLD 44)))
      (let [metrics (.getFontMetrics gfx)
            x (quot (- size (.stringWidth metrics letter)) 2)
            y (+ (quot (- size (.getHeight metrics)) 2) (.getAscent metrics))]
        (.drawString gfx letter (int x) (int y))))
    (.dispose gfx)
    image))

(defn- make-line-image
  ^BufferedImage [axis]
  (let [width 16
        height 4
        image (BufferedImage. width height BufferedImage/TYPE_INT_ARGB)
        rgb (bit-and (.getRGB (->awt-color (axis-color axis))) 0xffffff)]
    (doseq [x (range width)
            y (range height)
            :let [edge (min (+ x 0.5) (- width x 0.5))
                  alpha (min 1.0 (/ edge 4.0))]]
      (.setRGB image x y (unchecked-int (bit-or (bit-shift-left (long (* 255.0 alpha)) 24) rgb))))
    image))

(def ^:private ball-textures
  (delay
    (into {}
          (map (fn [axis] [axis (texture/image-texture [::ball axis] (make-ball-image axis))]))
          axis-order)))

(def ^:private line-textures
  (delay
    (into {}
          (map (fn [axis] [axis (texture/image-texture [::line axis] (make-line-image axis))]))
          [:+x :+y :+z])))

(defn- make-backdrop-image
  ^BufferedImage []
  (let [size 128
        image (BufferedImage. size size BufferedImage/TYPE_INT_ARGB)
        gfx (.createGraphics image)]
    (doto gfx
      (.setRenderingHint RenderingHints/KEY_ANTIALIASING RenderingHints/VALUE_ANTIALIAS_ON)
      (.setColor (Color. 0.3 0.3 0.3 0.20))
      (.fillOval 1 1 (- size 2) (- size 2))
      (.dispose))
    image))

(def ^:private backdrop-texture
  (delay (texture/image-texture [::backdrop] (make-backdrop-image))))

(def ^:private billboard-shader shaders/basic-texture-tint-local-space)

(def ^:private billboard-quad
  "Shared quad spanning -1 to 1 in the local XY plane."
  (delay
    (let [vertex-buffer (vtx/make-vertex-buffer (shaders/vertex-description billboard-shader) :static 6)
          byte-buffer (vtx/buf vertex-buffer)
          float-buffer (.asFloatBuffer byte-buffer)]
      (doseq [[^double x ^double y] [[-1.0 -1.0] [1.0 -1.0] [1.0 1.0] [1.0 1.0] [-1.0 1.0] [-1.0 -1.0]]]
        (doto float-buffer
          (.put (float x)) (.put (float y)) (.put (float 0.0))
          (.put (float (* 0.5 (+ 1.0 x)))) (.put (float (* 0.5 (+ 1.0 y))))))
      (.position byte-buffer (* (.position float-buffer) Float/BYTES))
      (vtx/flip! vertex-buffer))))

(defn- billboard-matrix
  "Places the billboard quad at `center`, spanning `half-u` along `u` and
  `half-v` along `v`, facing `toward`."
  ^Matrix4d [^Vector3d center ^Vector3d u ^Vector3d v ^Vector3d toward half-u half-v]
  (let [half-u (double half-u)
        half-v (double half-v)]
    (doto (Matrix4d.)
      (.setColumn 0 (* (.x u) half-u) (* (.y u) half-u) (* (.z u) half-u) 0.0)
      (.setColumn 1 (* (.x v) half-v) (* (.y v) half-v) (* (.z v) half-v) 0.0)
      (.setColumn 2 (.x toward) (.y toward) (.z toward) 0.0)
      (.setColumn 3 (.x center) (.y center) (.z center) 1.0))))

(defn- handle-billboards
  "Returns textured quads in back-to-front draw order, with the backdrop first."
  [^Quat4d camera-rotation ^double backdrop-alpha hot-handle]
  ;; Cancel the model's inverse camera rotation to keep each quad facing the screen.
  (let [screen-axis (fn [x y z] (math/rotate camera-rotation (Vector3d. x y z)))
        ^Vector3d right (screen-axis 1.0 0.0 0.0)
        ^Vector3d up (screen-axis 0.0 1.0 0.0)
        ^Vector3d toward (screen-axis 0.0 0.0 1.0)
        balls (into []
                    (map (fn [axis]
                           (let [center (doto (Vector3d. ^Vector3d (axis->normal axis)) (.scale ball-distance))]
                             {:depth (.dot toward center)
                              :texture (@ball-textures axis)
                              :matrix (billboard-matrix center right up toward ball-radius ball-radius)
                              :alpha 1.0
                              :brightness (if (= hot-handle axis) 1.25 1.0)})))
                    axis-order)
        lines (into []
                    (keep (fn [axis]
                            (let [normal ^Vector3d (axis->normal axis)
                                  side (doto (Vector3d.) (.cross toward normal))
                                  ;; Trim in screen space so lines cannot show through
                                  ;; translucent balls when opposite axes overlap.
                                  screen-length (* (.length side) ^double ball-distance)
                                  length (* ^double ball-distance (- 1.0 (/ ^double ball-radius (max screen-length 1e-6))))]
                              (when (pos? length)
                                (let [center (doto (Vector3d. normal) (.scale (* 0.5 length)))]
                                  {:depth (.dot toward center)
                                   :texture (@line-textures axis)
                                   :matrix (billboard-matrix center (doto side (.normalize)) normal toward
                                                             (* 2.0 ^double line-half-width) (* 0.5 length))
                                   :alpha 1.0
                                   :brightness (if (= hot-handle axis) 1.25 1.0)})))))
                    [:+x :+y :+z])]
    (cond->> (sort-by :depth (into balls lines))
      (pos? backdrop-alpha) (cons {:texture @backdrop-texture
                                   :matrix (billboard-matrix (Vector3d.) right up toward backdrop-radius backdrop-radius)
                                   :alpha backdrop-alpha
                                   :brightness 1.0}))))

(defn- draw-handles!
  [^GL2 gl render-args ^Camera camera backdrop-alpha hot-handle]
  ;; The gizmo must remain visible over scene geometry.
  (.glDisable gl GL2/GL_DEPTH_TEST)
  (.glBlendFunc gl GL2/GL_ONE GL2/GL_ONE_MINUS_SRC_ALPHA)
  (doseq [{:keys [texture matrix alpha brightness]} (handle-billboards (:rotation camera) backdrop-alpha hot-handle)
          :let [vertex-binding (vtx/use-with [::billboard-quad] @billboard-quad billboard-shader)
                handle-args (assoc render-args :world-view-proj (doto (Matrix4d. ^Matrix4d (:world-view-proj render-args)) (.mul ^Matrix4d matrix)))
                alpha (double alpha)
                ;; Textures use premultiplied alpha, so the tint's RGB must include alpha too.
                color (* alpha (double brightness))]]
    (gl/with-gl-bindings gl handle-args [billboard-shader vertex-binding texture]
      (shader/set-samplers-by-index billboard-shader gl 0 (:texture-units texture))
      (shader/set-uniform billboard-shader gl "tint" (Vector4d. color color color alpha))
      (gl/gl-draw-arrays gl GL2/GL_TRIANGLES 0 6)))
  ;; Restore the blend function expected by the other scene renderers.
  (.glBlendFunc gl GL2/GL_SRC_ALPHA GL2/GL_ONE_MINUS_SRC_ALPHA))

(defn- pick-handles!
  [^GL2 gl render-args renderable-by-selection-data]
  (let [vertex-buffers @pick-vertex-buffers
        pick! (fn [selection-data]
                (let [vertex-buffer (vertex-buffers selection-data)
                      id-color (scene-picking/renderable-picking-id-uniform (renderable-by-selection-data selection-data))
                      vertex-binding (vtx/use-with [::pick selection-data] vertex-buffer pick-shader)]
                  (gl/with-gl-bindings gl render-args [pick-shader vertex-binding]
                    (shader/set-uniform pick-shader gl "color" id-color)
                    (gl/gl-draw-arrays gl GL2/GL_TRIANGLES 0 (count vertex-buffer)))))]
    (.glEnable gl GL2/GL_DEPTH_TEST)
    ;; The enclosing backdrop must not occlude the axis handles during picking.
    (.glDepthMask gl false)
    (when (renderable-by-selection-data :backdrop)
      (pick! :backdrop))
    (.glDepthMask gl true)
    (doseq [axis axis-order
            :when (renderable-by-selection-data axis)]
      (pick! axis))
    (.glDepthMask gl false)
    (.glDisable gl GL2/GL_DEPTH_TEST)))

(defn- render-axis-gizmo [^GL2 gl render-args renderables _rcount]
  (let [camera ^Camera (:camera render-args)
        viewport (:viewport render-args)
        gizmo-args (merge render-args
                          (math/derive-render-transforms (gizmo-model-matrix camera viewport)
                                                         geom/Identity4d
                                                         (gizmo-projection-matrix render-args viewport)
                                                         (:texture render-args)))]
    (if (= pass/manipulator-selection (:pass render-args))
      (pick-handles! gl gizmo-args (into {} (map (juxt :selection-data identity)) renderables))
      (let [{:keys [backdrop-alpha hot-handle]} (:user-data (first renderables))]
        (draw-handles! gl gizmo-args camera (double (or backdrop-alpha 0.0)) hot-handle)))))

(g/defnk produce-renderables [_node-id backdrop-alpha camera hot-handle]
  (if (c/mode-2d? camera)
    {}
    ;; Keep the backdrop pickable while invisible so hovering can reveal it.
    (let [renderables (coll/into-> (conj axis-order :backdrop) []
                        (map (fn [selection-data]
                               {:batch-key gizmo-batch-key
                                :node-id _node-id
                                :passes [pass/manipulator pass/manipulator-selection]
                                :render-fn render-axis-gizmo
                                :select-batch-key gizmo-batch-key
                                :selection-data selection-data
                                :tags #{:axis-gizmo}
                                :user-data {:backdrop-alpha backdrop-alpha
                                            :hot-handle hot-handle}})))]
      {pass/manipulator renderables
       pass/manipulator-selection renderables})))

(defn- frame-to-axis! [camera-node-id current-camera axis]
  (let [axis-forward (doto (Vector3d. ^Vector3d (axis->normal axis)) (.negate))
        ;; Clicking an already aligned axis switches to its opposite view.
        target-axis (if (>= (.dot ^Vector3d (c/camera-forward-vector current-camera) axis-forward)
                            0.999)
                      ({:+x :-x :-x :+x :+y :-y :-y :+y :+z :-z :-z :+z} axis)
                      axis)]
    (c/frame-camera-to-axis! camera-node-id current-camera target-axis true)))

(defn handle-input [self _input-state action selection-data]
  (let [hits (get selection-data self)
        handle (or (coll/first-where #(not= :backdrop %) hits) (first hits))
        press (g/user-data self ::press)
        {:keys [x y]} action]
    (case (:type action)
      :mouse-pressed (if-not (and (= :primary (:button action))
                                  handle)
                       action
                       (do
                         (g/user-data! self ::press {:handle handle :x x :y y})
                         nil))
      :mouse-moved (if press
                     (do
                       (when (> (Math/hypot (- (double x) (double (:x press)))
                                            (- (double y) (double (:y press))))
                                4.0)
                         (g/let-ec [camera-node-id (g/node-value self :camera-node-id evaluation-context)
                                    animating (g/node-value camera-node-id :animating evaluation-context)]
                           (when-not animating
                             (c/cancel-dolly! camera-node-id)
                             ;; Hand subsequent drag events to the camera controller.
                             (c/start-tumble! camera-node-id x y)
                             (g/user-data! self ::press nil)
                             (g/user-data! self ::dragging true)
                             (g/transact {:undoable false} (g/set-property self :hot-handle nil)))))
                       nil)
                     (g/let-ec [hot-handle (g/node-value self :hot-handle evaluation-context)]
                       (when (not= handle hot-handle)
                         (g/transact {:undoable false} (g/set-property self :hot-handle handle)))
                       action))
      :mouse-released (if-not press
                        action
                        (do
                          (g/user-data! self ::press nil)
                          (when-not (= :backdrop (:handle press))
                            (g/let-ec [camera-node-id (g/node-value self :camera-node-id evaluation-context)
                                       animating (g/node-value camera-node-id :animating evaluation-context)
                                       local-camera (g/node-value camera-node-id :local-camera evaluation-context)]
                              (when-not animating
                                (frame-to-axis! camera-node-id local-camera (:handle press)))))
                          nil))
      :mouse-exited (g/let-ec [hot-handle (g/node-value self :hot-handle evaluation-context)]
                      (when hot-handle
                        (g/transact {:undoable false} (g/set-property self :hot-handle nil)))
                      action)
      action)))

(defn- handle-update-tick
  [self input-state dt]
  (g/let-ec [alpha (double (g/node-value self :backdrop-alpha evaluation-context))
             hot-handle (g/node-value self :hot-handle evaluation-context)
             camera-node-id (g/node-value self :camera-node-id evaluation-context)]
    ;; The camera handles drag release, so detect its completion from camera state.
    (when (and (g/user-data self ::dragging)
               (not= :tumble (:movement (g/user-data camera-node-id ::c/camera-state))))
      (g/user-data! self ::dragging false))
    (let [target (if (or hot-handle
                         (g/user-data self ::press)
                         (g/user-data self ::dragging))
                   1.0
                   0.0)
          step (/ (double dt) 0.15)
          next-alpha (if (< alpha target)
                       (min target (+ alpha step))
                       (max target (- alpha step)))]
      (when (not= next-alpha alpha)
        (g/transact
          {:undoable false}
          (g/set-property self :backdrop-alpha next-alpha)))
      input-state)))

(g/defnode AxisGizmoController
  (property hot-handle g/Keyword)
  (property backdrop-alpha g/Num (default 0.0))

  (input camera-node-id g/NodeID)
  (input camera Camera)

  (output input-handler Runnable :cached (g/constantly handle-input))
  (output update-tick-handler Runnable :cached (g/constantly handle-update-tick))
  (output renderables pass/RenderData :cached produce-renderables))
