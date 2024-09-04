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

(ns editor.graphics
  (:require [editor.buffers :as buffers]
            [dynamo.graph :as g]
            [editor.geom :as geom]
            [editor.gl.shader :as shader]
            [editor.gl.vertex2 :as vtx]
            [editor.math :as math]
            [editor.properties :as properties]
            [editor.protobuf :as protobuf]
            [editor.types :as types]
            [editor.util :as eutil]
            [editor.validation :as validation]
            [internal.util :as iutil]
            [potemkin.namespaces :as namespaces]
            [util.coll :as coll :refer [pair]]
            [util.fn :as fn]
            [util.murmur :as murmur]
            [util.num :as num])
  (:import [com.dynamo.graphics.proto Graphics$CoordinateSpace Graphics$VertexAttribute Graphics$VertexAttribute$SemanticType]
           [com.google.protobuf ByteString]
           [com.jogamp.opengl GL2]
           [editor.gl.vertex2 VertexBuffer]
           [java.nio ByteBuffer]
           [javax.vecmath Matrix4d]))

(set! *warn-on-reflection* true)

(namespaces/import-vars [editor.gl.vertex2 attribute-name->key])

(def ^:private default-attribute-data-type (protobuf/default Graphics$VertexAttribute :data-type))

(def ^:private attribute-data-type-infos
  [{:data-type :type-byte
    :value-keyword :long-values
    :byte-size 1}
   {:data-type :type-unsigned-byte
    :value-keyword :long-values
    :byte-size 1}
   {:data-type :type-short
    :value-keyword :long-values
    :byte-size 2}
   {:data-type :type-unsigned-short
    :value-keyword :long-values
    :byte-size 2}
   {:data-type :type-int
    :value-keyword :long-values
    :byte-size 4}
   {:data-type :type-unsigned-int
    :value-keyword :long-values
    :byte-size 4}
   {:data-type :type-float
    :value-keyword :double-values
    :byte-size 4}])

(def attribute-data-type?
  (into #{}
        (map :data-type)
        attribute-data-type-infos))

(def coordinate-space? (protobuf/valid-enum-values Graphics$CoordinateSpace))

(def semantic-type? (protobuf/valid-enum-values Graphics$VertexAttribute$SemanticType))

(def ^:private attribute-data-type->attribute-value-keyword
  (fn/make-case-fn (map (juxt :data-type :value-keyword)
                        attribute-data-type-infos)))

(defn attribute-value-keyword [attribute-data-type normalize]
  {:pre [(attribute-data-type? attribute-data-type)]}
  (if normalize
    :double-values
    (attribute-data-type->attribute-value-keyword attribute-data-type)))

(def ^:private attribute-data-type->byte-size
  (fn/make-case-fn (map (juxt :data-type :byte-size)
                        attribute-data-type-infos)))

(defn attribute->doubles [{:keys [data-type] :as attribute}]
  {:pre [(attribute-data-type? data-type)]}
  (into (vector-of :double)
        (map unchecked-double)
        (if (or (:normalize attribute)
                (= :type-float data-type))
          (:v (:double-values attribute))
          (:v (:long-values attribute)))))

(defn attribute->any-doubles [attribute]
  (into (vector-of :double)
        (map unchecked-double)
        (or (not-empty (:v (:double-values attribute)))
            (not-empty (:v (:long-values attribute))))))

(defn- attribute->value-keyword [attribute]
  (if (not-empty (:v (:double-values attribute)))
    :double-values
    (if (not-empty (:v (:long-values attribute)))
      :long-values
      nil)))

(defn- doubles->stored-values [double-values attribute-value-keyword]
  (case attribute-value-keyword
    :double-values
    double-values

    :long-values
    (into (vector-of :long)
          (map unchecked-long)
          double-values)))

(defn doubles->storage [double-values attribute-data-type normalize]
  (let [attribute-value-keyword (attribute-value-keyword attribute-data-type normalize)
        stored-values (doubles->stored-values double-values attribute-value-keyword)]
    (pair attribute-value-keyword stored-values)))

(defn override-attributes->vertex-attribute-overrides [attributes]
  (into {}
        (map (fn [vertex-attribute]
               [(attribute-name->key (:name vertex-attribute))
                {:name (:name vertex-attribute)
                 :values (attribute->any-doubles vertex-attribute)
                 :value-keyword (attribute->value-keyword vertex-attribute)}]))
        attributes))

(defn doubles-outside-attribute-range-error-message [double-values attribute]
  (let [[^double min ^double max]
        (if (:normalize attribute)
          (case (:data-type attribute)
            (:type-float :type-byte :type-short :type-int) [-1.0 1.0]
            (:type-unsigned-byte :type-unsigned-short :type-unsigned-int) [0.0 1.0])
          (case (:data-type attribute)
            :type-float [num/float-min-double num/float-max-double]
            :type-byte [num/byte-min-double num/byte-max-double]
            :type-short [num/short-min-double num/short-max-double]
            :type-int [num/int-min-double num/int-max-double]
            :type-unsigned-byte [num/ubyte-min-double num/ubyte-max-double]
            :type-unsigned-short [num/ushort-min-double num/ushort-max-double]
            :type-unsigned-int [num/uint-min-double num/uint-max-double]))]
    (when (not-every? (fn [^double val]
                        (<= min val max))
                      double-values)
      (eutil/format* "'%s' attribute components must be between %.0f and %.0f"
                     (:name attribute)
                     min
                     max))))

