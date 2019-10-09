(ns editor.mesh
  (:require [clojure.string :as str]
            [dynamo.graph :as g]
            [editor.build-target :as bt]
            [editor.defold-project :as project]
            [editor.geom :as geom]
            [editor.gl.pass :as pass]
            [editor.graph-util :as gu]
            [editor.image :as image]
            [editor.material :as material]
            [editor.properties :as properties]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.validation :as validation]
            [editor.workspace :as workspace]
            [editor.gl.texture :as texture]
            [editor.math :as math]
            [editor.gl :as gl]
            [editor.gl.shader :as shader]
            [editor.gl.vertex2 :as vtx]
            [editor.scene-cache :as scene-cache]
            [editor.buffer :as buffer])
  (:import [com.dynamo.mesh.proto MeshProto$MeshDesc MeshProto$MeshDesc$PrimitiveType]
           [editor.gl.shader ShaderLifecycle]
           [editor.types AABB]
           [com.jogamp.opengl GL2]
           [javax.vecmath Matrix4d Point3d Matrix3d Vector3d]
           [editor.gl.vertex2 VertexBuffer]
           [java.nio ByteBuffer]))

(set! *warn-on-reflection* true)

(def ^:private mesh-icon "icons/32/Icons_22-Model.png")

(g/defnk produce-pb-msg [primitive-type position-stream normal-stream material vertices textures]
  (cond-> {:material (resource/resource->proj-path material)
           :vertices (resource/resource->proj-path vertices)
           :textures (mapv resource/resource->proj-path textures)
           :primitive-type primitive-type}
    (not (str/blank? position-stream))
    (assoc :position-stream position-stream)
    (not (str/blank? normal-stream))
    (assoc :normal-stream normal-stream)))

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

(defn- validate-stream-id [_node-id label vertices-resource stream-id stream-ids]
  (when (and vertices-resource (not-empty stream-id))
    (validation/prop-error :fatal _node-id label validation/prop-stream-missing? stream-id stream-ids)))

