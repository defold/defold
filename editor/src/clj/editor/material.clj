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

(ns editor.material
  (:require [dynamo.graph :as g]
            [editor.build-target :as bt]
            [editor.code.data :as code.data]
            [editor.code.shader-compilation :as shader-compilation]
            [editor.defold-project :as project]
            [editor.gl.shader :as shader]
            [editor.graph-util :as gu]
            [editor.graphics :as graphics]
            [editor.graphics.types :as graphics.types]
            [editor.localization :as localization]
            [editor.pipeline.shader-gen :as shader-gen]
            [editor.protobuf :as protobuf]
            [editor.protobuf-forms :as protobuf-forms]
            [editor.protobuf-forms-util :as protobuf-forms-util]
            [editor.render-program-utils :as render-program-utils]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.validation :as validation]
            [editor.workspace :as workspace]
            [internal.util :as util]
            [util.coll :as coll :refer [pair]]
            [util.murmur :as murmur]
            [util.num :as num])
  (:import [com.dynamo.bob.pipeline MaterialBuilder]
           [com.dynamo.graphics.proto Graphics$CoordinateSpace Graphics$VertexAttribute Graphics$VertexAttribute$DataType Graphics$VertexAttribute$SemanticType Graphics$VertexAttribute$VectorType Graphics$VertexStepFunction]
           [com.dynamo.render.proto Material$MaterialDesc Material$MaterialDesc$Sampler Material$MaterialDesc$VertexSpace]
           [com.jogamp.opengl GL2]
           [editor.gl.shader ShaderLifecycle]
           [javax.vecmath Matrix4d Vector4d]))

(set! *warn-on-reflection* true)

