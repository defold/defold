(ns editor.collision-object
  (:require [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.app-view :as app-view]
            [editor.build-target :as bt]
            [editor.collision-groups :as collision-groups]
            [editor.colors :as colors]
            [editor.defold-project :as project]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.pass :as pass]
            [editor.gl.shader :as shader]
            [editor.gl.vertex2 :as vtx]
            [editor.graph-util :as gu]
            [editor.handler :as handler]
            [editor.math :as math]
            [editor.outline :as outline]
            [editor.properties :as properties]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.scene :as scene]
            [editor.scene-picking :as scene-picking]
            [editor.scene-tools :as scene-tools]
            [editor.types :as types]
            [editor.validation :as validation]
            [editor.workspace :as workspace]
            [schema.core :as s])
  (:import [com.dynamo.physics.proto Physics$CollisionObjectDesc Physics$CollisionObjectType Physics$CollisionShape$Shape]
           [com.jogamp.opengl GL2]
           [javax.vecmath Matrix4d Point4d Quat4d Vector3d]))

(set! *warn-on-reflection* true)

(def collision-object-icon "icons/32/Icons_49-Collision-object.png")

(def shape-type-ui
  {:type-sphere  {:label "Sphere"
                  :icon  "icons/32/Icons_45-Collistionshape-convex-Sphere.png"
                  :physics-types #{"2D" "3D"}}
   :type-box     {:label "Box"
                  :icon  "icons/32/Icons_44-Collistionshape-convex-Box.png"
                  :physics-types #{"2D" "3D"}}
   :type-capsule {:label "Capsule"
                  :icon  "icons/32/Icons_46-Collistionshape-convex-Cylinder.png"
                  :physics-types #{"3D"}}})

(defn- shape-type-label
  [shape-type]
  (get-in shape-type-ui [shape-type :label]))

(defn- shape-type-icon
  [shape-type]
  (get-in shape-type-ui [shape-type :icon]))

(defn- shape-type-physics-types
  [shape-type]
  (get-in shape-type-ui [shape-type :physics-types]))

(defn- project-physics-type [project-settings]
  (if (.equalsIgnoreCase "2D" (get project-settings ["physics" "type"] "2D"))
    "2D"
    "3D"))

(g/deftype PhysicsType (s/enum "2D" "3D"))

(g/defnode Shape
  (inherits outline/OutlineNode)
  (inherits scene/SceneNode)

  (input color g/Any)
  (input project-physics-type PhysicsType)

  (property shape-type g/Any
            (dynamic visible (g/constantly false)))
  (property node-outline-key g/Str
            (dynamic visible (g/constantly false)))

  (output transform-properties g/Any scene/produce-unscalable-transform-properties)
  (output shape-data g/Any :abstract)
  (output scene g/Any :abstract)

  (output shape g/Any (g/fnk [shape-type position rotation shape-data]
                        {:shape-type shape-type
                         :position position
                         :rotation rotation
                         :data shape-data}))

  (output node-outline outline/OutlineData :cached (g/fnk [_node-id shape-type node-outline-key]
                                                     {:node-id _node-id
                                                      :node-outline-key node-outline-key
                                                      :label (shape-type-label shape-type)
                                                      :icon (shape-type-icon shape-type)})))

;;--------------------------------------------------------------------
;; rendering

;; We cannot batch-render the collision shapes, since we need to cancel out non-
;; uniform scaling on the individual shape transforms. Thus, we are free to use
;; shared object-space vertex buffers and transform them in the vertex shader.

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

(def ^:private box-lines
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

(def ^:private box-triangles
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

(def ^:private disc-lines
  {:primitive-type GL2/GL_LINE_LOOP
   :vbuf (vtx/flip!
           (reduce
             (fn [vbuf [x y _]]
               (pos-vtx-put! vbuf x y 0.0 0.0))
             (->pos-vtx (count disc-perimeter) :static)
             disc-perimeter))})

(def ^:private disc-triangles
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

(def ^:private capsule-lines
  {:primitive-type GL2/GL_LINES
   :vbuf (vtx/flip!
           (reduce
             (fn [vbuf [quad]]
               (-> vbuf
                   (pos-vtx-put-point! (quad 0))
                   (pos-vtx-put-point! (quad 3))))
             (->pos-vtx (* 2 (/ (count capsule-quads) 4)) :static)
             (partition 4 capsule-quads)))})

(def ^:private capsule-triangles
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

(def shape-alpha 0.1)

(def selected-shape-alpha 0.3)

(defn unify-scale [renderable]
  (let [{:keys [^Quat4d world-rotation
                ^Vector3d world-scale
                ^Vector3d world-translation]} renderable
        min-scale (min (.-x world-scale) (.-y world-scale) (.-z world-scale))
        physics-world-transform (doto (Matrix4d.)
                                  (.setIdentity)
                                  (.setScale min-scale)
                                  (.setTranslation world-translation)
                                  (.setRotation world-rotation))]
    (assoc renderable :world-transform physics-world-transform)))

(defn wrap-uniform-scale [render-fn]
  (fn [gl render-args renderables n]
    (render-fn gl render-args (map unify-scale renderables) n)))

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

(def ^:private render-lines-uniform-scale (wrap-uniform-scale render-lines))

(def ^:private render-triangles-uniform-scale (wrap-uniform-scale render-triangles))

(g/defnk produce-sphere-shape-scene
  [_node-id transform diameter color node-outline-key project-physics-type]
  (let [radius (* 0.5 diameter)
        is-2d (= "2D" project-physics-type)
        ext-z (if is-2d 0.0 radius)
        ext [radius radius ext-z]
        neg-ext [(- radius) (- radius) (- ext-z)]
        aabb (geom/coords->aabb ext neg-ext)
        point-scale (float-array ext)]
    {:node-id _node-id
     :node-outline-key node-outline-key
     :transform transform
     :aabb aabb
     :renderable {:render-fn render-triangles-uniform-scale
                  :tags #{:collision-shape}
                  :passes [pass/transparent pass/selection]
                  :user-data {:color color
                              :double-sided is-2d
                              :point-scale point-scale
                              :geometry (if is-2d disc-triangles capsule-triangles)}}
     :children [{:node-id _node-id
                 :aabb aabb
                 :renderable {:render-fn render-lines-uniform-scale
                              :tags #{:collision-shape :outline}
                              :passes [pass/outline]
                              :user-data {:color color
                                          :point-scale point-scale
                                          :geometry (if is-2d disc-lines capsule-lines)}}}]}))

(g/defnk produce-box-shape-scene
  [_node-id transform dimensions color node-outline-key project-physics-type]
  (let [[w h d] dimensions
        ext-x (* 0.5 w)
        ext-y (* 0.5 h)
        ext-z (case project-physics-type
                "2D" 0.0
                "3D" (* 0.5 d))
        ext [ext-x ext-y ext-z]
        neg-ext [(- ext-x) (- ext-y) (- ext-z)]
        aabb (geom/coords->aabb ext neg-ext)
        point-scale (float-array ext)]
    {:node-id _node-id
     :node-outline-key node-outline-key
     :transform transform
     :aabb aabb
     :renderable {:render-fn render-triangles-uniform-scale
                  :tags #{:collision-shape}
                  :passes [pass/transparent pass/selection]
                  :user-data {:color color
                              :double-sided (zero? ext-z)
                              :point-count (if (zero? ext-z) 6 (count (:vbuf box-triangles)))
                              :point-scale point-scale
                              :geometry box-triangles}}
     :children [{:node-id _node-id
                 :aabb aabb
                 :renderable {:render-fn render-lines-uniform-scale
                              :tags #{:collision-shape :outline}
                              :passes [pass/outline]
                              :user-data {:color color
                                          :point-count (if (zero? ext-z) 8 (count (:vbuf box-lines)))
                                          :point-scale point-scale
                                          :geometry box-lines}}}]}))

(g/defnk produce-capsule-shape-scene
  [_node-id transform diameter height color node-outline-key project-physics-type]
  ;; NOTE: Capsules are currently only supported when physics type is 3D.
  (let [radius (* 0.5 diameter)
        half-height (* 0.5 height)
        ext-y (+ half-height radius)
        ext-z (case project-physics-type
                "2D" 0.0
                "3D" radius)
        ext [radius ext-y ext-z]
        neg-ext [(- radius) (- ext-y) (- ext-z)]
        aabb (geom/coords->aabb ext neg-ext)
        point-scale (float-array [radius radius ext-z])
        point-offset-by-w (float-array [0.0 half-height 0.0])]
    {:node-id _node-id
     :node-outline-key node-outline-key
     :transform transform
     :aabb aabb
     :renderable {:render-fn render-triangles-uniform-scale
                  :tags #{:collision-shape}
                  :passes [pass/transparent pass/selection]
                  :user-data {:color color
                              :point-scale point-scale
                              :point-offset-by-w point-offset-by-w
                              :geometry capsule-triangles}}
     :children [{:node-id _node-id
                 :aabb aabb
                 :renderable {:render-fn render-lines-uniform-scale
                              :tags #{:collision-shape :outline}
                              :passes [pass/outline]
                              :user-data {:color color
                                          :point-scale point-scale
                                          :point-offset-by-w point-offset-by-w
                                          :geometry capsule-lines}}}]}))

(g/defnode SphereShape
  (inherits Shape)

  (property diameter g/Num
            (dynamic error (validation/prop-error-fnk :fatal validation/prop-zero-or-below? diameter)))

  (display-order [Shape :diameter])

  (output scene g/Any produce-sphere-shape-scene)

  (output shape-data g/Any (g/fnk [diameter]
                             [(/ diameter 2)])))

(defmethod scene-tools/manip-scalable? ::SphereShape [_node-id] true)

(defmethod scene-tools/manip-scale ::SphereShape
  [evaluation-context node-id ^Vector3d delta]
  (let [diameter (g/node-value node-id :diameter evaluation-context)]
    (g/set-property node-id :diameter (properties/round-scalar (* diameter (Math/abs (.getX delta)))))))

(defmethod scene-tools/manip-scale-manips ::SphereShape
  [node-id]
  [:scale-xy])


(g/defnode BoxShape
  (inherits Shape)

  (property dimensions types/Vec3
            (dynamic error (validation/prop-error-fnk :fatal
                                                      (fn [d _] (when (some #(<= % 0.0) d)
                                                                  "All dimensions must be greater than zero"))
                                                      dimensions))
            (dynamic edit-type (g/constantly {:type types/Vec3 :labels ["W" "H" "D"]})))

  (display-order [Shape :dimensions])

  (output scene g/Any produce-box-shape-scene)

  (output shape-data g/Any (g/fnk [dimensions]
                             (let [[w h d] dimensions]
                               [(/ w 2) (/ h 2) (/ d 2)]))))

(defmethod scene-tools/manip-scalable? ::BoxShape [_node-id] true)

(defmethod scene-tools/manip-scale ::BoxShape
  [evaluation-context node-id ^Vector3d delta]
  (let [[w h d] (g/node-value node-id :dimensions evaluation-context)]
    (g/set-property node-id :dimensions [(properties/round-scalar (Math/abs (* w (.getX delta))))
                                         (properties/round-scalar (Math/abs (* h (.getY delta))))
                                         (properties/round-scalar (Math/abs (* d (.getZ delta))))])))

(g/defnode CapsuleShape
  (inherits Shape)

  (property diameter g/Num
            (dynamic error (validation/prop-error-fnk :fatal validation/prop-zero-or-below? diameter)))
  (property height g/Num
            (dynamic error (validation/prop-error-fnk :fatal validation/prop-zero-or-below? height)))

  (display-order [Shape :diameter :height])

  (output scene g/Any produce-capsule-shape-scene)

  (output shape-data g/Any (g/fnk [diameter height]
                             [(/ diameter 2) height])))

(defmethod scene-tools/manip-scalable? ::CapsuleShape [_node-id] true)

(defmethod scene-tools/manip-scale ::CapsuleShape
  [evaluation-context node-id ^Vector3d delta]
  (let [[d h] (mapv #(g/node-value node-id % evaluation-context) [:diameter :height])]
    (g/set-property node-id
                    :diameter (properties/round-scalar (Math/abs (* d (.getX delta))))
                    :height (properties/round-scalar (Math/abs (* h (.getY delta)))))))

(defmethod scene-tools/manip-scale-manips ::CapsuleShape
  [node-id]
  [:scale-x :scale-y :scale-xy])

(defn attach-shape-node
  [resolve-node-outline-key? parent shape-node]
  (concat
    (g/connect shape-node :_node-id              parent     :nodes)
    (g/connect shape-node :node-outline          parent     :child-outlines)
    (g/connect shape-node :scene                 parent     :child-scenes)
    (g/connect shape-node :shape                 parent     :shapes)
    (g/connect parent     :collision-group-color shape-node :color)
    (g/connect parent     :project-physics-type  shape-node :project-physics-type)
    (when resolve-node-outline-key?
      (g/update-property shape-node :node-outline-key outline/next-node-outline-key (outline/taken-node-outline-keys parent)))))

(defmulti decode-shape-data
  (fn [shape data] (:shape-type shape)))

(defmethod decode-shape-data :type-sphere
  [shape [r]]
  {:diameter (* 2 r)})

(defmethod decode-shape-data :type-box
  [shape [ext-x ext-y ext-z]]
  {:dimensions [(* 2 ext-x) (* 2 ext-y) (* 2 ext-z)]})

(defmethod decode-shape-data :type-capsule
  [shape [r h]]
  {:diameter (* 2 r)
   :height h})

(defn make-shape-node
  [parent {:keys [shape-type] :as shape}]
  (let [graph-id (g/node-id->graph-id parent)
        node-type (case shape-type
                    :type-sphere SphereShape
                    :type-box BoxShape
                    :type-capsule CapsuleShape)
        node-props (dissoc shape :index :count)]
    (g/make-nodes
      graph-id
      [shape-node [node-type node-props]]
      (attach-shape-node false parent shape-node))))

(defn- load-embedded-shape [embedded-collision-shape-data {:keys [index count] :as shape}]
  (let [shape-data (subvec embedded-collision-shape-data index (+ index count))
        decoded-shape-data (decode-shape-data shape shape-data)]
    (merge shape decoded-shape-data)))

(defn load-collision-object
  [project self resource co]
  (concat
    (g/set-property self
      :collision-shape (workspace/resolve-resource resource (:collision-shape co))
      :type (:type co)
      :mass (:mass co)
      :friction (:friction co)
      :restitution (:restitution co)
      :group (:group co)
      :mask (some->> (:mask co) (string/join ", "))
      :linear-damping (:linear-damping co)
      :angular-damping (:angular-damping co)
      :locked-rotation (:locked-rotation co))
    (g/connect self :collision-group-node project :collision-group-nodes)
    (g/connect project :collision-groups-data self :collision-groups-data)
    (g/connect project :settings self :project-settings)
    (when-some [{:keys [data shapes]} (:embedded-collision-shape co)]
      (sequence (comp (map #(assoc %1 :node-outline-key %2))
                      (map (partial load-embedded-shape data))
                      (map (partial make-shape-node self)))
                shapes
                (outline/gen-node-outline-keys (map (comp shape-type-label :shape-type)
                                                    shapes))))))

(g/defnk produce-scene
  [_node-id child-scenes]
  {:node-id _node-id
   :aabb geom/null-aabb
   :renderable {:passes [pass/selection]}
   :children child-scenes})

(defn- produce-embedded-collision-shape
  [shapes]
  (when (seq shapes)
    (loop [idx 0
           [shape & rest] shapes
           ret {:shapes [] :data []}]
      (if-not shape
        ret
        (let [data (:data shape)
              data-len (count data)
              shape-msg (-> shape
                            (assoc :index idx :count data-len)
                            (dissoc :data))]
          (recur (+ idx data-len)
                 rest
                 (-> ret
                     (update :shapes conj shape-msg)
                     (update :data into data))))))))

(g/defnk produce-pb-msg
  [collision-shape-resource type mass friction restitution
   group mask angular-damping linear-damping locked-rotation
   shapes]
  {:collision-shape (resource/resource->proj-path collision-shape-resource)
   :type type
   :mass mass
   :friction friction
   :restitution restitution
   :group group
   :mask (when mask (->> (string/split mask #",") (map string/trim) (remove string/blank?)))
   :linear-damping linear-damping
   :angular-damping angular-damping
   :locked-rotation locked-rotation
   :embedded-collision-shape (produce-embedded-collision-shape shapes)})

(defn build-collision-object
  [resource dep-resources user-data]
  (let [[shape] (vals dep-resources)
        pb-msg (cond-> (:pb-msg user-data)
                 shape (assoc :collision-shape (resource/proj-path shape)))]
    {:resource resource
     :content (protobuf/map->bytes Physics$CollisionObjectDesc pb-msg)}))

(defn- merge-convex-shape [collision-shape convex-shape]
  (if convex-shape
    (let [collision-shape (or collision-shape {:shapes [] :data []})
          shape {:shape-type (:shape-type convex-shape)
                 :position [0 0 0]
                 :rotation [0 0 0 1]
                 :index (count (:data collision-shape))
                 :count (count (:data convex-shape))}]
      (-> collision-shape
        (update :shapes conj shape)
        (update :data into (:data convex-shape))))
    collision-shape))

(g/defnk produce-build-targets
  [_node-id resource pb-msg collision-shape dep-build-targets mass type project-physics-type shapes]
  (let [dep-build-targets (flatten dep-build-targets)
        convex-shape (when (and collision-shape (= "convexshape" (:ext (resource/resource-type collision-shape))))
                       (get-in (first dep-build-targets) [:user-data :pb]))
        pb-msg (if convex-shape
                 (dissoc pb-msg :collision-shape) ; Convex shape will be merged into :embedded-collision-shape below.
                 pb-msg)
        dep-build-targets (if convex-shape [] dep-build-targets)
        deps-by-source (into {} (map #(let [res (:resource %)] [(:resource res) res]) dep-build-targets))
        dep-resources (if convex-shape
                        [] ; Convex shape is merged into :embedded-collision-shape
                        (map (fn [[label resource]]
                               [label (get deps-by-source resource)])
                             [[:collision-shape collision-shape]])) ; This is a tilemap resource.
        pb-msg (update pb-msg :embedded-collision-shape merge-convex-shape convex-shape)]
    (g/precluding-errors
      [(validation/prop-error :fatal _node-id :collision-shape validation/prop-resource-not-exists? collision-shape "Collision Shape")
       (when (= :collision-object-type-dynamic type)
         (validation/prop-error :fatal _node-id :mass validation/prop-zero-or-below? mass "Mass"))
       (when (and (empty? (:collision-shape pb-msg))
                  (empty? (:embedded-collision-shape pb-msg)))
         (g/->error _node-id :collision-shape :fatal collision-shape "Collision Object has no shapes"))
       (sequence (comp (map :shape-type)
                       (distinct)
                       (remove #(contains? (shape-type-physics-types %) project-physics-type))
                       (map #(format "%s shapes are not supported in %s physics" (shape-type-label %) project-physics-type))
                       (map #(g/->error _node-id :shapes :fatal shapes %)))
                 shapes)]
      [(bt/with-content-hash
         {:node-id _node-id
          :resource (workspace/make-build-resource resource)
          :build-fn build-collision-object
          :user-data {:pb-msg pb-msg
                      :dep-resources dep-resources}
          :deps dep-build-targets})])))

(g/defnk produce-collision-group-color
  [collision-groups-data group]
  (collision-groups/color collision-groups-data group))

(g/defnode CollisionObjectNode
  (inherits resource-node/ResourceNode)

  (input shapes g/Any :array)
  (input child-scenes g/Any :array)
  (input collision-shape-resource resource/Resource)
  (input dep-build-targets g/Any :array)
  (input collision-groups-data g/Any)
  (input project-settings g/Any)

  (property collision-shape resource/Resource
            (value (gu/passthrough collision-shape-resource))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter evaluation-context self old-value new-value
                                            [:resource :collision-shape-resource]
                                            [:build-targets :dep-build-targets])))
            (dynamic edit-type (g/constantly {:type resource/Resource :ext #{"convexshape" "tilemap"}}))
            (dynamic error (g/fnk [_node-id collision-shape]
                                  (when collision-shape
                                    (validation/prop-error :fatal _node-id :collision-shape validation/prop-resource-not-exists? collision-shape "Collision Shape")))))

  (property type g/Any
            (dynamic edit-type (g/constantly (properties/->pb-choicebox Physics$CollisionObjectType))))

  (property mass g/Num
            (value (g/fnk [mass type]
                     (if (= :collision-object-type-dynamic type) mass 0.0)))
            (dynamic read-only? (g/fnk [type]
                                  (not= :collision-object-type-dynamic type)))
            (dynamic error (g/fnk [_node-id mass type]
                             (when (= :collision-object-type-dynamic type)
                               (validation/prop-error :fatal _node-id :mass validation/prop-zero-or-below? mass "Mass")))))

  (property friction g/Num)
  (property restitution g/Num)
  (property linear-damping g/Num
            (default 0))
  (property angular-damping g/Num
            (default 0))
  (property locked-rotation g/Bool
            (default false))

  (property group g/Str)
  (property mask g/Str)

  (output scene g/Any :cached produce-scene)
  (output project-physics-type PhysicsType (g/fnk [project-settings] (project-physics-type project-settings)))
  (output node-outline outline/OutlineData :cached (g/fnk [_node-id child-outlines]
                                                     {:node-id _node-id
                                                      :node-outline-key "Collision Object"
                                                      :label "Collision Object"
                                                      :icon collision-object-icon
                                                      :children (outline/natural-sort child-outlines)
                                                      :child-reqs [{:node-type Shape
                                                                    :tx-attach-fn (partial attach-shape-node true)}]}))

  (output pb-msg g/Any :cached produce-pb-msg)
  (output save-value g/Any (gu/passthrough pb-msg))
  (output build-targets g/Any :cached produce-build-targets)
  (output collision-group-node g/Any :cached (g/fnk [_node-id group] {:node-id _node-id :collision-group group}))
  (output collision-group-color g/Any :cached produce-collision-group-color))

(defn- sanitize-collision-object [co]
  (let [embedded-shape (:embedded-collision-shape co)]
    (cond-> co
      (empty? (:shapes embedded-shape)) (dissoc co :embedded-collision-shape))))

(defn register-resource-types [workspace]
  (resource-node/register-ddf-resource-type workspace
                                    :ext "collisionobject"
                                    :node-type CollisionObjectNode
                                    :ddf-type Physics$CollisionObjectDesc
                                    :load-fn load-collision-object
                                    :sanitize-fn sanitize-collision-object
                                    :icon collision-object-icon
                                    :view-types [:scene :text]
                                    :view-opts {:scene {:grid true}}
                                    :tags #{:component}
                                    :tag-opts {:component {:transform-properties #{}}}
                                    :label "Collision Object"))

;; outline context menu

(defn- default-shape
  [shape-type]
  (merge (protobuf/pb->map (Physics$CollisionShape$Shape/getDefaultInstance))
         {:shape-type shape-type}
         (case shape-type
           :type-sphere {:diameter 20.0}
           :type-box {:dimensions [20.0 20.0 20.0]}
           :type-capsule {:diameter 20.0 :height 40.0})))

(defn- add-shape-handler
  [collision-object-node shape-type select-fn]
  (let [op-seq (gensym)
        node-outline-key (outline/next-node-outline-key (shape-type-label shape-type)
                                                        (outline/taken-node-outline-keys collision-object-node))
        shape (assoc (default-shape shape-type) :node-outline-key node-outline-key)
        shape-node (first (g/tx-nodes-added
                            (g/transact
                              (concat
                                (g/operation-label "Add Shape")
                                (g/operation-sequence op-seq)
                                (make-shape-node collision-object-node shape)))))]
    (when (some? select-fn)
      (g/transact
        (concat
          (g/operation-sequence op-seq)
          (select-fn [shape-node]))))))

(defn- selection->collision-object [selection]
  (handler/adapt-single selection CollisionObjectNode))

(handler/defhandler :add :workbench
  (label [user-data]
         (if-not user-data
           "Add Shape"
           (shape-type-label (:shape-type user-data))))
  (active? [selection] (selection->collision-object selection))
  (run [selection user-data app-view]
    (add-shape-handler (selection->collision-object selection) (:shape-type user-data) (fn [node-ids] (app-view/select app-view node-ids))))
  (options [selection user-data]
           (let [self (selection->collision-object selection)]
             (when-not user-data
               (->> shape-type-ui
                    (reduce-kv (fn [res shape-type {:keys [label icon]}]
                                 (conj res {:label label
                                            :icon icon
                                            :command :add
                                            :user-data {:_node-id self :shape-type shape-type}}))
                               [])
                    (sort-by :label)
                    (into []))))))
