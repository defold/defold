(ns editor.collada-scene
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.animation-set :as animation-set]
            [editor.collada :as collada]
            [editor.defold-project :as project]
            [editor.gl :as gl]
            [editor.gl.shader :as shader]
            [editor.gl.vertex :as vtx]
            [editor.gl.texture :as texture]
            [editor.gl.pass :as pass]
            [editor.geom :as geom]
            [editor.render :as render]
            [editor.resource :as resource]
            [editor.rig :as rig]
            [editor.math :as math]
            [editor.workspace :as workspace]
            [internal.graph.error-values :as error-values])
  (:import [com.jogamp.opengl GL GL2]
           [editor.types AABB]
           [javax.vecmath Matrix3d Matrix4d Point3d Vector3d]))

(set! *warn-on-reflection* true)

(def mesh-icon "icons/32/Icons_27-AT-Mesh.png")

(vtx/defvertex vtx-pos-nrm-tex
  (vec3 position)
  (vec3 normal)
  (vec2 texcoord0))

(shader/defshader shader-ver-pos-nrm-tex
  (attribute vec4 position)
  (attribute vec3 normal)
  (attribute vec2 texcoord0)
  (varying vec3 var_normal)
  (varying vec2 var_texcoord0)
  (defn void main []
    (setq gl_Position (* gl_ModelViewProjectionMatrix position))
    (setq var_texcoord0 texcoord0)
    (setq var_normal normal)))

(shader/defshader shader-frag-pos-nrm-tex
  (varying vec3 var_normal)
  (varying vec2 var_texcoord0)
  (uniform sampler2D texture)
  (defn void main []
    (setq gl_FragColor (vec4 (* (.xyz (texture2D texture var_texcoord0.xy)) var_normal.z) 1.0))))

(def shader-pos-nrm-tex (shader/make-shader ::shader shader-ver-pos-nrm-tex shader-frag-pos-nrm-tex))

(defn- mesh->vb [^Matrix4d world-transform mesh]
  (let [positions         (vec (partition 3 (:positions mesh)))
        normals           (vec (partition 3 (:normals mesh)))
        texcoords         (vec (partition 2 (:texcoord0 mesh)))
        positions-indices (vec (:indices mesh))
        normals-indices   (vec (:normals-indices mesh))
        texcoords-indices (vec (:texcoord0-indices mesh))
        vcount (count positions-indices)
        normal-transform (let [tmp (Matrix3d.)]
                           (.getRotationScale world-transform tmp)
                           (.invert tmp)
                           (.transpose tmp)
                           tmp)
        world-point (Point3d.)
        world-normal (Vector3d.)]
    (loop [vb (->vtx-pos-nrm-tex vcount)
           vi 0]
      (if (< vi vcount)
        (let [[model-px model-py model-pz] (nth positions (nth positions-indices vi 0) [0.0 0.0 0.0])
              [model-nx model-ny model-nz] (nth normals   (nth normals-indices vi 0)   [0.0 0.0 1.0])
              [tu tv]    (nth texcoords (nth texcoords-indices vi 0) [0.0 0.0])]
          (.set world-point model-px model-py model-pz)
          (.transform world-transform world-point)
          (.set world-normal model-nx model-ny model-nz)
          (.transform normal-transform world-normal)
          (.normalize world-normal) ; need to normalize since since normal-transform may be scaled
          (recur (conj! vb [(.x world-point) (.y world-point) (.z world-point)
                            (.x world-normal) (.y world-normal) (.z world-normal)
                            tu tv]) (inc vi)))
        (persistent! vb)))))

(defn mesh-set->vbs [world-transform mesh-set]
  (into []
        (comp (mapcat :meshes)
              (remove (comp :positions empty?))
              (map (partial mesh->vb world-transform)))
        (:mesh-entries mesh-set)))

