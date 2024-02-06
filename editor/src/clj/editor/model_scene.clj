;; Copyright 2020-2024 The Defold Foundation
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

(ns editor.model-scene
  (:require [dynamo.graph :as g]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.pass :as pass]
            [editor.gl.shader :as shader]
            [editor.gl.texture :as texture]
            [editor.gl.vertex2 :as vtx]
            [editor.graphics :as graphics]
            [editor.math :as math]
            [editor.model-loader :as model-loader]
            [editor.render :as render]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.rig :as rig]
            [editor.scene-cache :as scene-cache]
            [editor.scene-picking :as scene-picking]
            [editor.workspace :as workspace]
            [internal.graph.error-values :as error-values])
  (:import [com.google.protobuf ByteString]
           [com.jogamp.opengl GL GL2]
           [editor.gl.vertex2 VertexBuffer]
           [editor.types AABB]
           [java.nio ByteBuffer ByteOrder FloatBuffer IntBuffer ShortBuffer]
           [javax.vecmath Matrix3f Matrix4d Matrix4f Point3d Point3f Vector3f]))

(set! *warn-on-reflection* true)

(def mesh-icon "icons/32/Icons_27-AT-Mesh.png")
(def model-file-types ["dae" "gltf" "glb"])
(def animation-file-types ["animationset" "dae" "gltf" "glb"])

(vtx/defvertex vtx-pos-nrm-tex
  (vec3 position)
  (vec3 normal)
  (vec2 texcoord0))

(vtx/defvertex vtx-pos-tex
  (vec3 position)
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
  (uniform sampler2D texture_sampler)
  (defn void main []
    (setq gl_FragColor (vec4 (* (.xyz (texture2D texture_sampler var_texcoord0.xy)) var_normal.z) 1.0))))

(def shader-pos-nrm-tex (shader/make-shader ::shader shader-ver-pos-nrm-tex shader-frag-pos-nrm-tex))

(shader/defshader model-id-vertex-shader
  (attribute vec4 position)
  (attribute vec2 texcoord0)
  (varying vec2 var_texcoord0)
  (defn void main []
    (setq gl_Position (* gl_ModelViewProjectionMatrix position))
    (setq var_texcoord0 texcoord0)))

(shader/defshader model-id-fragment-shader
  (varying vec2 var_texcoord0)
  (uniform sampler2D texture_sampler)
  (uniform vec4 id)
  (defn void main []
    (setq vec4 color (texture2D texture_sampler var_texcoord0.xy))
    (if (> color.a 0.05)
      (setq gl_FragColor id)
      (discard))))

(def id-shader (shader/make-shader ::model-id-shader model-id-vertex-shader model-id-fragment-shader {"id" :id}))

(defn- transf-n
  [^Matrix3f m3f normals]
  (let [normal-tmp (Vector3f.)]
    (mapv (fn [[^float x ^float y ^float z]]
            (.set normal-tmp x y z)
            (.transform m3f normal-tmp)
            (.normalize normal-tmp) ; need to normalize since normal-transform may be scaled
            [(.x normal-tmp) (.y normal-tmp) (.z normal-tmp)])
          normals)))

(defn- mesh->attribute-data [mesh ^Matrix4d world-transform ^Matrix3f normal-transform vertex-attribute-bytes vertex-space]
  (let [^ints indices (:indices mesh)
        ^floats positions (:positions mesh)
        ^floats texcoords (:texcoord0 mesh)
        ^floats normals   (:normals mesh)
        mesh-data-out
        (reduce (fn [out-data ^long vi]
                  (let [p-base (* 3 vi)
                        p-0 (double (get positions p-base))
                        p-1 (double (get positions (+ 1 p-base)))
                        p-2 (double (get positions (+ 2 p-base)))

                        tc0-base (* 2 vi)
                        tc0-0 (double (get texcoords tc0-base))
                        tc0-1 (double (get texcoords (+ 1 tc0-base)))

                        n-base (* 3 vi)
                        n-0 (get normals n-base)
                        n-1 (get normals (+ 1 n-base))
                        n-2 (get normals (+ 2 n-base))]
                    (assoc out-data
                      :position-data (conj (:position-data out-data) [p-0 p-1 p-2])
                      :normal-data (conj (:normal-data out-data) [n-0 n-1 n-2])
                      :uv-data (conj (:uv-data out-data) [tc0-0 tc0-1]))))
                {:position-data []
                 :uv-data []
                 :normal-data []
                 :world-transform world-transform
                 :vertex-attribute-bytes vertex-attribute-bytes}
                indices)]
    (-> mesh-data-out
        (cond-> (= vertex-space :vertex-space-world)
                (assoc :position-data (geom/transf-p world-transform (:position-data mesh-data-out))
                       :normal-data (transf-n normal-transform (:normal-data mesh-data-out))))
        (assoc :texcoord-datas [{:uv-data (:uv-data mesh-data-out)}]))))

