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
            [editor.scene :as scene]
            [editor.scene-cache :as scene-cache]
            [editor.scene-picking :as scene-picking]
            [editor.workspace :as workspace]
            [internal.graph.error-values :as error-values]
            [util.coll :as coll])
  (:import [com.google.protobuf ByteString]
           [com.jogamp.opengl GL GL2]
           [editor.gl.vertex2 VertexBuffer]
           [java.nio ByteOrder]
           [javax.vecmath Matrix4d]))

(set! *warn-on-reflection* true)

(def mesh-icon "icons/32/Icons_27-AT-Mesh.png")
(def model-file-types ["dae" "gltf" "glb"])
(def animation-file-types ["animationset" "dae" "gltf" "glb"])

(shader/defshader preview-vertex-shader
  (uniform mat4 world_view_proj_matrix)
  (uniform mat4 normal_matrix)
  (attribute vec4 position)
  (attribute vec4 normal)
  (attribute vec2 texcoord0)
  (varying vec3 var_normal)
  (varying vec2 var_texcoord0)
  (defn void main []
    (setq gl_Position (* world_view_proj_matrix position))
    (setq var_texcoord0 texcoord0)
    (setq var_normal (normalize (.xyz (* normal_matrix normal))))))

(shader/defshader preview-fragment-shader
  (varying vec3 var_normal)
  (varying vec2 var_texcoord0)
  (uniform sampler2D texture_sampler)
  (defn void main []
    (setq float brightness (.z var_normal))
    (setq vec3 color (.xyz (texture2D texture_sampler var_texcoord0)))
    (setq gl_FragColor (vec4 (* color brightness) 1.0))))

(def preview-shader (shader/make-shader ::shader preview-vertex-shader preview-fragment-shader {"normal_matrix" :normal "world_view_proj_matrix" :world-view-proj}))

(vtx/defvertex id-vertex
  (vec3 position)
  (vec2 texcoord0))

(shader/defshader id-vertex-shader
  (attribute vec4 position)
  (attribute vec2 texcoord0)
  (varying vec2 var_texcoord0)
  (defn void main []
    (setq gl_Position (* gl_ModelViewProjectionMatrix position))
    (setq var_texcoord0 texcoord0)))

(shader/defshader id-fragment-shader
  (varying vec2 var_texcoord0)
  (uniform sampler2D texture_sampler)
  (uniform vec4 id)
  (defn void main []
    (setq vec4 color (texture2D texture_sampler var_texcoord0.xy))
    (if (> color.a 0.05)
      (setq gl_FragColor id)
      (discard))))

(def id-shader (shader/make-shader ::id-shader id-vertex-shader id-fragment-shader {"id" :id}))

(defn- doubles->floats
  ^floats [ds]
  (float-array (map float ds)))

(defn- bytes-to-indices
  ^ints [^ByteString indices index-format]
  (let [byte-buffer (.order (.asReadOnlyByteBuffer indices) ByteOrder/LITTLE_ENDIAN)

        num-indices
        (int (case index-format
               :indexbuffer-format-16 (/ (.size indices) 2)
               :indexbuffer-format-32 (/ (.size indices) 4)))

        out-indices (int-array num-indices)]

    (case index-format
      :indexbuffer-format-16
      (let [short-buffer (.asShortBuffer byte-buffer)]
        (dotimes [i num-indices]
          (aset out-indices i (int (.get short-buffer i)))))

      :indexbuffer-format-32
      (let [int-buffer (.asIntBuffer byte-buffer)]
        (.get int-buffer out-indices)))

    out-indices))

(defn- flat->vectors [^long component-count interleaved-component-values]
  (let [vectors (into []
                      (coll/partition-all-primitives :double component-count)
                      interleaved-component-values)]
    ;; Return nil if the interleaved component values cannot be evenly
    ;; partitioned into the desired component count.
    (when (or (coll/empty? vectors)
              (= component-count (count (peek vectors))))
      vectors)))