(defn render-mesh [^GL2 gl render-args renderables rcount]
  (let [pass (:pass render-args)
        renderable (first renderables)
        node-id (:node-id renderable)
        user-data (:user-data renderable)
        world-transform (:world-transform renderable)
        vbs (try (mesh-set->vbs world-transform (:mesh-set user-data)) (catch Exception _ []))
        shader (:shader user-data)
        textures (:textures user-data)]
    (cond
      (= pass pass/outline)
      (let [outline-vertex-binding (vtx/use-with [node-id ::outline] (render/gen-outline-vb renderables rcount) render/shader-outline)]
        (gl/with-gl-bindings gl render-args [render/shader-outline outline-vertex-binding]
          (gl/gl-draw-arrays gl GL/GL_LINES 0 (* rcount 8))))

      (= pass pass/opaque)
      (doseq [vb vbs]
        (let [vertex-binding (vtx/use-with [node-id ::mesh] vb shader)]
          (gl/with-gl-bindings gl render-args [shader vertex-binding]
            (doseq [[name t] textures]
              (gl/bind gl t render-args)
              (shader/set-uniform shader gl name (- (:unit t) GL/GL_TEXTURE0)))
            (gl/gl-enable gl GL/GL_CULL_FACE)
            (gl/gl-cull-face gl GL/GL_BACK)
            (.glBlendFunc gl GL/GL_ONE GL/GL_ONE_MINUS_SRC_ALPHA)
            (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 (count vb))
            (gl/gl-disable gl GL/GL_CULL_FACE)
            (.glBlendFunc gl GL/GL_SRC_ALPHA GL/GL_ONE_MINUS_SRC_ALPHA)
            (doseq [[name t] textures]
              (gl/unbind gl t render-args)))))

      (= pass pass/selection)
      (doseq [vb vbs]
        (let [vertex-binding (vtx/use-with [node-id ::mesh] vb shader)]
          (gl/with-gl-bindings gl render-args [shader vertex-binding]
            (gl/gl-enable gl GL/GL_CULL_FACE)
            (gl/gl-cull-face gl GL/GL_BACK)
            (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 (count vb))
            (gl/gl-disable gl GL/GL_CULL_FACE)))))))

(g/defnk produce-animation-set [content]
  (:animation-set content))

(g/defnk produce-animation-set-build-target [_node-id resource animation-set]
  (let [animation-set-with-hash-ids (animation-set/hash-animation-set-ids animation-set)]
    (rig/make-animation-set-build-target (resource/workspace resource) _node-id animation-set-with-hash-ids)))

(g/defnk produce-mesh-set [content]
  (:mesh-set content))

(g/defnk produce-mesh-set-build-target [_node-id resource mesh-set]
  (rig/make-mesh-set-build-target (resource/workspace resource) _node-id mesh-set))

(g/defnk produce-skeleton [content]
  (:skeleton content))

(g/defnk produce-skeleton-build-target [_node-id resource skeleton]
  (rig/make-skeleton-build-target (resource/workspace resource) _node-id skeleton))

(g/defnk produce-content [resource]
  (try
    (with-open [stream (io/input-stream resource)]
      (collada/load-scene stream))
    (catch NumberFormatException _
      (error-values/error-fatal "The scene contains invalid numbers, likely produced by a buggy exporter."))))

(g/defnk produce-scene [_node-id aabb mesh-set]
  (try
    ;; Provoke errors that would occur during vb creation.
    ;; Can't reuse for rendering, need to apply world transform.
    (mesh-set->vbs (math/->mat4) mesh-set)
    {:node-id _node-id
     :aabb aabb
     :renderable {:render-fn render-mesh
                  :batch-key _node-id
                  :select-batch-key _node-id
                  :user-data {:mesh-set mesh-set
                              :shader shader-pos-nrm-tex
                              :textures {"texture" texture/white-pixel}}
                  :passes [pass/opaque pass/selection pass/outline]}}
    (catch Exception _
      (error-values/error-fatal "Failed to produce vertex buffers from mesh set. The scene might contain invalid data."))))

(g/defnode ColladaSceneNode
  (inherits project/ResourceNode)

  (output content g/Any :cached produce-content)
  (output aabb AABB :cached (g/fnk [mesh-set]
                                   (reduce (fn [aabb [x y z]]
                                             (geom/aabb-incorporate aabb x y z))
                                           (geom/null-aabb)
                                           (partition 3 (get-in mesh-set [:components 0 :positions])))))
  (output animation-set g/Any produce-animation-set)
  (output animation-set-build-target g/Any :cached produce-animation-set-build-target)
  (output mesh-set g/Any produce-mesh-set)
  (output mesh-set-build-target g/Any :cached produce-mesh-set-build-target)
  (output skeleton g/Any produce-skeleton)
  (output skeleton-build-target g/Any :cached produce-skeleton-build-target)
  (output scene g/Any :cached produce-scene))

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace
                                    :ext "dae"
                                    :label "Collada Scene"
                                    :node-type ColladaSceneNode
                                    :icon mesh-icon
                                    :view-types [:scene :text]))
