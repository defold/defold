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

(ns editor.protobuf
  "This ns primarily converts between protobuf java objs and clojure maps.
It does this at runtime, which is why it relies on java reflection to
call the appropriate methods. Since this is very expensive (specifically
fetching the Method from the Class), it uses memoization wherever possible.
It should be possible to use macros instead and retain the same API.
Macros currently mean no foreseeable performance gain, however."
  (:require [camel-snake-kebab :refer [->CamelCase ->kebab-case]]
            [clojure.java.io :as io]
            [clojure.string :as string]
            [editor.system :as system]
            [editor.util :as util]
            [internal.java :as java]
            [util.coll :as coll :refer [pair]]
            [util.defonce :as defonce]
            [util.digest :as digest]
            [util.fn :as fn]
            [util.text-util :as text-util])
  (:import [com.dynamo.proto DdfExtensions DdfMath$Matrix4 DdfMath$Point3 DdfMath$Quat DdfMath$Vector3 DdfMath$Vector3One DdfMath$Vector4 DdfMath$Vector4One DdfMath$Vector4WOne]
           [com.google.protobuf DescriptorProtos$FieldOptions Descriptors$Descriptor Descriptors$EnumDescriptor Descriptors$EnumValueDescriptor Descriptors$FieldDescriptor Descriptors$FieldDescriptor$JavaType Descriptors$FieldDescriptor$Type Descriptors$FileDescriptor Message Message$Builder ProtocolMessageEnum TextFormat]
           [java.io ByteArrayOutputStream StringReader]
           [java.lang.reflect Method]
           [java.nio.charset StandardCharsets]
           [java.util Collection]
           [javax.vecmath Matrix4d Point3d Quat4d Vector3d Vector4d]
           [org.apache.commons.io FilenameUtils]))

(set! *warn-on-reflection* true)

(def ^:private decorate-protobuf-exceptions
  (and *assert*
       (system/defold-dev?)
       (not (Boolean/getBoolean "defold.exception.decorate.disable"))))

(def ^:private enforce-strict-pb-map-keys
  (and *assert*
       (system/defold-dev?)
       (Boolean/getBoolean "defold.protobuf.strict.enable")))

(defonce/protocol GenericDescriptor
  (proto ^Message [this])
  (desc-name ^String [this])
  (full-name ^String [this])
  (file ^Descriptors$FileDescriptor [this])
  (containing-type ^Descriptors$Descriptor [this]))

(defonce/protocol PbConverter
  (msg->vecmath [^Message pb v] "Return the javax.vecmath equivalent for the Protocol Buffer message")
  (msg->clj [^Message pb v]))

