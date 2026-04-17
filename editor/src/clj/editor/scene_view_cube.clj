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
            [editor.colors :as colors]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.pass :as pass]
            [editor.math :as math]
            [editor.scene-picking :as scene-picking]
            [editor.scene-text :as scene-text]
            [util.coll :as coll])
  (:import [com.jogamp.opengl GL2]
           [editor.types AABB Camera Region]
           [javax.vecmath Matrix4d Point3d Quat4d Vector3d Vector4d]))

(set! *warn-on-reflection* true)

(def ^:private cube-batch-key ::view-cube)
(def ^:private cube-margin 16.0)
(def ^:private cube-scale 20.0)
(def ^:private cube-depth (* cube-scale 4.0))

;; Virtual camera parameters used when the scene is in perspective mode. The
;; cube is modeled as a unit cube centered at the origin; the virtual camera
;; sits along +Z at `cube-perspective-distance` looking at -Z. `fov-deg` is
;; wide enough that the rotated diagonal of the cube (sqrt 3 ≈ 1.73) still
;; fits in clip space when tilted corner-on to the camera.
(def ^:private cube-perspective-fov-deg 30.0)
(def ^:private cube-perspective-distance 4.0)

;; Scale factor that makes an axis-aligned cube face project to exactly
;; cube-scale screen pixels when viewed through the perspective camera. Solves
;; f * k / (d - k) = 1 so f*k/(d-k) yields NDC 1 at the near face corner.
(def ^:private cube-perspective-scale
  (let [half-fov-rad (* cube-perspective-fov-deg Math/PI (/ 1.0 360.0))
        f (/ 1.0 (Math/tan half-fov-rad))]
    (/ cube-perspective-distance (+ f 1.0))))

(def ^:private face-order [:+x :-x :+y :-y :+z :-z])

;; Use the same hues as the scene grid axis lines so the cube reads as an
;; extension of the in-scene gizmos. The alpha here only affects the base tint:
;; faces are drawn fully opaque via face-color to avoid the cube looking
;; see-through, while hovered faces switch to defold-yellow.
(def ^:private face->data
  {:+x {:center (Point3d. 1.0 0.0 0.0)
        :color colors/scene-grid-x-axis
        :label "+X"
        :normal (Vector3d. 1.0 0.0 0.0)
        :quad [[1.0 -1.0 -1.0] [1.0 -1.0 1.0] [1.0 1.0 1.0] [1.0 1.0 -1.0]]}
   :-x {:center (Point3d. -1.0 0.0 0.0)
        :color colors/scene-grid-x-axis
        :label "-X"
        :normal (Vector3d. -1.0 0.0 0.0)
        :quad [[-1.0 -1.0 1.0] [-1.0 -1.0 -1.0] [-1.0 1.0 -1.0] [-1.0 1.0 1.0]]}
   :+y {:center (Point3d. 0.0 1.0 0.0)
        :color colors/scene-grid-y-axis
        :label "+Y"
        :normal (Vector3d. 0.0 1.0 0.0)
        :quad [[-1.0 1.0 -1.0] [1.0 1.0 -1.0] [1.0 1.0 1.0] [-1.0 1.0 1.0]]}
   :-y {:center (Point3d. 0.0 -1.0 0.0)
        :color colors/scene-grid-y-axis
        :label "-Y"
        :normal (Vector3d. 0.0 -1.0 0.0)
        :quad [[-1.0 -1.0 1.0] [1.0 -1.0 1.0] [1.0 -1.0 -1.0] [-1.0 -1.0 -1.0]]}
   :+z {:center (Point3d. 0.0 0.0 1.0)
        :color colors/scene-grid-z-axis
        :label "+Z"
        :normal (Vector3d. 0.0 0.0 1.0)
        :quad [[-1.0 -1.0 1.0] [-1.0 1.0 1.0] [1.0 1.0 1.0] [1.0 -1.0 1.0]]}
   :-z {:center (Point3d. 0.0 0.0 -1.0)
        :color colors/scene-grid-z-axis
        :label "-Z"
        :normal (Vector3d. 0.0 0.0 -1.0)
        :quad [[1.0 -1.0 -1.0] [1.0 1.0 -1.0] [-1.0 1.0 -1.0] [-1.0 -1.0 -1.0]]}})

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

