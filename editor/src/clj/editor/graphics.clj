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

(ns editor.graphics
  (:require [dynamo.graph :as g]
            [editor.buffers :as buffers]
            [editor.geom :as geom]
            [editor.gl.vertex2 :as vtx]
            [editor.graphics.types :as graphics.types]
            [editor.properties :as properties]
            [editor.protobuf :as protobuf]
            [editor.types :as t]
            [editor.util :as eutil]
            [editor.validation :as validation]
            [internal.util :as iutil]
            [util.coll :as coll :refer [pair]]
            [util.eduction :as e]
            [util.fn :as fn]
            [util.murmur :as murmur]
            [util.num :as num])
  (:import [com.dynamo.graphics.proto Graphics$VertexAttribute]
           [com.google.protobuf ByteString]
           [editor.gl.vertex2 VertexBuffer]
           [java.nio ByteBuffer]
           [javax.vecmath Matrix4d]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def default-attribute-data-type (protobuf/default Graphics$VertexAttribute :data-type))

(def default-attribute-semantic-type (protobuf/default Graphics$VertexAttribute :semantic-type))

(def default-attribute-vector-type (protobuf/default Graphics$VertexAttribute :vector-type))

(def default-attribute-step-function (protobuf/default Graphics$VertexAttribute :step-function))

(defn- disambiguating-vector-type? [vector-type]
  ;; Can this vector-type disambiguate between two vector-types guessed sorely
  ;; from the number of values? Currently only relevant to the case when we have
  ;; exactly four values. In that case, we could have a :vector-type-vec4 or a
  ;; :vector-type-mat2. If we have the more uncommon :vector-type-mat2, we
  ;; explicitly write it to the file for override attribute values.
  (case vector-type
    :vector-type-mat2 true
    false))

(defn engine-provided-attribute? [attribute]
  {:pre [(map? attribute)] ; Graphics$VertexAttribute in map format.
   :post [(boolean? %)]}
  (graphics.types/engine-provided-semantic-type? (:semantic-type attribute default-attribute-semantic-type)))

(defn- attribute-data-type->attribute-value-keyword [attribute-data-type]
  (case attribute-data-type
    (:type-byte :type-unsigned-byte :type-short :type-unsigned-short :type-int :type-unsigned-int) :long-values
    (:type-float) :double-values))

(defn attribute-value-keyword [attribute-data-type normalize]
  {:pre [(graphics.types/data-type? attribute-data-type)]}
  (if normalize
    :double-values
    (attribute-data-type->attribute-value-keyword attribute-data-type)))

(defn attribute->doubles [attribute]
  (let [data-type (:data-type attribute default-attribute-data-type)
        source-values (if (or (:normalize attribute)
                              (= :type-float data-type))
                        (:v (:double-values attribute))
                        (:v (:long-values attribute)))]
    (if (coll/empty? source-values)
      (graphics.types/default-attribute-doubles
        (:semantic-type attribute default-attribute-semantic-type)
        (:vector-type attribute default-attribute-vector-type))
      (into (vector-of :double)
            (map unchecked-double)
            source-values))))

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

(defn- guess-old-vector-type [^long element-count new-vector-type]
  ;; With most element counts we can know for certain, but if we have exactly
  ;; four elements, we could have either a vec4 or a mat2.
  (case element-count
    1 :vector-type-scalar
    2 :vector-type-vec2
    3 :vector-type-vec3
    4 (case new-vector-type
        ;; If we know we're targeting a scalar or vector type, we assume we have
        ;; a vector type. It makes no difference, really.
        :vector-type-scalar :vector-type-vec4
        :vector-type-vec2 :vector-type-vec4
        :vector-type-vec3 :vector-type-vec4
        :vector-type-vec4 :vector-type-vec4

        ;; Similarly, if we're targeting a matrix type, assume we have a matrix
        ;; as well. It makes a difference if we're targeting a mat3 or a mat4,
        ;; since our four values will be written to the top-left of the matrix.
        :vector-type-mat2 :vector-type-mat2
        :vector-type-mat3 :vector-type-mat2
        :vector-type-mat4 :vector-type-mat2

        ;; If we don't even know what we're targeting, assume we have a vector.
        ;; In the case where we had a :vector-type-mat2 override, it would
        ;; specify the :vector-type because the disambiguating-vector-type?
        ;; function returns true for :vector-type-mat2, so we wouldn't even be
        ;; in this function to begin with.
        nil :vector-type-mat4)
    9 :vector-type-mat3
    16 :vector-type-mat4))

(defn override-attributes->vertex-attribute-overrides [attributes]
  (into {}
        (map (fn [vertex-attribute]
               (let [attribute-name (:name vertex-attribute)
                     attribute-key (graphics.types/attribute-name-key attribute-name)
                     values (attribute->any-doubles vertex-attribute)
                     value-keyword (attribute->value-keyword vertex-attribute)

                     ;; Only written if it can't be guessed from the element
                     ;; count. I.e. either :vector-type-mat2 or nil. See the
                     ;; disambiguating-vector-type? function for details.
                     vector-type (or (:vector-type vertex-attribute)
                                     (guess-old-vector-type (count values) nil))]
                 (pair attribute-key
                       {:name attribute-name
                        :values values
                        :value-keyword value-keyword
                        :vector-type vector-type}))))
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

(defn- semantic-type->w [semantic-type]
  (nth (graphics.types/default-attribute-doubles semantic-type :vector-type-vec4) 3))

(defn convert-double-values [double-values semantic-type old-vector-type new-vector-type]
  {:pre [(graphics.types/semantic-type? semantic-type)
         (or (nil? old-vector-type) (graphics.types/vector-type? old-vector-type))
         (graphics.types/vector-type? new-vector-type)]}
  (if (coll/empty? double-values)
    (graphics.types/default-attribute-doubles semantic-type new-vector-type)
    (case (or old-vector-type
              (guess-old-vector-type (count double-values) new-vector-type))

      :vector-type-scalar
      (case new-vector-type
        :vector-type-scalar double-values
        :vector-type-vec2 (let [[a] double-values]
                            (vector-of :double a a))
        :vector-type-vec3 (let [[a] double-values]
                            (vector-of :double a a a))
        :vector-type-vec4 (let [[a] double-values]
                            (vector-of :double a a a a))
        :vector-type-mat2 (let [[a] double-values]
                            (vector-of :double
                              a 0
                              0 a))
        :vector-type-mat3 (let [[a] double-values]
                            (vector-of :double
                              a 0 0
                              0 a 0
                              0 0 a))
        :vector-type-mat4 (let [[a] double-values]
                            (vector-of :double
                              a 0 0 0
                              0 a 0 0
                              0 0 a 0
                              0 0 0 a)))

      :vector-type-vec2
      (case new-vector-type
        :vector-type-scalar (let [[a b] double-values]
                              (vector-of :double a))
        :vector-type-vec2 double-values
        :vector-type-vec3 (let [[a b] double-values]
                            (vector-of :double a b 0))
        :vector-type-vec4 (let [[a b] double-values
                                w (semantic-type->w semantic-type)]
                            (vector-of :double a b 0 w))
        :vector-type-mat2 (let [[a b] double-values]
                            (vector-of :double
                              a b
                              0 0))
        :vector-type-mat3 (let [[a b] double-values]
                            (vector-of :double
                              a b 0
                              0 0 0
                              0 0 0))
        :vector-type-mat4 (let [[a b] double-values]
                            (vector-of :double
                              a b 0 0
                              0 0 0 0
                              0 0 0 0
                              0 0 0 0)))

      :vector-type-vec3
      (case new-vector-type
        :vector-type-scalar (let [[a b c] double-values]
                              (vector-of :double a))
        :vector-type-vec2 (let [[a b c] double-values]
                            (vector-of :double a b))
        :vector-type-vec3 double-values
        :vector-type-vec4 (let [[a b c] double-values
                                w (semantic-type->w semantic-type)]
                            (vector-of :double a b c w))
        :vector-type-mat2 (let [[a b c] double-values]
                            (vector-of :double
                              a b
                              c 0))
        :vector-type-mat3 (let [[a b c] double-values]
                            (vector-of :double
                              a b c
                              0 0 0
                              0 0 0))
        :vector-type-mat4 (let [[a b c] double-values]
                            (vector-of :double
                              a b c 0
                              0 0 0 0
                              0 0 0 0
                              0 0 0 0)))

      :vector-type-vec4
      (case new-vector-type
        :vector-type-scalar (let [[a b c d] double-values]
                              (vector-of :double a))
        :vector-type-vec2 (let [[a b c d] double-values]
                            (vector-of :double a b))
        :vector-type-vec3 (let [[a b c d] double-values]
                            (vector-of :double a b c))
        :vector-type-vec4 double-values
        :vector-type-mat2 double-values
        :vector-type-mat3 (let [[a b c d] double-values]
                            (vector-of :double
                              a b c
                              d 0 0
                              0 0 0))
        :vector-type-mat4 (let [[a b c d] double-values]
                            (vector-of :double
                              a b c d
                              0 0 0 0
                              0 0 0 0
                              0 0 0 0)))

      :vector-type-mat2
      (case new-vector-type
        :vector-type-scalar (subvec double-values 0 1)
        :vector-type-vec2 (subvec double-values 0 2)
        :vector-type-vec3 (subvec double-values 0 3)
        :vector-type-vec4 double-values
        :vector-type-mat2 double-values
        :vector-type-mat3 (let [[a b
                                 c d]
                                double-values]
                            (vector-of :double
                              a b 0
                              c d 0
                              0 0 1))
        :vector-type-mat4 (let [[a b
                                 c d]
                                double-values]
                            (vector-of :double
                              a b 0 0
                              c d 0 0
                              0 0 1 0
                              0 0 0 1)))

      :vector-type-mat3
      (case new-vector-type
        :vector-type-scalar (subvec double-values 0 1)
        :vector-type-vec2 (subvec double-values 0 2)
        :vector-type-vec3 (subvec double-values 0 3)
        :vector-type-vec4 (subvec double-values 0 4)
        :vector-type-mat2 (let [[a b c
                                 d e f
                                 g h i]
                                double-values]
                            (vector-of :double
                              a b
                              d e))
        :vector-type-mat3 double-values
        :vector-type-mat4 (let [[a b c
                                 d e f
                                 g h i]
                                double-values]
                            (vector-of :double
                              a b c 0
                              d e f 0
                              g h i 0
                              0 0 0 1)))

      :vector-type-mat4
      (case new-vector-type
        :vector-type-scalar (subvec double-values 0 1)
        :vector-type-vec2 (subvec double-values 0 2)
        :vector-type-vec3 (subvec double-values 0 3)
        :vector-type-vec4 (subvec double-values 0 4)
        :vector-type-mat2 (let [[a b c d
                                 e f g h
                                 i j k l
                                 m n o p]
                                double-values]
                            (vector-of :double
                              a b
                              e f))
        :vector-type-mat3 (let [[a b c d
                                 e f g h
                                 i j k l
                                 m n o p]
                                double-values]
                            (vector-of :double
                              a b c
                              e f g
                              i j k))
        :vector-type-mat4 double-values))))

(defn- attribute-values+data-type->byte-size [attribute-values attribute-data-type]
  (* (count attribute-values) (graphics.types/data-type-byte-size attribute-data-type)))

(defn- make-attribute-bytes
  ^bytes [attribute-data-type normalize attribute-values]
  (let [attribute-value-byte-count (attribute-values+data-type->byte-size attribute-values attribute-data-type)
        attribute-bytes (byte-array attribute-value-byte-count)
        byte-buffer (vtx/wrap-buf attribute-bytes)
        buffer-data-type (graphics.types/data-type-buffer-data-type attribute-data-type)]
    (vtx/buf-push! byte-buffer buffer-data-type normalize attribute-values)
    attribute-bytes))

(defn- default-attribute-bytes-raw [semantic-type attribute-data-type vector-type normalize]
  (let [default-values (graphics.types/default-attribute-doubles semantic-type vector-type)]
    (make-attribute-bytes attribute-data-type normalize default-values)))

(def default-attribute-bytes (memoize default-attribute-bytes-raw))

(defn attribute->bytes+error-message
  ([attribute]
   (attribute->bytes+error-message attribute nil))
  ([{:keys [data-type vector-type normalize] :as attribute} override-values]
   {:pre [(map? attribute)
          (graphics.types/data-type? data-type)
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

(defn combined-attribute-infos [shader-attribute-reflection-infos material-attribute-infos default-coordinate-space]
  ;; Note: The order of the supplied shader-attribute-reflection-infos and
  ;; material-attribute-infos will dictate the order in which the
  ;; attribute-buffers will be assigned to attributes that share the same
  ;; semantic-type.
  (let [shader-attribute-reflection-infos-by-name-key
        (coll/pair-map-by :name-key shader-attribute-reflection-infos)

        decorated-material-attribute-infos
        (e/keep (fn [{:keys [name-key] :as material-attribute-info}]
                  (when-some [shader-attribute-reflection-info (shader-attribute-reflection-infos-by-name-key name-key)]
                    (graphics.types/attribute-info-with-reflection-info material-attribute-info shader-attribute-reflection-info)))
                material-attribute-infos)]

    (coll/transfer
      [decorated-material-attribute-infos
       shader-attribute-reflection-infos]
      []
      cat
      (iutil/distinct-by :name-key)
      (map #(graphics.types/assign-attribute-transform % default-coordinate-space)))))

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

(defn- attribute-info->vector-type [{:keys [element-count vector-type] :as _attribute-info}]
  ;; This function can return nil if element-count and vector-type is nil,
  ;; which happens for default values. This is only used for sanitation,
  ;; so we shouldn't return the default value here. In legacy file formats, only
  ;; scalar, vec2, vec3, vec4 attributes were supported.
  (or vector-type
      (when (pos-int? element-count)
        (case (long element-count)
          1 :vector-type-scalar
          2 :vector-type-vec2
          3 :vector-type-vec3
          4 :vector-type-vec4))))

(defn- overridable-semantic-type? [semantic-type]
  (case semantic-type
    :semantic-type-none true
    :semantic-type-position false
    :semantic-type-texcoord false
    :semantic-type-page-index false
    :semantic-type-color true
    :semantic-type-normal false
    :semantic-type-tangent false
    :semantic-type-world-matrix false
    :semantic-type-normal-matrix false))

(defn overridable-attribute-info? [attribute-info]
  (overridable-semantic-type? (:semantic-type attribute-info default-attribute-semantic-type)))

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
    (cond-> (dissoc attribute :name-hash :element-count :double-values :long-values :binary-values)

            (and (some? attribute-vector-type)
                 (not= attribute-vector-type default-attribute-vector-type))
            (assoc :vector-type attribute-vector-type)

            (not (engine-provided-attribute? attribute))
            (assoc attribute-value-keyword {:v attribute-values}))))

(defn sanitize-attribute-override [attribute]
  ;; Graphics$VertexAttribute in map format.
  (-> (if (disambiguating-vector-type? (:vector-type attribute default-attribute-vector-type))
        attribute
        (dissoc attribute :vector-type))
      (dissoc :binary-values :coordinate-space :data-type :element-count :name-hash :normalize :semantic-type)
      (sanitize-attribute-value-field :double-values)
      (sanitize-attribute-value-field :long-values)))

(defn vertex-attribute-overrides->save-values [vertex-attribute-overrides material-attribute-infos]
  (let [declared-material-attribute-key? (into #{} (map :name-key) material-attribute-infos)

        material-attribute-save-values
        (eduction
          (keep
            (fn [material-attribute-info]
              (let [attribute-key (:name-key material-attribute-info)
                    override-info (vertex-attribute-overrides attribute-key)]
                (when-some [override-values (coll/not-empty (:values override-info))]
                  ;; Ensure our saved values have the expected element-count.
                  ;; If the material has been edited, this might have changed,
                  ;; but edits made using specialized widgets like the one we
                  ;; use to edit color properties may also alter the vector-type
                  ;; from what the material dictates.
                  (let [{:keys [data-type name normalize semantic-type]} material-attribute-info
                        material-attribute-vector-type (:vector-type material-attribute-info)
                        override-attribute-vector-type (:vector-type override-info)
                        _ (assert (graphics.types/vector-type? override-attribute-vector-type))
                        converted-values (convert-double-values override-values semantic-type override-attribute-vector-type material-attribute-vector-type)
                        [attribute-value-keyword stored-values] (doubles->storage converted-values data-type normalize)
                        explicit-vector-type (when (disambiguating-vector-type? material-attribute-vector-type)
                                               material-attribute-vector-type)]
                    (protobuf/make-map-without-defaults Graphics$VertexAttribute
                      :name name
                      :vector-type explicit-vector-type
                      attribute-value-keyword {:v stored-values}))))))
          material-attribute-infos)

        orphaned-attribute-save-values
        (eduction
          (keep
            (fn [[attribute-key override-info]]
              (when-some [override-values (coll/not-empty (:values override-info))]
                (when-not (declared-material-attribute-key? attribute-key)
                  (let [attribute-name (:name override-info)
                        attribute-value-keyword (:value-keyword override-info)
                        override-attribute-vector-type (:vector-type override-info)
                        _ (assert (graphics.types/vector-type? override-attribute-vector-type))
                        explicit-vector-type (when (disambiguating-vector-type? override-attribute-vector-type)
                                               override-attribute-vector-type)]
                    (protobuf/make-map-without-defaults Graphics$VertexAttribute
                      :name attribute-name
                      :vector-type explicit-vector-type
                      attribute-value-keyword (when (coll/not-empty override-values)
                                                {:v override-values})))))))
          vertex-attribute-overrides)]

    ;; Merge attributes from both collections and sort by name to ensure we get
    ;; a consistent attribute order in the files.
    (->> [material-attribute-save-values
          orphaned-attribute-save-values]
         (into [] cat)
         (sort-by :name))))

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

(defn- attribute-property-type [attribute]
  (case (:semantic-type attribute)
    :semantic-type-color t/Color
    (case (:vector-type attribute)
      :vector-type-scalar g/Num
      :vector-type-vec2 t/Vec2
      :vector-type-vec3 t/Vec3
      :vector-type-vec4 t/Vec4
      :vector-type-mat2 t/Mat2
      :vector-type-mat3 t/Mat3
      :vector-type-mat4 t/Mat4)))

(defn- attribute-update-property [current-property-value attribute-info new-value]
  (let [name-key (:name-key attribute-info)
        override-info (name-key current-property-value)
        attribute-value-keyword (attribute-value-keyword (:data-type attribute-info) (:normalize attribute-info))
        override-value-keyword (or attribute-value-keyword (:value-keyword override-info))
        override-name (or (:name attribute-info) (:name override-info))
        vector-type (:vector-type attribute-info)]
    (assoc current-property-value name-key {:values new-value
                                            :value-keyword override-value-keyword
                                            :vector-type vector-type
                                            :name override-name})))

(defn- attribute-clear-property [current-property-value {:keys [name-key] :as _attribute}]
  {:pre [(keyword? name-key)]}
  (dissoc current-property-value name-key))

(defn- attribute-key->property-key-raw [attribute-key material-index]
  (keyword (str "attribute_" material-index "_" (name attribute-key))))

(def attribute-key->property-key (memoize attribute-key->property-key-raw))

(defn- attribute-override-property-edit-type [{:keys [semantic-type vector-type] :as attribute-info} property-type]
  {:pre [(keyword? vector-type)
         (graphics.types/semantic-type? semantic-type)]}
  (let [attribute-update-fn (fn attribute-update-fn [_evaluation-context self _old-value new-value]
                              (let [values (if (= g/Num property-type)
                                             (vector-of :double new-value)
                                             new-value)]
                                (g/update-property self :vertex-attribute-overrides attribute-update-property attribute-info values)))
        attribute-clear-fn (fn attribute-clear-fn [self _property-label]
                             (g/update-property self :vertex-attribute-overrides attribute-clear-property attribute-info))]
    (cond-> {:type property-type
             :set-fn attribute-update-fn
             :clear-fn attribute-clear-fn}

            (= semantic-type :semantic-type-color)
            (assoc :ignore-alpha (not= :vector-type-vec4 vector-type)))))

(defn- attribute-property-value [attribute-values property-type semantic-type source-vector-type target-vector-type]
  (if (= g/Num property-type)
    (first attribute-values) ; The widget expects a number, not a vector.
    (convert-double-values attribute-values semantic-type source-vector-type target-vector-type)))

(defn attribute-property-entries [_node-id material-attribute-infos material-index vertex-attribute-overrides]
  (let [declared-material-attribute-key? (into #{} (map :name-key) material-attribute-infos)]
    (concat
      ;; Original and overridden vertex attributes with a match in the material.
      (keep (fn [material-attribute-info]
              (when (overridable-attribute-info? material-attribute-info)
                (let [attribute-key (:name-key material-attribute-info)
                      semantic-type (:semantic-type material-attribute-info)
                      override-attribute-info (vertex-attribute-overrides attribute-key)
                      override-values (:values override-attribute-info)
                      material-values (:values material-attribute-info)
                      attribute-values (or override-values material-values)
                      attribute-values-vector-type (:vector-type (or override-attribute-info material-attribute-info))
                      _ (assert (graphics.types/vector-type? attribute-values-vector-type))
                      property-type (attribute-property-type material-attribute-info)
                      property-vector-type (case semantic-type
                                             :semantic-type-color :vector-type-vec4
                                             (:vector-type material-attribute-info))
                      edit-type (attribute-override-property-edit-type material-attribute-info property-type)
                      property-key (attribute-key->property-key attribute-key material-index)
                      label (properties/keyword->name attribute-key)
                      value (attribute-property-value attribute-values property-type semantic-type attribute-values-vector-type property-vector-type)
                      error (when (some? override-values)
                              (validate-doubles override-values material-attribute-info _node-id property-key))
                      prop (cond-> {:node-id _node-id
                                    :type property-type
                                    :edit-type edit-type
                                    :label label
                                    :value value
                                    :error error}

                                   ;; Insert the original material values as original-value if there is a vertex override.
                                   (some? override-values)
                                   (assoc :original-value material-values))]
                  (pair property-key prop))))
            material-attribute-infos)

      ;; Rogue vertex attributes without a match in the material.
      (for [[name-key vertex-override-info] vertex-attribute-overrides
            :when (not (declared-material-attribute-key? name-key))
            :let [name (:name vertex-override-info)
                  values (:values vertex-override-info)
                  semantic-type :semantic-type-none
                  vector-type (:vector-type vertex-override-info)
                  _ (assert (graphics.types/vector-type? vector-type))
                  assumed-attribute-info {:name name
                                          :name-key name-key
                                          :semantic-type semantic-type
                                          :vector-type vector-type}
                  property-type (attribute-property-type assumed-attribute-info)]]
        [(attribute-key->property-key name-key material-index)
         {:node-id _node-id
          :value (attribute-property-value values property-type semantic-type vector-type vector-type)
          :label name
          :type property-type
          :edit-type (attribute-override-property-edit-type assumed-attribute-info property-type)
          :original-value []}]))))

(defn attribute-bytes-by-attribute-key [_node-id material-attribute-infos material-index vertex-attribute-overrides]
  (let [vertex-attribute-bytes
        (into {}
              (map (fn [{:keys [name-key] :as attribute-info}]
                     {:pre [(keyword? name-key)]}
                     (let [override-info (get vertex-attribute-overrides name-key)
                           override-values (:values override-info)
                           [bytes error] (if (nil? override-values)
                                           (pair (:bytes attribute-info) (:error attribute-info))
                                           (let [{:keys [semantic-type vector-type]} attribute-info
                                                 override-vector-type (:vector-type override-info)
                                                 _ (assert (graphics.types/vector-type? override-vector-type))
                                                 converted-values (convert-double-values override-values semantic-type override-vector-type vector-type)
                                                 [bytes error-message :as bytes+error-message] (attribute->bytes+error-message attribute-info converted-values)]
                                             (if (nil? error-message)
                                               bytes+error-message
                                               (let [property-key (attribute-key->property-key name-key material-index)
                                                     error (g/->error _node-id property-key :fatal override-values error-message)]
                                                 (pair bytes error)))))]
                       (pair name-key (or error bytes)))))
              material-attribute-infos)]
    (g/precluding-errors (vals vertex-attribute-bytes)
      vertex-attribute-bytes)))

(defn- decorate-attribute-exception [exception attribute vertex]
  (ex-info "Failed to encode vertex attribute."
           (-> attribute
               (select-keys [:name :semantic-type :data-type :vector-type :normalize :coordinate-space])
               (assoc :vertex vertex)
               (assoc :vertex-elements (count vertex)))
           exception))

(defn- renderable-data->world-positions-v3 [renderable-data]
  (let [local-positions (:position-data renderable-data)
        world-transform (:world-transform renderable-data)]
    (geom/transf-p world-transform local-positions)))

(defn- renderable-data->world-normals-v3 [renderable-data]
  (let [local-directions (:normal-data renderable-data)
        normal-transform (:normal-transform renderable-data)]
    (geom/transf-n normal-transform local-directions)))

(defn- renderable-data->world-tangents-v4 [renderable-data]
  (let [tangents (:tangent-data renderable-data)
        normal-transform (:normal-transform renderable-data)]
    (geom/transf-tangents normal-transform tangents)))

(defn- mat4-double-values [^Matrix4d matrix vector-type]
  (-> matrix
      (geom/as-array)
      (convert-double-values :semantic-type-none :vector-type-mat4 vector-type))) ; Semantic type does not matter for this.

(defn put-attributes!
  ^VertexBuffer [^VertexBuffer vbuf renderable-datas]
  (let [vertex-description (.vertex-description vbuf)
        ^long vertex-byte-stride (:size vertex-description)
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
                    (let [attribute-doubles (convert-double-values attribute-doubles semantic-type nil vector-type)]
                      (vtx/buf-put! buf vertex-byte-offset buffer-data-type normalize attribute-doubles))
                    (+ vertex-byte-offset vertex-byte-stride))
                  (long vertex-byte-offset)
                  vertices))

        put-renderables!
        (fn put-renderables!
          ^long [^long attribute-byte-offset put-vertices! renderable-data->vertices]
          (reduce (fn [^long vertex-byte-offset renderable-data]
                    (let [vertices (renderable-data->vertices renderable-data)]
                      (put-vertices! vertex-byte-offset vertices)))
                  attribute-byte-offset
                  renderable-datas))

        mesh-data-exists?
        (let [renderable-data (first renderable-datas)
              texcoord-datas (:texcoord-datas renderable-data)]
          (if (nil? renderable-data)
            fn/constantly-false
            (fn mesh-data-exists? [semantic-type ^long channel]
              ;; Each time we see a specific semantic type on an attribute, we
              ;; increase the channel number. The idea is to supply the first
              ;; set of texture coordinates in the mesh to the first attribute
              ;; of :semantic-type-texcoord, the second set of texture
              ;; coordinates to the next attribute of :semantic-type-texcoord,
              ;; and so on. The channel is only considered for certain semantic
              ;; types where it makes sense to do so. For some semantic types,
              ;; the same mesh data will be supplied to all attributes of that
              ;; semantic type.
              (case semantic-type
                :semantic-type-position
                ;; We only support one set of positions. The same mesh data will
                ;; be used for all position attributes. This allows for local
                ;; space and world space attributes to coexist.
                (some? (:position-data renderable-data))

                :semantic-type-texcoord
                ;; We support multiple sets of texture coordinates. The mesh
                ;; data will apply only to the specific channel.
                (some? (get-in texcoord-datas [channel :uv-data]))

                :semantic-type-page-index
                ;; We consider the page index to be associated with a specific
                ;; set of texture coordinates. The mesh data will apply only to
                ;; the specific channel.
                (some? (get-in texcoord-datas [channel :page-index]))

                :semantic-type-color
                ;; We currently only support one set of vertex colors from the
                ;; mesh, but may want to support more in the future. The mesh
                ;; data will apply only to the specific channel.
                (and (zero? channel)
                     (some? (:color-data renderable-data)))

                :semantic-type-normal
                ;; We only support one set of normals. The same mesh data will
                ;; be used for all normal attributes. This allows for local
                ;; space and world space attributes to coexist.
                (some? (:normal-data renderable-data))

                :semantic-type-tangent
                ;; We consider the tangent space to be associated with a
                ;; specific set of texture coordinates. The mesh data will apply
                ;; only to the specific channel. However, we currently only
                ;; support one set of tangents from the mesh.
                (and (zero? channel)
                     (some? (:tangent-data renderable-data)))

                :semantic-type-world-matrix
                ;; The world-matrix transforms local-space positions in the mesh
                ;; into world-space. The same matrix will be supplied to all the
                ;; world-matrix attributes.
                (boolean (:has-semantic-type-world-matrix renderable-data))

                :semantic-type-normal-matrix
                ;; The normal-matrix transforms local-space normals in the mesh
                ;; into view-space. The same matrix will be supplied to all the
                ;; normal-matrix attributes.
                (boolean (:has-semantic-type-normal-matrix renderable-data))

                false))))]

    (reduce (fn [reduce-info attribute]
              (let [semantic-type (:semantic-type attribute)
                    data-type (:data-type attribute)
                    vector-type (:vector-type attribute)
                    buffer-data-type (graphics.types/data-type-buffer-data-type data-type)
                    element-count (graphics.types/vector-type-component-count vector-type)
                    normalize (:normalize attribute)
                    name-key (:name-key attribute)
                    attribute-byte-offset (long (:attribute-byte-offset reduce-info))
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
                      (put-renderables! attribute-byte-offset put-attribute-doubles! renderable-data->world-positions-v3)
                      (put-renderables! attribute-byte-offset put-attribute-doubles! :position-data))

                    :semantic-type-texcoord
                    (put-renderables! attribute-byte-offset put-attribute-doubles! #(get-in % [:texcoord-datas channel :uv-data]))

                    :semantic-type-page-index
                    (put-renderables! attribute-byte-offset put-attribute-doubles!
                                      (fn renderable-data->page-indices [renderable-data]
                                        (let [vertex-count (count (:position-data renderable-data))
                                              page-index (get-in renderable-data [:texcoord-datas channel :page-index])]
                                          (repeat vertex-count [(double page-index)]))))

                    :semantic-type-color
                    (put-renderables! attribute-byte-offset put-attribute-doubles! :color-data)

                    :semantic-type-normal
                    (case (:coordinate-space attribute)
                      :coordinate-space-world
                      (put-renderables! attribute-byte-offset put-attribute-doubles! renderable-data->world-normals-v3)
                      (put-renderables! attribute-byte-offset put-attribute-doubles! :normal-data))

                    :semantic-type-tangent
                    (case (:coordinate-space attribute)
                      :coordinate-space-world
                      (put-renderables! attribute-byte-offset put-attribute-doubles! renderable-data->world-tangents-v4)
                      (put-renderables! attribute-byte-offset put-attribute-doubles! :tangent-data))

                    :semantic-type-world-matrix
                    (put-renderables! attribute-byte-offset put-attribute-doubles!
                                      (fn renderable-data->world-matrices [renderable-data]
                                        (let [vertex-count (count (:position-data renderable-data))
                                              matrix-values (mat4-double-values (:world-transform renderable-data) vector-type)]
                                          (repeat vertex-count matrix-values))))

                    :semantic-type-normal-matrix
                    (put-renderables! attribute-byte-offset put-attribute-doubles!
                                      (fn renderable-data->normal-matrices [renderable-data]
                                        (let [vertex-count (count (:position-data renderable-data))
                                              matrix-values (mat4-double-values (:normal-transform renderable-data) vector-type)]
                                          (repeat vertex-count matrix-values)))))

                  ;; Mesh data doesn't exist. Use the attribute data from the
                  ;; material or overrides. If the material does not declare an
                  ;; attribute bound by the shader, use a default that makes
                  ;; sense for the attribute.
                  (let [attribute-byte-count-max (* element-count (buffers/type-size buffer-data-type))
                        attribute-data-type (graphics.types/buffer-data-type-data-type buffer-data-type)
                        default-attribute-bytes (default-attribute-bytes semantic-type attribute-data-type vector-type normalize)]
                    (put-renderables!
                      attribute-byte-offset put-attribute-bytes!
                      (fn renderable-data->attribute-bytes [renderable-data]
                        (let [vertex-count (count (:position-data renderable-data))
                              attribute-bytes (get (:vertex-attribute-bytes renderable-data) name-key default-attribute-bytes)]
                          ;; Clamp the buffer if the container format is smaller than specified in the material
                          (if (> (count attribute-bytes) attribute-byte-count-max)
                            (let [attribute-bytes-clamped (byte-array attribute-byte-count-max)]
                              (System/arraycopy attribute-bytes 0 attribute-bytes-clamped 0 attribute-byte-count-max)
                              (repeat vertex-count attribute-bytes-clamped))
                            (repeat vertex-count attribute-bytes)))))))
                (-> reduce-info
                    (update semantic-type inc)
                    (assoc :attribute-byte-offset (+ attribute-byte-offset
                                                     (graphics.types/attribute-info-byte-size attribute))))))
            ;; The reduce-info is a map of how many times we've encountered an
            ;; attribute of a particular semantic-type. We also include a
            ;; special key, :attribute-byte-offset, to keep track of where the
            ;; next encountered attribute will start writing its data to the
            ;; vertex buffer.
            (into {:attribute-byte-offset 0}
                  (map (fn [semantic-type]
                         (pair semantic-type 0)))
                  graphics.types/semantic-types)
            (:attributes vertex-description))
    (.position buf (.limit buf))
    (vtx/flip! vbuf)))