(g/defnk produce-build-targets [_node-id resource pb-msg dep-build-targets material vertices position-stream normal-stream stream-ids]
  (or (some->> [(prop-resource-error :fatal _node-id :material material "Material")
                (prop-resource-error :fatal _node-id :vertices vertices "Vertices")
                (validate-stream-id _node-id :position-stream vertices position-stream stream-ids)
                (validate-stream-id _node-id :normal-stream vertices normal-stream stream-ids)]
               (filterv some?)
               not-empty
               g/error-aggregate)
      (let [workspace (resource/workspace resource)
            pb-msg (select-keys pb-msg [:material :vertices :textures :primitive-type :position-stream :normal-stream])
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

(defn- request-vb! [^GL2 gl node-id user-data]
  (let [request-id node-id
        data (-> user-data
                 (select-keys [:array-streams
                               :normal-stream-name
                               :position-stream-name
                               :scratch-arrays
                               :vertex-space
                               :world-transform])
                 (update :world-transform math/vecmath->clj))] ; todo: do we need to convert it to a clj transform?
    (scene-cache/request-object! ::vb request-id gl data)))

(defn- render-scene-opaque [^GL2 gl render-args renderables rcount]
  (let [renderable (first renderables)
        user-data (:user-data renderable)
        shader (:shader user-data)
        textures (:textures user-data)
        vertex-space (:vertex-space user-data)
        vertex-space-world-transform (if (= vertex-space :vertex-space-world)
                                       (doto (Matrix4d.) (.setIdentity)) ; already applied the world transform to vertices
                                       (:world-transform renderable))
        render-args (merge render-args
                           (math/derive-render-transforms vertex-space-world-transform
                                                          (:view render-args)
                                                          (:projection render-args)
                                                          (:texture render-args)))]
    (gl/with-gl-bindings gl render-args [shader]
      (doseq [[name t] textures]
        (gl/bind gl t render-args)
        (shader/set-uniform shader gl name (- (:unit t) GL2/GL_TEXTURE0)))
      (.glBlendFunc gl GL2/GL_ONE GL2/GL_ONE_MINUS_SRC_ALPHA)
      (gl/gl-enable gl GL2/GL_CULL_FACE)
      (gl/gl-cull-face gl GL2/GL_BACK)
      (doseq [renderable renderables
              :let [node-id (:node-id renderable)
                    user-data (:user-data renderable)
                    vb (request-vb! gl node-id user-data)
                    vertex-binding (vtx/use-with [node-id ::mesh] vb shader)]]
        (gl/with-gl-bindings gl render-args [vertex-binding]
          (gl/gl-draw-arrays gl GL2/GL_TRIANGLES 0 (count vb))))
      (gl/gl-disable gl GL2/GL_CULL_FACE)
      (.glBlendFunc gl GL2/GL_SRC_ALPHA GL2/GL_ONE_MINUS_SRC_ALPHA)
      (doseq [[name t] textures]
        (gl/unbind gl t render-args)))))

(defn- render-scene-opaque-selection [^GL2 gl render-args renderables rcount]
  ;; todo
  )

(defn- gen-scratch-arrays [streams]
  ;; todo
  )

(defn- render-scene [^GL2 gl render-args renderables rcount]
  (let [pass (:pass render-args)]
    (condp = pass
      pass/opaque
      (render-scene-opaque gl render-args renderables rcount)

      pass/opaque-selection
      (render-scene-opaque-selection gl render-args renderables rcount))))


(g/defnk produce-scene [_node-id streams position-stream normal-stream shader gpu-textures vertex-space]
  (if (nil? streams)
    {:aabb geom/empty-bounding-box
     :renderable {:passes [pass/selection]}}

    {:node-id _node-id
     :aabb aabb
     :renderable {:render-fn render-scene
                  :tags #{:model}
                  :batch-key _node-id ; TODO: investigate if we can batch meshes better
                  :select-batch-key _node-id
                  :user-data {:array-streams (into []
                                                   (comp (map buffer/stream->array-stream)
                                                         (map (fn [{:keys [type count] :as stream}]
                                                                (assoc stream :put-fn (buffer/get-put-fn type count)))))
                                                   streams)
                              :put-vertex-fn ()
                              :position-stream-name position-stream
                              :normal-stream-name normal-stream
                              :shader shader
                              :textures gpu-textures
                              :scratch-arrays (gen-scratch-arrays meshes)}
                  :passes [pass/opaque pass/opaque-selection]}
     :children [{:node-id _node-id
                 :aabb aabb
                 :renderable {:render-fn render-outline
                              :tags #{:model :outline}
                              :batch-key _node-id
                              :select-batch-key _node-id
                              :passes [pass/outline]}}]}

    (update scene :renderable
            (fn [r]
              (cond-> r
                      shader (assoc-in [:user-data :shader] shader)
                      true (assoc-in [:user-data :textures] gpu-textures)
                      true (assoc-in [:user-data :vertex-space] vertex-space)
                      true (update :batch-key
                                   (fn [old-key]
                                     ;; We can only batch-render models that use
                                     ;; :vertex-space-world. In :vertex-space-local
                                     ;; we must supply individual transforms for
                                     ;; each model instance in the shader uniforms.
                                     (when (= :vertex-space-world vertex-space)
                                       [old-key shader gpu-textures]))))))
    ))

(defn- vset [v i value]
  (let [c (count v)
        v (if (<= c i) (into v (repeat (- i c) nil)) v)]
    (assoc v i value)))

(g/defnode MeshNode
  (inherits resource-node/ResourceNode)

  (property name g/Str (dynamic visible (g/constantly false)))

  (property material resource/Resource
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

  (property vertices resource/Resource
            (value (gu/passthrough vertices-resource))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter evaluation-context self old-value new-value
                                            [:resource :vertices-resource]
                                            [:build-targets :dep-build-targets]
                                            [:stream-ids :stream-ids]
                                            [:streams :streams])))
            (dynamic edit-type (g/constantly {:type resource/Resource
                                              :ext "buffer"})))
  (property textures resource/ResourceVec
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

  (property primitive-type g/Any (default :primitive-triangles)
            (dynamic edit-type (g/constantly (properties/->pb-choicebox MeshProto$MeshDesc$PrimitiveType))))

  (property position-stream g/Str
            (dynamic error (g/fnk [_node-id vertices stream-ids position-stream]
                             (validate-stream-id _node-id :position-stream vertices position-stream stream-ids)))
            (dynamic edit-type (g/fnk [stream-ids] (properties/->choicebox (conj stream-ids "")))))

  (property normal-stream g/Str
            (dynamic error (g/fnk [_node-id vertices stream-ids normal-stream]
                             (validate-stream-id _node-id :normal-stream vertices normal-stream stream-ids)))
            (dynamic edit-type (g/fnk [stream-ids] (properties/->choicebox (conj stream-ids "")))))

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

  (output pb-msg g/Any :cached produce-pb-msg)
  (output save-value g/Any (gu/passthrough pb-msg))
  (output build-targets g/Any :cached produce-build-targets)
  (output gpu-textures g/Any :cached produce-gpu-textures)
  (output scene g/Any :cached produce-scene)
  (input aabb AABB)
  (output aabb AABB (gu/passthrough aabb))
  (output _properties g/Properties :cached (g/fnk [_node-id _declared-properties textures samplers]
                                                  (let [resource-type (get-in _declared-properties [:properties :material :type])
                                                        prop-entry {:node-id _node-id
                                                                    :type resource-type
                                                                    :edit-type {:type resource/Resource
                                                                                :ext (conj image/exts "cubemap")}}
                                                        keys (map :name samplers)
                                                        p (->> keys
                                                               (map-indexed (fn [i s]
                                                                              [(keyword (format "texture%d" i))
                                                                               (-> prop-entry
                                                                                   (assoc :value (get textures i)
                                                                                          :label s)
                                                                                   (assoc-in [:edit-type :set-fn]
                                                                                             (fn [_evaluation-context self old-value new-value]
                                                                                               (g/update-property self :textures vset i new-value))))])))]
                                                    (-> _declared-properties
                                                        (update :properties into p)
                                                        (update :display-order into (map first p)))))))

(defn load-mesh [project self resource pb]
  (concat
    (g/set-property self :primitive-type (:primitive-type pb))
    (g/set-property self :position-stream (:position-stream pb))
    (g/set-property self :normal-stream (:normal-stream pb))
    (for [res [:material :vertices [:textures]]]
      (if (vector? res)
        (let [res (first res)]
          (g/set-property self res (mapv #(workspace/resolve-resource resource %) (get pb res))))
        (->> (get pb res)
          (workspace/resolve-resource resource)
          (g/set-property self res))))))

(defn register-resource-types [workspace]
  (resource-node/register-ddf-resource-type workspace
    :ext "mesh"
    :label "Mesh"
    :node-type MeshNode
    :ddf-type MeshProto$MeshDesc
    :load-fn load-mesh
    :icon mesh-icon
    :view-types [:scene :text]
    :tags #{:component}
    :tag-opts {:component {:transform-properties #{:position :rotation}}}))

(defn- transform-position-data-v2! [data-array ^Matrix4d world-transform]
  (let [^Point3d temp-point (Point3d.)
        array-length (alength data-array)]
    (loop [i 0]
      (if (= i array-length)
        data-array
        (do
          (.set temp-point
                (aget data-array i)
                (aget data-array (inc i))
                0.0)
          (.transform world-transform temp-point)
          (aset data-array i (.x temp-point))
          (aset data-array (inc i) (.y temp-point))
          (recur (+ i 2)))))))

(defn- transform-position-data-v3! [data-array ^Matrix4d world-transform]
  (let [^Point3d temp-point (Point3d.)
        array-length (alength data-array)]
    (loop [i 0]
      (if (= i array-length)
        data-array
        (do
          (.set temp-point
                (aget data-array i)
                (aget data-array (inc i))
                (aget data-array (+ i 2)))
          (.transform world-transform temp-point)
          (aset data-array i (.x temp-point))
          (aset data-array (inc i) (.y temp-point))
          (aset data-array (+ i 2) (.z temp-point))
          (recur (+ i 3)))))))

(defn- transform-normal-data-v3! [data-array ^Matrix3d normal-transform]
  (let [^Vector3d temp-normal (Vector3d.)
        array-length (alength data-array)]
    (loop [i 0]
      (if (= i array-length)
        data-array
        (do
          (.set temp-normal
                (aget data-array i)
                (aget data-array (inc i))
                (aget data-array (+ i 2)))
          (.transform normal-transform temp-normal)
          (.normalize temp-normal)
          (aset data-array i (.x temp-normal))
          (aset data-array (inc i) (.y temp-normal))
          (aset data-array (+ i 2) (.z temp-normal))
          (recur (+ i 3)))))))

(defn- populate-scratch-array! [scratch-array array-stream-data]
  (System/arraycopy array-stream-data 0 scratch-array 0 (alength array-stream-data))
  scratch-array)

(defn- transform-position-array-stream [{:keys [count data] :as array-stream} scratch-array ^Matrix4d world-transform]
  (assoc array-stream
    :data (case count
            2 (transform-position-data-v2! (populate-scratch-array! scratch-array data) world-transform)
            3 (transform-position-data-v3! (populate-scratch-array! scratch-array data) world-transform))))

(defn- transform-normal-array-stream [{:keys [data] :as array-stream} scratch-array ^Matrix3d normal-transform]
  (assoc array-stream
    :data (-> scratch-array
              (populate-scratch-array! data)
              (transform-normal-data-v3! normal-transform))))

(defn- world-transform->normal-transform
  ^Matrix3d [^Matrix4d world-transform]
  (let [normal-transform (Matrix3d.)]
    (.getRotationScale world-transform normal-transform)
    (.invert normal-transform)
    (.transpose normal-transform)
    normal-transform))


(defn- populate-vb! [^VertexBuffer vb {:keys [array-streams position-stream-name normal-stream-name scratch-arrays vertex-space world-transform]}]
  (assert (= (count array-streams) (count scratch-arrays)))
  (let [array-streams' (if (not= vertex-space :vertex-space-world)
                         array-streams
                         (map (fn [{:keys [name] :as array-stream} scratch-array]
                                (cond
                                  (and (not-empty position-stream-name)
                                       (= name position-stream-name))
                                  (transform-position-array-stream array-stream scratch-array world-transform)

                                  (and (not-empty normal-stream-name)
                                       (= name normal-stream-name))
                                  (transform-normal-array-stream array-stream scratch-array (world-transform->normal-transform world-transform))

                                  :else
                                  array-stream))
                              array-streams
                              scratch-arrays)]

    ; todo use get-put-fn from buffer to fill interleaved buffer
    ))

(defn- stream-type->gl-type [stream-type]
  (case stream-type
    :value-type-uint8 GL2/GL_UNSIGNED_BYTE
    :value-type-uint16 GL2/GL_UNSIGNED_SHORT
    :value-type-uint32 GL2/GL_UNSIGNED_INT
    :value-type-int8 GL2/GL_BYTE
    :value-type-int16 GL2/GL_SHORT
    :value-type-int32 GL2/GL_INT
    :value-type-float32 GL2/GL_FLOAT))

(defn- stream->attribute [stream]
  {:components (:count stream)
   :type (stream-type->gl-type (:type stream))
   :name (:name stream)
   :normalized? false}) ; todo figure out if this should be configurable

(defn- max-stream-length [streams]
  (transduce (map (fn [stream]
                    (/ (count (:data stream))
                       (:count stream))))
             max
             streams))

(defn- make-vb-from-array-streams [array-streams]
  (let [attributes (map stream->attribute array-streams)
        vertex-count (max-stream-length array-streams)
        vertex-description (vtx/make-vertex-description nil attributes)]
    (vtx/make-vertex-buffer vertex-description :static vertex-count)))

(defn- update-vb! [^GL2 gl ^VertexBuffer vb data]
  (let [data' (update data :world-transform
                      (fn [world-transform]
                        (doto (Matrix4d.)
                          (math/clj->vecmath world-transform))))]
    (-> vb
        (vtx/clear!)
        (populate-vb! data')
        (vtx/flip!))))

(defn- make-vb [^GL2 gl data]
  (let [{:keys [array-streams]} data
        vb (make-vb-from-array-streams array-streams)]
    (update-vb! gl vb data)))

(defn- destroy-vbs! [^GL2 gl vbs _])

(scene-cache/register-object-cache! ::vb make-vb update-vb! destroy-vbs!)