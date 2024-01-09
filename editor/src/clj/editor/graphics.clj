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
  (:require [dynamo.graph :as g]
            [editor.gl.vertex2 :as vtx]
            [editor.gl.shader :as shader]
            [editor.properties :as properties]
            [editor.types :as types]
            [editor.util :as util]
            [editor.validation :as validation]
            [potemkin.namespaces :as namespaces]
            [util.coll :as coll :refer [pair]]
            [util.murmur :as murmur]
            [util.num :as num])
  (:import [com.jogamp.opengl GL2]
           [com.google.protobuf ByteString]))

(set! *warn-on-reflection* true)

(namespaces/import-vars [editor.gl.vertex2 attribute-name->key])

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

(def ^:private attribute-data-type->attribute-value-keyword
  (into {}
        (map (juxt :data-type :value-keyword))
        attribute-data-type-infos))

(defn attribute-value-keyword [attribute-data-type normalize]
  (if normalize
    :double-values
    (attribute-data-type->attribute-value-keyword attribute-data-type)))

(def ^:private attribute-data-type->byte-size
  (into {}
        (map (juxt :data-type :byte-size))
        attribute-data-type-infos))

(defn attribute->doubles [attribute]
  (into (vector-of :double)
        (map unchecked-double)
        (if (or (:normalize attribute)
                (= :type-float (:data-type attribute)))
          (:v (:double-values attribute))
          (:v (:long-values attribute)))))

(defn attribute->any-doubles [attribute]
  (into (vector-of :double)
        (map unchecked-double)
        (or (not-empty (:v (:double-values attribute)))
            (not-empty (:v (:long-values attribute))))))

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
      (util/format* "'%s' attribute components must be between %.0f and %.0f"
                    (:name attribute)
                    min
                    max))))

(defn validate-doubles [double-values attribute node-id prop-kw]
  (validation/prop-error :fatal node-id prop-kw doubles-outside-attribute-range-error-message double-values attribute))

(defn resize-doubles [double-values semantic-type element-count]
  (let [fill-value (case semantic-type
                     :semantic-type-color 1.0 ; Default to opaque white for color attributes.
                     0.0)]
    (coll/resize double-values element-count fill-value)))

(defn- default-attribute-doubles-raw [semantic-type element-count]
  (resize-doubles (vector-of :double) semantic-type element-count))

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

(defn- default-attribute-bytes-raw [semantic-type data-type element-count normalize]
  (let [default-values (default-attribute-doubles semantic-type element-count)]
    (make-attribute-bytes data-type normalize default-values)))

(def default-attribute-bytes (memoize default-attribute-bytes-raw))

(defn attribute->bytes+error-message
  ([attribute]
   (attribute->bytes+error-message attribute nil))
  ([{:keys [data-type normalize] :as attribute} override-values]
   {:pre [(map? attribute)
          (contains? attribute :values)
          (or (nil? override-values) (sequential? override-values))]}
   (try
     (let [values (or override-values (:values attribute))
           bytes (make-attribute-bytes data-type normalize values)]
       [bytes nil])
     (catch IllegalArgumentException exception
       (let [{:keys [element-count name semantic-type]} attribute
             default-bytes (default-attribute-bytes semantic-type data-type element-count normalize)
             exception-message (ex-message exception)
             error-message (format "Vertex attribute '%s' - %s" name exception-message)]
         [default-bytes error-message])))))

(defn attribute-info->build-target-attribute [attribute-info]
  {:pre [(map? attribute-info)
         (keyword? (:name-key attribute-info))]}
  (let [^bytes attribute-bytes (:bytes attribute-info)]
    (-> attribute-info
        (dissoc :bytes :error :name-key :values)
        (assoc :name-hash (murmur/hash64 (:name attribute-info))
               :binary-values (ByteString/copyFrom attribute-bytes)))))

(defn- attribute-info->vtx-attribute [attribute-info]
  {:pre [(map? attribute-info)
         (keyword? (:name-key attribute-info))]}
  {:name (:name attribute-info)
   :name-key (:name-key attribute-info)
   :semantic-type (:semantic-type attribute-info)
   :coordinate-space (:coordinate-space attribute-info)
   :type (attribute-data-type->buffer-data-type (:data-type attribute-info))
   :components (:element-count attribute-info)
   :normalize (:normalize attribute-info false)})

(defn make-vertex-description [attribute-infos]
  (let [vtx-attributes (mapv attribute-info->vtx-attribute attribute-infos)]
    (vtx/make-vertex-description nil vtx-attributes)))

(defn sanitize-attribute [{:keys [data-type normalize] :as attribute}]
  ;; Graphics$VertexAttribute in map format.
  (let [attribute-value-keyword (attribute-value-keyword data-type normalize)
        attribute-values (:v (get attribute attribute-value-keyword))]
    ;; TODO:
    ;; Currently the protobuf read function returns empty instances of every
    ;; OneOf variant. Strip out the empty ones.
    ;; Once we update the protobuf loader, we shouldn't need to do this here.
    ;; We still want to remove the default empty :name-hash string, though.
    (-> attribute
        (dissoc :name-hash :double-values :long-values :binary-values)
        (assoc attribute-value-keyword {:v attribute-values}))))