(defn mesh->renderable-data [mesh]
  (let [positions (flat->vectors 3 (:positions mesh))
        normals (flat->vectors 3 (:normals mesh))
        tangents (flat->vectors 3 (:tangents mesh))
        colors (flat->vectors 4 (:colors mesh))
        texcoord0s (flat->vectors (:num-texcoord0-components mesh 0) (:texcoord0 mesh))
        texcoord1s (flat->vectors (:num-texcoord1-components mesh 0) (:texcoord1 mesh))
        indices (bytes-to-indices (:indices mesh) (:indices-format mesh))
        max-index (reduce max 0 indices)
        positions-count (count positions)]
    (if (and (some? positions)
             (some? normals)
             (some? tangents)
             (some? colors)
             (some? texcoord0s)
             (some? texcoord1s)
             (< max-index positions-count)
             (or (coll/empty? normals)
                 (= positions-count (count normals)))
             (or (coll/empty? tangents)
                 (= positions-count (count tangents)))
             (or (coll/empty? colors)
                 (= positions-count (count colors)))
             (or (coll/empty? texcoord0s)
                 (= positions-count (count texcoord0s)))
             (or (coll/empty? texcoord1s)
                 (= positions-count (count texcoord1s))))
      (let [position-data (some-> positions not-empty (mapv indices))
            normal-data (some-> normals not-empty (mapv indices))
            tangent-data (some-> tangents not-empty (mapv indices))
            color-data (some-> colors not-empty (mapv indices))
            texcoord0-data (some-> texcoord0s not-empty (mapv indices))
            texcoord1-data (some-> texcoord1s not-empty (mapv indices))
            texcoord-datas (cond-> [(when texcoord0-data {:uv-data texcoord0-data})]
                                   texcoord1-data (conj {:uv-data texcoord1-data}))]
        (cond-> {:position-data position-data}
                normal-data (assoc :normal-data normal-data)
                tangent-data (assoc :tangent-data tangent-data)
                color-data (assoc :color-data color-data)
                (some some? texcoord-datas) (assoc :texcoord-datas texcoord-datas)))
      (error-values/error-fatal "Failed to produce vertex buffers from mesh set. The scene might contain invalid data."))))

(defn mesh->vb! [^VertexBuffer vbuf ^Matrix4d world-transform ^Matrix4d normal-transform vertex-attribute-bytes mesh-renderable-data]
  (let [mesh-renderable-data
        (cond-> mesh-renderable-data
                world-transform (assoc :world-transform world-transform)
                normal-transform (assoc :normal-transform normal-transform)
                vertex-attribute-bytes (assoc :vertex-attribute-bytes vertex-attribute-bytes))]
    (graphics/put-attributes! vbuf [mesh-renderable-data])
    vbuf))

(defn- request-vb! [^GL2 gl request-id mesh-renderable-data ^Matrix4d attribute-world-transform ^Matrix4d attribute-normal-transform vertex-description vertex-attribute-bytes]
  (let [data {:mesh-renderable-data mesh-renderable-data
              :world-transform attribute-world-transform
              :normal-transform attribute-normal-transform
              :vertex-description vertex-description
              :vertex-attribute-bytes vertex-attribute-bytes}]
    (scene-cache/request-object! ::vb request-id gl data)))

(defn- make-matrix-attribute-data [mesh-renderable-data ^Matrix4d mtx]
  (let [vertex-count (count (:position-data mesh-renderable-data))
        world-transform-array (math/vecmath->clj (doto ^Matrix4d mtx (.transpose)))]
    (into [] (repeat vertex-count world-transform-array))))

