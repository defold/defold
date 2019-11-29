(ns editor.scene-shapes
  "Helpers for rendering of shared object-space vertex buffers that are
  transformed in the vertex shader."
  (:require [editor.colors :as colors]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.pass :as pass]
            [editor.gl.shader :as shader]
            [editor.gl.vertex2 :as vtx]
            [editor.math :as math]
            [editor.scene-picking :as scene-picking])
  (:import [com.jogamp.opengl GL2]
           [javax.vecmath Point4d]))

(set! *warn-on-reflection* true)

(vtx/defvertex pos-vtx
  ;; The W component is multiplied with the point_offset_by_w uniform, which is
  ;; used to offset the point in local space. We use this to offset the caps of
  ;; the capsule shape.
  (vec4 position))

(shader/defshader vertex-shader
  (uniform mat4 world_view_proj)
  (uniform vec4 point_scale)
  (uniform vec4 point_offset_by_w)
  (attribute vec4 position)
  (defn void main []
    (setq vec3 point
          (+ (* position.xyz
                point_scale.xyz)
             (* position.w
                point_offset_by_w.xyz)))
    (setq gl_Position
          (* world_view_proj
             (vec4 point 1.0)))))

(shader/defshader fragment-shader
  (uniform vec4 color) ; `color` also used in selection pass to render picking id
  (defn void main []
    (setq gl_FragColor color)))

(def shader (shader/make-shader ::shader vertex-shader fragment-shader {"world_view_proj" :world-view-proj}))

(def box-lines
  {:primitive-type GL2/GL_LINES
   :vbuf (-> (->pos-vtx 24 :static)

             ;; Pos Z
             (pos-vtx-put! 1.0 1.0 1.0 0.0)
             (pos-vtx-put! -1.0 1.0 1.0 0.0)
             (pos-vtx-put! -1.0 1.0 1.0 0.0)
             (pos-vtx-put! -1.0 -1.0 1.0 0.0)
             (pos-vtx-put! -1.0 -1.0 1.0 0.0)
             (pos-vtx-put! 1.0 -1.0 1.0 0.0)
             (pos-vtx-put! 1.0 -1.0 1.0 0.0)
             (pos-vtx-put! 1.0 1.0 1.0 0.0)

             ;; Neg Z
             (pos-vtx-put! 1.0 1.0 -1.0 0.0)
             (pos-vtx-put! 1.0 -1.0 -1.0 0.0)
             (pos-vtx-put! 1.0 -1.0 -1.0 0.0)
             (pos-vtx-put! -1.0 -1.0 -1.0 0.0)
             (pos-vtx-put! -1.0 -1.0 -1.0 0.0)
             (pos-vtx-put! -1.0 1.0 -1.0 0.0)
             (pos-vtx-put! -1.0 1.0 -1.0 0.0)
             (pos-vtx-put! 1.0 1.0 -1.0 0.0)

             ;; Connecting lines
             (pos-vtx-put! 1.0 1.0 1.0 0.0)
             (pos-vtx-put! 1.0 1.0 -1.0 0.0)
             (pos-vtx-put! -1.0 1.0 1.0 0.0)
             (pos-vtx-put! -1.0 1.0 -1.0 0.0)
             (pos-vtx-put! -1.0 -1.0 1.0 0.0)
             (pos-vtx-put! -1.0 -1.0 -1.0 0.0)
             (pos-vtx-put! 1.0 -1.0 1.0 0.0)
             (pos-vtx-put! 1.0 -1.0 -1.0 0.0)

             (vtx/flip!))})