(def attribute-key->default-attribute-info
  (into {}
        (map (fn [{:keys [data-type element-count name normalize semantic-type] :as attribute}]
               (let [attribute-key (attribute-name->key name)
                     values (default-attribute-doubles semantic-type element-count)
                     bytes (default-attribute-bytes semantic-type data-type element-count normalize)
                     attribute-info (assoc attribute
                                      :name-key attribute-key
                                      :values values
                                      :bytes bytes)]
                 [attribute-key attribute-info])))
        [{:name "position"
          :semantic-type :semantic-type-position
          :coordinate-space :coordinate-space-world
          :data-type :type-float
          :element-count 4}
         {:name "color"
          :semantic-type :semantic-type-color
          :data-type :type-float
          :element-count 4}
         {:name "texcoord0"
          :semantic-type :semantic-type-texcoord
          :data-type :type-float
          :element-count 2}
         {:name "page_index"
          :semantic-type :semantic-type-page-index
          :data-type :type-float
          :element-count 1}]))

(defn shader-bound-attributes [^GL2 gl shader material-attribute-infos manufactured-stream-keys]
  (let [shader-bound-attribute? (comp boolean (shader/attribute-infos shader gl) :name)
        declared-material-attribute-key? (into #{} (map :name-key) material-attribute-infos)
        manufactured-attribute-infos (into []
                                           (comp (remove declared-material-attribute-key?)
                                                 (map attribute-key->default-attribute-info))
                                           manufactured-stream-keys)
        all-attributes (into manufactured-attribute-infos material-attribute-infos)]
    (filterv shader-bound-attribute? all-attributes)))

(defn attributes->save-values [material-attribute-infos vertex-attribute-overrides]
  (into []
        (keep (fn [{:keys [data-type element-count name name-key normalize semantic-type]}]
                (when-some [override-values (get vertex-attribute-overrides name-key)]
                  ;; Ensure our saved values have the expected element-count.
                  ;; If the material has been edited, this might have changed,
                  ;; but specialized widgets like the one we use to edit color
                  ;; properties may also produce a different element count from
                  ;; what the material dictates.
                  (let [resized-values (resize-doubles override-values semantic-type element-count)
                        [attribute-value-keyword stored-values] (doubles->storage resized-values data-type normalize)]
                    {:name name
                     attribute-value-keyword {:v stored-values}}))))
        material-attribute-infos))

(defn attributes->build-target [material-attribute-infos vertex-attribute-overrides vertex-attribute-bytes]
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
    (:semantic-type-position :semantic-type-texcoord :semantic-type-page-index) false
    nil false
    true))

(defn- attribute-property-type [attribute]
  (case (:semantic-type attribute)
    :semantic-type-color types/Color
    (case (int (:element-count attribute))
      1 g/Num
      2 types/Vec2
      3 types/Vec3
      4 types/Vec4)))

(defn- attribute-expected-element-count [attribute]
  (case (:semantic-type attribute)
    :semantic-type-color 4
    (:element-count attribute)))

(defn- attribute-update-property [current-property-value attribute new-value]
  (assoc current-property-value (:name-key attribute) new-value))

(defn- attribute-clear-property [current-property-value attribute]
  (dissoc current-property-value (:name-key attribute)))

(defn- attribute-key->property-key-raw [attribute-key]
  (keyword (str "attribute_" (name attribute-key))))

(def attribute-key->property-key (memoize attribute-key->property-key-raw))

(defn- attribute-edit-type [attribute property-type]
  (let [attribute-element-count (:element-count attribute)
        attribute-semantic-type (:semantic-type attribute)
        attribute-update-fn (fn [_evaluation-context self _old-value new-value]
                              (let [values (if (= g/Num property-type)
                                             (vector-of :double new-value)
                                             new-value)]
                                (g/update-property self :vertex-attribute-overrides attribute-update-property attribute values)))
        attribute-clear-fn (fn [self _property-label]
                             (g/update-property self :vertex-attribute-overrides attribute-clear-property attribute))]
    (cond-> {:type property-type
             :set-fn attribute-update-fn
             :clear-fn attribute-clear-fn}

            (= attribute-semantic-type :semantic-type-color)
            (assoc :ignore-alpha? (not= 4 attribute-element-count)))))

(defn attribute-properties-by-property-key [_node-id material-attribute-infos vertex-attribute-overrides]
  (keep (fn [attribute-info]
          (when (editable-attribute-info? attribute-info)
            (let [attribute-key (:name-key attribute-info)
                  semantic-type (:semantic-type attribute-info)
                  material-values (:values attribute-info)
                  override-values (vertex-attribute-overrides attribute-key)
                  attribute-values (or override-values material-values)
                  property-type (attribute-property-type attribute-info)
                  expected-element-count (attribute-expected-element-count attribute-info)
                  edit-type (attribute-edit-type attribute-info property-type)
                  property-key (attribute-key->property-key attribute-key)
                  label (properties/keyword->name attribute-key)
                  value (if (= g/Num property-type)
                          (first attribute-values) ; The widget expects a number, not a vector.
                          (resize-doubles attribute-values semantic-type expected-element-count))
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
        material-attribute-infos))

(defn attribute-bytes-by-attribute-key [_node-id material-attribute-infos vertex-attribute-overrides]
  (let [vertex-attribute-bytes
        (into {}
              (map (fn [{:keys [name-key] :as attribute-info}]
                     (let [override-values (get vertex-attribute-overrides name-key)
                           [bytes error] (if (nil? override-values)
                                           [(:bytes attribute-info) (:error attribute-info)]
                                           (let [{:keys [element-count semantic-type]} attribute-info
                                                 resized-values (resize-doubles override-values semantic-type element-count)
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