(defn- render-mesh-opaque-impl [^GL2 gl render-args renderable request-prefix override-shader override-vertex-description extra-render-args]
  (let [{:keys [node-id user-data ^Matrix4d world-transform]} renderable
        {:keys [material-attribute-infos mesh-renderable-data textures vertex-attribute-bytes]} user-data
        normal-transform (math/derive-normal-transform world-transform)
        shader (or override-shader (:shader user-data))
        default-coordinate-space (case (:vertex-space user-data :vertex-space-local)
                                   :vertex-space-local :coordinate-space-local
                                   :vertex-space-world :coordinate-space-world)
        vertex-description (or override-vertex-description
                               (let [manufactured-attribute-keys [:position :texcoord0 :normal :mtx-world :mtx-normal]
                                     shader-bound-attributes (graphics/shader-bound-attributes gl shader material-attribute-infos manufactured-attribute-keys default-coordinate-space)]
                                 (graphics/make-vertex-description shader-bound-attributes)))
        vertex-attributes (:attributes vertex-description)
        coordinate-space-info (graphics/coordinate-space-info vertex-attributes)
        render-transforms (math/derive-render-transforms world-transform
                                                         (:view render-args)
                                                         (:projection render-args)
                                                         (:texture render-args)
                                                         coordinate-space-info)
        render-args (merge render-args render-transforms extra-render-args)
        world-space-semantic-types (:coordinate-space-world coordinate-space-info)
        attribute-world-transform (when (contains? world-space-semantic-types :semantic-type-position)
                                    world-transform)
        attribute-normal-transform (when (contains? world-space-semantic-types :semantic-type-normal)
                                     normal-transform)

        attribute-world-matrix-data (when (graphics/contains-semantic-type? vertex-attributes :semantic-type-world-matrix)
                                      (make-matrix-attribute-data mesh-renderable-data world-transform))
        attribute-world-normal-data (when (graphics/contains-semantic-type? vertex-attributes :semantic-type-normal-matrix)
                                      (make-matrix-attribute-data mesh-renderable-data normal-transform))
        mesh-renderable-data (assoc mesh-renderable-data
                               :world-matrix-data attribute-world-matrix-data
                               :normal-matrix-data attribute-world-normal-data)

        request-id (if (or attribute-world-transform attribute-normal-transform attribute-world-matrix-data attribute-world-normal-data)
                     [request-prefix node-id mesh-renderable-data vertex-attribute-bytes vertex-description] ; World-space attributes present. The request needs to be unique for this node-id.
                     [request-prefix mesh-renderable-data vertex-attribute-bytes vertex-description]) ; No world-space attributes present. We can share the GPU objects between instances of this mesh.
        vb (request-vb! gl request-id mesh-renderable-data attribute-world-transform attribute-normal-transform vertex-description vertex-attribute-bytes)
        vertex-binding (vtx/use-with request-id vb shader)]
    (gl/with-gl-bindings gl render-args [vertex-binding shader]
      (doseq [[name t] textures]
        (gl/bind gl t render-args)
        (shader/set-samplers-by-name shader gl name (:texture-units t)))
      (gl/gl-disable gl GL/GL_BLEND)
      (gl/gl-enable gl GL/GL_CULL_FACE)
      (gl/gl-cull-face gl GL/GL_BACK)
      (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 (count vb))
      (gl/gl-disable gl GL/GL_CULL_FACE)
      (gl/gl-enable gl GL/GL_BLEND)
      (doseq [[_name t] textures]
        (gl/unbind gl t render-args)))))

(defn- render-mesh-opaque [^GL2 gl render-args renderable]
  (render-mesh-opaque-impl gl render-args renderable ::mesh nil nil nil))

(defn- render-mesh-opaque-selection [^GL2 gl render-args renderable]
  (let [extra-render-args {:id (scene-picking/renderable-picking-id-uniform renderable)}]
    (render-mesh-opaque-impl gl render-args renderable ::mesh-selection id-shader id-vertex extra-render-args)))

(defn- render-mesh [^GL2 gl render-args renderables rcount]
  (assert (= 1 rcount) "Batching is disabled in the editor for simplicity.")
  (let [pass (:pass render-args)
        renderable (first renderables)]
    (condp = pass
      pass/opaque
      (render-mesh-opaque gl render-args renderable)

      pass/opaque-selection
      (render-mesh-opaque-selection gl render-args renderable))))

(defn- render-outline [^GL2 gl render-args renderables rcount]
  (assert (= 1 rcount) "Batching is disabled in the editor for simplicity.")
  (when (= pass/outline (:pass render-args))
    (let [renderable (first renderables)
          node-id (:node-id renderable)
          request-id [::outline node-id]]
      (render/render-aabb-outline gl render-args request-id renderables rcount))))

