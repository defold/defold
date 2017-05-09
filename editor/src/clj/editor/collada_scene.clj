(ns editor.collada-scene
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.animation-set :as animation-set]
            [editor.collada :as collada]
            [editor.defold-project :as project]
            [editor.gl :as gl]
            [editor.gl.shader :as shader]
            [editor.gl.vertex :as vtx1]
            [editor.gl.vertex2 :as vtx]
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

(defn- transform-array3! [^doubles array xform]
  (let [d3 (double-array 3)
        c (/ (alength array) 3)]
    (loop [i 0]
      (if (< i c)
        (do
          (System/arraycopy array (* i 3) d3 0 3)
          (xform d3)
          (System/arraycopy d3 0 array (* i 3) 3)
          (recur (inc i)))
        array))))

(defn- to-scratch! [mesh scratch component]
  (let [^doubles src (get mesh component)
        c (alength src)
        ^doubles dst (get scratch component)]
    (System/arraycopy src 0 dst 0 c)
    dst))

(defn mesh->vb! [vb ^Matrix4d world-transform mesh scratch]
  (let [^doubles positions (to-scratch! mesh scratch :positions)
        ^doubles normals   (to-scratch! mesh scratch :normals)
        ^doubles texcoords (to-scratch! mesh scratch :texcoord0)
        ^ints positions-indices (:indices mesh)
        ^ints normals-indices   (:normals-indices mesh)
        ^ints texcoords-indices (:texcoord0-indices mesh)
        vcount (alength positions-indices)
        ^doubles positions (let [world-point (Point3d.)]
                             (transform-array3! positions (fn [^doubles d3]
                                                            (.set world-point d3)
                                                            (.transform world-transform world-point)
                                                            (.get world-point d3))))
        ^doubles normals (let [world-normal (Vector3d.)
                               normal-transform (let [tmp (Matrix3d.)]
                                                  (.getRotationScale world-transform tmp)
                                                  (.invert tmp)
                                                  (.transpose tmp)
                                                  tmp)]
                           (transform-array3! normals (fn [^doubles d3]
                                                        (.set world-normal d3)
                                                        (.transform normal-transform world-normal)
                                                        (.normalize world-normal) ; need to normalize since since normal-transform may be scaled
                                                        (.get world-normal d3))))]
    (vtx/clear! vb)
    (loop [vb vb
           vi 0]
      (if (< vi vcount)
        (let [pi (* 3 (aget positions-indices vi))
              ni (* 3 (aget normals-indices vi))
              ti (* 2 (aget texcoords-indices vi))]
          (recur (vtx-pos-nrm-tex-put! vb
                   (aget positions pi) (aget positions (+ 1 pi)) (aget positions (+ 2 pi))
                   (aget normals ni) (aget normals (+ 1 ni)) (aget normals (+ 2 ni))
                   (aget texcoords ti) (aget texcoords (+ 1 ti)))
            (inc vi)))
        (vtx/prepare! vb)))))

