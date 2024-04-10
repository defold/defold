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

(ns editor.material
  (:require [dynamo.graph :as g]
            [editor.build-target :as bt]
            [editor.code.shader :as code.shader]
            [editor.defold-project :as project]
            [editor.gl.shader :as shader]
            [editor.graph-util :as gu]
            [editor.graphics :as graphics]
            [editor.protobuf :as protobuf]
            [editor.protobuf-forms :as protobuf-forms]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.validation :as validation]
            [editor.workspace :as workspace]
            [internal.util :as util]
            [util.coll :as coll :refer [pair]]
            [util.murmur :as murmur]
            [util.num :as num])
  (:import [com.dynamo.bob.pipeline ShaderProgramBuilder]
           [com.dynamo.graphics.proto Graphics$CoordinateSpace Graphics$VertexStepFunction Graphics$VertexAttribute$DataType Graphics$VertexAttribute$SemanticType Graphics$VertexAttribute$ShaderType]
           [com.dynamo.render.proto Material$MaterialDesc Material$MaterialDesc$ConstantType Material$MaterialDesc$FilterModeMag Material$MaterialDesc$FilterModeMin Material$MaterialDesc$VertexSpace Material$MaterialDesc$WrapMode]
           [com.jogamp.opengl GL2]
           [editor.gl.shader ShaderLifecycle]
           [javax.vecmath Matrix4d Vector4d]))

(set! *warn-on-reflection* true)

(defn- hack-downgrade-constant-value
  "HACK/FIXME: The value field in MaterialDesc$Constant was changed from
  `optional` to `repeated` in material.proto so that we can set uniform array
  values in the runtime. However, we do not yet support editing of array values
  in the material constant editing widget, and MaterialDesc$Constant is used for
  both the runtime binary format and the material file format. For the time
  being, we only read (and allow editing of) the first value from uniform
  arrays. Since there is no way to add more uniform array entries from the
  editor, it should be safe to do so until we can support uniform arrays fully."
  [upgraded-constant-value]
  (first upgraded-constant-value))

(defn- hack-upgrade-constant-value
  "HACK/FIXME: See above for the detailed background. We must convert the legacy
  `optional` value to a `repeated` value when writing the runtime binary format."
  [downgraded-constant-value]
  (if (some? downgraded-constant-value)
    [downgraded-constant-value]
    []))

(defn- hack-downgrade-constant [constant]
  (update constant :value hack-downgrade-constant-value))

(defn- hack-upgrade-constant [constant]
  (update constant :value hack-upgrade-constant-value))

(def ^:private hack-downgrade-constants (partial mapv hack-downgrade-constant))

(def ^:private hack-upgrade-constants (partial mapv hack-upgrade-constant))

(defn- attribute->editable-attribute [attribute]
  {:pre [(map? attribute)]} ; Graphics$VertexAttribute in map format.
  ;; The protobuf stores values in different arrays depending on their type.
  ;; Here, we convert all of them into a vector of doubles for easy editing.
  ;; This is fine, since doubles can accurately represent values in the entire
  ;; signed and unsigned 32-bit integer range from -2147483648 to 4294967295.
  (-> attribute
      (dissoc :double-values :long-values) ; Only one of these will be present, but we want neither.
      (assoc :values (graphics/attribute->doubles attribute))))

(defn- editable-attribute->attribute [{:keys [data-type normalize values] :as editable-attribute}]
  {:pre [(map? editable-attribute)
         (vector? values)]}
  (let [[attribute-value-keyword stored-values] (graphics/doubles->storage values data-type normalize)]
    (-> editable-attribute
        (dissoc :values)
        (assoc attribute-value-keyword {:v stored-values}))))

(defn- save-value-attributes [editable-attributes]
  (mapv editable-attribute->attribute editable-attributes))

(defn- build-target-attributes [attribute-infos]
  (mapv graphics/attribute-info->build-target-attribute attribute-infos))

(g/defnk produce-base-pb-msg [name vertex-program fragment-program vertex-constants fragment-constants samplers tags vertex-space max-page-count :as base-pb-msg]
  (-> base-pb-msg
      (update :vertex-program resource/resource->proj-path)
      (update :fragment-program resource/resource->proj-path)
      (update :vertex-constants hack-upgrade-constants)
      (update :fragment-constants hack-upgrade-constants)))