(defn mesh->vb! [^VertexBuffer vbuf ^Matrix4d world-transform vertex-space vertex-attribute-bytes mesh]
  (let [normal-transform (let [tmp (Matrix3f.)]
                           (.getRotationScale world-transform tmp)
                           (.invert tmp)
                           (.transpose tmp)
                           tmp)
        mesh-data (mesh->attribute-data mesh world-transform normal-transform vertex-attribute-bytes vertex-space)]
    (graphics/put-attributes! vbuf [mesh-data])
    vbuf))

(defn- request-vb! [^GL2 gl node-id mesh ^Matrix4d world-transform vertex-space vertex-description vertex-attribute-bytes]
  (let [clj-world (math/vecmath->clj world-transform)
        request-id [node-id mesh]
        data {:mesh mesh :world-transform clj-world :vertex-space vertex-space :vertex-description vertex-description :vertex-attribute-bytes vertex-attribute-bytes}]
    (scene-cache/request-object! ::vb request-id gl data)))

(defn- render-scene-opaque [^GL2 gl render-args renderables rcount]
  (let [renderable (first renderables)
        user-data (:user-data renderable)
        shader (:shader user-data)
        textures (:textures user-data)
        vertex-space (:vertex-space user-data)
        meshes (:meshes user-data)
        mesh (first meshes)
        ^Matrix4d local-transform (:transform mesh) ; Each mesh uses the local matrix of the model it belongs to
        ^Matrix4d world-transform (:world-transform renderable)
        world-matrix (doto (Matrix4d. world-transform) (.mul local-transform))
        vertex-space-world-transform (if (= vertex-space :vertex-space-world)
                                       (doto (Matrix4d.) (.setIdentity)) ; already applied the world transform to vertices
                                       world-matrix)
        render-args (merge render-args
                           (math/derive-render-transforms vertex-space-world-transform
                                                          (:view render-args)
                                                          (:projection render-args)
                                                          (:texture render-args)))]
    (gl/with-gl-bindings gl render-args [shader]
      (doseq [[name t] textures]
        (gl/bind gl t render-args)
        (shader/set-samplers-by-name shader gl name (:texture-units t)))
      (.glBlendFunc gl GL/GL_ONE GL/GL_ONE_MINUS_SRC_ALPHA)
      (gl/gl-enable gl GL/GL_CULL_FACE)
      (gl/gl-cull-face gl GL/GL_BACK)
      (doseq [renderable renderables
              :let [node-id (:node-id renderable)
                    user-data (:user-data renderable)
                    meshes (:meshes user-data)
                    mesh (first meshes)

                    ^Matrix4d local-transform (:transform mesh) ; Each mesh uses the local matrix of the model it belongs to
                    ^Matrix4d world-transform (:world-transform renderable)
                    world-matrix (doto (Matrix4d. world-transform) (.mul local-transform))

                    shader-bound-attributes (graphics/shader-bound-attributes gl shader (:material-attribute-infos user-data) [:position :texcoord0 :normal] :coordinate-space-local)
                    vertex-description (graphics/make-vertex-description shader-bound-attributes)
                    vertex-attribute-bytes (:vertex-attribute-bytes user-data)]
              mesh meshes
              :let [vb (request-vb! gl node-id mesh world-matrix vertex-space vertex-description vertex-attribute-bytes)
                    vertex-binding (vtx/use-with [node-id ::mesh] vb shader)]]
          (gl/with-gl-bindings gl render-args [vertex-binding]
            (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 (count vb))))
      (gl/gl-disable gl GL/GL_CULL_FACE)
      (.glBlendFunc gl GL/GL_SRC_ALPHA GL/GL_ONE_MINUS_SRC_ALPHA)
      (doseq [[name t] textures]
        (gl/unbind gl t render-args)))))