(def box-triangles
  {:primitive-type GL2/GL_TRIANGLES
   :vbuf (-> (->pos-vtx 36 :static)

             ;; We start with the camera-facing face so that we can render just
             ;; the first face in case we are rendering a 2D box.

             ;; Neg Z
             (pos-vtx-put! 1.0 1.0 -1.0 0.0)
             (pos-vtx-put! 1.0 -1.0 -1.0 0.0)
             (pos-vtx-put! -1.0 -1.0 -1.0 0.0)
             (pos-vtx-put! -1.0 -1.0 -1.0 0.0)
             (pos-vtx-put! -1.0 1.0 -1.0 0.0)
             (pos-vtx-put! 1.0 1.0 -1.0 0.0)

             ;; Pos Z
             (pos-vtx-put! 1.0 1.0 1.0 0.0)
             (pos-vtx-put! -1.0 1.0 1.0 0.0)
             (pos-vtx-put! -1.0 -1.0 1.0 0.0)
             (pos-vtx-put! -1.0 -1.0 1.0 0.0)
             (pos-vtx-put! 1.0 -1.0 1.0 0.0)
             (pos-vtx-put! 1.0 1.0 1.0 0.0)

             ;; Neg Y
             (pos-vtx-put! 1.0 -1.0 1.0 0.0)
             (pos-vtx-put! -1.0 -1.0 1.0 0.0)
             (pos-vtx-put! -1.0 -1.0 -1.0 0.0)
             (pos-vtx-put! -1.0 -1.0 -1.0 0.0)
             (pos-vtx-put! 1.0 -1.0 -1.0 0.0)
             (pos-vtx-put! 1.0 -1.0 1.0 0.0)

             ;; Pos Y
             (pos-vtx-put! 1.0 1.0 1.0 0.0)
             (pos-vtx-put! 1.0 1.0 -1.0 0.0)
             (pos-vtx-put! -1.0 1.0 -1.0 0.0)
             (pos-vtx-put! -1.0 1.0 -1.0 0.0)
             (pos-vtx-put! -1.0 1.0 1.0 0.0)
             (pos-vtx-put! 1.0 1.0 1.0 0.0)

             ;; Neg X
             (pos-vtx-put! -1.0 1.0 1.0 0.0)
             (pos-vtx-put! -1.0 1.0 -1.0 0.0)
             (pos-vtx-put! -1.0 -1.0 -1.0 0.0)
             (pos-vtx-put! -1.0 -1.0 -1.0 0.0)
             (pos-vtx-put! -1.0 -1.0 1.0 0.0)
             (pos-vtx-put! -1.0 1.0 1.0 0.0)

             ;; Pos X
             (pos-vtx-put! 1.0 1.0 1.0 0.0)
             (pos-vtx-put! 1.0 -1.0 1.0 0.0)
             (pos-vtx-put! 1.0 -1.0 -1.0 0.0)
             (pos-vtx-put! 1.0 -1.0 -1.0 0.0)
             (pos-vtx-put! 1.0 1.0 -1.0 0.0)
             (pos-vtx-put! 1.0 1.0 1.0 0.0)

             (vtx/flip!))})

(def ^:private disc-perimeter
  (->> geom/origin-geom
       (geom/transl [0.0 1.0 0.0])
       (geom/circling 32)))

(def disc-lines
  {:primitive-type GL2/GL_LINE_LOOP
   :vbuf (vtx/flip!
           (reduce
             (fn [vbuf [x y _]]
               (pos-vtx-put! vbuf x y 0.0 0.0))
             (->pos-vtx (count disc-perimeter) :static)
             disc-perimeter))})

(def disc-triangles
  {:primitive-type GL2/GL_TRIANGLE_FAN
   :vbuf (-> (reduce
               (fn [vbuf [x y _]]
                 (pos-vtx-put! vbuf x y 0.0 0.0))
               (pos-vtx-put!
                 (->pos-vtx (+ 2 (count disc-perimeter)) :static)
                 0.0 0.0 0.0 0.0)
               disc-perimeter)
             (pos-vtx-put! 0.0 1.0 0.0 0.0)
             (vtx/flip!))})

(defn- pos-nrm->quad-point
  ^Point4d [[^double x ^double y ^double z] ^double w]
  (Point4d. x y z w))

(defn- pos-nrm-face->quad [[v0 v1 v2 _ v4] ^double w]
  [(pos-nrm->quad-point v0 w)
   (pos-nrm->quad-point v1 w)
   (pos-nrm->quad-point v4 w)
   (pos-nrm->quad-point v2 w)])

(defn- pos-nrm-face->waist-quad [[v0 v1]]
  [(pos-nrm->quad-point v0 1.0)
   (pos-nrm->quad-point v1 1.0)
   (pos-nrm->quad-point v1 -1.0)
   (pos-nrm->quad-point v0 -1.0)])

