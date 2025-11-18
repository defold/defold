;; Copyright 2020-2025 The Defold Foundation
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

(ns editor.mesh
  (:require [clojure.string :as str]
            [dynamo.graph :as g]
            [editor.buffer :as buffer]
            [editor.build-target :as bt]
            [editor.defold-project :as project]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.pass :as pass]
            [editor.gl.shader :as shader]
            [editor.gl.vertex2 :as vtx]
            [editor.graph-util :as gu]
            [editor.graphics.types :as graphics.types]
            [editor.image :as image]
            [editor.localization :as localization]
            [editor.material :as material]
            [editor.math :as math]
            [editor.properties :as properties]
            [editor.protobuf :as protobuf]
            [editor.render-util :as render-util]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.scene-cache :as scene-cache]
            [editor.scene-picking :as scene-picking]
            [editor.types :as types]
            [editor.validation :as validation]
            [editor.workspace :as workspace]
            [util.coll :as coll])
  (:import [com.dynamo.gamesys.proto MeshProto$MeshDesc MeshProto$MeshDesc$PrimitiveType]
           [com.jogamp.opengl GL2]
           [editor.gl.shader ShaderLifecycle]
           [editor.gl.vertex2 VertexBuffer]
           [editor.graphics.types ElementType]
           [editor.types AABB]
           [java.nio ByteBuffer]
           [javax.vecmath Matrix4d Point3d Vector4d]))

(set! *warn-on-reflection* true)

(def ^:private mesh-icon "icons/32/Icons_22-Model.png")

