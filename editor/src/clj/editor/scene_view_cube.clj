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
           [javax.vecmath Matrix4d Point3d Quat4d Vector3d]))

(set! *warn-on-reflection* true)

(def ^:private cube-batch-key ::view-cube)
(def ^:private cube-margin 16.0)
(def ^:private cube-scale 20.0)
(def ^:private cube-depth (* cube-scale 4.0))

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

(defn- cube-model-matrix
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

(defn- visible-face?
  [^Quat4d rotation axis]
  (pos? (.z (math/rotate rotation ^Vector3d (:normal (face->data axis))))))

(defn- face-color [axis hot-face]
  (let [base-color (:color (face->data axis))]
    (if (= axis hot-face)
      (colors/alpha colors/defold-yellow 1.0)
      (colors/alpha base-color 0.95))))

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

(defn- draw-labels! [^GL2 gl ^Quat4d rotation ^Matrix4d model-matrix axes]
  (doseq [axis axes
          :when (visible-face? rotation axis)]
    (let [^Point3d center (face-center model-matrix axis)
          [label-width label-height] (scene-text/bounds gl (:label (face->data axis)))]
      (scene-text/overlay gl
                          (:label (face->data axis))
                          (float (- (.x center) (/ label-width 2.0)))
                          (float (+ (.y center) (/ label-height 2.0)))
                          1.0 1.0 1.0 1.0))))

(defn- cube-projection-matrix
  "Computes the projection matrix for drawing the view cube in screen space.

  In picking passes, render-args contains a :picking-matrix that was used to
  zoom the scene's projection into the tool picking rect. Since our cube
  completely replaces the projection with its own screen-space ortho, we must
  re-apply the picking-matrix on top of our ortho; otherwise the cube ends up
  being drawn across the whole picking buffer and every click in the scene view
  gets intercepted by the cube."
  ^Matrix4d [render-args ^Region viewport]
  (let [ortho ^Matrix4d (c/region-orthographic-projection-matrix viewport (- cube-depth) cube-depth)]
    (if-let [^Matrix4d picking-matrix (:picking-matrix render-args)]
      (doto (Matrix4d. picking-matrix) (.mul ortho))
      ortho)))

(defn- with-view-cube-matrices! [^GL2 gl render-args ^Region viewport ^Matrix4d model-matrix render-fn]
  (.glMatrixMode gl GL2/GL_PROJECTION)
  (.glPushMatrix gl)
  (try
    (gl/gl-load-matrix-4d gl (cube-projection-matrix render-args viewport))
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

(defn- with-overlay-text-matrices! [^GL2 gl render-args ^Region viewport render-fn]
  (.glMatrixMode gl GL2/GL_PROJECTION)
  (.glPushMatrix gl)
  (try
    (gl/gl-load-matrix-4d gl (cube-projection-matrix render-args viewport))
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
        all-axes (->> renderables
                      (map :selection-data)
                      (sort-by (partial face-depth model-matrix)))
        axes all-axes]
    (gl/gl-enable gl GL2/GL_DEPTH_TEST)
    (.glDepthMask gl true)
    (try
      (with-view-cube-matrices! gl render-args viewport model-matrix
        #(doseq [axis axes]
           (draw-face! gl
                       axis
                       (if (= pass/manipulator-selection (:pass render-args))
                         (scene-picking/picking-id->color (:picking-id (renderable-by-axis axis)))
                         (face-color axis hot-face)))
           (when (= pass/manipulator (:pass render-args))
             (draw-face-outline! gl axis))))
      (finally
        (.glDepthMask gl false)
        (gl/gl-disable gl GL2/GL_DEPTH_TEST)))
    (when (= pass/manipulator (:pass render-args))
      (with-overlay-text-matrices! gl render-args viewport
        #(draw-labels! gl camera-rotation model-matrix (into [] (filter (partial visible-face? camera-rotation)) all-axes))))))

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