(defn- render-scene-opaque-selection [^GL2 gl render-args renderables _rcount]
  (let [renderable (first renderables)
        user-data (:user-data renderable)
        textures (:textures user-data)]
    (gl/gl-enable gl GL/GL_CULL_FACE)
    (gl/gl-cull-face gl GL/GL_BACK)
    (doseq [renderable renderables
            :let [node-id (:node-id renderable)
                  user-data (:user-data renderable)
                  meshes (:meshes user-data)
                  mesh (first meshes)
                  ^Matrix4d local-transform (:transform mesh) ; Each mesh uses the local matrix of the model it belongs to
                  ^Matrix4d world-transform (:world-transform renderable)
                  world-matrix (doto (Matrix4d. world-transform) (.mul local-transform))
                  render-args (assoc render-args :id (scene-picking/renderable-picking-id-uniform renderable))]]
      (gl/with-gl-bindings gl render-args [id-shader]
        (doseq [[name t] textures]
          (gl/bind gl t render-args)
          (shader/set-samplers-by-name id-shader gl name (:texture-units t)))
        (doseq [mesh meshes
                :let [vb (request-vb! gl node-id mesh world-matrix :vertex-space-world vtx-pos-tex nil)
                      vertex-binding (vtx/use-with [node-id ::mesh-selection] vb id-shader)]]
          (gl/with-gl-bindings gl render-args [vertex-binding]
            (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 (count vb))))
        (doseq [[name t] textures]
          (gl/unbind gl t render-args))))
    (gl/gl-disable gl GL/GL_CULL_FACE)))

(defn- render-scene [^GL2 gl render-args renderables rcount]
  (let [pass (:pass render-args)]
    (condp = pass
      pass/opaque
      (render-scene-opaque gl render-args renderables rcount)

      pass/opaque-selection
      (render-scene-opaque-selection gl render-args renderables rcount))))

(defn- render-outline [^GL2 gl render-args renderables rcount]
  (let [pass (:pass render-args)]
    (condp = pass
      pass/outline
      (let [renderable (first renderables)
            node-id (:node-id renderable)]
        (render/render-aabb-outline gl render-args [node-id ::outline] renderables rcount)))))

(g/defnk produce-mesh-set [content]
  (:mesh-set content))

(defn- doubles->floats [ds]
  (float-array (map float ds)))

(defn- bytes-to-indices [^ByteString indices index-format]
  (let [num-bytes (.size indices)
        ba (byte-array num-bytes)
        _ (.copyTo indices ba 0)
        bb (ByteBuffer/wrap ba)
        _ (.order bb ByteOrder/LITTLE_ENDIAN)
        bo (if (= :indexbuffer-format-16 index-format)
             (.asShortBuffer bb)
             (.asIntBuffer bb))

        ;; Make it into a human readable/printable int array
        count (if (= :indexbuffer-format-16 index-format)
                (/ num-bytes 2)
                (/ num-bytes 4))
        ia (int-array count)]
    (if (= :indexbuffer-format-16 index-format)
      (dotimes [i count]
        (aset ia i (int (.get ^ShortBuffer bo i))))
      (dotimes [i count]
        (aset ia i (.get ^IntBuffer bo i))))
    ia))

(defn- arrayify [mesh]
  (-> mesh
      (update :positions doubles->floats)
      (update :normals doubles->floats)
      (update :texcoord0 doubles->floats)
      (update :indices (fn [bytes] (bytes-to-indices bytes (:indices-format mesh))))))

(defn- get-and-update-meshes [model]
  (let [transform (:local model)
        local-matrix (math/clj->mat4 (:translation transform) (:rotation transform) (:scale transform))
        meshes (:meshes model)
        out (map arrayify meshes)
        out (map (fn [mesh] (assoc mesh :transform local-matrix)) out)]
    out))

(g/defnk produce-meshes [mesh-set]
  (let [models (:models mesh-set)
        meshes (flatten (map get-and-update-meshes models))]
    meshes))

(g/defnk produce-mesh-set-build-target [_node-id resource mesh-set]
  (rig/make-mesh-set-build-target (resource/workspace resource) _node-id mesh-set))

(g/defnk produce-skeleton [content]
  (:skeleton content))

(g/defnk produce-skeleton-build-target [_node-id resource skeleton]
  (rig/make-skeleton-build-target (resource/workspace resource) _node-id skeleton))