(defn validate-doubles [double-values attribute node-id prop-kw]
  (validation/prop-error :fatal node-id prop-kw doubles-outside-attribute-range-error-message double-values attribute))

;; For positions, we want to have a 1.0 in the W coordinate so that they can be
;; transformed correctly by a 4D matrix. For colors, we default to opaque white.
(def ^:private default-attribute-element-values (vector-of :double 0.0 0.0 0.0 0.0))
(def ^:private default-attribute-element-values-mat4 (vector-of :double
                                                                0.0 0.0 0.0 0.0
                                                                0.0 0.0 0.0 0.0
                                                                0.0 0.0 0.0 0.0
                                                                0.0 0.0 0.0 0.0))
;; Transform values (i.e identity world, normal matrices)
(def ^:private default-attribute-transform-values-mat4 (vector-of :double
                                                                  1.0 0.0 0.0 0.0
                                                                  0.0 1.0 0.0 0.0
                                                                  0.0 0.0 1.0 0.0
                                                                  0.0 0.0 0.0 1.0))

;; Position semantic values
(def ^:private default-position-element-values (vector-of :double 0.0 0.0 0.0 1.0))
(def ^:private default-position-element-values-mat4 (vector-of :double
                                                               0.0 0.0 0.0 1.0
                                                               0.0 0.0 0.0 1.0
                                                               0.0 0.0 0.0 1.0
                                                               0.0 0.0 0.0 1.0))
;; Color semantic values
(def ^:private default-color-element-values (vector-of :double 1.0 1.0 1.0 1.0))
(def ^:private default-color-element-values-mat4 (vector-of :double
                                                            1.0 1.0 1.0 1.0
                                                            1.0 1.0 1.0 1.0
                                                            1.0 1.0 1.0 1.0
                                                            1.0 1.0 1.0 1.0))

(defn- vector-type-is-matrix? [attribute-vector-type]
  (case attribute-vector-type
    :vector-type-mat2 true
    :vector-type-mat3 true
    :vector-type-mat4 true
    false))

(defn vector-type->component-count [attribute-vector-type]
  (case attribute-vector-type
    :vector-type-scalar 1
    :vector-type-vec2 2
    :vector-type-vec3 3
    :vector-type-vec4 4
    :vector-type-mat2 4
    :vector-type-mat3 9
    :vector-type-mat4 16))

(defn- shader-uniform-type->vector-type [shader-uniform-type]
  (case shader-uniform-type
    :float :vector-type-scalar
    :float-vec2 :vector-type-vec2
    :float-vec3 :vector-type-vec3
    :float-vec4 :vector-type-vec4
    :float-mat2 :vector-type-mat2
    :float-mat3 :vector-type-mat3
    :float-mat4 :vector-type-mat4))

(defn resize-doubles [double-values semantic-type new-vector-type]
  {:pre [(vector? double-values)
         (or (nil? semantic-type) (keyword? semantic-type))
         (keyword? new-vector-type)]}
  (let [old-element-count (count double-values)
        new-element-count (vector-type->component-count new-vector-type)]
    (cond
      (< new-element-count old-element-count)
      (subvec double-values 0 new-element-count)

      (> new-element-count old-element-count)
      (let [default-element-values
            (if (vector-type-is-matrix? new-vector-type)
              (case semantic-type
                :semantic-type-position default-position-element-values-mat4
                :semantic-type-color default-color-element-values-mat4
                :semantic-type-world-matrix default-attribute-transform-values-mat4
                :semantic-type-normal-matrix default-attribute-transform-values-mat4
                default-attribute-element-values-mat4)

              (case semantic-type
                :semantic-type-position default-position-element-values
                :semantic-type-color default-color-element-values
                default-attribute-element-values))]
        (into double-values
              (subvec default-element-values old-element-count new-element-count)))

      :else
      double-values)))

(defn- default-attribute-doubles-raw [semantic-type vector-type]
  {:pre [(semantic-type? semantic-type)
         (keyword? vector-type)]}
  (resize-doubles (vector-of :double) semantic-type vector-type))

(def default-attribute-doubles (memoize default-attribute-doubles-raw))

(defn attribute-data-type->buffer-data-type [attribute-data-type]
  (case attribute-data-type
    :type-byte :byte
    :type-unsigned-byte :ubyte
    :type-short :short
    :type-unsigned-short :ushort
    :type-int :int
    :type-unsigned-int :uint
    :type-float :float))

(defn attribute-values+data-type->byte-size [attribute-values attribute-data-type]
  (* (count attribute-values) (attribute-data-type->byte-size attribute-data-type)))