(shader/defshader model-id-vertex-shader
  (attribute vec4 position)
  (attribute vec2 texcoord0)
  (uniform mat4 world_view_proj)
  (varying vec2 var_texcoord0)
  (defn void main []
    (setq gl_Position (* world_view_proj position))
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

(def id-shader (shader/make-shader ::model-id-shader model-id-vertex-shader model-id-fragment-shader {"id" :id "world_view_proj" :world-view-proj}))

(g/defnk produce-save-value [primitive-type position-stream normal-stream material vertices textures]
  (protobuf/make-map-without-defaults MeshProto$MeshDesc
    :material (resource/resource->proj-path material)
    :vertices (resource/resource->proj-path vertices)
    :textures (mapv resource/resource->proj-path textures)
    :primitive-type primitive-type
    :position-stream position-stream
    :normal-stream normal-stream))

(defn- build-pb [resource dep-resources user-data]
  (let [pb  (:pb user-data)
        pb  (reduce (fn [pb [label resource]]
                      (if (vector? label)
                        (assoc-in pb label resource)
                        (assoc pb label resource)))
                    pb
                    (map (fn [[label res]]
                           [label (resource/proj-path (get dep-resources res))])
                         (:dep-resources user-data)))]
    {:resource resource :content (protobuf/map->bytes MeshProto$MeshDesc pb)}))

(defn- prop-resource-error [nil-severity _node-id prop-kw prop-value prop-name]
  (or (validation/prop-error nil-severity _node-id prop-kw validation/prop-nil? prop-value prop-name)
      (validation/prop-error :fatal _node-id prop-kw validation/prop-resource-not-exists? prop-value prop-name)))

(defn- res-fields->resources [pb-msg deps-by-source fields]
  (->> (mapcat (fn [field] (if (vector? field) (mapv (fn [i] (into [(first field) i] (rest field))) (range (count (get pb-msg (first field))))) [field])) fields)
    (map (fn [label] [label (get deps-by-source (if (vector? label) (get-in pb-msg label) (get pb-msg label)))]))))

(defn- prop-stream-id-error-message [stream-id stream-ids vertex-space prop-kw]
  (when (seq stream-ids)
    (if (str/blank? stream-id)
      (when (= :vertex-space-world vertex-space)
        (let [prop-name (validation/keyword->name prop-kw)]
          (format "'%s' must be specified when using a material with a world-space 'Vertex Space' setting" prop-name)))
      (when (not-any? #(= stream-id %) stream-ids)
        (format "Stream '%s' could not be found in the specified buffer" stream-id)))))

(defn- validate-stream-id [_node-id prop-kw stream-id stream-ids vertices-resource vertex-space]
  (when (some? vertices-resource)
    (validation/prop-error :fatal _node-id prop-kw prop-stream-id-error-message stream-id stream-ids vertex-space prop-kw)))

(defn position-stream-name? [stream-name specified-position-stream-name]
  (if-not (str/blank? specified-position-stream-name)
    (= stream-name specified-position-stream-name)
    (= stream-name "position")))

(defn normal-stream-name? [stream-name specified-normal-stream-name]
  (if-not (str/blank? specified-normal-stream-name)
    (= stream-name specified-normal-stream-name)
    (= stream-name "normal")))

(g/defnk produce-build-targets [_node-id resource save-value dep-build-targets material vertices vertex-space position-stream normal-stream stream-ids]
  (or (some->> [(prop-resource-error :fatal _node-id :material material "Material")
                (prop-resource-error :fatal _node-id :vertices vertices "Vertices")
                (validate-stream-id _node-id :position-stream position-stream stream-ids vertices vertex-space)
                (validate-stream-id _node-id :normal-stream normal-stream stream-ids vertices vertex-space)]
               (filterv some?)
               not-empty
               g/error-aggregate)
      (let [pb-msg (select-keys save-value [:material :vertices :textures :primitive-type :position-stream :normal-stream])
            dep-build-targets (flatten dep-build-targets)
            deps-by-source (into {} (map #(let [res (:resource %)] [(resource/proj-path (:resource res)) res]) dep-build-targets))
            dep-resources (into (res-fields->resources pb-msg deps-by-source [:material :vertices])
                            (filter second (res-fields->resources pb-msg deps-by-source [[:textures]])))]
        [(bt/with-content-hash
           {:node-id _node-id
            :resource (workspace/make-build-resource resource)
            :build-fn build-pb
            :user-data {:pb pb-msg
                        :dep-resources dep-resources}
            :deps dep-build-targets})])))

(g/defnk produce-gpu-textures [_node-id samplers gpu-texture-generators]
  (into {} (map (fn [unit-index sampler {tex-fn :f tex-args :args}]
                  (let [request-id [_node-id unit-index]
                        params     (material/sampler->tex-params sampler)
                        texture    (tex-fn tex-args request-id params unit-index)]
                    [(:name sampler) texture]))
                (range)
                samplers
                gpu-texture-generators)))

(defn- request-vb! [^GL2 gl node-id user-data world-transform]
  (let [request-id [node-id (:vertex-attributes user-data) (:vertex-count user-data)]
        world-transform-clj (math/vecmath->clj world-transform)
        data (-> user-data
                 (select-keys [:array-streams
                               :normal-stream-name
                               :position-stream-name
                               :put-vertices-fn
                               :scratch-arrays
                               :vertex-attributes
                               :vertex-count
                               :vertex-space])
                 (assoc :world-transform world-transform-clj))] ; todo: do we need to convert it to a clj transform?
    (scene-cache/request-object! ::vb request-id gl data)))

(defn- gl-primitive-type
  ^long [primitive-type]
  (case primitive-type
    :primitive-triangles GL2/GL_TRIANGLES
    :primitive-triangle-strip GL2/GL_TRIANGLE_STRIP
    :primitive-lines GL2/GL_LINES))

(defn- render-scene-opaque [^GL2 gl render-args renderables _renderable-count]
  (let [renderable (first renderables)
        user-data (:user-data renderable)
        gl-primitive-type (gl-primitive-type (:primitive-type user-data))
        shader (:shader user-data)
        textures (:textures user-data)
        vertex-space (:vertex-space user-data)
        shader-world-transform (if (= vertex-space :vertex-space-world)
                                 (doto (Matrix4d.) (.setIdentity)) ; already applied the world transform to vertices
                                 (:world-transform renderable))
        render-args (merge render-args
                           (math/derive-render-transforms shader-world-transform ; TODO(instancing): Can we use the render-args as-is?
                                                          (:view render-args)
                                                          (:projection render-args)
                                                          (:texture render-args)))]
    (gl/with-gl-bindings gl render-args [shader]
      (doseq [[name texture] textures]
        (gl/bind gl texture render-args)
        (shader/set-samplers-by-name shader gl name (:texture-units texture)))
      (.glBlendFunc gl GL2/GL_ONE GL2/GL_ONE_MINUS_SRC_ALPHA)
      (gl/gl-enable gl GL2/GL_CULL_FACE)
      (gl/gl-cull-face gl GL2/GL_BACK)
      (doseq [renderable renderables
              :let [node-id (:node-id renderable)
                    user-data (:user-data renderable)
                    vb (request-vb! gl node-id user-data (:world-transform renderable))
                    vertex-binding (vtx/use-with [node-id ::mesh] vb shader)]]
        (gl/with-gl-bindings gl render-args [vertex-binding]
          (gl/gl-draw-arrays gl gl-primitive-type 0 (count vb))))
      (gl/gl-disable gl GL2/GL_CULL_FACE)
      (.glBlendFunc gl GL2/GL_SRC_ALPHA GL2/GL_ONE_MINUS_SRC_ALPHA)
      (doseq [[_name texture] textures]
        (gl/unbind gl texture render-args)))))

(defn- render-scene-opaque-selection [^GL2 gl render-args renderables _renderable-count]
  (assert (= 1 (count renderables)))
  (let [renderable (first renderables)
        node-id (:node-id renderable)
        user-data (assoc (:user-data renderable) :vertex-space :vertex-space-local)
        gl-primitive-type (gl-primitive-type (:primitive-type user-data))
        textures (:textures user-data)
        world-transform (:world-transform renderable)
        render-args (merge render-args
                           (math/derive-render-transforms world-transform ; TODO(instancing): Can we use the render-args as-is?
                                                          (:view render-args)
                                                          (:projection render-args)
                                                          (:texture render-args)))
        render-args (assoc render-args :id (scene-picking/renderable-picking-id-uniform renderable))]
    (gl/with-gl-bindings gl render-args [id-shader]
      (doseq [[name texture] textures]
        (gl/bind gl texture render-args)
        (shader/set-samplers-by-name id-shader gl name (:texture-units texture)))
      (gl/gl-enable gl GL2/GL_CULL_FACE)
      (gl/gl-cull-face gl GL2/GL_BACK)
      (let [vb (request-vb! gl node-id user-data world-transform)
            vertex-binding (vtx/use-with [node-id ::mesh-selection] vb id-shader)]
        (gl/with-gl-bindings gl render-args [vertex-binding]
          (gl/gl-draw-arrays gl gl-primitive-type 0 (count vb))))
      (gl/gl-disable gl GL2/GL_CULL_FACE)
      (doseq [[_name texture] textures]
        (gl/unbind gl texture render-args)))))

(defn- gen-scratch-arrays [array-streams]
  (mapv (fn [{:keys [data type]}]
          (buffer/stream-data->array nil type (count data)))
        array-streams))

(defn- render-scene [^GL2 gl render-args renderables rcount]
  ;; TODO(instancing): Update rendering to use AttributeBufferBindings. Share scene representation with ModelSceneNode?
  (let [pass (:pass render-args)]
    (condp = pass
      pass/opaque
      (render-scene-opaque gl render-args renderables rcount)

      pass/opaque-selection
      (render-scene-opaque-selection gl render-args renderables rcount))))

(defn data-type->buffer-value-type [type]
  (case type
    :type-float :value-type-float32
    :type-unsigned-byte :value-type-uint8
    :type-unsigned-short :value-type-uint16
    :type-unsigned-int :value-type-uint32
    :type-byte :value-type-int8
    :type-short :value-type-int16
    :type-int :value-type-int32))

(defn- buffer-value-type->data-type [buffer-value-type]
  {:post [(graphics.types/data-type? %)]}
  (case buffer-value-type
    :value-type-float32 :type-float
    :value-type-uint8 :type-unsigned-byte
    :value-type-uint16 :type-unsigned-short
    :value-type-uint32 :type-unsigned-int
    :value-type-int8 :type-byte
    :value-type-int16 :type-short
    :value-type-int32 :type-int))

(defn- make-put-vertex-fn-raw [element-types]
  (let [put-fns (mapv (fn [^ElementType element-type]
                        (let [vector-type (.-vector-type element-type)
                              data-type (.-data-type element-type)
                              buffer-value-type (data-type->buffer-value-type data-type)
                              component-count (graphics.types/vector-type-component-count vector-type)]
                          (buffer/get-put-fn buffer-value-type component-count)))
                      element-types)]
    (fn put-vertex! [source-arrays ^ByteBuffer byte-buffer ^long vertex-index]
      (dorun
        (map (fn [source-array put-fn]
               (put-fn source-array byte-buffer vertex-index))
             source-arrays
             put-fns)))))

(def make-put-vertex-fn
  (memoize make-put-vertex-fn-raw))

(defn- make-put-vertices-fn-raw [element-types]
  (let [put-vertex-fn (make-put-vertex-fn element-types)
        first-element-type ^ElementType (first element-types)
        first-vector-type (.-vector-type first-element-type)
        first-component-count (graphics.types/vector-type-component-count first-vector-type)]
    (fn put-vertices! [source-arrays ^ByteBuffer byte-buffer]
      (let [vertex-count (/ (count (first source-arrays)) first-component-count)]
        (loop [vertex-index 0]
          (if (= vertex-index vertex-count)
            vertex-count
            (do (put-vertex-fn source-arrays byte-buffer vertex-index)
                (recur (inc vertex-index)))))))))

(def make-put-vertices-fn
  (memoize make-put-vertices-fn-raw))

(defn- stream->attribute-info [stream position-stream-name normal-stream-name]
  {:post [(graphics.types/attribute-info? %)]}
  (let [attribute-name (:name stream)
        attribute-key (graphics.types/attribute-name-key attribute-name)]
    {:name attribute-name
     :name-key attribute-key
     :vector-type (graphics.types/component-count-vector-type (:count stream) false)
     :data-type (buffer-value-type->data-type (:type stream))
     :normalize false ; TODO: Figure out if this should be configurable.
     :coordinate-space :coordinate-space-default
     :step-function :vertex-step-function-vertex
     :semantic-type (condp = attribute-name
                      position-stream-name :semantic-type-position
                      normal-stream-name :semantic-type-normal
                      (graphics.types/infer-semantic-type attribute-key))}))

(defn- max-stream-length [streams]
  (transduce (map (fn [stream]
                    (/ (count (:data stream))
                       (:count stream))))
             max
             0
             streams))

(g/defnk produce-scene [_node-id aabb streams primitive-type position-stream normal-stream shader gpu-textures vertex-space]
  (if (nil? streams)
    {:aabb aabb
     :renderable {:passes [pass/selection]}}

    (let [vertex-count (max-stream-length streams)
          attribute-infos (mapv #(stream->attribute-info % position-stream normal-stream) streams)
          array-streams (mapv (partial buffer/stream->array-stream vertex-count) streams)
          element-types (mapv graphics.types/attribute-info-element-type attribute-infos)
          put-vertices-fn (make-put-vertices-fn element-types)]
      {:node-id _node-id
       :aabb aabb
       :renderable {:render-fn render-scene
                    :tags #{:model}
                    :batch-key (when (= :vertex-space-world vertex-space)
                                 [attribute-infos shader gpu-textures])
                    :select-batch-key _node-id
                    :user-data {:array-streams array-streams
                                :vertex-attributes attribute-infos
                                :vertex-count vertex-count
                                :put-vertices-fn put-vertices-fn
                                :position-stream-name position-stream
                                :normal-stream-name normal-stream
                                :shader shader
                                :textures gpu-textures
                                :scratch-arrays (gen-scratch-arrays array-streams)
                                :vertex-space vertex-space
                                :primitive-type primitive-type}
                    :passes [pass/opaque pass/opaque-selection]}
       :children [{:node-id _node-id
                   :aabb aabb
                   :renderable (render-util/make-aabb-outline-renderable #{:model})}]})))

(g/defnk produce-aabb [streams position-stream]
  (if-some [{:keys [count data]} (coll/first-where #(position-stream-name? (:name %) position-stream) streams)]
    (if (empty? data)
      geom/empty-bounding-box
      (let [[min-p max-p]
            (transduce
              (comp (partition-all count)
                    (map (fn [[x y z]]
                           (Point3d. (double x)
                                     (double y)
                                     (if (nil? z)
                                       0.0
                                       (double z))))))
              (completing (fn [[^Point3d min-p ^Point3d max-p] ^Point3d p]
                            [(math/min-point min-p p)
                             (math/max-point max-p p)]))
              [(Point3d. Double/POSITIVE_INFINITY
                         Double/POSITIVE_INFINITY
                         Double/POSITIVE_INFINITY)
               (Point3d. Double/NEGATIVE_INFINITY
                         Double/NEGATIVE_INFINITY
                         Double/NEGATIVE_INFINITY)]
              data)]
        (types/->AABB min-p max-p)))
    geom/empty-bounding-box))

(defn- vset [v i value]
  (let [c (count v)
        v (if (<= c i) (coll/into-vector v (repeat (- i c) nil)) v)]
    (assoc v i value)))

(g/defnode MeshNode
  (inherits resource-node/ResourceNode)

  (property material resource/Resource ; Required protobuf field.
            (value (gu/passthrough material-resource))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter evaluation-context self old-value new-value
                                            [:resource :material-resource]
                                            [:samplers :samplers]
                                            [:build-targets :dep-build-targets]
                                            [:shader :shader]
                                            [:vertex-space :vertex-space])))
            (dynamic error (g/fnk [_node-id material]
                                  (prop-resource-error :fatal _node-id :material material "Material")))
            (dynamic edit-type (g/constantly {:type resource/Resource
                                              :ext "material"})))

  (property vertices resource/Resource ; Required protobuf field.
            (value (gu/passthrough vertices-resource))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter evaluation-context self old-value new-value
                                            [:resource :vertices-resource]
                                            [:build-targets :dep-build-targets]
                                            [:stream-ids :stream-ids]
                                            [:streams :streams])))
            (dynamic edit-type (g/constantly {:type resource/Resource
                                              :ext "buffer"}))
            (dynamic label (properties/label-dynamic :mesh :vertices))
            (dynamic tooltip (properties/tooltip-dynamic :mesh :vertices)))

  (property textures resource/ResourceVec ; Nil is valid default.
            (value (gu/passthrough texture-resources))
            (set (fn [evaluation-context self old-value new-value]
                   (let [project (project/get-project (:basis evaluation-context) self)
                         connections [[:resource :texture-resources]
                                      [:build-targets :dep-build-targets]
                                      [:gpu-texture-generator :gpu-texture-generators]]]
                     (concat
                       (for [r old-value]
                         (if r
                           (project/disconnect-resource-node evaluation-context project r self connections)
                           (g/disconnect project :nil-resource self :texture-resources)))
                       (for [r new-value]
                         (if r
                           (:tx-data (project/connect-resource-node evaluation-context project r self connections))
                           (g/connect project :nil-resource self :texture-resources)))))))
            (dynamic visible (g/constantly false)))

  (property primitive-type g/Any (default (protobuf/default MeshProto$MeshDesc :primitive-type))
            (dynamic edit-type (g/constantly (properties/->pb-choicebox MeshProto$MeshDesc$PrimitiveType)))
            (dynamic label (properties/label-dynamic :mesh :primitive-type))
            (dynamic tooltip (properties/tooltip-dynamic :mesh :primitive-type)))

  (property position-stream g/Str (default (protobuf/default MeshProto$MeshDesc :position-stream))
            (dynamic error (g/fnk [_node-id vertices vertex-space stream-ids position-stream]
                             (validate-stream-id _node-id :position-stream position-stream stream-ids vertices vertex-space)))
            (dynamic edit-type (g/fnk [stream-ids] (properties/->choicebox (conj stream-ids ""))))
            (dynamic label (properties/label-dynamic :mesh :position-stream))
            (dynamic tooltip (properties/tooltip-dynamic :mesh :position-stream)))

  (property normal-stream g/Str (default (protobuf/default MeshProto$MeshDesc :normal-stream))
            (dynamic error (g/fnk [_node-id vertices vertex-space stream-ids normal-stream]
                             (validate-stream-id _node-id :normal-stream normal-stream stream-ids vertices vertex-space)))
            (dynamic edit-type (g/fnk [stream-ids] (properties/->choicebox (conj stream-ids ""))))
            (dynamic label (properties/label-dynamic :mesh :normal-stream))
            (dynamic tooltip (properties/tooltip-dynamic :mesh :normal-stream)))

  (input stream-ids g/Any)
  (input material-resource resource/Resource)
  (input samplers g/Any)
  (input vertices-resource resource/Resource)
  (input texture-resources resource/Resource :array)
  (input gpu-texture-generators g/Any :array)
  (input dep-build-targets g/Any :array)
  (input streams g/Any)
  (input shader ShaderLifecycle)
  (input vertex-space g/Keyword)

  (output save-value g/Any :cached produce-save-value)
  (output build-targets g/Any :cached produce-build-targets)
  (output gpu-textures g/Any :cached produce-gpu-textures)
  (output scene g/Any :cached produce-scene)
  (output aabb AABB :cached produce-aabb)
  (output _properties g/Properties :cached (g/fnk [_node-id _declared-properties textures samplers]
                                                  (let [resource-type (get-in _declared-properties [:properties :material :type])
                                                        prop-entry {:node-id _node-id
                                                                    :type resource-type
                                                                    :edit-type {:type resource/Resource
                                                                                :ext (conj image/exts "cubemap" "render_target")}}
                                                        keys (map :name samplers)
                                                        p (->> keys
                                                               (map-indexed (fn [i s]
                                                                              [(keyword (format "texture%d" i))
                                                                               (-> prop-entry
                                                                                   (assoc :value (get textures i)
                                                                                          :label s)
                                                                                   (assoc-in [:edit-type :set-fn]
                                                                                             (fn [_evaluation-context self _old-value new-value]
                                                                                               (g/update-property self :textures vset i new-value))))])))]
                                                    (-> _declared-properties
                                                        (update :properties into p)
                                                        (update :display-order into (map first p)))))))

(defn- load-mesh [_project self resource pb]
  {:pre [(map? pb)]} ; MeshProto$MeshDesc in map format.
  (let [resolve-resource #(workspace/resolve-resource resource %)
        resolve-resources #(mapv resolve-resource %)]
    (gu/set-properties-from-pb-map self MeshProto$MeshDesc pb
      primitive-type :primitive-type
      position-stream :position-stream
      normal-stream :normal-stream
      material (resolve-resource :material)
      vertices (resolve-resource :vertices)
      textures (resolve-resources :textures))))

(defn register-resource-types [workspace]
  (resource-node/register-ddf-resource-type workspace
    :ext "mesh"
    :label (localization/message "resource.type.mesh")
    :node-type MeshNode
    :ddf-type MeshProto$MeshDesc
    :load-fn load-mesh
    :icon mesh-icon
    :view-types [:scene :text]
    :tags #{:component}
    :tag-opts {:component {:transform-properties #{:position :rotation}}}))

(defn- transform-array-fn-form [^long component-count data-array-tag-sym data-array-cast-fn-sym vector-type]
  (let [data-array-sym (gensym "data-array")
        transform-sym (gensym "transform")
        temp-vector-sym (gensym "temp-vector")
        array-length-sym (gensym "array-length")
        index-sym (gensym "index")
        data-array-cast-fn-var (resolve data-array-cast-fn-sym)]
    `(fn [~(with-meta data-array-sym {:tag data-array-tag-sym})
          ~(with-meta transform-sym {:tag (.getName Matrix4d)})]
       (let [^Vector4d ~temp-vector-sym (Vector4d.)
             ~array-length-sym (alength ~data-array-sym)]
         (loop [~index-sym 0]
           (if (= ~index-sym ~array-length-sym)
             ~data-array-sym
             (do
               ~@(filter
                   some?
                   [`(.set ~temp-vector-sym
                           (aget ~data-array-sym ~index-sym)
                           (aget ~data-array-sym (inc ~index-sym))
                           ~(if (> component-count 2)
                              `(aget ~data-array-sym (+ ~index-sym 2))
                              0.0)
                           ~(case vector-type
                              :point 1.0
                              :normal 0.0))
                    `(.transform ~transform-sym ~temp-vector-sym)
                    (when (= :normal vector-type)
                      `(.normalize ~temp-vector-sym))
                    `(aset ~data-array-sym ~index-sym (~data-array-cast-fn-var (.x ~temp-vector-sym)))
                    `(aset ~data-array-sym (inc ~index-sym) (~data-array-cast-fn-var (.y ~temp-vector-sym)))
                    (when (> component-count 2)
                      `(aset ~data-array-sym (+ ~index-sym 2) (~data-array-cast-fn-var (.z ~temp-vector-sym))))
                    `(recur (+ ~index-sym ~component-count))]))))))))

(def ^:private transform-array-fns-by-key
  (into {}
        (for [component-count (range 2 4)
              vector-type [:point :normal]
              [data-array-tag-sym data-array-cast-fn-sym pb-value-types]
              [['bytes 'unchecked-byte [:value-type-uint8 :value-type-int8]]
               ['shorts 'unchecked-short [:value-type-uint16 :value-type-int16]]
               ['ints 'unchecked-int [:value-type-uint32 :value-type-int32]]
               ['floats 'unchecked-float [:value-type-float32]]]
              :let [transform-array-fn (eval (transform-array-fn-form component-count data-array-tag-sym data-array-cast-fn-sym vector-type))]
              pb-value-type pb-value-types]
          [[component-count pb-value-type vector-type] transform-array-fn])))

(defn get-transform-array-fn [pb-value-type ^long component-count vector-type]
  (get transform-array-fns-by-key [component-count pb-value-type vector-type]))

(defn- populate-scratch-array! [scratch-array array-stream-data]
  (System/arraycopy array-stream-data 0 scratch-array 0 (count array-stream-data))
  scratch-array)

(defn- transform-array-stream! [vector-type {:keys [count data type] :as array-stream} scratch-array ^Matrix4d transform]
  (let [transform-array-fn (get-transform-array-fn type count vector-type)]
    (assoc array-stream
      :data (-> scratch-array
                (populate-scratch-array! data)
                (transform-array-fn transform)))))

(defn- populate-vb! [^VertexBuffer vb {:keys [array-streams normal-stream-name position-stream-name put-vertices-fn scratch-arrays vertex-space world-transform]}]
  (assert (= (count array-streams) (count scratch-arrays)))
  (let [array-streams'
        (if (not= vertex-space :vertex-space-world)
          array-streams
          (map (fn [{:keys [name] :as array-stream} scratch-array]
                 (cond
                   (position-stream-name? name position-stream-name)
                   (transform-array-stream! :point array-stream scratch-array world-transform)

                   (normal-stream-name? name normal-stream-name)
                   (let [normal-transform (math/derive-normal-transform world-transform)]
                     (transform-array-stream! :normal array-stream scratch-array normal-transform))

                   :else
                   array-stream))
               array-streams
               scratch-arrays))
        source-arrays (mapv :data array-streams')
        vertex-count (put-vertices-fn source-arrays (.buf vb))]
    (vtx/position! vb vertex-count)))

(defn- make-vb-from-vertex-attributes [vertex-attributes vertex-count]
  (let [vertex-description (graphics.types/make-vertex-description vertex-attributes)]
    (vtx/make-vertex-buffer vertex-description :static vertex-count)))

(defn- update-vb! [^GL2 _gl ^VertexBuffer vb data]
  (let [data' (update data :world-transform
                      (fn [world-transform]
                        (doto (Matrix4d.)
                          (math/clj->vecmath world-transform))))]
    (-> vb
        (vtx/clear!)
        (populate-vb! data')
        (vtx/flip!))))

(defn- make-vb [^GL2 gl data]
  (let [{:keys [vertex-attributes vertex-count]} data
        vb (make-vb-from-vertex-attributes vertex-attributes vertex-count)]
    (update-vb! gl vb data)))

(defn- destroy-vbs! [^GL2 _gl _vbs _])

(scene-cache/register-object-cache! ::vb make-vb update-vb! destroy-vbs!)