(defn- face-center
  ^Point3d [^Matrix4d model-matrix axis]
  (let [center (Point3d. ^Point3d (:center (face->data axis)))]
    (.transform model-matrix center)
    center))

(defn- face-depth
  ^double [^Matrix4d model-matrix axis]
  (.z (face-center model-matrix axis)))

(defn- cube-center [^Region viewport]
  (let [half-scale cube-scale]
    [(+ (.left viewport) cube-margin half-scale)
     (- (.bottom viewport) cube-margin half-scale)]))

(defn- cube-rotation
  ^Quat4d [^Camera camera]
  (doto (Quat4d. ^Quat4d (:rotation camera))
    (.conjugate)))

(defn- scene-camera-perspective?
  [^Camera camera]
  (= :perspective (:type camera)))

(defn- cube-ortho-model-matrix
  ^Matrix4d [^Camera camera ^Region viewport]
  ;; The overlay ortho projection maps pixel-space Y downward, so GL +Y ends up
  ;; pointing to the bottom of the screen. To render the view cube upright we
  ;; build the model matrix as T * reflectY * R * S so that the cube rotates in
  ;; a standard right-handed space (using the inverse camera rotation) and is
  ;; then flipped to match screen-space Y just before translation. Composing the
  ;; Y flip this way avoids the gimbal-lock issues of trying to mirror
  ;; individual Euler components of the camera rotation.
  (let [[x y] (cube-center viewport)
        rotation (cube-rotation camera)
        rs ^Matrix4d (math/->mat4-uniform (Vector3d. 0.0 0.0 0.0) rotation cube-scale)
        reflect-y ^Matrix4d (doto ^Matrix4d (math/->mat4)
                              (.setElement 1 1 -1.0))
        translation ^Matrix4d (doto ^Matrix4d (math/->mat4)
                                (.setTranslation (Vector3d. x y 0.0)))]
    (doto translation
      (.mul reflect-y)
      (.mul rs))))

(defn- cube-perspective-model-matrix
  "Places a scaled cube at (0,0,-d) relative to a virtual camera at the origin
  looking down -Z. The virtual camera's clip-space output is later remapped to
  the cube's screen corner by cube-perspective-projection-matrix, so no Y flip
  is needed here (GL clip +Y already lines up with the top of the viewport).
  The scale factor is chosen so that an axis-aligned face lines up with the
  intended cube-scale screen area."
  ^Matrix4d [^Camera camera]
  (let [rotation (cube-rotation camera)
        rs ^Matrix4d (math/->mat4-uniform (Vector3d. 0.0 0.0 0.0) rotation cube-perspective-scale)
        translation ^Matrix4d (doto ^Matrix4d (math/->mat4)
                                (.setTranslation (Vector3d. 0.0 0.0 (- cube-perspective-distance))))]
    (doto translation
      (.mul rs))))

(defn- cube-model-matrix
  ^Matrix4d [^Camera camera ^Region viewport]
  (if (scene-camera-perspective? camera)
    (cube-perspective-model-matrix camera)
    (cube-ortho-model-matrix camera viewport)))

(defn- cube-ortho-projection-matrix
  ^Matrix4d [^Region viewport]
  (c/region-orthographic-projection-matrix viewport (- cube-depth) cube-depth))