(def ^:private editable-attribute-optional-field-defaults
  (-> Graphics$VertexAttribute
      (protobuf/default-message #{:optional})
      (dissoc :binary-values :double-values :long-values :name-hash)))

(defn- attribute->editable-attribute [attribute]
  {:pre [(map? attribute)]} ; Graphics$VertexAttribute in map format.
  ;; The protobuf stores values in different arrays depending on their type.
  ;; Here, we convert all of them into a vector of doubles for easy editing.
  ;; This is fine, since doubles can accurately represent values in the entire
  ;; signed and unsigned 32-bit integer range from -2147483648 to 4294967295.
  (let [attribute (merge editable-attribute-optional-field-defaults attribute)
        values (graphics/attribute->doubles attribute)]
    (-> attribute
        (dissoc :double-values :long-values) ; Only one of these will be present, but we want neither.
        (assoc :values values))))

(defn- editable-attribute->attribute [{:keys [data-type normalize values] :as editable-attribute}]
  {:pre [(map? editable-attribute)
         (vector? values)]}
  (let [[attribute-value-keyword stored-values] (graphics/doubles->storage values data-type normalize)]
    (protobuf/clear-defaults Graphics$VertexAttribute
      (-> editable-attribute
          (dissoc :values)
          (protobuf/assign attribute-value-keyword
                           (when (and (not (graphics/engine-provided-attribute? editable-attribute))
                                      (coll/not-empty stored-values))
                             {:v stored-values}))))))

(defn- save-value-attributes [editable-attributes]
  (mapv editable-attribute->attribute editable-attributes))

(defn- build-target-attributes [attribute-infos]
  (mapv graphics/attribute-info->build-target-attribute attribute-infos))

(g/defnk produce-base-pb-msg [name vertex-program fragment-program vertex-constants fragment-constants ^:raw samplers tags vertex-space max-page-count]
  (protobuf/make-map-without-defaults Material$MaterialDesc
    :name name
    :vertex-program (resource/resource->proj-path vertex-program)
    :fragment-program (resource/resource->proj-path fragment-program)
    :vertex-constants (render-program-utils/editable-constants->constants vertex-constants)
    :fragment-constants (render-program-utils/editable-constants->constants fragment-constants)
    :samplers (render-program-utils/editable-samplers->samplers samplers)
    :tags tags
    :vertex-space vertex-space
    :max-page-count max-page-count))

(g/defnk produce-save-value [base-pb-msg attributes]
  (protobuf/assign-repeated base-pb-msg
    :attributes (save-value-attributes attributes)))

(defn- build-material [resource build-resource->fused-build-resource user-data]
  (let [build-resource->fused-build-resource-path (comp resource/proj-path build-resource->fused-build-resource)
        material-desc-with-fused-build-resource-paths
        (-> (:material-desc-with-build-resources user-data)
            (update :program build-resource->fused-build-resource-path))]
    {:resource resource
     :content (protobuf/map->bytes Material$MaterialDesc material-desc-with-fused-build-resource-paths)}))

(defn- prop-resource-error [_node-id prop-kw prop-value prop-name resource-ext]
  (validation/prop-error :fatal _node-id prop-kw validation/prop-resource-ext? prop-value resource-ext prop-name))

(defn- build-target-samplers [samplers max-page-count]
  (mapv (fn [sampler]
          (assoc sampler
            :name-hash (murmur/hash64 (:name sampler))
            :name-indirections (mapv (fn [slice-index]
                                       (murmur/hash64 (str (:name sampler) "_" slice-index)))
                                     (range max-page-count))))
        samplers))

(defn- attribute-info->error-values [{:keys [data-type error name normalize]} node-id label]
  (filterv some?
           [error
            (when (and normalize
                       (= :type-float data-type))
              (g/->error node-id label :fatal nil
                         (format "'%s' attribute uses normalize with float data type"
                                 name)))]))

(defn- build-target-pbr-params [shader-reflection]
  (when-some [pbr-parameters-proto (MaterialBuilder/makePbrParametersProtoMessage shader-reflection)]
    (protobuf/pb->map-without-defaults pbr-parameters-proto)))

(g/defnk produce-build-targets [_node-id attribute-infos base-pb-msg fragment-program fragment-shader-source-info max-page-count exclude-gles-sm100 resource vertex-program vertex-shader-source-info]
  (or (g/flatten-errors
        (prop-resource-error _node-id :vertex-program vertex-program "Vertex Program" "vp")
        (prop-resource-error _node-id :fragment-program fragment-program "Fragment Program" "fp")
        (mapcat #(attribute-info->error-values % _node-id :attributes) attribute-infos))
      (let [shader-desc-build-target (shader-compilation/make-shader-build-target _node-id [vertex-shader-source-info fragment-shader-source-info] max-page-count exclude-gles-sm100)
            build-target-samplers (build-target-samplers (:samplers base-pb-msg) max-page-count)
            build-target-attributes (build-target-attributes attribute-infos)
            build-target-pbr-params (build-target-pbr-params (:shader-reflection shader-desc-build-target))
            dep-build-targets [shader-desc-build-target]
            material-desc-with-build-resources (assoc base-pb-msg
                                                 :program (:resource shader-desc-build-target)
                                                 :samplers build-target-samplers
                                                 :attributes build-target-attributes
                                                 :pbr-parameters build-target-pbr-params)]
        [(bt/with-content-hash
           {:node-id _node-id
            :resource (workspace/make-build-resource resource)
            :build-fn build-material
            :user-data {:material-desc-with-build-resources material-desc-with-build-resources}
            :deps dep-build-targets})])))

(defn- constant->val [constant]
  (case (:type constant)
    :constant-type-user (let [[x y z w] (:value constant)]
                          (Vector4d. x y z w))
    :constant-type-user-matrix4 (let [[x y z w] (:value constant)]
                                  (doto (Matrix4d.)
                                    (.setElement 0 0 x)
                                    (.setElement 1 0 y)
                                    (.setElement 2 0 z)
                                    (.setElement 3 0 w)))
    :constant-type-viewproj :view-proj
    :constant-type-world :world
    :constant-type-texture :texture
    :constant-type-view :view
    :constant-type-projection :projection
    :constant-type-normal :normal
    :constant-type-worldview :world-view
    :constant-type-worldviewproj :world-view-proj))

(defn- transpile-shader-source
  [shader-resource-node-id shader-resource ^String shader-source ^long max-page-count]
  ;; TODO(instancing): The shader-source has been preprocessed and will contain
  ;; lines from include directives, so we do not report the correct source paths
  ;; and line numbers here.
  (let [shader-proj-path (resource/proj-path shader-resource)]
    (try
      (shader-gen/transpile-shader-source shader-proj-path shader-source max-page-count)
      (catch Exception exception
        (let [ex-data (ex-data exception)]
          (if-not (shader-gen/shader-transpile-ex-data? ex-data)
            (throw exception)
            (let [ex-cause (ex-cause exception)
                  ex-message (ex-message exception)
                  cause-message (.getMessage ex-cause)
                  message (str ex-message \newline cause-message)
                  error-resource (or (some->> ex-data :error-proj-path (workspace/resolve-resource shader-resource))
                                     shader-resource)
                  error-cursor-range (some-> ex-data :error-line-number code.data/line-number->CursorRange)
                  user-data (cond-> {:resource error-resource}
                                    error-cursor-range (assoc :cursor-range error-cursor-range))]
              (g/->error shader-resource-node-id :lines :fatal nil message user-data))))))))

(g/defnk produce-combined-shader-info [_node-id vertex-program vertex-shader-source-info fragment-program fragment-shader-source-info max-page-count]
  (or (prop-resource-error _node-id :vertex-program vertex-program "Vertex Program" "vp")
      (prop-resource-error _node-id :fragment-program fragment-program "Fragment Program" "fp")
      (let [augmented-shader-infos
            (mapv (fn [{:keys [node-id resource shader-source]}]
                    (transpile-shader-source node-id resource shader-source max-page-count))
                  [vertex-shader-source-info
                   fragment-shader-source-info])]
        (g/precluding-errors augmented-shader-infos
          (shader-gen/combined-shader-info augmented-shader-infos)))))

(g/defnk produce-shader-request-data [combined-shader-info]
  (shader/make-shader-request-data
    (:shader-type+source-pairs combined-shader-info)
    (:location+attribute-name-pairs combined-shader-info)
    (:array-sampler-name->slice-sampler-names combined-shader-info)
    (:strip-resource-binding-namespace-regex-str combined-shader-info)))

(g/defnk produce-shader [_node-id combined-shader-info shader-request-data vertex-constants fragment-constants samplers]
  (let [{:keys [array-sampler-name->slice-sampler-names attribute-reflection-infos]} combined-shader-info

        uniform-values-by-name
        (-> {}
            (into (map (fn [constant]
                         (pair (:name constant) (constant->val constant))))
                  (concat vertex-constants fragment-constants))
            (into (comp
                    (mapcat (fn [{sampler-name :name}]
                              (or (array-sampler-name->slice-sampler-names sampler-name)
                                  [sampler-name])))
                    (map (fn [resolved-sampler-name]
                           (pair resolved-sampler-name nil))))
                  samplers))]

    (shader/make-shader-lifecycle _node-id shader-request-data attribute-reflection-infos uniform-values-by-name)))

(g/defnk produce-samplers [^:raw samplers default-sampler-filter-modes]
  ;; Replace any default filter modes with the setting from game.project.
  (let [{:keys [filter-mode-mag-default filter-mode-min-default]} default-sampler-filter-modes]
    (mapv (fn [sampler]
            {:pre [(map? sampler)]} ; Material$MaterialDesc$Sampler in map format.
            (-> sampler
                (update
                  :filter-mag
                  (fn [filter-mag]
                    (case filter-mag
                      :filter-mode-mag-default filter-mode-mag-default
                      filter-mag)))
                (update
                  :filter-min
                  (fn [filter-min]
                    (case filter-min
                      :filter-mode-min-default filter-mode-min-default
                      filter-min)))))
          samplers)))

(defn- vector-type->form-field-type [vector-type]
  (case vector-type
    :vector-type-scalar :vec4
    :vector-type-vec2 :vec4
    :vector-type-vec3 :vec4
    :vector-type-vec4 :vec4
    :vector-type-mat2 :mat4
    :vector-type-mat3 :mat4
    :vector-type-mat4 :mat4))

(def unsupported-semantic-types
  #{:semantic-type-bone-weights
    :semantic-type-bone-indices})

(def ^:private vertex-attribute-fields
  [{:path [:semantic-type]
    :localization-key "material.attributes.semantic-type"
    :type :choicebox
    :options (remove #(unsupported-semantic-types (first %)) (protobuf-forms/make-enum-options Graphics$VertexAttribute$SemanticType))
    :default graphics/default-attribute-semantic-type}
   {:path [:step-function]
    :localization-key "material.attributes.step-function"
    :type :choicebox
    :options (protobuf-forms/make-enum-options Graphics$VertexStepFunction)
    :default graphics/default-attribute-step-function}
   {:path [:coordinate-space]
    :localization-key "material.attributes.coordinate-space"
    :type :choicebox
    :options (protobuf-forms/make-enum-options Graphics$CoordinateSpace)
    :default :coordinate-space-local}
   {:path [:data-type]
    :localization-key "material.attributes.data-type"
    :type :choicebox
    :options (protobuf-forms/make-enum-options Graphics$VertexAttribute$DataType)
    :default graphics/default-attribute-data-type}
   {:path [:vector-type]
    :localization-key "material.attributes.vector-type"
    :type :choicebox
    :options (protobuf-forms/make-enum-options Graphics$VertexAttribute$VectorType)
    :default graphics/default-attribute-vector-type}
   {:path [:values]
    :localization-key "material.attributes.value"
    :type (vector-type->form-field-type graphics/default-attribute-vector-type)
    :default (graphics.types/default-attribute-doubles graphics/default-attribute-semantic-type graphics/default-attribute-vector-type)}
   {:path [:normalize]
    :localization-key "material.attributes.normalize"
    :type :boolean
    :default false}])

(def ^:private ^long value-vertex-attribute-field-index
  (coll/first-index-where #(= [:values] (:path %))
                          vertex-attribute-fields))

(def ^:private form-data
  {:navigation false
   :sections
   [{:localization-key "material"
     :fields
     [{:path [:name]
       :localization-key "material.name"
       :type :string
       :default "New Material"}
      {:path [:vertex-program]
       :localization-key "material.vertex-program"
       :type :resource :filter "vp"}
      {:path [:fragment-program]
       :localization-key "material.fragment-program"
       :type :resource :filter "fp"}
      {:path [:attributes]
       :localization-key "material.attributes"
       :type :2panel
       :panel-key {:path [:name]
                   :type :string
                   :default "new_attribute"}
       :panel-form-fn
       (fn panel-form-fn [selected-attribute]
         {:sections
          [{:fields
            (cond
              (nil? selected-attribute)
              vertex-attribute-fields

              (graphics/engine-provided-attribute? selected-attribute)
              (coll/remove-index vertex-attribute-fields value-vertex-attribute-field-index)

              :else
              (assoc vertex-attribute-fields
                value-vertex-attribute-field-index
                (let [semantic-type (:semantic-type selected-attribute graphics/default-attribute-semantic-type)
                      vector-type (:vector-type selected-attribute graphics/default-attribute-vector-type)
                      type (vector-type->form-field-type vector-type)
                      default (graphics.types/default-attribute-doubles semantic-type vector-type)]
                  {:path [:values]
                   :localization-key "material.attributes.value"
                   :type type
                   :default default})))}]})}
      (render-program-utils/gen-form-data-constants "material.vertex-constants" :vertex-constants)
      (render-program-utils/gen-form-data-constants "material.fragment-constants" :fragment-constants)
      (render-program-utils/gen-form-data-samplers "material.samplers" :samplers)
      {:path [:tags]
       :localization-key "material.tags"
       :type :list
       :element {:type :string :default "New Tag"}}
      {:path [:vertex-space]
       :localization-key "material.vertex-space"
       :type :choicebox
       :options (protobuf-forms/make-enum-options Material$MaterialDesc$VertexSpace)
       :default (ffirst (protobuf/enum-values Material$MaterialDesc$VertexSpace))}
      {:path [:max-page-count]
       :localization-key "material.max-page-count"
       :type :integer
       :default 0}]}]})

(defn- coerce-attribute [new-attribute old-attribute]
  ;; This assumes only a single property will change at a time, which is the
  ;; case when editing an attribute using the form view.
  (let [old-vector-type (:vector-type old-attribute)
        old-normalize (:normalize old-attribute)
        new-vector-type (:vector-type new-attribute)
        new-normalize (:normalize new-attribute)]
    (assert (graphics.types/vector-type? old-vector-type))
    (assert (graphics.types/vector-type? new-vector-type))
    (cond
      ;; If an attribute changes from a non-normalized value to a normalized one
      ;; or vice versa, attempt to remap the value range. Note that we cannot do
      ;; anything about attribute overrides stored elsewhere in the project.
      ;; These overrides will instead need to produce errors if they are outside
      ;; the allowed value range.
      (and (not= old-normalize new-normalize)
           (not= :type-float (:data-type new-attribute)))
      (let [coerce-fn
            (if new-normalize
              ;; Converting from non-normalized to normalized values.
              (case (:data-type new-attribute)
                :type-byte num/byte-range->normalized
                :type-unsigned-byte num/ubyte-range->normalized
                :type-short num/short-range->normalized
                :type-unsigned-short num/ushort-range->normalized
                :type-int num/int-range->normalized
                :type-unsigned-int num/uint-range->normalized)

              ;; Converting from normalized to non-normalized values.
              (case (:data-type new-attribute)
                :type-byte num/normalized->byte-double
                :type-unsigned-byte num/normalized->ubyte-double
                :type-short num/normalized->short-double
                :type-unsigned-short num/normalized->ushort-double
                :type-int num/normalized->int-double
                :type-unsigned-int num/normalized->uint-double))]
        (update new-attribute :values coll/transform (map coerce-fn)))

      ;; If the vector type changes, resize the default value in the material.
      ;; This change will also cause attribute overrides stored elsewhere in the
      ;; project to be saved with the updated vector type.
      (not= old-vector-type new-vector-type)
      (let [semantic-type (:semantic-type new-attribute)]
        (update new-attribute :values #(graphics/convert-double-values % semantic-type old-vector-type new-vector-type)))

      ;; If something else changed, do not attempt value coercion.
      :else
      new-attribute)))

(defn- set-form-value-fn [property value user-data]
  (case property
    :attributes
    ;; When setting the attributes, coerce the existing values to conform to the
    ;; updated data and vector type.
    (let [old-attributes-by-name (coll/pair-map-by :name (:attributes user-data))]
      (mapv (fn [new-attribute]
              (if-some [old-attribute (get old-attributes-by-name (:name new-attribute))]
                (coerce-attribute new-attribute old-attribute)
                new-attribute))
            value))

    ;; Default case.
    value))

(defn- set-form-op [{:keys [node-id] :as user-data} [property] value]
  (let [processed-value (set-form-value-fn property value user-data)]
    (g/set-property node-id property processed-value)))

(g/defnk produce-form-data [_node-id name attributes vertex-program fragment-program vertex-constants fragment-constants max-page-count ^:raw samplers tags vertex-space :as args]
  (let [values (select-keys args (mapcat :path (get-in form-data [:sections 0 :fields])))
        form-values (into {} (map (fn [[k v]] [[k] v]) values))]
    (-> form-data
        (assoc :values form-values)
        (assoc :form-ops {:user-data {:node-id _node-id :attributes attributes}
                          :set set-form-op
                          :clear protobuf-forms-util/clear-form-op}))))

(def ^:private wrap-mode->gl {:wrap-mode-repeat GL2/GL_REPEAT
                              :wrap-mode-mirrored-repeat GL2/GL_MIRRORED_REPEAT
                              :wrap-mode-clamp-to-edge GL2/GL_CLAMP_TO_EDGE})

(defn- filter-mode-min->gl [filter-min default-tex-params]
  (case filter-min
    :filter-mode-min-default (or (:min-filter default-tex-params) GL2/GL_NEAREST_MIPMAP_LINEAR)
    :filter-mode-min-nearest GL2/GL_NEAREST
    :filter-mode-min-linear GL2/GL_LINEAR
    :filter-mode-min-nearest-mipmap-nearest GL2/GL_NEAREST_MIPMAP_NEAREST
    :filter-mode-min-nearest-mipmap-linear GL2/GL_NEAREST_MIPMAP_LINEAR
    :filter-mode-min-linear-mipmap-nearest GL2/GL_LINEAR_MIPMAP_NEAREST
    :filter-mode-min-linear-mipmap-linear GL2/GL_LINEAR_MIPMAP_LINEAR
    GL2/GL_NEAREST_MIPMAP_LINEAR))

(defn- filter-mode-mag->gl [filter-mag default-tex-params]
  (case filter-mag
    :filter-mode-mag-default (or (:mag-filter default-tex-params) GL2/GL_LINEAR)
    :filter-mode-mag-nearest GL2/GL_NEAREST
    :filter-mode-mag-linear GL2/GL_LINEAR
    GL2/GL_LINEAR))

(def ^:private default-pb-sampler
  (protobuf/make-map-without-defaults Material$MaterialDesc$Sampler
    :wrap-u :wrap-mode-clamp-to-edge
    :wrap-v :wrap-mode-clamp-to-edge
    :filter-min :filter-mode-min-linear
    :filter-mag :filter-mode-mag-linear))

(def ^:private default-editable-sampler (render-program-utils/sampler->editable-sampler default-pb-sampler))

(defn sampler->tex-params
  ([sampler]
   (sampler->tex-params sampler nil))
  ([sampler default-tex-params]
   (let [s (or sampler default-editable-sampler)
         params {:wrap-s (wrap-mode->gl (:wrap-u s))
                 :wrap-t (wrap-mode->gl (:wrap-v s))
                 :min-filter (filter-mode-min->gl (:filter-min s) default-tex-params)
                 :mag-filter (filter-mode-mag->gl (:filter-mag s) default-tex-params)
                 :name (:name s)
                 :default-tex-params default-tex-params}]
     (if (and (not sampler) default-tex-params)
       (merge params default-tex-params)
       params))))

(g/defnk produce-attribute-infos [_node-id attributes]
  (mapv (fn [attribute]
          (let [name (:name attribute)
                name-key (graphics.types/attribute-name-key name)
                [bytes error-message] (graphics/attribute->bytes+error-message attribute)]
            (cond-> (assoc attribute
                      :bytes bytes
                      :name-key name-key)

                    (some? error-message)
                    (assoc
                      :error (g/->error _node-id :attributes :fatal nil error-message)))))
        attributes))

(defmulti handle-sampler-names-changed
  (fn [evaluation-context target-node old-name-index new-name-index sampler-renames sampler-deletions]
    (let [basis (:basis evaluation-context)]
      (g/node-type-kw basis target-node))))

(defmethod handle-sampler-names-changed :default [_ _ _ _ _ _])

(defn- notify-sampler-names-targets-setter [evaluation-context self label old-value new-value]
  (let [old-names (mapv :name old-value)
        new-names (mapv :name new-value)]
    (when-not (= old-names new-names)
      (let [old-name-index (util/name-index old-names identity)
            new-name-index (util/name-index new-names identity)
            renames (util/detect-renames old-name-index new-name-index)
            deletions (util/detect-deletions old-name-index new-name-index)]
        (into []
              (comp
                (map first)
                (distinct)
                (mapcat #(handle-sampler-names-changed evaluation-context % old-name-index new-name-index renames deletions)))
              (g/targets-of (:basis evaluation-context) self label))))))

(g/defnode MaterialNode
  (inherits resource-node/ResourceNode)

  (property name g/Str ; Required protobuf field.
            (dynamic visible (g/constantly false)))

  (property vertex-program resource/Resource ; Required protobuf field.
    (dynamic visible (g/constantly false))
    (value (gu/passthrough vertex-resource))
    (set (fn [evaluation-context self old-value new-value]
           (project/resource-setter evaluation-context self old-value new-value
                                    [:resource :vertex-resource]
                                    [:shader-source-info :vertex-shader-source-info]))))

  (property fragment-program resource/Resource ; Required protobuf field.
    (dynamic visible (g/constantly false))
    (value (gu/passthrough fragment-resource))
    (set (fn [evaluation-context self old-value new-value]
           (project/resource-setter evaluation-context self old-value new-value
                                    [:resource :fragment-resource]
                                    [:shader-source-info :fragment-shader-source-info]))))

  (property max-page-count g/Int (default (protobuf/default Material$MaterialDesc :max-page-count))
            (dynamic visible (g/constantly false)))
  (property attributes g/Any ; Nil is valid default.
            (dynamic visible (g/constantly false)))
  (property vertex-constants g/Any ; Nil is valid default.
            (dynamic visible (g/constantly false)))
  (property fragment-constants g/Any ; Nil is valid default.
            (dynamic visible (g/constantly false)))
  (property samplers g/Any ; Nil is valid default.
            (dynamic visible (g/constantly false))
            (set (fn [evaluation-context self old-value new-value]
                   (notify-sampler-names-targets-setter evaluation-context self :samplers old-value new-value))))
  (property tags g/Any ; Nil is valid default.
            (dynamic visible (g/constantly false)))
  (property vertex-space g/Keyword (default (protobuf/default Material$MaterialDesc :vertex-space))
            (dynamic visible (g/constantly false)))

  (output form-data g/Any :cached produce-form-data)

  (input default-sampler-filter-modes g/Any)
  (input vertex-resource resource/Resource)
  (input vertex-shader-source-info g/Any)
  (input fragment-resource resource/Resource)
  (input fragment-shader-source-info g/Any)
  (input exclude-gles-sm100 g/Any)

  (output base-pb-msg g/Any produce-base-pb-msg)

  (output save-value g/Any produce-save-value)
  (output build-targets g/Any :cached produce-build-targets)
  (output combined-shader-info g/Any :cached produce-combined-shader-info)
  (output shader-request-data g/Any :cached produce-shader-request-data)
  (output shader ShaderLifecycle :cached produce-shader)
  (output samplers [g/KeywordMap] :cached produce-samplers)
  (output attribute-infos [g/KeywordMap] :cached produce-attribute-infos))

(defn- legacy-texture->sampler [name]
  (assoc default-pb-sampler :name name))

(defn load-material [project self resource material-desc]
  {:pre [(map? material-desc)]} ; Material$MaterialDesc in map format.
  (let [resolve-resource #(workspace/resolve-resource resource %)
        attributes->editable-attributes #(mapv attribute->editable-attribute %)]
    (concat
      (g/connect project :default-sampler-filter-modes self :default-sampler-filter-modes)
      (g/connect project :exclude-gles-sm100 self :exclude-gles-sm100)
      (gu/set-properties-from-pb-map self Material$MaterialDesc material-desc
        vertex-program (resolve-resource :vertex-program)
        fragment-program (resolve-resource :fragment-program)
        vertex-constants (render-program-utils/constants->editable-constants :vertex-constants)
        fragment-constants (render-program-utils/constants->editable-constants :fragment-constants)
        attributes (attributes->editable-attributes :attributes)
        name :name
        samplers (render-program-utils/samplers->editable-samplers :samplers)
        tags :tags
        vertex-space :vertex-space
        max-page-count :max-page-count))))

(defn- sanitize-material
  "The old format specified :textures as string names. Convert these into
  :samplers if we encounter them. Ignores :textures that already have
  :samplers with the same name. Also ensures that there are no duplicate
  entries in the :samplers list, based on :name."
  [material-desc]
  ;; Material$MaterialDesc in map format.
  (let [existing-samplers (:samplers material-desc)
        samplers-created-from-textures (map legacy-texture->sampler (:textures material-desc))
        samplers (into []
                       (util/distinct-by :name)
                       (concat existing-samplers
                               samplers-created-from-textures))]
    (-> material-desc
        (dissoc :textures)
        (protobuf/assign-repeated :samplers samplers)
        (protobuf/sanitize-repeated :vertex-constants render-program-utils/sanitize-constant)
        (protobuf/sanitize-repeated :fragment-constants render-program-utils/sanitize-constant)
        (protobuf/sanitize-repeated :attributes graphics/sanitize-attribute-definition))))

(defn register-resource-types [workspace]
  (resource-node/register-ddf-resource-type workspace
    :ext "material"
    :label (localization/message "resource.type.material")
    :node-type MaterialNode
    :ddf-type Material$MaterialDesc
    :load-fn load-material
    :sanitize-fn sanitize-material
    :icon "icons/32/Icons_31-Material.png"
    :icon-class :property
    :view-types [:cljfx-form-view :text]))