(g/defnk produce-bones [content]
  (:bones content))

(g/defnk produce-content [_node-id resource]
      (model-loader/load-scene _node-id resource))

(g/defnk produce-animation-info [resource]
  [{:path (resource/proj-path resource) :parent-id "" :resource resource}])

(g/defnk produce-animation-ids [content]
  (:animation-ids content))

(g/defnk produce-material-ids [content]
  (let [ret (:material-ids content)]
    (if (zero? (count ret))
      ["default"]
      ret)))

(defn- index-oob [vs is comp-count]
  (> (* comp-count (reduce max 0 is)) (count vs)))

(defn- validate-meshes [meshes]
  (when-let [es (seq (keep (fn [m]
                             (let [{:keys [^floats positions ^floats normals ^floats texcoord0 ^ints indices]} m]
                               (when (or (and (not= (alength normals) 0) (not= (alength positions) (alength normals))) ; normals optional
                                         (index-oob positions indices 3))
                                 (error-values/error-fatal "Failed to produce vertex buffers from mesh set. The scene might contain invalid data."))))
                       meshes))]
    (error-values/error-aggregate es)))

(g/defnk produce-scene [_node-id aabb meshes]
  (or (validate-meshes meshes)
      {:node-id _node-id
       :aabb aabb
       :renderable {:render-fn render-scene
                    :tags #{:model}
                    :batch-key _node-id
                    :select-batch-key _node-id
                    :user-data {:meshes meshes
                                :shader shader-pos-nrm-tex
                                :textures {"texture" @texture/white-pixel}}
                    :passes [pass/opaque pass/opaque-selection]}
       :children [{:node-id _node-id
                   :aabb aabb
                   :renderable {:render-fn render-outline
                                :tags #{:model :outline}
                                :batch-key _node-id
                                :select-batch-key _node-id
                                :passes [pass/outline]}}]}))

(g/defnode ModelSceneNode
  (inherits resource-node/ResourceNode)

  (output content g/Any :cached produce-content)
  
  ; TODO: Ask for this info from the importer
  (output aabb AABB :cached (g/fnk [meshes]
                              (loop [aabb geom/null-aabb
                                     meshes meshes]
                                (if-let [m (first meshes)]
                                  (let [^floats ps (:positions m)
                                        c (alength ps)
                                        ^Matrix4d local-transform (:transform m) ; Each mesh uses the local matrix of the model it belongs to
                                        aabb (loop [i 0
                                                    aabb aabb]
                                               (if (< i c)
                                                 (let [x (aget ps i)
                                                       y (aget ps (+ 1 i))
                                                       z (aget ps (+ 2 i))
                                                       p (Point3d. x y z)
                                                       _ (.transform local-transform p)]
                                                   (recur (+ i 3) (geom/aabb-incorporate aabb p)))
                                                 aabb))]
                                    (recur aabb (next meshes)))
                                  aabb))))
  (output bones g/Any produce-bones)
  (output animation-info g/Any produce-animation-info)
  (output animation-ids g/Any produce-animation-ids)
  (output material-ids g/Any produce-material-ids)
  (output mesh-set g/Any produce-mesh-set)
  (output meshes g/Any :cached produce-meshes)
  (output mesh-set-build-target g/Any :cached produce-mesh-set-build-target)
  (output skeleton g/Any produce-skeleton)
  (output skeleton-build-target g/Any :cached produce-skeleton-build-target)
  (output scene g/Any :cached produce-scene))

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace
                                    :ext model-file-types
                                    :label "Model Scene"
                                    :node-type ModelSceneNode
                                    :icon mesh-icon
                                    :view-types [:scene :text]))

(defn- update-vb [^GL2 gl ^VertexBuffer vb data]
  (let [{:keys [mesh world-transform vertex-space vertex-attribute-bytes]} data
        world-transform (doto (Matrix4d.) (math/clj->vecmath world-transform))]
    (mesh->vb! vb world-transform vertex-space vertex-attribute-bytes mesh)))

(defn- make-vb [^GL2 gl data]
  (let [{:keys [mesh vertex-description]} data
        vbuf (vtx/make-vertex-buffer vertex-description :dynamic (alength ^ints (:indices mesh)))]
    (update-vb gl vbuf data)))

(defn- destroy-vbs [^GL2 gl vbs _])

(scene-cache/register-object-cache! ::vb make-vb update-vb destroy-vbs)
