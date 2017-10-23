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
            [editor.math :as math]
            [editor.render :as render]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.rig :as rig]
            [editor.scene-cache :as scene-cache]
            [editor.validation :as validation]
            [editor.workspace :as workspace]
            [internal.graph.error-values :as error-values])
  (:import [com.jogamp.opengl GL GL2]
           [java.nio ByteBuffer FloatBuffer]
           [editor.types AABB]
           [editor.gl.vertex2 VertexBuffer]
           [javax.vecmath Matrix3f Matrix4d Matrix4f Point3f Vector3f]))

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

(defmacro umul [a b]
  `(unchecked-multiply ~a ~b))

(defmacro uadd [a b]
  `(unchecked-add-int ~a ~b))

(defn- transform-array3! [^floats array xform]
  (let [d3 (float-array 3)
        c (/ (alength array) 3)]
    (dotimes [i c]
      (System/arraycopy array (umul i 3) d3 0 3)
      (xform d3)
      (System/arraycopy d3 0 array (umul i 3) 3))
    array))

(defn- to-scratch! [mesh scratch component]
  (let [^floats src (get mesh component)
        c (alength src)
        ^floats dst (get scratch component)]
    (System/arraycopy src 0 dst 0 c)
    dst))

(defmacro put! [vb x y z]
  `(vtx-pos-nrm-tex-put! ~vb ~x ~y ~z 0 0 0 0 0))

(defn mesh->vb! [^VertexBuffer vb ^Matrix4d world-transform mesh scratch]
  (let [^floats positions (to-scratch! mesh scratch :positions)
        ^floats normals   (to-scratch! mesh scratch :normals)
        ^floats texcoords (to-scratch! mesh scratch :texcoord0)
        ^ints positions-indices (:indices mesh)
        ^ints normals-indices   (:normals-indices mesh)
        ^ints texcoords-indices (:texcoord0-indices mesh)
        vcount (alength positions-indices)
        ^Matrix4f world-transform (Matrix4f. world-transform)
        ^floats positions (let [world-point (Point3f.)]
                            (transform-array3! positions (fn [^floats d3]
                                                           (.set world-point d3)
                                                           (.transform world-transform world-point)
                                                           (.get world-point d3))))
       ^floats normals (let [world-normal (Vector3f.)
                             normal-transform (let [tmp (Matrix3f.)]
                                                (.getRotationScale world-transform tmp)
                                                (.invert tmp)
                                                (.transpose tmp)
                                                tmp)]
                         (transform-array3! normals (fn [^floats d3]
                                                      (.set world-normal d3)
                                                      (.transform normal-transform world-normal)
                                                      (.normalize world-normal) ; need to normalize since since normal-transform may be scaled
                                                      (.get world-normal d3))))]
    ;; Raw access to run as fast as possible
    ;; 3x faster than using generated *-put! function
    ;; Not clear how to turn this into pretty API
    (let [^ByteBuffer b (.buf vb)
          ^FloatBuffer fb (.asFloatBuffer b)]
      (dotimes [vi vcount]
        (let [pi (aget positions-indices vi)
              ni (aget normals-indices vi)
              ti (aget texcoords-indices vi)]
          (.put fb positions (umul 3 pi) 3)
          (.put fb normals (umul 3 ni) 3)
          (.put fb texcoords (umul 2 ti) 2))))
    ;; Since we have bypassed the vb and internal ByteBuffer, manually update the position
    (vtx/position! vb vcount)))

(defn- request-vb! [^GL2 gl node-id mesh ^Matrix4d world-transform scratch]
  (let [clj-world (math/vecmath->clj world-transform)
        request-id [node-id mesh]
        data {:mesh mesh :world-transform clj-world :scratch scratch}]
    (scene-cache/request-object! ::vb request-id gl data)))

(defn render-scene [^GL2 gl render-args renderables rcount]
  (let [pass (:pass render-args)]
    (cond
      (or (= pass pass/opaque) (= pass pass/selection))
      (let [renderable (first renderables)
            node-id (:node-id renderable)
            user-data (:user-data renderable)
            scratch (:scratch-arrays user-data)
            meshes (:meshes user-data)
            shader (:shader user-data)
            textures (:textures user-data)]
        (gl/with-gl-bindings gl render-args [shader]
          (when (= pass pass/opaque)
            (doseq [[name t] textures]
              (gl/bind gl t render-args)
              (shader/set-uniform shader gl name (- (:unit t) GL/GL_TEXTURE0)))
            (.glBlendFunc gl GL/GL_ONE GL/GL_ONE_MINUS_SRC_ALPHA))
          (gl/gl-enable gl GL/GL_CULL_FACE)
          (gl/gl-cull-face gl GL/GL_BACK)
          (doseq [renderable renderables]
            (let [node-id (:node-id renderable)
                  user-data (:user-data renderable)
                  meshes (:meshes user-data)
                  world-transform (:world-transform renderable)]
              (doseq [mesh meshes]
                (let [vb (request-vb! gl node-id mesh world-transform scratch)
                      vertex-binding (vtx/use-with [node-id ::mesh] vb shader)]
                  (gl/with-gl-bindings gl render-args [vertex-binding]
                    (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 (count vb)))))))
          (gl/gl-disable gl GL/GL_CULL_FACE)
          (when (= pass pass/opaque)
            (.glBlendFunc gl GL/GL_SRC_ALPHA GL/GL_ONE_MINUS_SRC_ALPHA)
            (doseq [[name t] textures]
              (gl/unbind gl t render-args)))))

      (= pass pass/outline)
      (let [renderable (first renderables)
            node-id (:node-id renderable)]
        (render/render-aabb-outline gl render-args [node-id ::outline] renderables rcount)))))

(g/defnk produce-animation-set [resource content]
  (let [animation-set (:animation-set content)
        id (resource/base-name resource)]
    (update animation-set :animations
            (fn [animations]
              (into [] (map #(assoc % :id id)) animations)))))

(g/defnk produce-animation-set-build-target [_node-id resource animation-set]
  (let [animation-set-with-hash-ids (animation-set/hash-animation-set-ids animation-set)]
    (rig/make-animation-set-build-target (resource/workspace resource) _node-id animation-set-with-hash-ids)))

(g/defnk produce-mesh-set [content]
  (:mesh-set content))

(defn- doubles->floats [ds]
  (float-array (map float ds)))

(defn- arrayify [mesh]
  (-> mesh
    (update :positions doubles->floats)
    (update :normals doubles->floats)
    (update :texcoord0 doubles->floats)
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

(g/defnk produce-content [_node-id resource]
  (try
    (with-open [stream (io/input-stream resource)]
      (collada/load-scene stream))
    (catch NumberFormatException _
      (error-values/error-fatal "The scene contains invalid numbers, likely produced by a buggy exporter." {:type :invalid-content}))
    (catch java.io.FileNotFoundException _
      (validation/file-not-found-error _node-id :content :fatal resource))
    (catch Exception _
      (validation/invalid-content-error _node-id :content :fatal resource))))

(defn- vbuf-size [meshes]
  (reduce (fn [sz m] (max sz (alength ^ints (:indices m)))) 0 meshes))

(defn- index-oob [vs is comp-count]
  (> (* comp-count (reduce max is)) (count vs)))

(defn- validate-meshes [meshes]
  (when-let [es (seq (keep (fn [m]
                             (let [{:keys [^floats positions ^floats normals ^floats texcoord0 ^ints indices ^ints normals-indices ^ints texcoord0-indices]} m]
                               (when (or (not (= (alength indices) (alength normals-indices) (alength texcoord0-indices)))
                                       (index-oob positions indices 3)
                                       (index-oob normals normals-indices 3)
                                       (index-oob texcoord0 texcoord0-indices 2))
                                 (error-values/error-fatal "Failed to produce vertex buffers from mesh set. The scene might contain invalid data."))))
                       meshes))]
    (error-values/error-aggregate es)))

(defn- gen-scratch-arrays [meshes]
  (into {} (map (fn [component] [component (float-array (reduce max 0 (map (comp count component) meshes)))]) [:positions :normals :texcoord0])))

(g/defnk produce-scene [_node-id aabb meshes]
  (or (validate-meshes meshes)
    {:node-id _node-id
     :aabb aabb
     :renderable {:render-fn render-scene
                  :batch-key _node-id
                  :select-batch-key _node-id
                  :user-data {:meshes meshes
                              :shader shader-pos-nrm-tex
                              :textures {"texture" texture/white-pixel}
                              :scratch-arrays (gen-scratch-arrays meshes)}
                  :passes [pass/opaque pass/selection pass/outline]}}))

(g/defnode ColladaSceneNode
  (inherits resource-node/ResourceNode)

  (output content g/Any :cached produce-content)
  (output aabb AABB :cached (g/fnk [meshes]
                              (loop [aabb (geom/null-aabb)
                                     meshes meshes]
                                (if-let [m (first meshes)]
                                  (let [^floats ps (:positions m)
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

(defn- update-vb [^GL2 gl ^VertexBuffer vb data]
  (let [{:keys [mesh world-transform scratch]} data]
    (-> vb
      (vtx/clear!)
      (mesh->vb! (doto (Matrix4d.) (math/clj->vecmath world-transform)) mesh scratch)
      (vtx/flip!))))

(defn- make-vb [^GL2 gl data]
  (let [{:keys [mesh]} data
        vb (->vtx-pos-nrm-tex (alength ^ints (:indices mesh)))]
    (update-vb gl vb data)))

(defn- destroy-vbs [^GL2 gl vbs _])

(scene-cache/register-object-cache! ::vb make-vb update-vb destroy-vbs)