(g/defnk produce-mesh-set-build-target [_node-id resource content]
  (rig/make-mesh-set-build-target (resource/workspace resource) _node-id (:mesh-set content)))

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

(def ^:private default-material-ids ["default"])

(g/defnk produce-material-ids [content]
  (let [ret (:material-ids content)]
    (if (zero? (count ret))
      default-material-ids
      ret)))

(defn- make-renderable-mesh [mesh mesh-material-index->material-name]
  (let [mesh-renderable-data (mesh->renderable-data mesh)]
    (if (g/error-value? mesh-renderable-data)
      mesh-renderable-data
      (let [{:keys [aabb-min aabb-max ^int material-index]} mesh
            mesh-aabb (geom/coords->aabb aabb-min aabb-max)
            material-name (mesh-material-index->material-name material-index)]
        {:aabb mesh-aabb
         :material-name material-name
         :renderable-data mesh-renderable-data}))))

(defn- make-renderable-model [model mesh-material-index->material-name]
  (let [{:keys [translation rotation scale]} (:local model)
        model-transform (math/clj->mat4 translation rotation scale)

        renderable-meshes
        (mapv #(make-renderable-mesh % mesh-material-index->material-name)
              (:meshes model))]

    (g/precluding-errors renderable-meshes
      (let [model-aabb (transduce
                         (map :aabb)
                         geom/aabb-union
                         geom/null-aabb
                         renderable-meshes)]
        {:transform model-transform
         :aabb model-aabb
         :renderable-meshes renderable-meshes}))))

(defn- make-renderable-mesh-set [mesh-set mesh-material-index->material-name]
  (let [renderable-models
        (mapv #(make-renderable-model % mesh-material-index->material-name)
              (:models mesh-set))]

    (g/precluding-errors renderable-models
      (let [mesh-set-aabb (transduce
                            (map (fn [{:keys [aabb transform]}]
                                   (geom/aabb-transform aabb transform)))
                            geom/aabb-union
                            geom/null-aabb
                            renderable-models)]
        {:aabb mesh-set-aabb
         :renderable-models renderable-models}))))

(g/defnk produce-renderable-mesh-set [content]
  (let [mesh-material-index->material-name
        (or (some-> content :material-ids not-empty vec)
            default-material-ids)]
    (make-renderable-mesh-set (:mesh-set content) mesh-material-index->material-name)))

(defn- make-mesh-scene [renderable-mesh model-scene-resource-node-id]
  (let [{:keys [aabb material-name renderable-data]} renderable-mesh]
    {:aabb aabb
     :renderable {:render-fn render-mesh
                  :tags #{:model}
                  :batch-key nil ; Batching is disabled in the editor for simplicity.
                  :select-batch-key model-scene-resource-node-id
                  :passes [pass/opaque pass/opaque-selection]
                  :user-data {:mesh-renderable-data renderable-data
                              :vertex-space :vertex-space-local
                              :material-name material-name
                              :shader preview-shader
                              :textures {"texture_sampler" @texture/white-pixel}}}}))

(defn- make-model-scene [renderable-model model-scene-resource-node-id]
  (let [{:keys [transform aabb renderable-meshes]} renderable-model
        mesh-scenes (mapv #(make-mesh-scene % model-scene-resource-node-id)
                          renderable-meshes)]
    {:transform transform
     :aabb aabb
     :children mesh-scenes}))

(defn- make-scene [renderable-mesh-set model-scene-resource-node-id]
  (let [{:keys [aabb renderable-models]} renderable-mesh-set
        model-scenes (mapv #(make-model-scene % model-scene-resource-node-id)
                           renderable-models)]
    {:node-id model-scene-resource-node-id
     :aabb aabb
     :renderable {:render-fn render-outline
                  :tags #{:model :outline}
                  :batch-key nil ; Batching is disabled in the editor for simplicity.
                  :select-batch-key :not-rendered ; The render-fn only does anything during the outline pass.
                  :passes [pass/outline pass/opaque-selection]} ; Include in a selection pass to ensure it can be selected and manipulated.
     :children model-scenes}))

