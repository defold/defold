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

(ns editor.scene-view-cube
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
           [editor.types AABB Camera Region]
           [java.awt BasicStroke Color Font RenderingHints]
           [java.awt.geom Ellipse2D$Double]
           [java.awt.image BufferedImage]
           [javax.vecmath Matrix4d Quat4d Vector3d]))

(set! *warn-on-reflection* true)

(def ^:private cube-batch-key ::view-cube)
(def ^:private cube-margin 36.0)
(def ^:private cube-scale 30.0)
(def ^:private cube-depth (* cube-scale 4.0))

(def ^:private face-order [:+x :-x :+y :-y :+z :-z])

(def ^:private axis->normal
  {:+x (Vector3d. 1.0 0.0 0.0)
   :-x (Vector3d. -1.0 0.0 0.0)
   :+y (Vector3d. 0.0 1.0 0.0)
   :-y (Vector3d. 0.0 -1.0 0.0)
   :+z (Vector3d. 0.0 0.0 1.0)
   :-z (Vector3d. 0.0 0.0 -1.0)})

(def ^:private opposite-face
  {:+x :-x
   :-x :+x
   :+y :-y
   :-y :+y
   :+z :-z
   :-z :+z})

(def ^:private axis-alignment-epsilon
  "Minimum dot product between the current and requested camera forward vector
  that's considered to be facing that axis. Keeps the toggle-on-re-click logic
  robust against tiny numerical drift that would otherwise require the user to
  click twice to flip to the opposite face."
  0.999)

(defn- cube-center [^Region viewport]
  [(+ (.left viewport) cube-margin cube-scale)
   (- (.bottom viewport) cube-margin cube-scale)])

(defn- cube-model-matrix
  ^Matrix4d [^Camera camera ^Region viewport]
  ;; The overlay ortho projection maps pixel-space Y downward, so GL +Y ends up
  ;; pointing to the bottom of the screen. To render the gizmo upright we build
  ;; the model matrix as T * reflectY * R * S so that it rotates in a standard
  ;; right-handed space (using the inverse camera rotation) and is then flipped
  ;; to match screen-space Y just before translation. Composing the Y flip this
  ;; way avoids the gimbal-lock issues of trying to mirror individual Euler
  ;; components of the camera rotation.
  (let [[x y] (cube-center viewport)
        rotation (doto (Quat4d. ^Quat4d (:rotation camera)) (.conjugate))
        rs ^Matrix4d (math/->mat4-uniform (Vector3d. 0.0 0.0 0.0) rotation cube-scale)
        reflect-y ^Matrix4d (doto ^Matrix4d (math/->mat4)
                              (.setElement 1 1 -1.0))
        translation ^Matrix4d (doto ^Matrix4d (math/->mat4)
                                (.setTranslation (Vector3d. x y 0.0)))]
    (doto translation
      (.mul reflect-y)
      (.mul rs))))

(defn- cube-projection-matrix
  "Screen-space ortho projection for the gizmo.

  In picking passes, render-args contains a :picking-matrix that was used to
  zoom the scene's projection into the tool picking rect. Since the gizmo
  completely replaces the projection with its own, we re-apply the
  picking-matrix on top; otherwise the gizmo ends up drawn across the whole
  picking buffer and every click in the scene view is intercepted by it."
  ^Matrix4d [render-args ^Region viewport]
  (let [base ^Matrix4d (c/region-orthographic-projection-matrix viewport (- cube-depth) cube-depth)]
    (if-let [^Matrix4d picking-matrix (:picking-matrix render-args)]
      (doto (Matrix4d. picking-matrix) (.mul base))
      base)))

(defn- cube-render-args
  "Replaces the render-args transforms with the gizmo's own model and
  projection matrices."
  [render-args ^Camera camera ^Region viewport]
  (merge render-args
         (math/derive-render-transforms (cube-model-matrix camera viewport)
                                        geom/Identity4d
                                        (cube-projection-matrix render-args viewport)
                                        (:texture render-args))))

(defn- camera-facing-axis?
  "Returns true when the camera is (almost exactly) looking down `axis` at the
  scene, i.e. the clicked axis is the one already centered in the viewport."
  [^Camera camera axis]
  (let [target-forward (doto (Vector3d. ^Vector3d (axis->normal axis))
                         (.negate))
        current-forward ^Vector3d (c/camera-forward-vector camera)]
    (>= (.dot current-forward target-forward) axis-alignment-epsilon)))

;; Blender-style axis handles: a line from the center to a ball on each positive
;; axis, and a darker ball without a line on each negative axis.