(def ^:private capsule-quads
  (let [capsule-cap-lats 16
        capsule-cap-longs 32
        sphere-faces (geom/unit-sphere-pos-nrm capsule-cap-lats capsule-cap-longs)
        hemisphere-face-count (/ (* capsule-cap-lats capsule-cap-longs) 2)]
    (concat
      ;; Top cap
      (sequence (comp (take hemisphere-face-count)
                      (map #(pos-nrm-face->quad % 1.0)))
                sphere-faces)

      ;; Waist
      (sequence (comp (drop hemisphere-face-count)
                      (take capsule-cap-longs)
                      (map pos-nrm-face->waist-quad))
                sphere-faces)

      ;; Bottom cap
      (sequence (comp (drop hemisphere-face-count)
                      (map #(pos-nrm-face->quad % -1.0)))
                sphere-faces))))

(defn- pos-vtx-put-point! [vbuf ^Point4d point]
  (pos-vtx-put! vbuf (.x point) (.y point) (.z point) (.w point)))

(def capsule-lines
  {:primitive-type GL2/GL_LINES
   :vbuf (vtx/flip!
           (reduce
             (fn [vbuf [quad]]
               (-> vbuf
                   (pos-vtx-put-point! (quad 0))
                   (pos-vtx-put-point! (quad 3))))
             (->pos-vtx (* 2 (/ (count capsule-quads) 4)) :static)
             (partition 4 capsule-quads)))})

(def capsule-triangles
  {:primitive-type GL2/GL_TRIANGLES
   :vbuf (vtx/flip!
           (reduce
             (fn [vbuf quad]
               (-> vbuf
                   (pos-vtx-put-point! (quad 0))
                   (pos-vtx-put-point! (quad 1))
                   (pos-vtx-put-point! (quad 2))
                   (pos-vtx-put-point! (quad 2))
                   (pos-vtx-put-point! (quad 3))
                   (pos-vtx-put-point! (quad 0))))
             (->pos-vtx (* 6 (count capsule-quads)) :static)
             capsule-quads))})

(def ^:private shape-alpha 0.1)

(def ^:private selected-shape-alpha 0.3)

(def ^:private no-point-scale (float-array 4 1.0))

(def ^:private no-point-offset-by-w (float-array 4 0.0))

(defn render-lines [^GL2 gl render-args renderables _num-renderables]
  (assert (not= pass/selection (:pass render-args)) "color not intended for picking")
  (let [{:keys [selected user-data world-transform]} (first renderables)
        {:keys [color geometry]} user-data
        {:keys [primitive-type vbuf]} geometry
        color (float-array (if selected
                             colors/selected-outline-color
                             (colors/alpha color 1.0)))
        render-args (merge render-args
                           (math/derive-render-transforms
                             world-transform
                             (:view render-args)
                             (:projection render-args)
                             (:texture render-args)))
        point-count (:point-count user-data (count vbuf))
        point-scale (:point-scale user-data no-point-scale)
        point-offset-by-w (:point-offset-by-w user-data no-point-offset-by-w)
        request-id (System/identityHashCode vbuf)
        vertex-binding (vtx/use-with request-id vbuf shader)]
    (gl/with-gl-bindings gl render-args [shader vertex-binding]
      (shader/set-uniform shader gl "point_scale" point-scale)
      (shader/set-uniform shader gl "point_offset_by_w" point-offset-by-w)
      (shader/set-uniform shader gl "color" color)
      (gl/gl-draw-arrays gl primitive-type 0 point-count))))

(defn render-triangles [^GL2 gl render-args renderables _num-renderables]
  (let [renderable (first renderables)
        {:keys [selected user-data world-transform]} renderable
        {:keys [color double-sided geometry]} user-data
        {:keys [primitive-type vbuf]} geometry
        color (float-array
                (cond
                  (= pass/selection (:pass render-args))
                  (scene-picking/renderable-picking-id-uniform renderable)

                  selected
                  (colors/alpha color selected-shape-alpha)

                  :else
                  (colors/alpha color shape-alpha)))
        render-args (merge render-args
                           (math/derive-render-transforms
                             world-transform
                             (:view render-args)
                             (:projection render-args)
                             (:texture render-args)))
        point-count (:point-count user-data (count vbuf))
        point-scale (:point-scale user-data no-point-scale)
        point-offset-by-w (:point-offset-by-w user-data no-point-offset-by-w)
        request-id (System/identityHashCode vbuf)
        vertex-binding (vtx/use-with request-id vbuf shader)]
    (gl/with-gl-bindings gl render-args [shader vertex-binding]
      (when-not double-sided
        (gl/gl-enable gl GL2/GL_CULL_FACE)
        (gl/gl-cull-face gl GL2/GL_BACK))
      (shader/set-uniform shader gl "point_scale" point-scale)
      (shader/set-uniform shader gl "point_offset_by_w" point-offset-by-w)
      (shader/set-uniform shader gl "color" color)
      (gl/gl-draw-arrays gl primitive-type 0 point-count)
      (when-not double-sided
        (gl/gl-disable gl GL2/GL_CULL_FACE)))))