(g/defnk produce-scene [_node-id renderable-mesh-set]
  (make-scene renderable-mesh-set _node-id))

(defn- augment-mesh-scene [mesh-scene old-node-id new-node-id new-node-outline-key material-name->material-scene-info]
  (let [mesh-renderable (:renderable mesh-scene)
        material-name (:material-name mesh-renderable)
        material-scene-info (material-name->material-scene-info material-name)
        claimed-scene (scene/claim-child-scene old-node-id new-node-id new-node-outline-key mesh-scene)]
    (if (nil? material-scene-info)
      claimed-scene
      (let [{:keys [gpu-textures material-attribute-infos shader vertex-attribute-bytes vertex-space]} material-scene-info]
        (assert (map? gpu-textures))
        (assert (every? string? (keys gpu-textures)))
        (assert (every? texture/texture-lifecycle? (vals gpu-textures)))
        (assert (every? map? material-attribute-infos))
        (assert (every? keyword? (map :name-key material-attribute-infos)))
        (assert (shader/shader-lifecycle? shader))
        (assert (map? vertex-attribute-bytes))
        (assert (every? keyword? (keys vertex-attribute-bytes)))
        (assert (every? bytes? (vals vertex-attribute-bytes)))
        (assert (#{:vertex-space-local :vertex-space-world} vertex-space))
        (update
          claimed-scene
          :renderable
          (fn [renderable]
            (update
              renderable
              :user-data
              assoc
              :material-attribute-infos material-attribute-infos
              :shader shader
              :textures gpu-textures
              :vertex-attribute-bytes vertex-attribute-bytes
              :vertex-space vertex-space)))))))

(defn- augment-model-scene [model-scene old-node-id new-node-id new-node-outline-key material-name->material-scene-info]
  (let [mesh-scenes (:children model-scene)]
    (assoc (scene/claim-child-scene old-node-id new-node-id new-node-outline-key model-scene)
      :children (mapv #(augment-mesh-scene % old-node-id new-node-id new-node-outline-key material-name->material-scene-info)
                      mesh-scenes))))

(defn augment-scene [scene new-node-id new-node-outline-key material-name->material-scene-info]
  (if (g/error-value? scene)
    scene
    (let [old-node-id (:node-id scene)
          model-scenes (:children scene)]
      (assoc scene
        :node-id new-node-id
        :node-outline-key new-node-outline-key
        :children (mapv #(augment-model-scene % old-node-id new-node-id new-node-outline-key material-name->material-scene-info)
                        model-scenes)))))

(g/defnode ModelSceneNode
  (inherits resource-node/ResourceNode)

  (output content g/Any :cached produce-content)
  (output bones g/Any produce-bones)
  (output animation-info g/Any produce-animation-info)
  (output animation-ids g/Any produce-animation-ids)
  (output material-ids g/Any produce-material-ids)
  (output mesh-set-build-target g/Any :cached produce-mesh-set-build-target)
  (output skeleton g/Any produce-skeleton)
  (output skeleton-build-target g/Any :cached produce-skeleton-build-target)
  (output renderable-mesh-set g/Any :cached produce-renderable-mesh-set)
  (output scene g/Any :cached produce-scene))

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace
                                    :ext model-file-types
                                    :label "Model Scene"
                                    :node-type ModelSceneNode
                                    :icon mesh-icon
                                    :view-types [:scene :text]))

(defn- update-vb [^GL2 _gl ^VertexBuffer vb data]
  (let [{:keys [mesh-renderable-data ^Matrix4d world-transform ^Matrix4d normal-transform vertex-attribute-bytes]} data]
    (mesh->vb! vb world-transform normal-transform vertex-attribute-bytes mesh-renderable-data)
    vb))

(defn- make-vb [^GL2 gl data]
  (let [{:keys [mesh-renderable-data vertex-description]} data
        position-data (:position-data mesh-renderable-data)
        vbuf (vtx/make-vertex-buffer vertex-description :dynamic (count position-data))]
    (update-vb gl vbuf data)))

(defn- destroy-vbs [^GL2 gl vbs _])

(scene-cache/register-object-cache! ::vb make-vb update-vb destroy-vbs)