(def ^:private upper-pattern (re-pattern #"\p{javaUpperCase}"))

(defn escape-string
  ^String [^String string]
  (-> string
      (.getBytes StandardCharsets/UTF_8)
      (TextFormat/escapeBytes)))

(defn- default-instance-raw [^Class cls]
  (java/invoke-no-arg-class-method cls "getDefaultInstance"))

(def ^:private default-instance (fn/memoize default-instance-raw))

(defn pb-class? [value]
  (and (class? value)
       (not= Message value)
       (.isAssignableFrom Message value)))

(defn- new-builder
  ^Message$Builder [^Class cls]
  (java/invoke-no-arg-class-method cls "newBuilder"))

(defn- field-name->key-raw [^String field-name]
  (keyword (if (re-find upper-pattern field-name)
             (->kebab-case field-name)
             (string/replace field-name "_" "-"))))

(def field-name->key (fn/memoize field-name->key-raw))

(defn- field->key [^Descriptors$FieldDescriptor field-desc]
  (field-name->key (.getName field-desc)))

(definline boolean->int [value]
  `(if (boolean ~value)
     1
     0))

(definline int->boolean [value]
  `(if (some-> ~value (not= 0))
     true
     false))

(defn- enum-name->keyword-name
  ^String [^String enum-name]
  (util/lower-case* (string/replace enum-name "_" "-")))

(defn- enum-name->keyword-raw [^String enum-name]
  (keyword (enum-name->keyword-name enum-name)))

(def ^:private enum-name->keyword (fn/memoize enum-name->keyword-raw))

(defn- keyword->enum-name-raw
  ^String [keyword]
  (.intern (string/replace (util/upper-case* (name keyword)) "-" "_")))

(def ^:private keyword->enum-name (fn/memoize keyword->enum-name-raw))

(defn- keyword->field-name-raw
  ^String [keyword]
  (.intern (string/replace (name keyword) "-" "_")))

(def keyword->field-name (fn/memoize keyword->field-name-raw))

(defn- as-enum-desc
  ^Descriptors$EnumValueDescriptor [val-or-desc]
  (if (instance? ProtocolMessageEnum val-or-desc)
    (.getValueDescriptor ^ProtocolMessageEnum val-or-desc)
    val-or-desc))

(defn pb-enum->val [val-or-desc]
  (-> val-or-desc as-enum-desc .getName enum-name->keyword))

(defn pb-enum->int ^long [val-or-desc]
  (-> val-or-desc as-enum-desc .getNumber))

(defn- pb-primitive->clj-fn [field-type-key]
  (case field-type-key
    :enum pb-enum->val
    :bool boolean ; Java reflection returns unique Boolean instances. We want either Boolean/TRUE or Boolean/FALSE.
    :message (throw (IllegalArgumentException. "Non-primitive protobuf types are not supported."))
    identity))

(defn- pb-primitive->clj [pb-value field-type-key]
  (let [pb-value->clj (pb-primitive->clj-fn field-type-key)]
    (pb-value->clj pb-value)))

(declare ^:private pb->clj-fn)

(defn- pb-value->clj-fn [{:keys [field-type-key] :as field-info} default-included-field-rules]
  (let [pb-value->clj
        (case field-type-key
          :message (pb->clj-fn (:type field-info) default-included-field-rules)
          (pb-primitive->clj-fn field-type-key))]
    (if (not= :repeated (:field-rule field-info))
      pb-value->clj
      (fn repeated-pb-value->clj [^Collection values]
        (when-not (.isEmpty values)
          (mapv pb-value->clj values))))))

(def ^:private methods-by-name
  (fn/memoize
    (fn methods-by-name [^Class class]
      (into {}
            (map (fn [^Method m]
                   (pair (.getName m) m)))
            (java/get-declared-methods class)))))

(defn- lookup-method
  ^Method [^Class class method-name]
  (let [methods-by-name (methods-by-name class)]
    (or (get methods-by-name method-name)
        (throw (ex-info (str "Protobuf method lookup failed for " method-name " in " (.getName class))
                        {:available-names (vec (sort (keys methods-by-name)))
                         :class-name (.getName class)
                         :method-name method-name})))))

;; In order to get same behaviour as protobuf compiler, translated from:
;; https://github.com/google/protobuf/blob/2f4489a3e504e0a4aaffee69b551c6acc9e08374/src/google/protobuf/compiler/java/java_helpers.cc#L119
(defn underscores-to-camel-case-raw
  [^String s]
  (loop [i 0
         sb (StringBuilder. (.length s))
         cap-next? true]
    (if (< i (.length s))
      (let [c (.codePointAt s i)]
        (cond
          (<= (int \a) c (int \z))
          (let [c' (if cap-next? (Character/toUpperCase c) c)]
            (recur (inc i) (.appendCodePoint sb c') false))

          (<= (int \A) c (int \Z))
          (let [c' (if (and (zero? i) (not cap-next?)) (Character/toLowerCase c) c)]
            (recur (inc i) (.appendCodePoint sb c') false))

          (<= (int \0) c (int \9))
          (recur (inc i) (.appendCodePoint sb c) true)

          :else
          (recur (inc i) sb true)))
      (-> (if (and (pos? (.length s))
                   (= (int \#) (.codePointAt s (dec (.length s)))))
            (.appendCodePoint sb (int \_))
            sb)
          (.toString)
          (.intern)))))

(def underscores-to-camel-case (fn/memoize underscores-to-camel-case-raw))

(defonce ^:private resource-desc (.getDescriptor DdfExtensions/resource))

(defonce ^:private runtime-only-desc (.getDescriptor DdfExtensions/runtimeOnly))

(defn- options [^DescriptorProtos$FieldOptions field-options]
  (cond-> {}

          (.getField field-options resource-desc)
          (assoc :resource true)

          (.getField field-options runtime-only-desc)
          (assoc :runtime-only true)))

(defn field-value-class [^Class class ^Descriptors$FieldDescriptor field-desc]
  (let [java-name (underscores-to-camel-case (.getName field-desc))
        field-accessor-name (str "get" java-name)]
    (.getReturnType (lookup-method class field-accessor-name))))

(def ^:private field-type->key
  {Descriptors$FieldDescriptor$Type/BOOL :bool
   Descriptors$FieldDescriptor$Type/BYTES :bytes
   Descriptors$FieldDescriptor$Type/DOUBLE :double
   Descriptors$FieldDescriptor$Type/ENUM :enum
   Descriptors$FieldDescriptor$Type/FIXED32 :fixed-32
   Descriptors$FieldDescriptor$Type/FIXED64 :fixed-64
   Descriptors$FieldDescriptor$Type/FLOAT :float
   Descriptors$FieldDescriptor$Type/GROUP :group
   Descriptors$FieldDescriptor$Type/INT32 :int-32
   Descriptors$FieldDescriptor$Type/INT64 :int-64
   Descriptors$FieldDescriptor$Type/MESSAGE :message
   Descriptors$FieldDescriptor$Type/SFIXED32 :sfixed-32
   Descriptors$FieldDescriptor$Type/SFIXED64 :sfixed-64
   Descriptors$FieldDescriptor$Type/SINT32 :sint-32
   Descriptors$FieldDescriptor$Type/SINT64 :sint-64
   Descriptors$FieldDescriptor$Type/STRING :string
   Descriptors$FieldDescriptor$Type/UINT32 :uint-32
   Descriptors$FieldDescriptor$Type/UINT64 :uint-64})

(defn- field-get-method-raw
  ^Method [^Class cls java-name repeated]
  (let [get-method-name (str "get" java-name (if repeated "List" ""))]
    (lookup-method cls get-method-name)))

(def ^:private field-get-method (fn/memoize field-get-method-raw))

(defn- field-has-value-fn
  ^Method [^Class cls java-name repeated]
  (if repeated
    (let [count-method-name (str "get" java-name "Count")
          count-method (lookup-method cls count-method-name)]
      (fn repeated-field-has-value? [pb]
        (pos? (.invoke count-method pb java/no-args-array))))
    (let [has-method-name (str "has" java-name)
          has-method (lookup-method cls has-method-name)]
      (fn field-has-value? [pb]
        ;; Wrapped in boolean because reflection will produce unique Boolean
        ;; instances that cannot evaluate to false in if-expressions.
        (boolean (.invoke has-method pb java/no-args-array))))))

(defn pb-class->descriptor
  ^Descriptors$Descriptor [^Class cls]
  {:pre [(pb-class? cls)]}
  (java/invoke-no-arg-class-method cls "getDescriptor"))

(defn- field-infos-raw [^Class cls]
  (let [desc (pb-class->descriptor cls)]
    (into {}
          (map (fn [^Descriptors$FieldDescriptor field-desc]
                 (let [field-key (field->key field-desc)
                       field-rule (cond (.isRepeated field-desc) :repeated
                                        (.isRequired field-desc) :required
                                        (.isOptional field-desc) :optional
                                        :else (assert false))
                       field-type-key (field-type->key (.getType field-desc))
                       default (when (not= Descriptors$FieldDescriptor$JavaType/MESSAGE (.getJavaType field-desc))
                                 (pb-primitive->clj (.getDefaultValue field-desc) field-type-key))
                       declared-default (when (.hasDefaultValue field-desc)
                                          default)]
                   (pair field-key
                         {:type (field-value-class cls field-desc)
                          :java-name (underscores-to-camel-case (.getName field-desc))
                          :field-type-key field-type-key
                          :field-rule field-rule
                          :default default
                          :declared-default declared-default
                          :options (options (.getOptions field-desc))}))))
          (.getFields desc))))

(def field-infos (fn/memoize field-infos-raw))

(defn resource-field? [field-info]
  (and (= :string (:field-type-key field-info))
       (true? (:resource (:options field-info)))))

(defn message-field? [field-info]
  (= :message (:field-type-key field-info)))

(def field-key-set
  "Returns the set of field keywords applicable to the supplied protobuf Class."
  (fn/memoize (comp set keys field-infos)))

(defn- declared-default [^Class cls field]
  (if-some [field-info (get (field-infos cls) field)]
    (:declared-default field-info)
    (throw (ex-info (format "Field '%s' does not exist in protobuf class '%s'."
                            field
                            (.getName cls))
                    {:pb-class cls
                     :field field}))))

(declare resource-field-path-specs)

(defn- resource-field-path-specs-raw
  "Returns a list of path expressions pointing out all resource fields.

  path-expr := '[' elem+ ']'
  elem := :keyword                  ; index into structure using :keyword
  elem := '{' :keyword default '}'  ; index into structure using :keyword, with default value to use when not specified
  elem := '[' :keyword ']'          ; :keyword is a repeated field, use rest of step* (if any) to index into each repetition (a message)

  message ResourceSimple {
      optional string image = 1 [(resource) = true];
  }

  message ResourceDefaulted {
      optional string image = 1 [(resource) = true, default = '/default.png'];
  }

  message ResourceRepeated {
      repeated string images = 1 [(resource) = true];
  }

  message ResourceSimpleNested {
      optional ResourceSimple simple = 1;
  }

  message ResourceDefaultedNested {
      optional ResourceDefaulted defaulted = 1;
  }

  message ResourceRepeatedNested {
      optional ResourceRepeated repeated = 1;
  }

  message ResourceSimpleRepeatedlyNested {
      repeated ResourceSimple simples = 1;
  }

  message ResourceDefaultedRepeatedlyNested {
      repeated ResourceDefaulted defaulteds = 1;
  }

  message ResourceRepeatedRepeatedlyNested {
      repeated ResourceRepeated repeateds = 1;
  }

  (resource-field-path-specs-raw ResourceSimple)    => [ [:image] ]
  (resource-field-path-specs-raw ResourceDefaulted) => [ [{:image '/default.png'}] ]
  (resource-field-path-specs-raw ResourceRepeated)  => [ [[:images]] ]

  (resource-field-path-specs-raw ResourceSimpleNested)    => [ [:simple :image] ]
  (resource-field-path-specs-raw ResourceDefaultedNested) => [ [:defaulted {:image '/default.png'}] ]
  (resource-field-path-specs-raw ResourceRepeatedNested)  => [ [:repeated [:images]] ]

  (resource-field-path-specs-raw ResourceSimpleRepeatedlyNested)    => [ [[:simples] :image] ]
  (resource-field-path-specs-raw ResourceDefaultedRepeatedlyNested) => [ [[:defaulteds] {:image '/default.png'}] ]
  (resource-field-path-specs-raw ResourceRepeatedRepeatedlyNested)  => [ [[:repeateds] [:images]] ]"
  [^Class class]
  (into []
        (comp
          (map (fn [[key field-info]]
                 (cond
                   (resource-field? field-info)
                   (case (:field-rule field-info)
                     :repeated [[[key]]]
                     :required [[key]]
                     :optional (if-some [field-default (declared-default class key)]
                                 [[{key field-default}]]
                                 [[key]]))

                   (message-field? field-info)
                   (let [sub-paths (resource-field-path-specs (:type field-info))]
                     (when (seq sub-paths)
                       (let [prefix (if (= :repeated (:field-rule field-info))
                                      [[key]]
                                      [key])]
                         (mapv (partial into prefix) sub-paths)))))))
          cat
          (remove nil?))
        (field-infos class)))

(def resource-field-path-specs (fn/memoize resource-field-path-specs-raw))

(declare get-field-fn)

(defn- get-field-fn-raw [field-path-spec]
  (if (seq field-path-spec)
    (let [elem (first field-path-spec)
          sub-path-fn (get-field-fn (rest field-path-spec))]
      (cond
        (keyword? elem)
        (fn [pb]
          (sub-path-fn (elem pb)))

        (map? elem)
        (fn [pb]
          (let [[key default] (first elem)]
            (sub-path-fn (get pb key default))))

        (vector? elem)
        (let [pbs-fn (first elem)]
          (fn [pb]
            (into [] (mapcat sub-path-fn) (pbs-fn pb))))))
    (fn [pb] [pb])))

(def ^:private get-field-fn (fn/memoize get-field-fn-raw))

(defn- get-fields-fn-raw [field-path-specs]
  (let [get-field-fns (mapv get-field-fn field-path-specs)]
    (fn [pb]
      (into []
            (mapcat (fn [get-fn]
                      (get-fn pb)))
            get-field-fns))))

(def get-fields-fn (fn/memoize get-fields-fn-raw))

(declare get-field-value-path-fn)

(defn- get-field-value-path-fn-raw [field-path-spec]
  (let [field-path-spec-token (first field-path-spec)]
    (if (nil? field-path-spec-token)
      (fn [field-path field-value]
        [(pair field-path field-value)])
      (let [sub-path-fn (get-field-value-path-fn (rest field-path-spec))]
        (cond
          (keyword? field-path-spec-token)
          (fn [field-path pb-map]
            (let [field-value (get pb-map field-path-spec-token ::not-found)]
              (when (not= ::not-found field-value)
                (let [field-path (conj field-path field-path-spec-token)]
                  (sub-path-fn field-path field-value)))))

          (map? field-path-spec-token)
          (fn [field-path pb-map]
            (let [[field default-value] (first field-path-spec-token)
                  field-path (conj field-path field)
                  field-value (or (get pb-map field) default-value)]
              (sub-path-fn field-path field-value)))

          (vector? field-path-spec-token)
          (let [field (first field-path-spec-token)]
            (fn [field-path pb-map]
              (let [field-path (conj field-path field)
                    repeated-field-values (get pb-map field)]
                (into []
                      (coll/mapcat-indexed
                        (fn [index repeated-field-value]
                          (let [field-path (conj field-path index)]
                            (sub-path-fn field-path repeated-field-value))))
                      repeated-field-values)))))))))

(def ^:private get-field-value-path-fn (fn/memoize get-field-value-path-fn-raw))

(defn- get-field-value-paths-fn-raw [field-path-specs]
  (let [get-field-value-path-fns (mapv get-field-value-path-fn field-path-specs)]
    (fn [pb-map]
      {:pre [(map? pb-map)]} ;; Protobuf Message in map format.
      (into []
            (mapcat (fn [get-fn]
                      (get-fn [] pb-map)))
            get-field-value-path-fns))))

(def get-field-value-paths-fn (fn/memoize get-field-value-paths-fn-raw))

(defn get-field-value-paths [pb-map pb-class]
  ((get-field-value-paths-fn (resource-field-path-specs pb-class)) pb-map))

(defn- make-pb->clj-fn [fields]
  (fn pb->clj [pb]
    (->> fields
         (reduce (fn [pb-map [field-key pb->field-value]]
                   (if-some [field-value (pb->field-value pb)]
                     (assoc! pb-map field-key field-value)
                     pb-map))
                 (transient {}))
         (persistent!)
         (msg->clj pb))))

(defn- default-message-raw [^Class class default-included-field-rules]
  (let [pb->clj (pb->clj-fn class default-included-field-rules)]
    (pb->clj (default-instance class))))

(def default-message (fn/memoize default-message-raw))

(defn- pb->clj-fn-raw [^Class class default-included-field-rules]
  {:pre [(set? default-included-field-rules)
         (every? #{:optional :required} default-included-field-rules)]}
  (make-pb->clj-fn
    (mapv (fn [[field-key {:keys [field-rule java-name] :as field-info}]]
            (let [pb-value->clj (pb-value->clj-fn field-info default-included-field-rules)
                  repeated (= :repeated field-rule)
                  ^Method field-get-method (field-get-method class java-name repeated)
                  include-defaults (contains? default-included-field-rules field-rule)]
              (pair field-key
                    (cond
                      (and include-defaults
                           (= :optional field-rule)
                           (message-field? field-info))
                      (let [field-has-value? (field-has-value-fn class java-name repeated)
                            default-field-value (default-message (:type field-info) default-included-field-rules)]
                        (fn pb->field-clj-value [pb]
                          (if (field-has-value? pb)
                            (let [field-pb-value (.invoke field-get-method pb java/no-args-array)]
                              (pb-value->clj field-pb-value))
                            default-field-value)))

                      (or repeated
                          include-defaults)
                      (fn pb->field-clj-value-or-default [pb]
                        (let [field-pb-value (.invoke field-get-method pb java/no-args-array)]
                          (pb-value->clj field-pb-value)))

                      :else
                      (let [field-has-value? (field-has-value-fn class java-name repeated)]
                        (fn pb->field-clj-value [pb]
                          (when (field-has-value? pb)
                            (let [field-pb-value (.invoke field-get-method pb java/no-args-array)]
                              (pb-value->clj field-pb-value)))))))))
          (field-infos class))))

(def ^:private pb->clj-fn (fn/memoize pb->clj-fn-raw))

(def ^:private pb->clj-with-defaults-fn (fn/memoize #(pb->clj-fn % #{:optional :required})))

(def ^:private pb->clj-without-defaults-fn (fn/memoize #(pb->clj-fn % #{})))

(defn- clear-defaults-from-builder! [^Message$Builder builder]
  (reduce (fn [is-default ^Descriptors$FieldDescriptor field-desc]
            (if (= Descriptors$FieldDescriptor$JavaType/MESSAGE (.getJavaType field-desc))
              ;; Message field.
              (cond
                (.isOptional field-desc)
                (if-not (.hasField builder field-desc)
                  is-default
                  (let [field-builder (.toBuilder ^Message (.getField builder field-desc))
                        field-is-default (clear-defaults-from-builder! field-builder)]
                    (if field-is-default
                      (do
                        (.clearField builder field-desc)
                        is-default)
                      (do
                        (.setField builder field-desc (.build field-builder))
                        false))))

                (.isRequired field-desc)
                (let [field-builder (.toBuilder ^Message (.getField builder field-desc))
                      field-is-default (clear-defaults-from-builder! field-builder)]
                  (.setField builder field-desc (.build field-builder))
                  (and is-default field-is-default))

                (.isRepeated field-desc)
                (let [item-count (.getRepeatedFieldCount builder field-desc)]
                  (doseq [index (range item-count)]
                    (let [item-builder (.toBuilder ^Message (.getRepeatedField builder field-desc index))]
                      (clear-defaults-from-builder! item-builder)
                      (.setRepeatedField builder field-desc index (.build item-builder))))
                  (and is-default (zero? item-count)))

                :else
                is-default)

              ;; Non-message field.
              (cond
                (.isOptional field-desc)
                (cond
                  (not (.hasField builder field-desc))
                  is-default

                  (let [is-resource-field (-> field-desc (.getOptions) (.getField resource-desc))
                        default-value (.getDefaultValue field-desc)]
                    (if is-resource-field
                      (and (= "" default-value)
                           (= "" (.getField builder field-desc)))
                      (= default-value (.getField builder field-desc))))
                  (do
                    (.clearField builder field-desc)
                    is-default)

                  :else
                  false)

                (.isRequired field-desc)
                (and is-default (= (.getDefaultValue field-desc) (.getField builder field-desc)))

                (.isRepeated field-desc)
                (and is-default (zero? (.getRepeatedFieldCount builder field-desc)))

                :else
                is-default)))
          true
          (.getFields (.getDescriptorForType builder))))

(defn- clear-defaults-from-message
  ^Message [^Message message]
  (let [builder (.toBuilder message)]
    (clear-defaults-from-builder! builder)
    (.build builder)))

(defn pb->map-with-defaults
  [^Message pb]
  (let [pb->clj-with-defaults (pb->clj-with-defaults-fn (.getClass pb))]
    (pb->clj-with-defaults pb)))

(defn pb->map-without-defaults
  [^Message pb]
  (let [pb->clj-without-defaults (pb->clj-without-defaults-fn (.getClass pb))]
    (pb->clj-without-defaults (clear-defaults-from-message pb))))

(defn- default-value-raw [^Class cls]
  (default-message cls #{:optional}))

(def default-value (fn/memoize default-value-raw))

(defn- required-default-value-raw [^Class cls]
  (default-message cls #{:required}))

(def required-default-value (fn/memoize required-default-value-raw))

(defn default
  ([^Class cls field]
   (let [field-default (get (default-value cls) field ::not-found)]
     (if (not= ::not-found field-default)
       field-default
       (throw (ex-info (format "Field '%s' does not have a default in protobuf class '%s'."
                               field
                               (.getName cls))
                       {:pb-class cls
                        :field field})))))
  ([^Class cls field not-found]
   (get (default-value cls) field not-found)))

(defn required-default [^Class cls field]
  (let [field-default (get (required-default-value cls) field ::not-found)]
    (if (not= ::not-found field-default)
      field-default
      (throw (ex-info (format "Field '%s' is not required in protobuf class '%s'."
                              field
                              (.getName cls))
                      {:pb-class cls
                       :field field})))))

(defn- desc->proto-cls ^Class [desc]
  (let [cls-name (if-let [containing (containing-type desc)]
                   (str (.getName (desc->proto-cls containing)) "$" (desc-name desc))
                   (let [file (file desc)
                         options (.getOptions file)
                         package (if (.hasJavaPackage options)
                                   (.getJavaPackage options)
                                   (let [full (full-name desc)
                                         name (desc-name desc)]
                                     (subs full 0 (- (count full) (count name) 1))))
                         outer-cls (if (.hasJavaOuterClassname options)
                                     (.getJavaOuterClassname options)
                                     (->CamelCase (FilenameUtils/getBaseName (.getName file))))
                         inner-cls (desc-name desc)]
                     (str package "." outer-cls "$" inner-cls)))]
    (java/load-class! cls-name)))

(defn val->pb-enum [^Class enum-class val]
  (Enum/valueOf enum-class (keyword->enum-name val)))

(defn- int-from-clj [value]
  (int (if (instance? Boolean value)
         (boolean->int value)
         value)))

(defn- boolean-from-clj [value]
  ;; The reason we convert to Boolean object is for symmetry - the protobuf
  ;; system does this when loading from protobuf files.
  (Boolean/valueOf (boolean value)))

(defn- enum-from-clj-fn-raw [^Class enum-cls]
  (fn enum-from-clj [value]
    (val->pb-enum enum-cls value)))

(def ^:private enum-from-clj-fn (fn/memoize enum-from-clj-fn-raw))

(defn- primitive-builder [^Descriptors$FieldDescriptor desc]
  (let [type (.getJavaType desc)]
    (cond
      (= type Descriptors$FieldDescriptor$JavaType/INT) int-from-clj
      (= type Descriptors$FieldDescriptor$JavaType/LONG) long
      (= type Descriptors$FieldDescriptor$JavaType/FLOAT) float
      (= type Descriptors$FieldDescriptor$JavaType/DOUBLE) double
      (= type Descriptors$FieldDescriptor$JavaType/STRING) str
      (= type Descriptors$FieldDescriptor$JavaType/BOOLEAN) boolean-from-clj
      (= type Descriptors$FieldDescriptor$JavaType/BYTE_STRING) identity
      (= type Descriptors$FieldDescriptor$JavaType/ENUM) (let [enum-cls (desc->proto-cls (.getEnumType desc))]
                                                           (enum-from-clj-fn enum-cls))
      :else nil)))

(declare ^:private pb-builder ^:private vector-to-map-conversions)

(defn- pb-builder-raw [^Class class]
  (let [desc (pb-class->descriptor class)
        ^Method new-builder-method (java/get-declared-method class "newBuilder" [])
        builder-class (.getReturnType new-builder-method)
        ;; All methods relevant to us
        methods (into {}
                      (keep (fn [^Method m]
                              (let [m-name (.getName m)
                                    set-via-builder? (and (.startsWith m-name "set")
                                                          (when-some [^Class first-arg-class (first (.getParameterTypes m))]
                                                            (.endsWith (.getName first-arg-class) "$Builder")))]
                                (when (not set-via-builder?)
                                  (pair m-name m)))))
                      (java/get-declared-methods builder-class))
        field-descs (.getFields desc)
        setters (into {}
                      (map (fn [^Descriptors$FieldDescriptor fd]
                             (let [j-name (->CamelCase (.getName fd))
                                   repeated (.isRepeated fd)
                                   ^Method field-set-method (get methods (str (if repeated "addAll" "set") j-name))
                                   field-builder (if (= (.getJavaType fd) Descriptors$FieldDescriptor$JavaType/MESSAGE)
                                                   (let [^Method field-get-method (get methods (str "get" j-name))]
                                                     (pb-builder (.getReturnType field-get-method)))
                                                   (primitive-builder fd))
                                   field-key (field->key fd)
                                   value-fn (if repeated
                                              #(mapv field-builder %)
                                              field-builder)
                                   value-fn (if-not decorate-protobuf-exceptions
                                              value-fn
                                              (fn decorated-value-fn [clj-value]
                                                (try
                                                  (value-fn clj-value)
                                                  (catch Exception cause
                                                    (throw
                                                      (ex-info
                                                        (format "Failed to assign protobuf field %s ('%s') from value: %s"
                                                                field-key
                                                                (.getFullName fd)
                                                                clj-value)
                                                        {:pb-class class
                                                         :pb-field field-key
                                                         :clj-value clj-value}
                                                        cause))))))]
                               (pair field-key
                                     (fn setter! [^Message$Builder b v]
                                       (let [value (value-fn v)]
                                         (.invoke field-set-method b (object-array [value]))))))))
                      field-descs)
        builder-fn (if enforce-strict-pb-map-keys
                     (fn builder-fn [pb-map]
                       (let [builder (new-builder class)]
                         (doseq [[field-key clj-value] pb-map
                                 :when (some? clj-value)
                                 :let [setter! (get setters field-key)]]
                           (if setter!
                             (setter! builder clj-value)
                             (throw
                               (ex-info
                                 (format "Failed to assign unknown protobuf field %s from value: %s"
                                         field-key
                                         clj-value)
                                 {:pb-class class
                                  :pb-field field-key
                                  :pb-field-candidates (vec (sort (keys setters)))
                                  :clj-value clj-value}))))
                         (.build builder)))
                     (fn builder-fn [pb-map]
                       (let [builder (new-builder class)]
                         (doseq [[field-key clj-value] pb-map
                                 :when (some? clj-value)
                                 :let [setter! (get setters field-key)]
                                 :when setter!]
                           (setter! builder clj-value))
                         (.build builder))))]
    (if-some [vector->map (vector-to-map-conversions class)]
      (comp builder-fn vector->map)
      builder-fn)))

(def ^:private pb-builder (fn/memoize pb-builder-raw))

(defmacro map->pb [^Class cls m]
  (cond-> `((#'pb-builder ~cls) ~m)
          (class? (ns-resolve *ns* cls))
          (with-meta {:tag cls})))

(defmacro str->pb [^Class cls str]
  (cond-> `(TextFormat/parse ~str ~cls)
          (class? (resolve cls))
          (with-meta {:tag cls})))

(defn- break-embedded-newlines
  [^String pb-str]
  (.replace pb-str "\\n" "\\n\"\n  \""))

(defn pb->str [^Message pb format-newlines?]
  (cond-> (.printToString (TextFormat/printer) pb)
          format-newlines?
          (break-embedded-newlines)))

(defn pb->bytes [^Message pb]
  (let [out (ByteArrayOutputStream. (* 4 1024))]
    (.writeTo pb out)
    (.close out)
    (.toByteArray out)))

(def float-zero (Float/valueOf (float 0.0)))
(def float-one (Float/valueOf (float 1.0)))

(def vector3-zero [float-zero float-zero float-zero])
(def vector3-one [float-one float-one float-one])

(def vector4-zero [float-zero float-zero float-zero float-zero])
(def vector4-one [float-one float-one float-one float-one])
(def vector4-xyz-zero-w-one [float-zero float-zero float-zero float-one])
(def vector4-xyz-one-w-zero [float-one float-one float-one float-zero])

(def quat-identity [float-zero float-zero float-zero float-one])

(def matrix4-identity
  [float-one float-zero float-zero float-zero
   float-zero float-one float-zero float-zero
   float-zero float-zero float-one float-zero
   float-zero float-zero float-zero float-one])

(defn vector3->vector4-zero [vector3]
  (conj vector3 float-zero))

(defn vector3->vector4-one [vector3]
  (conj vector3 float-one))

(defn vector4->vector3 [vector4]
  (subvec vector4 0 3))

(defn sanitize-required-vector4-zero-as-vector3 [vector4]
  (let [vector3 (vector4->vector3 vector4)]
    (vector3->vector4-zero vector3)))

(defn sanitize-optional-vector4-zero-as-vector3 [vector4]
  (let [vector3 (vector4->vector3 vector4)]
    (when (not-every? zero? vector3)
      (vector3->vector4-zero vector3))))

(defn sanitize-required-vector4-one-as-vector3 [vector4]
  (let [vector3 (vector4->vector3 vector4)]
    (vector3->vector4-one vector3)))

(defn sanitize-optional-vector4-one-as-vector3 [vector4]
  (let [vector3 (vector4->vector3 vector4)]
    (when (not-every? #(= float-one %) vector3)
      (vector3->vector4-one vector3))))

(definline intern-float [num]
  `(let [float# ~num]
     (cond
       (= float-zero float#) float-zero
       (= float-one float#) float-one
       :else float#)))

(extend-protocol PbConverter
  DdfMath$Point3
  (msg->vecmath [_pb v] (Point3d. (:x v float-zero) (:y v float-zero) (:z v float-zero)))
  (msg->clj [_pb v]
    (let [x (intern-float (:x v float-zero))
          y (intern-float (:y v float-zero))
          z (intern-float (:z v float-zero))]
      (if (and (identical? float-zero x)
               (identical? float-zero y)
               (identical? float-zero z))
        vector3-zero
        [x y z])))

  DdfMath$Vector3
  (msg->vecmath [_pb v] (Vector3d. (:x v float-zero) (:y v float-zero) (:z v float-zero)))
  (msg->clj [_pb v]
    (let [x (intern-float (:x v float-zero))
          y (intern-float (:y v float-zero))
          z (intern-float (:z v float-zero))]
      (cond
        (and (identical? float-zero x)
             (identical? float-zero y)
             (identical? float-zero z))
        vector3-zero

        (and (identical? float-one x)
             (identical? float-one y)
             (identical? float-one z))
        vector3-one

        :else
        [x y z])))

  DdfMath$Vector3One
  (msg->vecmath [_pb v] (Vector3d. (:x v float-one) (:y v float-one) (:z v float-one)))
  (msg->clj [_pb v]
    (let [x (intern-float (:x v float-one))
          y (intern-float (:y v float-one))
          z (intern-float (:z v float-one))]
      (cond
        (and (identical? float-one x)
             (identical? float-one y)
             (identical? float-one z))
        vector3-one

        (and (identical? float-zero x)
             (identical? float-zero y)
             (identical? float-zero z))
        vector3-zero

        :else
        [x y z])))

  DdfMath$Vector4
  (msg->vecmath [_pb v] (Vector4d. (:x v float-zero) (:y v float-zero) (:z v float-zero) (:w v float-zero)))
  (msg->clj [_pb v]
    (let [x (intern-float (:x v float-zero))
          y (intern-float (:y v float-zero))
          z (intern-float (:z v float-zero))
          w (intern-float (:w v float-zero))]
      (cond
        (and (identical? float-zero x)
             (identical? float-zero y)
             (identical? float-zero z)
             (identical? float-zero w))
        vector4-zero

        (and (identical? float-one x)
             (identical? float-one y)
             (identical? float-one z)
             (identical? float-one w))
        vector4-one

        (and (identical? float-zero x)
             (identical? float-zero y)
             (identical? float-zero z)
             (identical? float-one w))
        vector4-xyz-zero-w-one

        (and (identical? float-one x)
             (identical? float-one y)
             (identical? float-one z)
             (identical? float-zero w))
        vector4-xyz-one-w-zero

        :else
        [x y z w])))

  DdfMath$Vector4One
  (msg->vecmath [_pb v] (Vector4d. (:x v float-one) (:y v float-one) (:z v float-one) (:w v float-one)))
  (msg->clj [_pb v]
    (let [x (intern-float (:x v float-one))
          y (intern-float (:y v float-one))
          z (intern-float (:z v float-one))
          w (intern-float (:w v float-one))]
      (cond
        (and (identical? float-one x)
             (identical? float-one y)
             (identical? float-one z)
             (identical? float-one w))
        vector4-one

        (and (identical? float-zero x)
             (identical? float-zero y)
             (identical? float-zero z)
             (identical? float-zero w))
        vector4-zero

        (and (identical? float-one x)
             (identical? float-one y)
             (identical? float-one z)
             (identical? float-zero w))
        vector4-xyz-one-w-zero

        (and (identical? float-zero x)
             (identical? float-zero y)
             (identical? float-zero z)
             (identical? float-one w))
        vector4-xyz-zero-w-one

        :else
        [x y z w])))

  DdfMath$Vector4WOne
  (msg->vecmath [_pb v] (Vector4d. (:x v float-zero) (:y v float-zero) (:z v float-zero) (:w v float-one)))
  (msg->clj [_pb v]
    (let [x (intern-float (:x v float-zero))
          y (intern-float (:y v float-zero))
          z (intern-float (:z v float-zero))
          w (intern-float (:w v float-one))]
      (cond
        (and (identical? float-zero x)
             (identical? float-zero y)
             (identical? float-zero z)
             (identical? float-one w))
        vector4-xyz-zero-w-one

        (and (identical? float-one x)
             (identical? float-one y)
             (identical? float-one z)
             (identical? float-one w))
        vector4-one

        (and (identical? float-zero x)
             (identical? float-zero y)
             (identical? float-zero z)
             (identical? float-zero w))
        vector4-zero

        (and (identical? float-one x)
             (identical? float-one y)
             (identical? float-one z)
             (identical? float-zero w))
        vector4-xyz-one-w-zero

        :else
        [x y z w])))

  DdfMath$Quat
  (msg->vecmath [_pb v] (Quat4d. (:x v float-zero) (:y v float-zero) (:z v float-zero) (:w v float-one)))
  (msg->clj [_pb v]
    (let [x (intern-float (:x v float-zero))
          y (intern-float (:y v float-zero))
          z (intern-float (:z v float-zero))
          w (intern-float (:w v float-one))]
      (if (and (identical? float-zero x)
               (identical? float-zero y)
               (identical? float-zero z)
               (identical? float-one w))
        quat-identity
        [x y z w])))

  DdfMath$Matrix4
  (msg->vecmath [_pb v]
    (Matrix4d. (:m00 v float-one) (:m01 v float-zero) (:m02 v float-zero) (:m03 v float-zero)
               (:m10 v float-zero) (:m11 v float-one) (:m12 v float-zero) (:m13 v float-zero)
               (:m20 v float-zero) (:m21 v float-zero) (:m22 v float-one) (:m23 v float-zero)
               (:m30 v float-zero) (:m31 v float-zero) (:m32 v float-zero) (:m33 v float-one)))
  (msg->clj [_pb v]
    (let [m00 (intern-float (:m00 v float-one))
          m01 (intern-float (:m01 v float-zero))
          m02 (intern-float (:m02 v float-zero))
          m03 (intern-float (:m03 v float-zero))
          m10 (intern-float (:m10 v float-zero))
          m11 (intern-float (:m11 v float-one))
          m12 (intern-float (:m12 v float-zero))
          m13 (intern-float (:m13 v float-zero))
          m20 (intern-float (:m20 v float-zero))
          m21 (intern-float (:m21 v float-zero))
          m22 (intern-float (:m22 v float-one))
          m23 (intern-float (:m23 v float-zero))
          m30 (intern-float (:m30 v float-zero))
          m31 (intern-float (:m31 v float-zero))
          m32 (intern-float (:m32 v float-zero))
          m33 (intern-float (:m33 v float-one))]
      (if (and (identical? float-one m00)
               (identical? float-zero m01)
               (identical? float-zero m02)
               (identical? float-zero m03)

               (identical? float-zero m10)
               (identical? float-one m11)
               (identical? float-zero m12)
               (identical? float-zero m13)

               (identical? float-zero m20)
               (identical? float-zero m21)
               (identical? float-one m22)
               (identical? float-zero m23)

               (identical? float-zero m30)
               (identical? float-zero m31)
               (identical? float-zero m32)
               (identical? float-one m33))
        matrix4-identity
        [m00 m01 m02 m03
         m10 m11 m12 m13
         m20 m21 m22 m23
         m30 m31 m32 m33])))

  Message
  (msg->vecmath [_pb v] v)
  (msg->clj [_pb v] v))

(extend-protocol GenericDescriptor
  Descriptors$Descriptor
  (proto [this] (.toProto this))
  (desc-name [this] (.getName this))
  (full-name [this] (.getFullName this))
  (file [this] (.getFile this))
  (containing-type [this] (.getContainingType this))

  Descriptors$EnumDescriptor
  (proto [this] (.toProto this))
  (desc-name [this] (.getName this))
  (full-name [this] (.getFullName this))
  (file [this] (.getFile this))
  (containing-type [this] (.getContainingType this)))

(defn map->str
  ([^Class cls m] (map->str cls m true))
  ([^Class cls m format-newlines?]
   (pb->str (map->pb cls m) format-newlines?)))

(defn map->bytes [^Class cls m]
  (pb->bytes (map->pb cls m)))

(defn read-pb-into!
  ^Message$Builder [^Message$Builder builder input]
  (with-open [reader (io/reader input)]
    (TextFormat/merge reader builder)
    builder))

(defmacro read-pb [^Class cls input]
  (cond-> `(.build (read-pb-into! (#'new-builder ~cls) ~input))
          (class? (resolve cls))
          (with-meta {:tag cls})))

(defn str->map-with-defaults [^Class cls ^String str]
  (pb->map-with-defaults
    (with-open [reader (StringReader. str)]
      (read-pb cls reader))))

(defn str->map-without-defaults [^Class cls ^String str]
  (pb->map-without-defaults
    (with-open [reader (StringReader. str)]
      (read-pb cls reader))))

(defonce ^:private single-byte-array-args [java/byte-array-class])

(defn- parser-fn-raw [^Class cls]
  (let [^Method parse-method (java/get-declared-method cls "parseFrom" single-byte-array-args)]
    (fn parser-fn [^bytes bytes]
      (.invoke parse-method nil (object-array [bytes])))))

(def parser-fn (fn/memoize parser-fn-raw))

(defmacro bytes->pb [^Class cls bytes]
  (cond-> `((parser-fn ~cls) ~bytes)
          (class? (resolve cls))
          (with-meta {:tag cls})))

(defn bytes->map-with-defaults [^Class cls bytes]
  (let [parser (parser-fn cls)]
    (-> bytes
        parser
        pb->map-with-defaults)))

(defn bytes->map-without-defaults [^Class cls bytes]
  (let [parser (parser-fn cls)]
    (-> bytes
        parser
        pb->map-without-defaults)))

(defn- protocol-message-enums-raw [^Class enum-class]
  (vec (java/invoke-no-arg-class-method enum-class "values")))

(def protocol-message-enums (fn/memoize protocol-message-enums-raw))

(defn- enum-values-raw [^Class enum-class]
  (mapv (fn [^ProtocolMessageEnum value]
          (pair (pb-enum->val value)
                {:display-name (-> (.getValueDescriptor value) (.getOptions) (.getExtension DdfExtensions/displayName))}))
        (protocol-message-enums enum-class)))

(def enum-values (fn/memoize enum-values-raw))

(defn- valid-enum-values-raw [^Class enum-class]
  (into (sorted-set)
        (map pb-enum->val)
        (protocol-message-enums enum-class)))

(def valid-enum-values (fn/memoize valid-enum-values-raw))

(defn- fields-by-indices-raw [^Class cls]
  (let [desc (pb-class->descriptor cls)]
    (into {}
          (map (fn [^Descriptors$FieldDescriptor field]
                 (pair (.getNumber field)
                       (field->key field))))
          (.getFields desc))))

(def fields-by-indices (fn/memoize fields-by-indices-raw))

(defn pb->hash
  ^bytes [^String algorithm ^Message pb]
  (with-open [digest-output-stream (digest/make-digest-output-stream algorithm)]
    (.writeTo pb digest-output-stream)
    (.digest (.getMessageDigest digest-output-stream))))

(defn map->sha1-hex
  ^String [^Class cls m]
  (digest/bytes->hex (pb->hash "SHA-1" (map->pb cls m))))

(def ^:private vector-to-map-conversions
  (->> {DdfMath$Point3 [:x :y :z]
        DdfMath$Vector3 [:x :y :z]
        DdfMath$Vector3One [:x :y :z]
        DdfMath$Vector4 [:x :y :z :w]
        DdfMath$Vector4One [:x :y :z :w]
        DdfMath$Vector4WOne [:x :y :z :w]
        DdfMath$Quat [:x :y :z :w]
        DdfMath$Matrix4 [:m00 :m01 :m02 :m03
                         :m10 :m11 :m12 :m13
                         :m20 :m21 :m22 :m23
                         :m30 :m31 :m32 :m33]}
       (into {}
             (map (fn [[^Class cls component-keys]]
                    (let [default-values (default-value cls)]
                      (pair cls
                            (fn vector->map [component-values]
                              (->> component-keys
                                   (reduce-kv
                                     (fn [result index key]
                                       (let [value (component-values index)]
                                         (if (= (default-values index) value)
                                           result
                                           (assoc! result key value))))
                                     (transient {}))
                                   (persistent!))))))))))

(defn make-map-with-defaults [^Class cls & kvs]
  (into (default-value cls)
        (comp (partition-all 2)
              (keep (fn [[key value :as entry]]
                      (cond
                        (nil? value)
                        nil

                        (sequential? value)
                        (when-not (coll/empty? value)
                          (let [as-vec (vec value)]
                            (if (identical? value as-vec)
                              entry
                              (pair key as-vec))))

                        :else
                        entry))))
        kvs))

(defn- without-defaults-xform-raw [^Class cls]
  (let [key->field-info (field-infos cls)
        key->default (default-value cls)]
    (keep (fn [[key value :as entry]]
            (when (some? value)
              (let [field-info (key->field-info key)]
                (case (:field-rule field-info)
                  :optional
                  (when (if (resource-field? field-info)
                          (if (= "" value)
                            (not= "" (key->default key))
                            true)
                          (not (or (and (message-field? field-info)
                                        (coll/empty? value))
                                   (= (key->default key) value))))
                    entry)

                  :repeated
                  (when (and (not (coll/empty? value))
                             (not= (key->default key) value))
                    (let [as-vec (vec value)]
                      (if (identical? value as-vec)
                        entry
                        (pair key as-vec))))

                  :required
                  (when-not (and (message-field? field-info)
                                 (coll/empty? value))
                    entry)

                  (throw (ex-info (str "Invalid field " key " for protobuf class " (.getName cls))
                                  {:key key
                                   :pb-class cls})))))))))

(def without-defaults-xform (fn/memoize without-defaults-xform-raw))

(defn make-map-without-defaults [^Class cls & kvs]
  (into {}
        (comp (partition-all 2)
              (without-defaults-xform cls))
        kvs))

(defn inject-defaults [^Class cls pb-map]
  (pb->map-with-defaults (map->pb cls pb-map)))

(defn clear-defaults [^Class cls pb-map]
  (pb->map-without-defaults (map->pb cls pb-map)))

(defn read-map-with-defaults [^Class cls input]
  (pb->map-with-defaults
    (read-pb cls input)))

(defn read-map-without-defaults [^Class cls input]
  (pb->map-without-defaults
    (read-pb cls input)))

(defn assign
  ([pb-map field-kw value]
   {:pre [(map? pb-map)
          (keyword? field-kw)]}
   (if (or (nil? value)
           (and (map? value)
                (coll/empty? value)))
     (dissoc pb-map field-kw)
     (assoc pb-map field-kw value)))
  ([pb-map field-kw value & kvs]
   (coll/reduce-partitioned 2 assign (assign pb-map field-kw value) kvs)))

(defn assign-repeated
  ([pb-map field-kw items]
   {:pre [(map? pb-map)
          (keyword? field-kw)]}
   (if (coll/empty? items)
     (dissoc pb-map field-kw)
     (assoc pb-map field-kw (vec items))))
  ([pb-map field-kw items & kvs]
   (coll/reduce-partitioned 2 assign-repeated (assign-repeated pb-map field-kw items) kvs)))

(defn sanitize
  ([pb-map field-kw]
   {:pre [(map? pb-map)
          (keyword? field-kw)]}
   (let [value (get pb-map field-kw ::not-found)]
     (if (nil? value)
       (dissoc pb-map field-kw)
       pb-map)))
  ([pb-map field-kw sanitize-value-fn]
   {:pre [(map? pb-map)
          (keyword? field-kw)
          (ifn? sanitize-value-fn)]}
   (let [value (get pb-map field-kw ::not-found)]
     (if (= ::not-found value)
       pb-map
       (assign pb-map field-kw (some-> value sanitize-value-fn))))))

(defn sanitize-repeated
  ([pb-map field-kw]
   {:pre [(map? pb-map)
          (keyword? field-kw)]}
   (let [items (get pb-map field-kw ::not-found)]
     (if (or (= ::not-found items)
             (not (coll/empty? items)))
       pb-map
       (dissoc pb-map field-kw))))
  ([pb-map field-kw sanitize-item-fn]
   {:pre [(map? pb-map)
          (keyword? field-kw)
          (ifn? sanitize-item-fn)]}
   (let [items (get pb-map field-kw ::not-found)]
     (if (= ::not-found items)
       pb-map
       (assign-repeated pb-map field-kw (some->> items (into [] (keep sanitize-item-fn))))))))

(defn make-map-search-match-fn
  "Returns a function that takes a value and returns a text-match map augmented
  with the matching :value if its protobuf-text representation matches the
  provided search-string. This function is suitable for use with coll/search and
  its ilk on a protobuf message in map format."
  [search-string]
  (let [re-pattern (text-util/search-string->re-pattern search-string :case-insensitive)
        search-string-is-numeric (text-util/search-string-numeric? search-string)]
    (fn match-fn [value]
      (some-> (cond
                (string? value)
                value

                (keyword? value)
                (keyword->enum-name value)

                (and search-string-is-numeric (number? value))
                (str value)

                (boolean? value)
                (if value "true" "false"))

              (text-util/string->text-match re-pattern)
              (assoc :value value)))))