(defn render-scene [^GL2 gl render-args renderables rcount]
  (let [pass (:pass render-args)]
    ;; Effectively ignoring batch-rendering
    (doseq [renderable renderables]
      (let [node-id (:node-id renderable)
            user-data (:user-data renderable)
            world-transform (:world-transform renderable)
            vb (:vbuf user-data)
            scratch (:scratch-arrays user-data)
            meshes (:meshes user-data)
            shader (:shader user-data)
            textures (:textures user-data)]
        (cond
          (= pass pass/outline)
          (let [outline-vertex-binding (vtx1/use-with [node-id ::outline] (render/gen-outline-vb renderables rcount) render/shader-outline)]
            (gl/with-gl-bindings gl render-args [render/shader-outline outline-vertex-binding]
              (gl/gl-draw-arrays gl GL/GL_LINES 0 (* rcount 8))))

          (= pass pass/opaque)
          (doseq [mesh meshes]
            (let [vb (mesh->vb! vb world-transform mesh scratch)
                  vertex-binding (vtx/use-with [node-id ::mesh] vb shader)]
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
          (doseq [mesh meshes]
            (let [vb (mesh->vb! vb world-transform mesh scratch)
                  vertex-binding (vtx/use-with [node-id ::mesh] vb shader)]
              (gl/with-gl-bindings gl render-args [shader vertex-binding]
                (gl/gl-enable gl GL/GL_CULL_FACE)
                (gl/gl-cull-face gl GL/GL_BACK)
                (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 (count vb))
                (gl/gl-disable gl GL/GL_CULL_FACE)))))))))

(g/defnk produce-animation-set [content]
  (:animation-set content))

(g/defnk produce-animation-set-build-target [_node-id resource animation-set]
  (let [animation-set-with-hash-ids (animation-set/hash-animation-set-ids animation-set)]
    (rig/make-animation-set-build-target (resource/workspace resource) _node-id animation-set-with-hash-ids)))

(g/defnk produce-mesh-set [content]
  (:mesh-set content))

(defn- arrayify [mesh]
  (-> mesh
    (update :positions double-array)
    (update :normals double-array)
    (update :texcoord0 double-array)
    (update :indices int-array)
    (update :normals-indices int-array)
    (update :texcoord0-indices int-array)))

(g/defnk produce-meshes [mesh-set]
  (into []
    (comp (mapcat :meshes)
      (remove (comp :positions empty?))
      (map arrayify))
    (:mesh-entries mesh-set)))

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

(defn- vbuf-size [meshes]
  (reduce (fn [sz m] (max sz (alength ^ints (:indices m)))) 0 meshes))

(defn- index-oob [vs is comp-count]
  (> (* comp-count (reduce max is)) (count vs)))

(defn- validate-meshes [meshes]
  (when-let [es (seq (keep (fn [m]
                             (let [{:keys [^doubles positions ^doubles normals ^doubles texcoord0 ^ints indices ^ints normals-indices ^ints texcoord0-indices]} m]
                               (when (or (not (= (alength indices) (alength normals-indices) (alength texcoord0-indices)))
                                       (index-oob positions indices 3)
                                       (index-oob normals normals-indices 3)
                                       (index-oob texcoord0 texcoord0-indices 2))
                                 (error-values/error-fatal "Failed to produce vertex buffers from mesh set. The scene might contain invalid data."))))
                       meshes))]
    (error-values/error-aggregate es)))

(defn- gen-scratch-arrays [meshes]
  (into {} (map (fn [component] [component (double-array (reduce max 0 (map (comp count component) meshes)))]) [:positions :normals :texcoord0])))

(g/defnk produce-scene [_node-id aabb meshes]
  (or (validate-meshes meshes)
    {:node-id _node-id
     :aabb aabb
     :renderable {:render-fn render-scene
                  :batch-key _node-id
                  :select-batch-key _node-id
                  :user-data {:meshes meshes
                              :vbuf (->vtx-pos-nrm-tex (vbuf-size meshes))
                              :shader shader-pos-nrm-tex
                              :textures {"texture" texture/white-pixel}
                              :scratch-arrays (gen-scratch-arrays meshes)}
                  :passes [pass/opaque pass/selection pass/outline]}}))

(g/defnode ColladaSceneNode
  (inherits project/ResourceNode)

  (output content g/Any :cached produce-content)
  (output aabb AABB :cached (g/fnk [meshes]
                              (loop [aabb (geom/null-aabb)
                                     meshes meshes]
                                (if-let [m (first meshes)]
                                  (let [^doubles ps (:positions m)
                                        c (alength ps)]
                                    (loop [i 0
                                           aabb aabb]
                                      (if (< i c)
                                        (let [x (aget ps i)
                                              y (aget ps (+ 1 i))
                                              z (aget ps (+ 2 i))]
                                          (recur (+ i 3) (geom/aabb-incorporate aabb x y z)))
                                        aabb)))
                                  aabb))))
  (output animation-set g/Any produce-animation-set)
  (output animation-set-build-target g/Any :cached produce-animation-set-build-target)
  (output mesh-set g/Any produce-mesh-set)
  (output meshes g/Any :cached produce-meshes)
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