(defn- cube-perspective-projection-matrix
  "Builds a projection matrix for the perspective mode of the view cube. The
  matrix is the product of an NDC remap and a standard symmetric perspective
  projection. The NDC remap scales the virtual camera's clip space down to
  cube-scale-sized pixels and translates it to the cube's screen corner, so the
  cube shows up in its usual place while still benefiting from perspective
  foreshortening."
  ^Matrix4d [^Region viewport]
  (let [width (double (- (.right viewport) (.left viewport)))
        height (double (- (.bottom viewport) (.top viewport)))
        [cx cy] (cube-center viewport)
        ;; The editor viewport uses top=0/bottom=height pixel coords, but the
        ;; GL viewport that actually consumes the final clip space has its Y
        ;; origin at the bottom. Flip Y accordingly when mapping cube center
        ;; pixels into clip coords.
        clip-cx (- (* 2.0 (/ (double cx) width)) 1.0)
        clip-cy (- 1.0 (* 2.0 (/ (double cy) height)))
        sx (/ (* 2.0 cube-scale) width)
        sy (/ (* 2.0 cube-scale) height)
        ;; Extend near/far planes generously so that a rotated cube (whose
        ;; corners can be up to scale*sqrt(3) away from the origin in any
        ;; direction) isn't clipped.
        diag-extent (* cube-perspective-scale (Math/sqrt 3.0))
        near (max 0.01 (- cube-perspective-distance diag-extent 0.25))
        far (+ cube-perspective-distance diag-extent 0.25)
        perspective ^Matrix4d (c/simple-perspective-projection-matrix
                                near far
                                cube-perspective-fov-deg
                                cube-perspective-fov-deg)
        remap ^Matrix4d (doto ^Matrix4d (math/->mat4)
                          (.setElement 0 0 sx)
                          (.setElement 1 1 sy)
                          (.setElement 0 3 clip-cx)
                          (.setElement 1 3 clip-cy))]
    (doto remap
      (.mul perspective))))

(defn- visible-face?
  [^Quat4d rotation axis]
  (pos? (.z (math/rotate rotation ^Vector3d (:normal (face->data axis))))))

(def ^:private face-desaturation
  "Amount (0..1) that the cube face colors are blended toward their luminance.
  Higher values tone down the face colors so the cube is less visually noisy
  against the scene while still hinting at the underlying axis hue."
  0.5)

(defn- desaturate [color ^double amount]
  (let [r (double (nth color 0))
        g (double (nth color 1))
        b (double (nth color 2))
        a (double (nth color 3))
        lum (+ (* 0.299 r) (* 0.587 g) (* 0.114 b))
        k (- 1.0 amount)]
    [(+ (* k r) (* amount lum))
     (+ (* k g) (* amount lum))
     (+ (* k b) (* amount lum))
     a]))

(defn- face-color [axis hot-face]
  (let [base-color (:color (face->data axis))]
    (if (= axis hot-face)
      (colors/alpha colors/defold-yellow 1.0)
      (colors/alpha (desaturate base-color face-desaturation) 0.95))))

(defn- camera-facing-axis?
  "Returns true when the camera is (almost exactly) looking down `axis` at the
  scene, i.e. the clicked face is the one already centered in the viewport."
  [^Camera camera axis]
  (let [target-forward (doto (Vector3d. ^Vector3d (:normal (face->data axis)))
                         (.negate))
        current-forward ^Vector3d (c/camera-forward-vector camera)]
    (>= (.dot current-forward target-forward) axis-alignment-epsilon)))

(defn- draw-face! [^GL2 gl axis color]
  (let [quad (:quad (face->data axis))]
    (gl/gl-color gl color)
    (.glBegin gl GL2/GL_QUADS)
    (doseq [[x y z] quad]
      (gl/gl-vertex-3d gl x y z))
    (.glEnd gl)
    color))

(defn- draw-face-outline! [^GL2 gl axis]
  (gl/gl-color gl (colors/alpha colors/bright-black 0.9))
  (.glBegin gl GL2/GL_LINE_LOOP)
  (doseq [[x y z] (:quad (face->data axis))]
    (gl/gl-vertex-3d gl x y z))
  (.glEnd gl))