(def ^:private stub-colors
  {0 [0.91 0.27 0.235] ; X, red.
   1 [0.357 0.761 0.212] ; Y, green.
   2 [0.235 0.482 0.91]}) ; Z, blue.

(def ^:private line-half-width 0.04)
(def ^:private ball-distance 1.1)
(def ^:private ball-radius 0.3)
(def ^:private ball-segments 12)

(def ^:private negative-stub
  "Brightness factor for the balls on negative axes."
  {:brightness 0.55})

(def ^:private color-saturation
  "How much of the stub colors' saturation is kept; the rest blends to gray."
  0.75)

(defn- axis-index ^long [axis]
  (let [^Vector3d n (axis->normal axis)]
    (cond (not (zero? (.x n))) 0
          (not (zero? (.y n))) 1
          :else 2)))

(defn- positive-axis? [axis]
  (contains? #{:+x :+y :+z} axis))

(defn- desaturate [rgb]
  (let [[r g b] rgb
        gray (+ (* 0.299 (double r)) (* 0.587 (double g)) (* 0.114 (double b)))]
    (mapv #(+ gray (* ^double color-saturation (- (double %) gray))) rgb)))

(defn- axis-color [axis]
  (cond->> (desaturate (stub-colors (axis-index axis)))
    (not (positive-axis? axis)) (mapv #(* (double %) (double (:brightness negative-stub))))))

;; Picking geometry: a sphere per ball, a box per line, and a sphere around
;; everything for the backdrop.

(defn- line-quads
  "Box from the center out to the ball along `axis`."
  [axis]
  (let [i (axis-index axis)
        [u v] (remove #{i} [0 1 2])
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
  (let [n (long ball-segments)
        point (fn [^long lat ^long lon]
                (let [theta (* Math/PI (/ (double lat) n))
                      phi (* 2.0 Math/PI (/ (double lon) n))
                      r (* radius (Math/sin theta))]
                  (mapv + center [(* r (Math/cos phi)) (* radius (Math/cos theta)) (* r (Math/sin phi))])))]
    (vec (for [lat (range n)
               lon (range n)]
           [(point lat lon) (point lat (inc lon)) (point (inc lat) (inc lon)) (point (inc lat) lon)]))))

(defn- handle-pick-quads [axis]
  (let [^Vector3d n (axis->normal axis)
        center (mapv #(* ^double ball-distance ^double %) [(.x n) (.y n) (.z n)])]
    (cond-> (sphere-quads center ball-radius)
      (positive-axis? axis) (into (line-quads axis)))))

(def ^:private pick-shader shaders/selection-color-local-space)

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

(defn- buffer-key
  "Tells static buffers apart in the GPU cache, so a reload that rebuilds them
  isn't masked by the old upload."
  [vertex-buffer]
  (System/identityHashCode vertex-buffer))

(def ^:private backdrop-radius (+ ^double ball-distance ^double ball-radius 0.1))

(def ^:private pick-vertex-buffers
  (delay (assoc (into {} (map (juxt identity (comp make-pick-vertex-buffer handle-pick-quads))) face-order)
           :backdrop (make-pick-vertex-buffer (sphere-quads [0.0 0.0 0.0] backdrop-radius)))))

;; The scene view has no multisampling, so the visible handles are drawn as
;; screen-facing quads with anti-aliased textures.

(def ^:private axis-letters {:+x "X" :+y "Y" :+z "Z"})
(def ^:private letter-color Color/BLACK)

(def ^:private ball-fill-alpha 0.3)
(def ^:private ball-outline-width 6.0)

(defn- ->awt-color
  (^Color [rgb] (->awt-color rgb 1.0))
  (^Color [[r g b] alpha] (Color. (float r) (float g) (float b) (float alpha))))

(defn- make-ball-image
  "Circle in the axis color with the axis letter on positive axes. Negative
  axes get a see-through fill inside an opaque outline."
  ^BufferedImage [axis]
  (let [size 64
        image (BufferedImage. size size BufferedImage/TYPE_INT_ARGB)
        gfx (.createGraphics image)
        ^String letter (axis-letters axis)]
    (doto gfx
      (.setRenderingHint RenderingHints/KEY_ANTIALIASING RenderingHints/VALUE_ANTIALIAS_ON)
      (.setRenderingHint RenderingHints/KEY_TEXT_ANTIALIASING RenderingHints/VALUE_TEXT_ANTIALIAS_ON)
      (.setColor (->awt-color (axis-color axis) (if letter 1.0 ball-fill-alpha)))
      (.fillOval 1 1 (- size 2) (- size 2)))
    (let [inset (+ 1.0 (* 0.5 ^double ball-outline-width))
          diameter (- size (* 2.0 inset))]
      (doto gfx
        (.setColor (->awt-color (axis-color axis)))
        (.setStroke (BasicStroke. (float ball-outline-width)))
        (.draw (Ellipse2D$Double. inset inset diameter diameter))))
    (when letter
      (doto gfx
        (.setColor letter-color)
        (.setFont (Font. Font/SANS_SERIF Font/BOLD 44)))
      (let [metrics (.getFontMetrics gfx)
            x (quot (- size (.stringWidth metrics letter)) 2)
            y (+ (quot (- size (.getHeight metrics)) 2) (.getAscent metrics))]
        (.drawString gfx letter (int x) (int y))))
    (.dispose gfx)
    image))

(defn- make-line-image
  "Strip in the axis color whose alpha fades out at both side edges."
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
  (into {}
        (map (fn [axis] [axis (texture/image-texture [::ball axis] (make-ball-image axis))]))
        face-order))

(def ^:private line-textures
  (into {}
        (map (fn [axis] [axis (texture/image-texture [::line axis] (make-line-image axis))]))
        [:+x :+y :+z]))

(def ^:private line-quad-half-width
  "Wider than the picking line, since a quarter of the texture on each side is
  the fade."
  (* 2.0 ^double line-half-width))

;; Godot-style backdrop: a sphere around the handles that can be grabbed to
;; orbit, shown as a faint disc while the gizmo is hovered or dragged.

(def ^:private backdrop-color (Color. 0.3 0.3 0.3 0.20))

(defn- make-backdrop-image
  ^BufferedImage []
  (let [size 128
        image (BufferedImage. size size BufferedImage/TYPE_INT_ARGB)
        gfx (.createGraphics image)]
    (doto gfx
      (.setRenderingHint RenderingHints/KEY_ANTIALIASING RenderingHints/VALUE_ANTIALIAS_ON)
      (.setColor backdrop-color)
      (.fillOval 1 1 (- size 2) (- size 2))
      (.dispose))
    image))

(def ^:private backdrop-texture
  (texture/image-texture [::backdrop] (make-backdrop-image)))

(def ^:private billboard-shader shaders/basic-texture-color-local-space)

(defn- make-billboard-vertex-buffer
  "Quad centered on `center`, spanning `half-u` along `u` and `half-v` along `v`,
  faded by `alpha`."
  [^Vector3d center ^Vector3d u ^Vector3d v half-u half-v alpha]
  (let [alpha (float alpha)
        half-u (double half-u)
        half-v (double half-v)
        corner (fn [^double du ^double dv]
                 (let [p (Vector3d. center)]
                   (.scaleAdd p (* du half-u) u p)
                   (.scaleAdd p (* dv half-v) v p)
                   p))
        vertex-buffer (vtx/make-vertex-buffer (shaders/vertex-description billboard-shader) :stream 6)
        byte-buffer (vtx/buf vertex-buffer)
        float-buffer (.asFloatBuffer byte-buffer)]
    (doseq [[du dv] [[-1.0 -1.0] [1.0 -1.0] [1.0 1.0] [1.0 1.0] [-1.0 1.0] [-1.0 -1.0]]
            :let [^Vector3d p (corner du dv)]]
      (doto float-buffer
        (.put (float (.x p))) (.put (float (.y p))) (.put (float (.z p)))
        (.put (float (* 0.5 (+ 1.0 ^double du)))) (.put (float (* 0.5 (+ 1.0 ^double dv))))
        (.put (float 1.0)) (.put (float 1.0)) (.put (float 1.0)) (.put alpha)))
    (.position byte-buffer (* (.position float-buffer) Float/BYTES))
    (vtx/flip! vertex-buffer)))

(defn- handle-billboards
  "Balls and lines as {:key :depth :texture :vertex-buffer} maps, back to
  front, after the backdrop while it's faded in. The camera rotation maps
  screen axes into the gizmo's model space."
  [^Quat4d camera-rotation ^double backdrop-alpha]
  (let [screen-axis (fn [x y z] (math/rotate camera-rotation (Vector3d. x y z)))
        ^Vector3d right (screen-axis 1.0 0.0 0.0)
        ^Vector3d up (screen-axis 0.0 1.0 0.0)
        ^Vector3d toward (screen-axis 0.0 0.0 1.0)
        balls (for [axis face-order
                    :let [center (doto (Vector3d. ^Vector3d (axis->normal axis)) (.scale ball-distance))]]
                {:key [::ball axis]
                 :depth (.dot toward center)
                 :texture (ball-textures axis)
                 :vertex-buffer (make-billboard-vertex-buffer center right up ball-radius ball-radius 1.0)})
        lines (for [axis [:+x :+y :+z]
                    :let [normal ^Vector3d (axis->normal axis)
                          side (doto (Vector3d.) (.cross toward normal))
                          ;; The negative balls are see-through, so stop the
                          ;; line at the ball's outline as seen on screen.
                          screen-length (* (.length side) ^double ball-distance)
                          length (* ^double ball-distance (- 1.0 (/ ^double ball-radius (max screen-length 1e-6))))]
                    :when (pos? length)
                    :let [center (doto (Vector3d. normal) (.scale (* 0.5 length)))]]
                {:key [::line axis]
                 :depth (.dot toward center)
                 :texture (line-textures axis)
                 :vertex-buffer (make-billboard-vertex-buffer center (doto side (.normalize)) normal
                                                              line-quad-half-width (* 0.5 length) 1.0)})]
    (cond->> (sort-by :depth (concat balls lines))
      (pos? backdrop-alpha) (cons {:key [::backdrop]
                                   :texture backdrop-texture
                                   :vertex-buffer (make-billboard-vertex-buffer (Vector3d.) right up backdrop-radius backdrop-radius backdrop-alpha)}))))

(defn- draw-handles!
  "Draws the handles back to front, blending their premultiplied textures.
  Without a depth test they always draw over the scene."
  [^GL2 gl render-args ^Camera camera backdrop-alpha]
  (.glDisable gl GL2/GL_DEPTH_TEST)
  (.glBlendFunc gl GL2/GL_ONE GL2/GL_ONE_MINUS_SRC_ALPHA)
  (doseq [{:keys [key texture vertex-buffer]} (handle-billboards (:rotation camera) backdrop-alpha)
          :let [vertex-binding (vtx/use-with key vertex-buffer billboard-shader)]]
    (gl/with-gl-bindings gl render-args [billboard-shader vertex-binding texture]
      (shader/set-samplers-by-index billboard-shader gl 0 (:texture-units texture))
      (gl/gl-draw-arrays gl GL2/GL_TRIANGLES 0 6)))
  (.glBlendFunc gl GL2/GL_SRC_ALPHA GL2/GL_ONE_MINUS_SRC_ALPHA))

(defn- pick-handles!
  "Draws the picking geometry. The backdrop skips the depth buffer so the
  handles draw over it."
  [^GL2 gl render-args renderable-by-selection-data]
  (let [vertex-buffers @pick-vertex-buffers
        pick! (fn [selection-data]
                (let [vertex-buffer (vertex-buffers selection-data)
                      args (assoc render-args :id-color (scene-picking/renderable-picking-id-uniform (renderable-by-selection-data selection-data)))
                      vertex-binding (vtx/use-with [::pick selection-data (buffer-key vertex-buffer)] vertex-buffer pick-shader)]
                  (gl/with-gl-bindings gl args [pick-shader vertex-binding]
                    (gl/gl-draw-arrays gl GL2/GL_TRIANGLES 0 (count vertex-buffer)))))]
    (.glEnable gl GL2/GL_DEPTH_TEST)
    (.glDepthMask gl false)
    (when (renderable-by-selection-data :backdrop)
      (pick! :backdrop))
    (.glDepthMask gl true)
    (doseq [axis face-order
            :when (renderable-by-selection-data axis)]
      (pick! axis))
    (.glDepthMask gl false)
    (.glDisable gl GL2/GL_DEPTH_TEST)))

(defn- render-view-cube [^GL2 gl render-args renderables _rcount]
  (let [camera ^Camera (:camera render-args)
        cube-args (cube-render-args render-args camera (:viewport render-args))]
    (if (= pass/manipulator-selection (:pass render-args))
      (pick-handles! gl cube-args (into {} (map (juxt :selection-data identity)) renderables))
      (draw-handles! gl cube-args camera (double (get-in (first renderables) [:user-data :backdrop-alpha] 0.0))))))

(g/defnk produce-renderables [_node-id backdrop-alpha]
  (let [renderables (coll/into-> (conj face-order :backdrop) []
                      (map (fn [selection-data]
                             {:batch-key cube-batch-key
                              :node-id _node-id
                              :passes [pass/manipulator pass/manipulator-selection]
                              :render-fn render-view-cube
                              :select-batch-key cube-batch-key
                              :selection-data selection-data
                              :tags #{:view-cube}
                              :user-data {:backdrop-alpha backdrop-alpha}})))]
    {pass/manipulator renderables
     pass/manipulator-selection renderables}))

(def ^:private drag-threshold
  "Pixels the cursor must move after pressing on the gizmo before it's a drag."
  4.0)

(defn- frame-to-face! [self face]
  (g/with-auto-evaluation-context evaluation-context
    (let [camera-node-id (g/node-value self :camera-node-id evaluation-context)
          viewport (g/node-value self :viewport evaluation-context)
          scene-aabb (g/node-value self :scene-aabb evaluation-context)
          current-camera (g/node-value camera-node-id :local-camera evaluation-context)
          ;; If the camera is already aligned with the clicked axis, flip to
          ;; the opposite axis so re-clicking toggles between +/- views.
          target-face (if (camera-facing-axis? current-camera face)
                        (opposite-face face)
                        face)]
      (when-not (geom/predefined-aabb? scene-aabb)
        (c/frame-camera-to-axis! camera-node-id viewport scene-aabb target-face true)))))

(defn- tumble-camera! [self ^double dx ^double dy]
  (let [camera-node-id (g/node-value self :camera-node-id)
        camera (g/node-value camera-node-id :local-camera)]
    (g/transact
      {:undoable false}
      (g/set-property camera-node-id :local-camera (c/tumble camera dx dy)))))

(defn handle-input [self _input-state action selection-data]
  (let [hits (get selection-data self)
        ;; Handles win over the backdrop behind them.
        face (or (first (remove #{:backdrop} hits)) (first hits))
        press (g/user-data self ::press)
        {:keys [x y screen-x screen-y]} action]
    (case (:type action)
      :mouse-pressed (if (and (= :primary (:button action))
                              face)
                       (do
                         (g/user-data! self ::press {:face face :x x :y y :last-x x :last-y y :dragging false})
                         nil)
                       action)
      :mouse-moved (if press
                     (let [{:keys [^double last-x ^double last-y]} press
                           dragging (or (:dragging press)
                                        (> (Math/hypot (- (double x) (double (:x press)))
                                                       (- (double y) (double (:y press))))
                                           drag-threshold))]
                       (if dragging
                         (let [image-view (g/node-value (g/node-value self :camera-node-id) :image-view)
                               ;; Wraps the cursor at the window edges like the camera's own orbit.
                               [x y] (c/warp-mouse-around-edges image-view screen-x screen-y x y last-x last-y)
                               dx (- last-x (double (:x action)))
                               dy (- last-y (double (:y action)))]
                           ;; Same direction as the camera's own orbit. Huge jumps are stale
                           ;; events from before a warp, so skip them.
                           (when (< (max (Math/abs dx) (Math/abs dy)) 150.0)
                             (tumble-camera! self dx dy))
                           (g/user-data! self ::press (assoc press :dragging true :last-x x :last-y y)))
                         (g/user-data! self ::press (assoc press :dragging false)))
                       nil)
                     (do
                       (when (not= face (g/node-value self :hot-face))
                         (g/transact (g/set-property self :hot-face face)))
                       action))
      :mouse-released (if press
                        (do
                          (g/user-data! self ::press nil)
                          (when-not (or (:dragging press) (= :backdrop (:face press)))
                            (frame-to-face! self (:face press)))
                          nil)
                        action)
      action)))

(def ^:private backdrop-fade-seconds 0.15)

(defn- handle-update-tick
  "Fades the backdrop in while the gizmo is hovered or dragged, and out after."
  [self input-state dt]
  (let [alpha (double (g/node-value self :backdrop-alpha))
        target (if (or (g/node-value self :hot-face) (g/user-data self ::press)) 1.0 0.0)
        step (/ (double dt) ^double backdrop-fade-seconds)
        next-alpha (if (< alpha target)
                     (min target (+ alpha step))
                     (max target (- alpha step)))]
    (when (not= next-alpha alpha)
      (g/transact
        {:undoable false}
        (g/set-property self :backdrop-alpha next-alpha)))
    input-state))

(g/defnode SceneViewCubeController
  (property hot-face g/Keyword)
  (property backdrop-alpha g/Num (default 0.0))

  (input camera-node-id g/NodeID)
  (input scene-aabb AABB)
  (input viewport Region)

  (output input-handler Runnable :cached (g/constantly handle-input))
  (output update-tick-handler Runnable :cached (g/constantly handle-update-tick))
  (output renderables pass/RenderData :cached produce-renderables))