(defn- make-attribute-bytes
  ^bytes [attribute-data-type normalize attribute-values]
  (let [attribute-value-byte-count (attribute-values+data-type->byte-size attribute-values attribute-data-type)
        attribute-bytes (byte-array attribute-value-byte-count)
        byte-buffer (vtx/wrap-buf attribute-bytes)
        buffer-data-type (attribute-data-type->buffer-data-type attribute-data-type)]
    (vtx/buf-push! byte-buffer buffer-data-type normalize attribute-values)
    attribute-bytes))

(defn- default-attribute-bytes-raw [semantic-type attribute-data-type vector-type normalize]
  (let [default-values (default-attribute-doubles semantic-type vector-type)]
    (make-attribute-bytes attribute-data-type normalize default-values)))

(def default-attribute-bytes (memoize default-attribute-bytes-raw))

(defn attribute->bytes+error-message
  ([attribute]
   (attribute->bytes+error-message attribute nil))
  ([{:keys [data-type vector-type normalize] :as attribute} override-values]
   {:pre [(map? attribute)
          (attribute-data-type? data-type)
          (keyword? vector-type)
          (contains? attribute :values)
          (or (nil? override-values) (sequential? override-values))]}
   (try
     (let [values (or override-values (:values attribute))
           bytes (make-attribute-bytes data-type normalize values)]
       [bytes nil])
     (catch IllegalArgumentException exception
       (let [{:keys [name semantic-type]} attribute
             default-bytes (default-attribute-bytes semantic-type data-type vector-type normalize)
             exception-message (ex-message exception)
             error-message (format "Vertex attribute '%s' - %s" name exception-message)]
         [default-bytes error-message])))))

(defn attribute-info->build-target-attribute [attribute-info]
  {:pre [(map? attribute-info)
         (string? (:name attribute-info))
         (keyword? (:name-key attribute-info))]}
  (let [^bytes attribute-bytes (:bytes attribute-info)]
    (-> attribute-info
        (dissoc :bytes :error :name-key :values)
        (assoc :name-hash (murmur/hash64 (:name attribute-info))
               :binary-values (ByteString/copyFrom attribute-bytes)))))

(defn- attribute-info->vtx-attribute [{:keys [coordinate-space data-type name name-key normalize semantic-type vector-type] :as attribute-info}]
  {:pre [(map? attribute-info)
         (coordinate-space? coordinate-space)
         (attribute-data-type? data-type)
         (keyword? vector-type)
         (string? name)
         (keyword? name-key)
         (or (nil? normalize) (boolean? normalize))
         (semantic-type? semantic-type)]}
  {:name name
   :name-key name-key
   :semantic-type semantic-type
   :coordinate-space coordinate-space
   :vector-type vector-type
   :type (attribute-data-type->buffer-data-type data-type)
   :components (vector-type->component-count vector-type)
   :normalize (true? normalize)})

(defn make-vertex-description [attribute-infos]
  (let [vtx-attributes (mapv attribute-info->vtx-attribute attribute-infos)]
    (vtx/make-vertex-description nil vtx-attributes)))