(defn- face-pixel-center
  "Projects the face center from cube-local space to on-screen pixel
  coordinates (in the editor's top=0 convention) so that text labels can be
  drawn on top of each face regardless of the active projection."
  ^Point3d [^Camera camera ^Region viewport axis]
  (let [mv ^Matrix4d (cube-model-matrix camera viewport)
        center ^Point3d (Point3d. ^Point3d (:center (face->data axis)))]
    (.transform mv center)
    (if (scene-camera-perspective? camera)
      (let [proj ^Matrix4d (cube-perspective-projection-matrix viewport)
            v (Vector4d. (.x center) (.y center) (.z center) 1.0)]
        (.transform proj v)
        (let [w (.w v)
              ndc-x (/ (.x v) w)
              ndc-y (/ (.y v) w)
              ndc-z (/ (.z v) w)
              width (double (- (.right viewport) (.left viewport)))
              height (double (- (.bottom viewport) (.top viewport)))
              ;; Undo the GL bottom=0 convention to get editor-style top=0 pixels.
              pixel-x (+ (.left viewport) (* 0.5 (+ 1.0 ndc-x) width))
              pixel-y (- (.bottom viewport) (* 0.5 (+ 1.0 ndc-y) height))]
          (Point3d. pixel-x pixel-y ndc-z)))
      center)))

(defn- draw-labels! [^GL2 gl ^Camera camera ^Region viewport ^Quat4d rotation axes]
  (doseq [axis axes
          :when (visible-face? rotation axis)]
    (let [^Point3d center (face-pixel-center camera viewport axis)
          [label-width label-height] (scene-text/bounds gl (:label (face->data axis)))]
      (scene-text/overlay gl
                          (:label (face->data axis))
                          (float (- (.x center) (/ label-width 2.0)))
                          (float (+ (.y center) (/ label-height 2.0)))
                          1.0 1.0 1.0 1.0))))

(defn- cube-projection-matrix
  "Computes the projection matrix for drawing the view cube. Picks between a
  screen-space ortho (matching the scene camera's orthographic mode) and a
  perspective projection (matching the scene camera's perspective mode).

  In picking passes, render-args contains a :picking-matrix that was used to
  zoom the scene's projection into the tool picking rect. Since our cube
  completely replaces the projection with its own, we re-apply the
  picking-matrix on top; otherwise the cube ends up drawn across the whole
  picking buffer and every click in the scene view is intercepted by the cube."
  ^Matrix4d [render-args ^Camera camera ^Region viewport]
  (let [base ^Matrix4d (if (scene-camera-perspective? camera)
                         (cube-perspective-projection-matrix viewport)
                         (cube-ortho-projection-matrix viewport))]
    (if-let [^Matrix4d picking-matrix (:picking-matrix render-args)]
      (doto (Matrix4d. picking-matrix) (.mul base))
      base)))

(defn- with-view-cube-matrices! [^GL2 gl render-args ^Camera camera ^Region viewport ^Matrix4d model-matrix render-fn]
  (.glMatrixMode gl GL2/GL_PROJECTION)
  (.glPushMatrix gl)
  (try
    (gl/gl-load-matrix-4d gl (cube-projection-matrix render-args camera viewport))
    (.glMatrixMode gl GL2/GL_MODELVIEW)
    (.glPushMatrix gl)
    (try
      (gl/gl-load-matrix-4d gl model-matrix)
      (render-fn)
      (finally
        (.glPopMatrix gl)))
    (finally
      (.glMatrixMode gl GL2/GL_PROJECTION)
      (.glPopMatrix gl)
      (.glMatrixMode gl GL2/GL_MODELVIEW))))

(defn- with-overlay-text-matrices!
  "Labels are always drawn with a simple pixel-space ortho so they stay
  perpendicular to the screen regardless of which projection the cube uses."
  [^GL2 gl ^Region viewport render-fn]
  (.glMatrixMode gl GL2/GL_PROJECTION)
  (.glPushMatrix gl)
  (try
    (gl/gl-load-matrix-4d gl (cube-ortho-projection-matrix viewport))
    (.glMatrixMode gl GL2/GL_MODELVIEW)
    (.glPushMatrix gl)
    (try
      (gl/gl-load-matrix-4d gl math/identity-mat4)
      (render-fn)
      (finally
        (.glPopMatrix gl)))
    (finally
      (.glMatrixMode gl GL2/GL_PROJECTION)
      (.glPopMatrix gl)
      (.glMatrixMode gl GL2/GL_MODELVIEW))))

(defn- render-view-cube [^GL2 gl render-args renderables _rcount]
  (let [renderable (first renderables)
        hot-face (get-in renderable [:user-data :hot-face])
        viewport ^Region (:viewport render-args)
        camera ^Camera (:camera render-args)
        camera-rotation (cube-rotation camera)
        model-matrix (cube-model-matrix camera viewport)
        renderable-by-axis (into {} (map (juxt :selection-data identity)) renderables)
        ;; Keep only the cube faces that actually face the user. Since the cube
        ;; is convex, visible faces never overlap in screen space, which means
        ;; we can draw them in any order without a depth buffer. Relying on
        ;; GL_DEPTH_TEST instead would make the cube z-fight with (or get
        ;; clipped by) the already-rendered scene geometry, which is why the
        ;; cube appeared to be cut in half by objects in the viewport.
        visible-axes (->> renderables
                          (map :selection-data)
                          (filter (partial visible-face? camera-rotation))
                          (sort-by (partial face-depth model-matrix)))]
    (with-view-cube-matrices! gl render-args camera viewport model-matrix
      #(doseq [axis visible-axes]
         (draw-face! gl
                     axis
                     (if (= pass/manipulator-selection (:pass render-args))
                       (scene-picking/picking-id->color (:picking-id (renderable-by-axis axis)))
                       (face-color axis hot-face)))
         (when (= pass/manipulator (:pass render-args))
           (draw-face-outline! gl axis))))
    (when (= pass/manipulator (:pass render-args))
      (with-overlay-text-matrices! gl viewport
        #(draw-labels! gl camera viewport camera-rotation visible-axes)))))

(g/defnk produce-renderables [_node-id hot-face]
  (let [renderables (coll/into-> face-order []
                      (map (fn [axis]
                             {:batch-key cube-batch-key
                              :node-id _node-id
                              :passes [pass/manipulator pass/manipulator-selection]
                              :render-fn render-view-cube
                              :select-batch-key cube-batch-key
                              :selection-data axis
                              :tags #{:view-cube}
                              :user-data {:hot-face hot-face}})))]
    {pass/manipulator renderables
     pass/manipulator-selection renderables}))

(defn handle-input [self _input-state action selection-data]
  (let [face (first (get selection-data self))]
    (case (:type action)
      :mouse-moved (do
                     (when (not= face (g/node-value self :hot-face))
                       (g/transact (g/set-property self :hot-face face)))
                     action)
      :mouse-pressed (if (and (= :primary (:button action))
                              face)
                       (do
                         (g/with-auto-evaluation-context evaluation-context
                           (let [camera-node-id (g/node-value self :camera-node-id evaluation-context)
                                 viewport (g/node-value self :viewport evaluation-context)
                                 scene-aabb (g/node-value self :scene-aabb evaluation-context)
                                 current-camera (g/node-value camera-node-id :local-camera evaluation-context)
                                 ;; If the camera is already aligned with the clicked face, flip to
                                 ;; the opposite axis so re-clicking toggles between +/- views.
                                 target-face (if (camera-facing-axis? current-camera face)
                                               (opposite-face face)
                                               face)]
                             (when-not (geom/predefined-aabb? scene-aabb)
                               (c/frame-camera-to-axis! camera-node-id viewport scene-aabb target-face true))))
                         nil)
                       action)
      action)))

(g/defnode SceneViewCubeController
  (property hot-face g/Keyword)

  (input camera-node-id g/NodeID)
  (input scene-aabb AABB)
  (input viewport Region)

  (output input-handler Runnable :cached (g/constantly handle-input))
  (output renderables pass/RenderData :cached produce-renderables))