(g/defnk produce-save-value [base-pb-msg attributes]
  (assoc base-pb-msg
    :attributes (save-value-attributes attributes)))

(defn- build-material [resource build-resource->fused-build-resource user-data]
  (let [build-resource->fused-build-resource-path (comp resource/proj-path build-resource->fused-build-resource)
        material-desc-with-fused-build-resource-paths
        (-> (:material-desc-with-build-resources user-data)
            (update :vertex-program build-resource->fused-build-resource-path)
            (update :fragment-program build-resource->fused-build-resource-path))]
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

(g/defnk produce-build-targets [_node-id attribute-infos base-pb-msg fragment-program fragment-shader-source-info max-page-count resource vertex-program vertex-shader-source-info]
  (or (g/flatten-errors
        (prop-resource-error _node-id :vertex-program vertex-program "Vertex Program" "vp")
        (prop-resource-error _node-id :fragment-program fragment-program "Fragment Program" "fp")
        (mapcat #(attribute-info->error-values % _node-id :attributes) attribute-infos))
      (let [compile-spirv true
            vertex-shader-build-target (code.shader/make-shader-build-target vertex-shader-source-info compile-spirv max-page-count)
            fragment-shader-build-target (code.shader/make-shader-build-target fragment-shader-source-info compile-spirv max-page-count)
            build-target-samplers (build-target-samplers (:samplers base-pb-msg) max-page-count)
            build-target-attributes (build-target-attributes attribute-infos)
            dep-build-targets [vertex-shader-build-target fragment-shader-build-target]
            material-desc-with-build-resources (assoc base-pb-msg
                                                 :vertex-program (:resource vertex-shader-build-target)
                                                 :fragment-program (:resource fragment-shader-build-target)
                                                 :samplers build-target-samplers
                                                 :attributes build-target-attributes)]
        [(bt/with-content-hash
           {:node-id _node-id
            :resource (workspace/make-build-resource resource)
            :build-fn build-material
            :user-data {:material-desc-with-build-resources material-desc-with-build-resources}
            :deps dep-build-targets})])))

(defn- transpile-shader-source [shader-ext ^String shader-source ^long max-page-count]
  (let [shader-stage (code.shader/shader-stage-from-ext shader-ext)
        shader-language (code.shader/shader-language-to-java :language-glsl-sm120) ; use the old gles2 compatible shaders
        is-debug true
        result (ShaderProgramBuilder/buildGLSLVariantTextureArray shader-source shader-stage shader-language is-debug max-page-count)
        full-source (.source result)
        array-sampler-names-array (.arraySamplers result)]
    {:shader-source full-source
     :array-sampler-names (vec array-sampler-names-array)}))

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

(g/defnk produce-shader [_node-id vertex-shader-source-info vertex-program fragment-shader-source-info fragment-program vertex-constants fragment-constants samplers max-page-count]
  (or (prop-resource-error _node-id :vertex-program vertex-program "Vertex Program" "vp")
      (prop-resource-error _node-id :fragment-program fragment-program "Fragment Program" "fp")
      (let [augmented-vertex-shader-info (transpile-shader-source "vp" (:shader-source vertex-shader-source-info) max-page-count)
            augmented-fragment-shader-info (transpile-shader-source "fp" (:shader-source fragment-shader-source-info) max-page-count)
            array-sampler-name->slice-sampler-names
            (into {}
                  (comp (distinct)
                        (map (fn [array-sampler-name]
                               (pair array-sampler-name
                                     (mapv (fn [page-index]
                                             (str array-sampler-name "_" page-index))
                                           (range max-page-count))))))
                  (concat
                    (:array-sampler-names augmented-vertex-shader-info)
                    (:array-sampler-names augmented-fragment-shader-info)))

            uniforms (-> {}
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
        (shader/make-shader _node-id (:shader-source augmented-vertex-shader-info) (:shader-source augmented-fragment-shader-info) uniforms array-sampler-name->slice-sampler-names))))

(def ^:private form-data
  (let [constant-values (protobuf/enum-values Material$MaterialDesc$ConstantType)]
    {:navigation false
     :sections
     [{:title "Material"
       :fields
       [{:path [:name]
         :label "Name"
         :type :string
         :default "New Material"}
        {:path [:vertex-program]
         :label "Vertex Program"
         :type :resource :filter "vp"}
        {:path [:fragment-program]
         :label "Fragment Program"
         :type :resource :filter "fp"}
        {:path [:attributes]
         :label "Vertex Attributes"
         :type :table
         :columns (let [semantic-type-values (protobuf/enum-values Graphics$VertexAttribute$SemanticType)
                        data-type-values (protobuf/enum-values Graphics$VertexAttribute$DataType)
                        coordinate-space-values (protobuf/enum-values Graphics$CoordinateSpace)
                        shader-type-values (protobuf/enum-values Graphics$VertexAttribute$ShaderType)
                        step-function-values (protobuf/enum-values Graphics$VertexStepFunction)
                        default-semantic-type :semantic-type-none
                        default-shader-type :shader-type-vec3
                        default-values (graphics/resize-doubles (vector-of :double) default-semantic-type default-shader-type)]
                    [{:path [:name]
                      :label "Name"
                      :type :string}
                     {:path [:semantic-type]
                      :label "Semantic Type"
                      :type :choicebox
                      :options (protobuf-forms/make-options semantic-type-values)
                      :default default-semantic-type}
                     {:path [:data-type]
                      :label "Data Type"
                      :type :choicebox
                      :options (protobuf-forms/make-options data-type-values)
                      :default :type-float}
                     {:path [:shader-type]
                      :label "Shader Type"
                      :type :choicebox
                      :options (protobuf-forms/make-options shader-type-values)
                      :default default-shader-type}
                     {:path [:normalize]
                      :label "Normalize"
                      :type :boolean
                      :default false}
                     {:path [:coordinate-space]
                      :label "Coordinate Space"
                      :type :choicebox
                      :options (protobuf-forms/make-options coordinate-space-values)
                      :default :coordinate-space-local}
                     {:path [:step-function]
                      :label "Step Function"
                      :type :choicebox
                      :options (protobuf-forms/make-options step-function-values)
                      :default :vertex-step-function-vertex}
                     {:path [:values]
                      :label "Value"
                      :type :vec4
                      :default default-values}])}
        {:path [:vertex-constants]
         :label "Vertex Constants"
         :type :table
         :columns [{:path [:name] :label "Name" :type :string}
                   {:path [:type]
                    :label "Type"
                    :type :choicebox
                    :options (protobuf-forms/make-options constant-values)
                    :default (ffirst constant-values)}
                   {:path [:value] :label "Value" :type :vec4}]}
        {:path [:fragment-constants]
         :label "Fragment Constants"
         :type :table
         :columns [{:path [:name] :label "Name" :type :string}
                   {:path [:type]
                    :label "Type"
                    :type :choicebox
                    :options (protobuf-forms/make-options constant-values)
                    :default (ffirst constant-values)}
                   {:path [:value] :label "Value" :type :vec4}]}
        {:path [:samplers]
         :label "Samplers"
         :type :table
         :columns (let [wrap-options (protobuf/enum-values Material$MaterialDesc$WrapMode)
                        min-options (protobuf/enum-values Material$MaterialDesc$FilterModeMin)
                        mag-options (protobuf/enum-values Material$MaterialDesc$FilterModeMag)]
                    [{:path [:name] :label "Name" :type :string}
                     {:path [:wrap-u]
                      :label "Wrap U"
                      :type :choicebox
                      :options (protobuf-forms/make-options wrap-options)
                      :default (ffirst wrap-options)}
                     {:path [:wrap-v]
                      :label "Wrap V"
                      :type :choicebox
                      :options (protobuf-forms/make-options wrap-options)
                      :default (ffirst wrap-options)}
                     {:path [:filter-min]
                      :label "Filter Min"
                      :type :choicebox
                      :options (protobuf-forms/make-options min-options)
                      :default (ffirst min-options)}
                     {:path [:filter-mag]
                      :label "Filter Mag"
                      :type :choicebox
                      :options (protobuf-forms/make-options mag-options)
                      :default (ffirst mag-options)}
                     {:path [:max-anisotropy]
                      :label "Max Anisotropy"
                      :type :number}])}
        {:path [:tags]
         :label "Tags"
         :type :list
         :element {:type :string :default "New Tag"}}
        {:path [:vertex-space]
         :label "Vertex Space"
         :type :choicebox
         :options (protobuf-forms/make-options (protobuf/enum-values Material$MaterialDesc$VertexSpace))
         :default (ffirst (protobuf/enum-values Material$MaterialDesc$VertexSpace))}
        {:path [:max-page-count]
         :label "Max Atlas Pages"
         :type :integer
         :default 0}]}]}))

(defn- coerce-attribute [new-attribute old-attribute]
  ;; This assumes only a single property will change at a time, which is the
  ;; case when editing an attribute using the form view.
  (let [old-shader-type (:shader-type old-attribute)
        old-normalize (:normalize old-attribute)
        new-shader-type (:shader-type new-attribute)
        new-normalize (:normalize new-attribute)]
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
        (update new-attribute :values #(into (empty %) (map coerce-fn) %)))

      ;; If the shader type changes, resize the default value in the material.
      ;; This change will also cause attribute overrides stored elsewhere in the
      ;; project to be saved with the updated shader type.
      (not= old-shader-type new-shader-type)
      (let [semantic-type (:semantic-type new-attribute)]
        (update new-attribute :values #(graphics/resize-doubles % semantic-type new-shader-type)))

      ;; If something else changed, do not attempt value coercion.
      :else
      new-attribute)))

(defn- set-form-value-fn [property value user-data]
  (case property
    :attributes
    ;; When setting the attributes, coerce the existing values to conform to the
    ;; updated data and shader type. The attributes cannot be reordered
    ;; using the form view, so we can assume any existing attribute will be at
    ;; the same index as the updated attribute.
    (let [old-attributes (:attributes user-data)]
      (into []
            (map-indexed (fn [index new-attribute]
                           (if-some [old-attribute (get old-attributes index)]
                             (coerce-attribute new-attribute old-attribute)
                             new-attribute)))
            value))

    ;; Default case.
    value))

(defn- set-form-op [{:keys [node-id] :as user-data} [property] value]
  (let [processed-value (set-form-value-fn property value user-data)]
    (g/set-property! node-id property processed-value)))

(defn- clear-form-op [{:keys [node-id]} [property]]
  (g/clear-property! node-id property))

(g/defnk produce-form-data [_node-id name attributes vertex-program fragment-program vertex-constants fragment-constants max-page-count samplers tags vertex-space :as args]
  (let [values (select-keys args (mapcat :path (get-in form-data [:sections 0 :fields])))
        form-values (into {} (map (fn [[k v]] [[k] v]) values))]
    (-> form-data
        (assoc :values form-values)
        (assoc :form-ops {:user-data {:node-id _node-id :attributes attributes}
                          :set set-form-op
                          :clear clear-form-op}))))

(def ^:private wrap-mode->gl {:wrap-mode-repeat GL2/GL_REPEAT
                              :wrap-mode-mirrored-repeat GL2/GL_MIRRORED_REPEAT
                              :wrap-mode-clamp-to-edge GL2/GL_CLAMP_TO_EDGE})

(defn- filter-mode-min->gl [filter-min default-tex-params]
  (case filter-min
    :filter-mode-min-default (:min-filter default-tex-params)
    :filter-mode-min-nearest GL2/GL_NEAREST
    :filter-mode-min-linear GL2/GL_LINEAR
    :filter-mode-min-nearest-mipmap-nearest GL2/GL_NEAREST_MIPMAP_NEAREST
    :filter-mode-min-nearest-mipmap-linear GL2/GL_NEAREST_MIPMAP_LINEAR
    :filter-mode-min-linear-mipmap-nearest GL2/GL_LINEAR_MIPMAP_NEAREST
    :filter-mode-min-linear-mipmap-linear GL2/GL_LINEAR_MIPMAP_LINEAR
    nil))

(defn- filter-mode-mag->gl [filter-mag default-tex-params]
  (case filter-mag
    :filter-mode-mag-default (:mag-filter default-tex-params)
    :filter-mode-mag-nearest GL2/GL_NEAREST
    :filter-mode-mag-linear GL2/GL_LINEAR
    nil))

(def ^:private default-sampler {:wrap-u :wrap-mode-clamp-to-edge
                                :wrap-v :wrap-mode-clamp-to-edge
                                :filter-min :filter-mode-min-linear
                                :filter-mag :filter-mode-mag-linear
                                :max-anisotropy 1.0})

(defn sampler->tex-params
  ([sampler]
   (sampler->tex-params sampler nil))
  ([sampler default-tex-params]
   (let [s (or sampler default-sampler)
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
                name-key (graphics/attribute-name->key name)
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

  (property name g/Str (dynamic visible (g/constantly false)))

  (property vertex-program resource/Resource
    (dynamic visible (g/constantly false))
    (value (gu/passthrough vertex-resource))
    (set (fn [evaluation-context self old-value new-value]
           (project/resource-setter evaluation-context self old-value new-value
                                    [:resource :vertex-resource]
                                    [:shader-source-info :vertex-shader-source-info]))))

  (property fragment-program resource/Resource
    (dynamic visible (g/constantly false))
    (value (gu/passthrough fragment-resource))
    (set (fn [evaluation-context self old-value new-value]
           (project/resource-setter evaluation-context self old-value new-value
                                    [:resource :fragment-resource]
                                    [:shader-source-info :fragment-shader-source-info]))))

  (property max-page-count g/Int (default 1) (dynamic visible (g/constantly false)))
  (property attributes g/Any (dynamic visible (g/constantly false)))
  (property vertex-constants g/Any (dynamic visible (g/constantly false)))
  (property fragment-constants g/Any (dynamic visible (g/constantly false)))
  (property samplers g/Any
            (dynamic visible (g/constantly false))
            (set (fn [evaluation-context self old-value new-value]
                   (notify-sampler-names-targets-setter evaluation-context self :samplers old-value new-value))))
  (property tags g/Any (dynamic visible (g/constantly false)))
  (property vertex-space g/Keyword (dynamic visible (g/constantly false)))

  (output form-data g/Any :cached produce-form-data)

  (input vertex-resource resource/Resource)
  (input vertex-shader-source-info g/Any)
  (input fragment-resource resource/Resource)
  (input fragment-shader-source-info g/Any)

  (output base-pb-msg g/Any produce-base-pb-msg)

  (output save-value g/Any produce-save-value)
  (output build-targets g/Any :cached produce-build-targets)
  (output shader ShaderLifecycle :cached produce-shader)
  (output samplers [g/KeywordMap] (gu/passthrough samplers))
  (output attribute-infos [g/KeywordMap] :cached produce-attribute-infos))

(defn- make-sampler [name]
  (assoc default-sampler :name name))

(defn load-material [project self resource pb]
  (concat
    (g/set-property self :vertex-program (workspace/resolve-resource resource (:vertex-program pb)))
    (g/set-property self :fragment-program (workspace/resolve-resource resource (:fragment-program pb)))
    (g/set-property self :vertex-constants (hack-downgrade-constants (:vertex-constants pb)))
    (g/set-property self :fragment-constants (hack-downgrade-constants (:fragment-constants pb)))
    (g/set-property self :attributes (mapv attribute->editable-attribute (:attributes pb)))
    (for [field [:name :samplers :tags :vertex-space :max-page-count]]
      (g/set-property self field (field pb)))))

(defn- sanitize-sampler [sampler]
  ;; Material$MaterialDesc$Sampler in map format.
  ;; TODO: The texture field _will_ be used in the editor, just not in this MVP
  (dissoc sampler :name-indirections :texture :name-hash)) ; Only used in built data by the runtime.

(defn- sanitize-material
  "The old format specified :textures as string names. Convert these into
  :samplers if we encounter them. Ignores :textures that already have
  :samplers with the same name. Also ensures that there are no duplicate
  entries in the :samplers list, based on :name."
  [material-desc]
  ;; Material$MaterialDesc in map format.
  (let [existing-samplers (map sanitize-sampler (:samplers material-desc))
        samplers-created-from-textures (map make-sampler (:textures material-desc))
        samplers (into []
                       (util/distinct-by :name)
                       (concat existing-samplers
                               samplers-created-from-textures))
        attributes (mapv graphics/sanitize-attribute (:attributes material-desc))]
    (-> material-desc
        (assoc :samplers samplers)
        (assoc :attributes attributes)
        (dissoc :textures))))

(defn register-resource-types [workspace]
  (resource-node/register-ddf-resource-type workspace
    :ext "material"
    :label "Material"
    :node-type MaterialNode
    :ddf-type Material$MaterialDesc
    :load-fn load-material
    :sanitize-fn sanitize-material
    :icon "icons/32/Icons_31-Material.png"
    :view-types [:cljfx-form-view :text]))