(defn coordinate-space-info
  "Returns a map of coordinate-space to sets of semantic-type that expect that
  coordinate-space. Only :semantic-type-position and :semantic-type-normal
  attributes are considered. We use this to determine if we need to include
  a :world-transform or :normal-transform in the renderable-datas we supply to
  the graphics/put-attributes! function. We can also use this information to
  figure out if we should cancel out the world transform from the render
  transforms we feed into shaders as uniforms in case the same shader is used to
  render both world-space and local-space mesh data.

  Example output:
  {:coordinate-space-local #{:semantic-type-position}
   :coordinate-space-world #{:semantic-type-position
                             :semantic-type-normal}}"
  [attribute-infos]
  (->> attribute-infos
       (filter (comp #{:semantic-type-position :semantic-type-normal} :semantic-type))
       (iutil/group-into
         {} #{}
         #(:coordinate-space % :coordinate-space-local)
         :semantic-type)))

;; This function can return nil if element-count and vector-type is nil,
;; which happens for default values. This is only used for sanitation,
;; so we shouldn't return the default value here.
(defn- attribute-info->vector-type [{:keys [element-count semantic-type vector-type] :as attribute-info}]
  (let [is-valid-vector-type (some? vector-type)
        is-valid-element-count (pos-int? element-count)]
    (cond
      is-valid-vector-type vector-type
      is-valid-element-count (vtx/element-count+semantic-type->vector-type element-count semantic-type))))

;; TODO(save-value-cleanup): We only really need to sanitize the attributes if a resource type has :read-defaults true.
(defn sanitize-attribute-value-v [attribute-value]
  (protobuf/sanitize-repeated attribute-value :v))

(defn sanitize-attribute-value-field [attribute attribute-value-keyword]
  (protobuf/sanitize attribute attribute-value-keyword sanitize-attribute-value-v))

(defn sanitize-attribute-definition [{:keys [data-type normalize] :as attribute}]
  ;; Graphics$VertexAttribute in map format.
  (let [attribute-value-keyword (attribute-value-keyword (or data-type default-attribute-data-type) normalize)
        attribute-values (:v (get attribute attribute-value-keyword))
        attribute-vector-type (attribute-info->vector-type attribute)]
    ;; TODO:
    ;; Currently the protobuf read function returns empty instances of every
    ;; OneOf variant. Strip out the empty ones.
    ;; Once we update the protobuf loader, we shouldn't need to do this here.
    ;; We still want to remove the default empty :name-hash string, though.
    (-> (if (and (some? attribute-vector-type) (not (= attribute-vector-type :vector-type-vec4)))
          (assoc attribute :vector-type attribute-vector-type)
          attribute)
        (dissoc :name-hash :element-count :double-values :long-values :binary-values)
        (assoc attribute-value-keyword {:v attribute-values}))))

(defn sanitize-attribute-override [attribute]
  ;; Graphics$VertexAttribute in map format.
  (-> attribute
      (dissoc :binary-values :coordinate-space :data-type :element-count :name-hash :normalize :semantic-type :vector-type)
      (sanitize-attribute-value-field :double-values)
      (sanitize-attribute-value-field :long-values)))

(def attribute-key->default-attribute-info
  (fn/make-case-fn
    (into {}
          (map (fn [{:keys [data-type name normalize semantic-type vector-type] :as attribute}]
                 (let [attribute-key (attribute-name->key name)
                       values (default-attribute-doubles semantic-type vector-type)
                       bytes (default-attribute-bytes semantic-type data-type vector-type normalize)
                       attribute-info (assoc attribute
                                        :name-key attribute-key
                                        :values values
                                        :bytes bytes)]
                   [attribute-key attribute-info])))
          [{:name "position"
            :semantic-type :semantic-type-position
            :coordinate-space :default ; Assigned default-coordinate-space parameter.
            :data-type :type-float
            :step-function :vertex-step-function-vertex
            :vector-type :vector-type-vec4}
           {:name "color"
            :semantic-type :semantic-type-color
            :coordinate-space :coordinate-space-local
            :data-type :type-float
            :step-function :vertex-step-function-vertex
            :vector-type :vector-type-vec4}
           {:name "texcoord0"
            :semantic-type :semantic-type-texcoord
            :coordinate-space :coordinate-space-local
            :data-type :type-float
            :step-function :vertex-step-function-vertex
            :vector-type :vector-type-vec2}
           {:name "page_index"
            :semantic-type :semantic-type-page-index
            :coordinate-space :coordinate-space-local
            :data-type :type-float
            :step-function :vertex-step-function-vertex
            :vector-type :vector-type-scalar}
           {:name "normal"
            :semantic-type :semantic-type-normal
            :coordinate-space :default ; Assigned default-coordinate-space parameter.
            :data-type :type-float
            :step-function :vertex-step-function-vertex
            :vector-type :vector-type-vec3}
           {:name "tangent"
            :semantic-type :semantic-type-tangent
            :coordinate-space :default ; Assigned default-coordinate-space parameter.
            :data-type :type-float
            :step-function :vertex-step-function-vertex
            :vector-type :vector-type-vec4}
           {:name "mtx_world"
            :semantic-type :semantic-type-world-matrix
            :coordinate-space :coordinate-space-world
            :data-type :type-float
            :step-function :vertex-step-function-instance
            :vector-type :vector-type-mat4}
           {:name "mtx_normal"
            :semantic-type :semantic-type-normal-matrix
            :coordinate-space :coordinate-space-world
            :data-type :type-float
            :step-function :vertex-step-function-instance
            :vector-type :vector-type-mat4}])))


(defn- compatible-vector-type [vector-type-value vector-type-container]
  (let [vector-type-value-comp-count (vector-type->component-count vector-type-value)
        vector-type-container-comp-count (vector-type->component-count vector-type-container)]
    (if (<= vector-type-value-comp-count vector-type-container-comp-count)
      vector-type-value
      vector-type-container)))

(defn shader-bound-attributes [^GL2 gl shader material-attribute-infos manufactured-attribute-keys default-coordinate-space]
  {:pre [(coordinate-space? default-coordinate-space)]}
  (let [shader-attribute-infos (shader/attribute-infos shader gl)
        shader-bound-attribute? (comp boolean shader-attribute-infos :name)
        declared-material-attribute-key? (into #{} (map :name-key) material-attribute-infos)

        manufactured-attribute-infos
        (eduction
          (remove declared-material-attribute-key?)
          (map attribute-key->default-attribute-info)
          (map (fn [attribute-info]
                 (cond-> attribute-info
                         (= :default (:coordinate-space attribute-info))
                         (assoc :coordinate-space default-coordinate-space))))
          manufactured-attribute-keys)

        ensure-correct-vector-type-fn (fn [attribute-info]
                                        (let [shader-attribute-info (get shader-attribute-infos (:name attribute-info))
                                              shader-attribute-vector-type (shader-uniform-type->vector-type (:type shader-attribute-info))
                                              compatible-vector-type (compatible-vector-type (:vector-type attribute-info) shader-attribute-vector-type)]
                                          (assoc attribute-info :vector-type compatible-vector-type)))

        filtered-attribute-infos (filterv shader-bound-attribute?
                                          (concat manufactured-attribute-infos
                                                  material-attribute-infos))]
    (mapv ensure-correct-vector-type-fn filtered-attribute-infos)))

(defn vertex-attribute-overrides->save-values [vertex-attribute-overrides material-attribute-infos]
  (let [declared-material-attribute-key? (into #{} (map :name-key) material-attribute-infos)
        material-attribute-save-values
        (into []
              (keep (fn [{:keys [data-type name name-key normalize semantic-type vector-type]}]
                      (when-some [override-values (some-> vertex-attribute-overrides (get name-key) :values coll/not-empty)]
                        ;; Ensure our saved values have the expected element-count.
                        ;; If the material has been edited, this might have changed,
                        ;; but specialized widgets like the one we use to edit color
                        ;; properties may also produce a different element count from
                        ;; what the material dictates.
                        (let [resized-values (resize-doubles override-values semantic-type vector-type)
                              [attribute-value-keyword stored-values] (doubles->storage resized-values data-type normalize)]
                          (protobuf/make-map-without-defaults Graphics$VertexAttribute
                            :name name
                            attribute-value-keyword {:v stored-values})))))
              material-attribute-infos)
        orphaned-attribute-save-values
        (into []
              (keep (fn [[name-key attribute-info]]
                      (when-not (contains? declared-material-attribute-key? name-key)
                        (let [attribute-name (:name attribute-info)
                              attribute-value-keyword (:value-keyword attribute-info)
                              attribute-values (:values attribute-info)]
                          (protobuf/make-map-without-defaults Graphics$VertexAttribute
                            :name attribute-name
                            attribute-value-keyword (when (coll/not-empty attribute-values)
                                                      {:v attribute-values})))))
                    vertex-attribute-overrides))]
    (concat material-attribute-save-values orphaned-attribute-save-values)))

(defn vertex-attribute-overrides->build-target [vertex-attribute-overrides vertex-attribute-bytes material-attribute-infos]
  (into []
        (keep (fn [{:keys [name-key] :as attribute-info}]
                ;; The values in vertex-attribute-overrides are ignored - we
                ;; only use it to check if we have an override value. The output
                ;; bytes are taken from the vertex-attribute-bytes map. They
                ;; have already been coerced to the expected size.
                (when (contains? vertex-attribute-overrides name-key)
                  (let [attribute-bytes (get vertex-attribute-bytes name-key)]
                    (attribute-info->build-target-attribute
                      (assoc attribute-info :bytes attribute-bytes))))))
        material-attribute-infos))

(defn- editable-attribute-info? [attribute-info]
  (case (:semantic-type attribute-info)
    (:semantic-type-position :semantic-type-texcoord :semantic-type-page-index :semantic-type-normal :semantic-type-world-matrix :semantic-type-normal-matrix) false
    nil false
    true))

(defn contains-semantic-type? [attributes semantic-type]
  (true? (some #(= semantic-type (:semantic-type %)) attributes)))

(defn- attribute-property-type [attribute]
  (case (:semantic-type attribute)
    :semantic-type-color types/Color
    (case (:vector-type attribute)
      :vector-type-scalar g/Num
      :vector-type-vec2 types/Vec2
      :vector-type-vec3 types/Vec3
      :vector-type-vec4 types/Vec4
      :vector-type-mat2 types/Vec4
      :vector-type-mat3 types/Vec4
      :vector-type-mat4 types/Vec4)))

(defn- attribute-expected-vector-type [{:keys [semantic-type vector-type] :as _attribute}]
  {:pre [(keyword? vector-type)]}
  (case semantic-type
    :semantic-type-color :vector-type-vec4
    vector-type))

(defn- attribute-update-property [current-property-value attribute new-value]
  (let [name-key (:name-key attribute)
        override-info (name-key current-property-value)
        attribute-value-keyword (attribute-value-keyword (:data-type attribute) (:normalize attribute))
        override-value-keyword (or attribute-value-keyword (:value-keyword override-info))
        override-name (or (:name attribute) (:name override-info))]
    (assoc current-property-value name-key {:values new-value
                                            :value-keyword override-value-keyword
                                            :name override-name})))

(defn- attribute-clear-property [current-property-value {:keys [name-key] :as _attribute}]
  {:pre [(keyword? name-key)]}
  (dissoc current-property-value name-key))

(defn- attribute-key->property-key-raw [attribute-key]
  (keyword (str "attribute_" (name attribute-key))))

(def attribute-key->property-key (memoize attribute-key->property-key-raw))

(defn- attribute-edit-type [{:keys [semantic-type vector-type] :as attribute} property-type]
  {:pre [(keyword? vector-type)
         (semantic-type? semantic-type)]}
  (let [attribute-update-fn (fn [_evaluation-context self _old-value new-value]
                              (let [values (if (= g/Num property-type)
                                             (vector-of :double new-value)
                                             new-value)]
                                (g/update-property self :vertex-attribute-overrides attribute-update-property attribute values)))
        attribute-clear-fn (fn [self _property-label]
                             (g/update-property self :vertex-attribute-overrides attribute-clear-property attribute))]
    (cond-> {:type property-type
             :set-fn attribute-update-fn
             :clear-fn attribute-clear-fn}

            (= semantic-type :semantic-type-color)
            (assoc :ignore-alpha? (not= :vector-type-vec4 vector-type)))))

(defn- attribute-value [attribute-values property-type semantic-type vector-type]
  {:pre [(semantic-type? semantic-type)
         (keyword? vector-type)]}
  (if (= g/Num property-type)
    (first attribute-values) ; The widget expects a number, not a vector.
    (resize-doubles attribute-values semantic-type vector-type)))

(defn attribute-properties-by-property-key [_node-id material-attribute-infos vertex-attribute-overrides]
  (let [name-keys (into #{} (map :name-key) material-attribute-infos)]
    (concat
      (keep (fn [attribute-info]
              (when (editable-attribute-info? attribute-info)
                (let [attribute-key (:name-key attribute-info)
                      semantic-type (:semantic-type attribute-info)
                      material-values (:values attribute-info)
                      override-values (:values (vertex-attribute-overrides attribute-key))
                      attribute-values (or override-values material-values)
                      property-type (attribute-property-type attribute-info)
                      expected-vector-type (attribute-expected-vector-type attribute-info)
                      edit-type (attribute-edit-type attribute-info property-type)
                      property-key (attribute-key->property-key attribute-key)
                      label (properties/keyword->name attribute-key)
                      value (attribute-value attribute-values property-type semantic-type expected-vector-type)
                      error (when (some? override-values)
                              (validate-doubles override-values attribute-info _node-id property-key))
                      prop {:node-id _node-id
                            :type property-type
                            :edit-type edit-type
                            :label label
                            :value value
                            :error error}]
                  ;; Insert the original material values as original-value if there is a vertex override.
                  (if (some? override-values)
                    [property-key (assoc prop :original-value material-values)]
                    [property-key prop]))))
            material-attribute-infos)
      (for [[name-key vertex-override-info] vertex-attribute-overrides
            :when (not (name-keys name-key))
            :let [values (:values vertex-override-info)
                  vector-type (if (number? values)
                                :vector-type-scalar
                                (vtx/element-count+semantic-type->vector-type (count values) :semantic-type-none))
                  assumed-attribute-info {:vector-type vector-type
                                          :name-key name-key
                                          :semantic-type :semantic-type-none}
                  property-type (attribute-property-type assumed-attribute-info)]]
        [(attribute-key->property-key name-key)
         {:node-id _node-id
          :value (attribute-value values property-type :semantic-type-none vector-type)
          :label (properties/keyword->name name-key)
          :type property-type
          :edit-type (attribute-edit-type assumed-attribute-info property-type)
          :original-value []}]))))

(defn attribute-bytes-by-attribute-key [_node-id material-attribute-infos vertex-attribute-overrides]
  (let [vertex-attribute-bytes
        (into {}
              (map (fn [{:keys [name-key] :as attribute-info}]
                     {:pre [(keyword? name-key)]}
                     (let [override-info (get vertex-attribute-overrides name-key)
                           override-values (:values override-info)
                           [bytes error] (if (nil? override-values)
                                           [(:bytes attribute-info) (:error attribute-info)]
                                           (let [{:keys [semantic-type vector-type]} attribute-info
                                                 resized-values (resize-doubles override-values semantic-type vector-type)
                                                 [bytes error-message :as bytes+error-message] (attribute->bytes+error-message attribute-info resized-values)]
                                             (if (nil? error-message)
                                               bytes+error-message
                                               (let [property-key (attribute-key->property-key name-key)
                                                     error (g/->error _node-id property-key :fatal override-values error-message)]
                                                 [bytes error]))))]
                       [name-key (or error bytes)])))
              material-attribute-infos)]
    (g/precluding-errors (vals vertex-attribute-bytes)
      vertex-attribute-bytes)))

(defn- decorate-attribute-exception [exception attribute vertex]
  (ex-info "Failed to encode vertex attribute."
           (-> attribute
               (select-keys [:name :semantic-type :type :components :normalize :coordinate-space])
               (assoc :vertex vertex)
               (assoc :vertex-elements (count vertex)))
           exception))

(defn- renderable-data->world-position-v3 [renderable-data]
  (let [local-positions (:position-data renderable-data)
        world-transform (:world-transform renderable-data)]
    (geom/transf-p world-transform local-positions)))

(defn- renderable-data->world-position-v4 [renderable-data]
  (let [local-positions (:position-data renderable-data)
        world-transform (:world-transform renderable-data)]
    (geom/transf-p4 world-transform local-positions)))

(defn- renderable-data->world-direction-v3 [renderable-data->local-directions renderable-data]
  (let [local-directions (renderable-data->local-directions renderable-data)
        normal-transform (:normal-transform renderable-data)]
    (geom/transf-n normal-transform local-directions)))

(defn- renderable-data->world-direction-v4 [renderable-data->local-directions renderable-data]
  (let [local-directions (renderable-data->local-directions renderable-data)
        normal-transform (:normal-transform renderable-data)]
    (geom/transf-n4 normal-transform local-directions)))

(defn- renderable-data->world-tangent-v4 [renderable-data]
  (let [tangents (:tangent-data renderable-data)
        normal-transform (:normal-transform renderable-data)]
    (geom/transf-tangents normal-transform tangents)))

(def ^:private renderable-data->world-normal-v3 (partial renderable-data->world-direction-v3 :normal-data))
(def ^:private renderable-data->world-normal-v4 (partial renderable-data->world-direction-v4 :normal-data))
(def ^:private renderable-data->world-tangent-v3 (partial renderable-data->world-direction-v3 :tangent-data))

(defn- matrix4+attribute->flat-array [^Matrix4d matrix attribute]
  (let [vector-component-count (vector-type->component-count (:vector-type attribute))
        matrix-flat-array (math/vecmath->clj (doto (Matrix4d. matrix) (.transpose)))
        matrix-row-column-count (vtx/vertex-attribute->row-column-count attribute)]
    (vec (flatten (if matrix-row-column-count
                    (map #(take matrix-row-column-count %)
                         (take matrix-row-column-count (partition 4 matrix-flat-array)))
                    (take vector-component-count matrix-flat-array))))))

(defn put-attributes! [^VertexBuffer vbuf renderable-datas]
  (let [vertex-description (.vertex-description vbuf)
        vertex-byte-stride (:size vertex-description)
        ^ByteBuffer buf (.buf vbuf)

        put-bytes!
        (fn put-bytes!
          ^long [^long vertex-byte-offset vertices]
          (reduce (fn [^long vertex-byte-offset attribute-bytes]
                    (vtx/buf-blit! buf vertex-byte-offset attribute-bytes)
                    (+ vertex-byte-offset vertex-byte-stride))
                  vertex-byte-offset
                  vertices))

        put-doubles!
        (fn put-doubles!
          [vertex-byte-offset semantic-type buffer-data-type vector-type normalize vertices]
          (reduce (fn [^long vertex-byte-offset attribute-doubles]
                    (let [attribute-doubles (resize-doubles attribute-doubles semantic-type vector-type)]
                      (vtx/buf-put! buf vertex-byte-offset buffer-data-type normalize attribute-doubles))
                    (+ vertex-byte-offset vertex-byte-stride))
                  (long vertex-byte-offset)
                  vertices))

        put-renderables!
        (fn put-renderables!
          ^long [^long attribute-byte-offset renderable-data->vertices put-vertices!]
          (reduce (fn [^long vertex-byte-offset renderable-data]
                    (let [vertices (renderable-data->vertices renderable-data)]
                      (put-vertices! vertex-byte-offset vertices)))
                  attribute-byte-offset
                  renderable-datas))

        mesh-data-exists?
        (let [renderable-data (first renderable-datas)
              texcoord-datas (:texcoord-datas renderable-data)]
          (if (nil? renderable-data)
            (constantly false)
            (fn mesh-data-exists? [semantic-type ^long channel]
              (case semantic-type
                :semantic-type-position
                (some? (:position-data renderable-data))

                :semantic-type-texcoord
                (some? (get-in texcoord-datas [channel :uv-data]))

                :semantic-type-page-index
                (some? (get-in texcoord-datas [channel :page-index]))

                :semantic-type-color
                (and (zero? channel)
                     (some? (:color-data renderable-data)))

                :semantic-type-normal
                (some? (:normal-data renderable-data))

                :semantic-type-tangent
                (and (zero? channel)
                     (some? (:tangent-data renderable-data)))

                :semantic-type-world-matrix
                (some? (:has-semantic-type-world-matrix renderable-data))

                :semantic-type-normal-matrix
                (some? (:has-semantic-type-normal-matrix renderable-data))

                false))))]

    (reduce (fn [reduce-info attribute]
              (let [semantic-type (:semantic-type attribute)
                    buffer-data-type (:type attribute)
                    element-count (long (:components attribute))
                    vector-type (:vector-type attribute)
                    normalize (:normalize attribute)
                    name-key (:name-key attribute)
                    ^long attribute-byte-offset (:attribute-byte-offset reduce-info)
                    channel (get reduce-info semantic-type)

                    put-attribute-bytes!
                    (fn put-attribute-bytes!
                      ^long [^long vertex-byte-offset vertices]
                      (try
                        (put-bytes! vertex-byte-offset vertices)
                        (catch Exception e
                          (throw (decorate-attribute-exception e attribute (first vertices))))))

                    put-attribute-doubles!
                    (fn put-attribute-doubles!
                      ^long [^long vertex-byte-offset vertices]
                      (try
                        (put-doubles! vertex-byte-offset semantic-type buffer-data-type vector-type normalize vertices)
                        (catch Exception e
                          (throw (decorate-attribute-exception e attribute (first vertices))))))]

                (if (mesh-data-exists? semantic-type channel)

                  ;; Mesh data exists for this attribute. It takes precedence
                  ;; over any attribute values specified on the material or
                  ;; overrides.
                  (case semantic-type
                    :semantic-type-position
                    (case (:coordinate-space attribute)
                      :coordinate-space-world
                      (let [renderable-data->world-position
                            (case element-count
                              3 renderable-data->world-position-v3
                              4 renderable-data->world-position-v4)]
                        (put-renderables! attribute-byte-offset renderable-data->world-position put-attribute-doubles!))
                      (put-renderables! attribute-byte-offset :position-data put-attribute-doubles!))

                    :semantic-type-texcoord
                    (put-renderables! attribute-byte-offset #(get-in % [:texcoord-datas channel :uv-data]) put-attribute-doubles!)

                    :semantic-type-page-index
                    (put-renderables! attribute-byte-offset
                                      (fn [renderable-data]
                                        (let [vertex-count (count (:position-data renderable-data))
                                              page-index (get-in renderable-data [:texcoord-datas channel :page-index])]
                                          (repeat vertex-count [(double page-index)])))
                                      put-attribute-doubles!)

                    :semantic-type-color
                    (put-renderables! attribute-byte-offset :color-data put-attribute-doubles!)

                    :semantic-type-normal
                    (case (:coordinate-space attribute)
                      :coordinate-space-world
                      (let [renderable-data->world-normal
                            (case element-count
                              3 renderable-data->world-normal-v3
                              4 renderable-data->world-normal-v4)]
                        (put-renderables! attribute-byte-offset renderable-data->world-normal put-attribute-doubles!))
                      (put-renderables! attribute-byte-offset :normal-data put-attribute-doubles!))

                    :semantic-type-tangent
                    (case (:coordinate-space attribute)
                      :coordinate-space-world
                      (let [renderable-data->world-tangent
                            (case element-count
                              3 renderable-data->world-tangent-v3
                              4 renderable-data->world-tangent-v4)]
                        (put-renderables! attribute-byte-offset renderable-data->world-tangent put-attribute-doubles!))
                      (put-renderables! attribute-byte-offset :tangent-data put-attribute-doubles!))

                    :semantic-type-world-matrix
                    (put-renderables! attribute-byte-offset
                                      (fn [renderable-data]
                                        (let [vertex-count (count (:position-data renderable-data))
                                              matrix-values (matrix4+attribute->flat-array (:world-transform renderable-data) attribute)]
                                          (repeat vertex-count matrix-values)))
                                      put-attribute-doubles!)

                    :semantic-type-normal-matrix
                    (put-renderables! attribute-byte-offset
                                      (fn [renderable-data]
                                        (let [vertex-count (count (:position-data renderable-data))
                                              matrix-values (matrix4+attribute->flat-array (:normal-transform renderable-data) attribute)]
                                          (repeat vertex-count matrix-values)))
                                      put-attribute-doubles!))

                  ;; Mesh data doesn't exist. Use the attribute data from the
                  ;; material or overrides.
                  (put-renderables! attribute-byte-offset
                                    (fn [renderable-data]
                                      (let [vertex-count (count (:position-data renderable-data))
                                            attribute-bytes (get (:vertex-attribute-bytes renderable-data) name-key)
                                            attribute-byte-count-max (* element-count (buffers/type-size buffer-data-type))]
                                        ;; Clamp the buffer if the container format is smaller than specified in the material
                                        (if (> (count attribute-bytes) attribute-byte-count-max)
                                          (let [attribute-bytes-clamped (byte-array attribute-byte-count-max)]
                                            (System/arraycopy attribute-bytes 0 attribute-bytes-clamped 0 attribute-byte-count-max)
                                            (repeat vertex-count attribute-bytes-clamped))
                                          (repeat vertex-count attribute-bytes))))
                                    put-attribute-bytes!))
                (-> reduce-info
                    (update semantic-type inc)
                    (assoc :attribute-byte-offset (+ attribute-byte-offset
                                                     (vtx/attribute-size attribute))))))
            ;; The reduce-info is a map of how many times we've encountered an
            ;; attribute of a particular semantic-type. We also include a
            ;; special key, :attribute-byte-offset, to keep track of where the
            ;; next encountered attribute will start writing its data to the
            ;; vertex buffer.
            (into {:attribute-byte-offset 0}
                  (map (fn [[semantic-type]]
                         (pair semantic-type 0)))
                  (protobuf/enum-values Graphics$VertexAttribute$SemanticType))
            (:attributes vertex-description))
    (.position buf (.limit buf))
    (vtx/flip! vbuf)))
