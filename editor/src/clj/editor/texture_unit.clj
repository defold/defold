(ns editor.texture-unit
  (:require [clojure.set :as set]
            [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.gl :as gl]
            [editor.gl.shader :as shader]
            [editor.graph-util :as gu]
            [editor.material :as material]
            [editor.properties :as properties]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [internal.util :as util])
  (:import (com.jogamp.opengl GL)
           (editor.gl.texture TextureLifecycle)))

(defn set-array-index [self prop-kw unit-index resource]
  (g/update-property self prop-kw util/vset unit-index resource))

(defn connect-resources [self old-value new-value connections]
  (let [project (project/get-project self)
        old-resources (into #{} (filter some?) old-value)
        new-resources (into #{} (filter some?) new-value)
        to-disconnect (set/difference old-resources new-resources)
        to-connect (set/difference new-resources old-resources)]
    (concat
      (for [resource to-disconnect]
        (project/disconnect-resource-node project resource self connections))
      (for [resource to-connect]
        (project/connect-resource-node project resource self connections)))))

(defn connect-resources-preserving-index [self old-value new-value connections]
  (let [project (project/get-project self)]
    (concat
      (for [resource old-value]
        (if (some? resource)
          (project/disconnect-resource-node project resource self connections)
          (for [[_ to] connections]
            (g/disconnect project :nil-value self to))))
      (for [resource new-value]
        (if (some? resource)
          (project/connect-resource-node project resource self connections)
          (for [[_ to] connections]
            (g/connect project :nil-value self to)))))))

(defn gpu-textures-by-sampler-name
  [default-tex-params gpu-texture-generators samplers]
  (if (empty? samplers)
    (when-some [{:keys [generate-fn user-data]} (first gpu-texture-generators)]
      {nil (generate-fn user-data default-tex-params 0)})
    (into {}
          (filter some?)
          (map (fn [unit-index sampler gpu-texture-generator]
                 (when (some? gpu-texture-generator)
                   (let [{:keys [generate-fn user-data]} gpu-texture-generator
                         params (material/sampler->tex-params sampler)
                         texture (generate-fn user-data params unit-index)]
                     [(:name sampler) texture])))
               (range)
               samplers
               gpu-texture-generators))))

(defn batch-key-entry [gpu-textures-by-sampler-name]
  (into {}
        (map (fn [[sampler-name ^TextureLifecycle gpu-texture]]
               [sampler-name [(.request-id gpu-texture) (.unit gpu-texture) (.params gpu-texture)]]))
        gpu-textures-by-sampler-name))

(defn bind-gpu-textures! [gl gpu-textures-by-sampler-name shader render-args]
  (doseq [[sampler-name gpu-texture] gpu-textures-by-sampler-name]
    (gl/bind gl gpu-texture render-args)
    (when (some? sampler-name)
      (shader/set-uniform shader gl sampler-name (- (:unit gpu-texture) GL/GL_TEXTURE0)))))

(defn unbind-gpu-textures! [gl gpu-textures-by-sampler-name shader render-args]
  (doseq [[sampler-name gpu-texture] gpu-textures-by-sampler-name]
    (gl/unbind gl gpu-texture render-args)
    (when (some? sampler-name)
      (shader/set-uniform shader gl sampler-name GL/GL_TEXTURE0))))

(defn unit-index->texture-prop-kw [^long unit-index]
  (case unit-index
    0 :texture0
    1 :texture1
    2 :texture2
    3 :texture3
    4 :texture4
    5 :texture5
    6 :texture6
    7 :texture7))

(def edit-type
  {:type resource/Resource
   :ext properties/sub-type-texture-ext})

(defn texture-unit-property-info [node-id textures-prop-kw prop-label array-index texture-resource]
  (assert (keyword? textures-prop-kw))
  (assert (string? prop-label))
  (assert (or (nil? texture-resource) (resource/resource? texture-resource)))
  {:node-id node-id
   :type resource/Resource
   :value texture-resource
   :label prop-label
   :edit-type (assoc edit-type
                :set-fn (fn [_evaluation-context self _old-value new-value]
                          (set-array-index self textures-prop-kw array-index new-value)))})

(defn texture-unit-property [node-id textures-prop-kw prop-label unit-index texture-resource]
  [(unit-index->texture-prop-kw unit-index)
   (texture-unit-property-info node-id textures-prop-kw prop-label unit-index texture-resource)])

(defn augment-properties [declared-properties texture-properties]
  (-> declared-properties
      (update :properties into texture-properties)
      (update :display-order conj (into ["Texture Samplers"]
                                        (map first)
                                        texture-properties))))

(defn texture-properties [node-id samplers textures]
  (when (sequential? samplers)
    (map (partial texture-unit-property node-id :textures)
         (map :name samplers)
         (range 8)
         (concat textures (repeat nil)))))

(defn texture-properties-with-texture-set [node-id samplers textures texture-set]
  (when (sequential? samplers)
    (concat
      {:texture0 {:node-id node-id
                  :type g/Str
                  :value (resource/resource->proj-path texture-set)
                  :label (:name (first samplers))
                  :read-only? true}}
      (map (partial texture-unit-property node-id :textures)
           (map :name (drop 1 samplers))
           (range 1 8)
           (drop 1 (concat textures (repeat nil)))))))

(defn properties [node-id declared-properties samplers textures]
  (augment-properties declared-properties (texture-properties node-id samplers textures)))

(defn properties-with-texture-set [node-id declared-properties samplers textures texture-set]
  (augment-properties declared-properties (texture-properties-with-texture-set node-id samplers textures texture-set)))

(defn resources->paths [resources]
  (let [kept-count (inc (or (util/last-index-where some? resources) -1))]
    (into []
          (comp (take kept-count)
                (map resource/resource->proj-path))
          resources)))

(g/defnode TextureUnitBaseNode
  (inherits resource-node/ResourceNode)

  (property textures resource/ResourceVec
            (value (gu/passthrough texture-resources))
            (set (fn [evaluation-context self _old-value new-value]
                   ;; TODO: Use old-value directly once DEFEDIT-1277 is fixed.
                   (let [old-value (g/node-value self :texture-resources evaluation-context)]
                     (concat
                       (connect-resources self old-value new-value [[:texture-build-target :dep-build-targets]])
                       (connect-resources-preserving-index self old-value new-value [[:resource :texture-resources]
                                                                                     [:texture-build-target :texture-build-targets]
                                                                                     [:gpu-texture-generator :gpu-texture-generators]])))))
            (dynamic visible (g/constantly false)))

  (input texture-resources resource/Resource :array)
  (input texture-build-targets g/Any :array)
  (input gpu-texture-generators g/Any :array))
