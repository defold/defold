;; Copyright 2020-2023 The Defold Foundation
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
            [editor.util :as util]
            [editor.workspace :as workspace]
            [internal.java :as j]
            [util.coll :as coll :refer [pair]]
            [util.digest :as digest])
  (:import [com.dynamo.proto DdfExtensions DdfMath$Matrix4 DdfMath$Point3 DdfMath$Quat DdfMath$Vector3 DdfMath$Vector4]
           [com.google.protobuf DescriptorProtos$FieldOptions Descriptors$Descriptor Descriptors$EnumDescriptor Descriptors$EnumValueDescriptor Descriptors$FieldDescriptor Descriptors$FieldDescriptor$JavaType Descriptors$FieldDescriptor$Type Descriptors$FileDescriptor Message Message$Builder ProtocolMessageEnum TextFormat]
           [java.io ByteArrayOutputStream StringReader]
           [java.lang.reflect Method]
           [java.util Collection]
           [javax.vecmath Matrix4d Point3d Quat4d Vector3d Vector4d]
           [org.apache.commons.io FilenameUtils]))

(set! *warn-on-reflection* true)

(defprotocol GenericDescriptor
  (proto ^Message [this])
  (desc-name ^String [this])
  (full-name ^String [this])
  (file ^Descriptors$FileDescriptor [this])
  (containing-type ^Descriptors$Descriptor [this]))

(defprotocol PbConverter
  (msg->vecmath [^Message pb v] "Return the javax.vecmath equivalent for the Protocol Buffer message")
  (msg->clj [^Message pb v]))

(def ^:private upper-pattern (re-pattern #"\p{javaUpperCase}"))

(defn- default-instance-raw [^Class cls]
  (j/invoke-no-arg-class-method cls "getDefaultInstance"))

(def ^:private default-instance (memoize default-instance-raw))

(defn- new-builder
  ^Message$Builder [^Class cls]
  (j/invoke-no-arg-class-method cls "newBuilder"))

(defn- field-name->key-raw [^String field-name]
  (keyword (if (re-find upper-pattern field-name)
             (->kebab-case field-name)
             (string/replace field-name "_" "-"))))

(def field-name->key (memoize field-name->key-raw))

(defn- field->key [^Descriptors$FieldDescriptor field-desc]
  (field-name->key (.getName field-desc)))

(defn- enum-name->keyword-raw [^String enum-name]
  (keyword (util/lower-case* (string/replace enum-name "_" "-"))))

(def ^:private enum-name->keyword (memoize enum-name->keyword-raw))

(defn- keyword->enum-name-raw
  ^String [keyword]
  (.intern (string/replace (util/upper-case* (name keyword)) "-" "_")))

(def ^:private keyword->enum-name (memoize keyword->enum-name-raw))

(defn pb-enum->val
  [val-or-desc]
  (let [^Descriptors$EnumValueDescriptor desc (if (instance? ProtocolMessageEnum val-or-desc)
                                                (.getValueDescriptor ^ProtocolMessageEnum val-or-desc)
                                                val-or-desc)]
    (enum-name->keyword (.getName desc))))

(defn- pb-value->clj-fn [field-info pb->clj-fn]
  (let [pb-value->clj
        (case (:field-type-key field-info)
          :message (pb->clj-fn (:type field-info))
          :enum pb-enum->val
          (if (= :boolean (:java-type-key field-info))
            boolean
            identity))]
    (if (not= :repeated (:field-rule field-info))
      pb-value->clj
      (fn repeated-pb-value->clj [^Collection values]
        (when-not (.isEmpty values)
          (mapv pb-value->clj values))))))

(def ^:private methods-by-name
  (memoize
    (fn methods-by-name [^Class class]
      (into {}
            (map (fn [^Method m]
                   (pair (.getName m) m)))
            (j/get-declared-methods class)))))

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

(def underscores-to-camel-case (memoize underscores-to-camel-case-raw))

(defonce ^:private resource-desc (.getDescriptor DdfExtensions/resource))

(defn- options [^DescriptorProtos$FieldOptions field-options]
  (cond-> {}
          (.getField field-options resource-desc)
          (assoc :resource true)))

(defn- field-type [^Class class ^Descriptors$FieldDescriptor field-desc]
  (let [java-name (underscores-to-camel-case (.getName field-desc))
        field-accessor-name (str "get" java-name)]
    (.getReturnType (lookup-method class field-accessor-name))))

(def ^:private java-type->key
  {Descriptors$FieldDescriptor$JavaType/BOOLEAN :boolean
   Descriptors$FieldDescriptor$JavaType/BYTE_STRING :byte-string
   Descriptors$FieldDescriptor$JavaType/DOUBLE :double
   Descriptors$FieldDescriptor$JavaType/ENUM :enum
   Descriptors$FieldDescriptor$JavaType/FLOAT :float
   Descriptors$FieldDescriptor$JavaType/INT :int
   Descriptors$FieldDescriptor$JavaType/LONG :long
   Descriptors$FieldDescriptor$JavaType/MESSAGE :message
   Descriptors$FieldDescriptor$JavaType/STRING :string})

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

(def ^:private field-get-method (memoize field-get-method-raw))

(defn- field-has-value-fn
  ^Method [^Class cls java-name repeated]
  (if repeated
    (let [count-method-name (str "get" java-name "Count")
          count-method (lookup-method cls count-method-name)]
      (fn repeated-field-has-value? [pb]
        (pos? (.invoke count-method pb j/no-args-array))))
    (let [has-method-name (str "has" java-name)
          has-method (lookup-method cls has-method-name)]
      (fn field-has-value? [pb]
        ;; Wrapped in boolean because reflection will produce unique Boolean
        ;; instances that cannot evaluate to false in if-expressions.
        (boolean (.invoke has-method pb j/no-args-array))))))

(defn- field-infos-raw [^Class cls]
  (let [^Descriptors$Descriptor desc (j/invoke-no-arg-class-method cls "getDescriptor")]
    (into {}
          (map (fn [^Descriptors$FieldDescriptor field-desc]
                 (pair (field->key field-desc)
                       {:type (field-type cls field-desc)
                        :java-name (underscores-to-camel-case (.getName field-desc))
                        :java-type-key (java-type->key (.getJavaType field-desc))
                        :options (options (.getOptions field-desc))
                        :field-type-key (field-type->key (.getType field-desc))
                        :field-rule (cond (.isRepeated field-desc) :repeated
                                          (.isRequired field-desc) :required
                                          (.isOptional field-desc) :optional
                                          :else (assert false))})))
          (.getFields desc))))

(def ^:private field-infos (memoize field-infos-raw))

(declare default resource-field-paths)

(defn resource-field-paths-raw
  "Returns a list of path expressions pointing out all resource fields.

  path-expr := '[' elem+ ']'
  elem := :keyword          ; index into structure using :keyword
  elem := '[' :keyword ']'  ; :keyword is a repeated field, use rest of step* (if any) to index into each repetition (a message)

  message Simple
  {
    required string image = 1 [(resource) = true];
  }

  message Repeated
  {
    repeated string images = 1 [(resource) = true];
  }

  message SimpleNested
  {
    required Simple simple;
  }

  message RepeatedNested
  {
    required Repeated repeated;
  }

  message SimpleRepeatedlyNested
  {
    repeated Simple simples;
  }

  message RepeatedRepeatedlyNested
  {
    repeated Repeated repeateds;
  }

  (resource-field-paths-raw Simple) -> [ [:image] ]
  (resource-field-paths-raw Repeated) -> [ [[:images]] ]

  (resource-field-paths-raw SimpleNested) -> [ [:simple :image] ]
  (resource-field-paths-raw RepeatedNested) -> [ [:simple [:images]] ]

  (resource-field-paths-raw SimpleRepeatedlyNested) -> [ [[:simples] :image] ]
  (resource-field-paths-raw RepeatedRepeatedlyNested) -> [ [[:repeateds] [:images]] ]"
  [^Class class]
  (let [resource-field? (fn [field] (and (= (:type field) String) (:resource (:options field))))
        message-field? (fn [field] (= (:field-type-key field) :message))]
    (into []
          (comp
            (map (fn [[key field-info]]
                   (cond
                     (resource-field? field-info)
                     (if (= :repeated (:field-rule field-info))
                       [ [[key]] ]
                       (if-some [default-value (default class key)]
                         [ [{key default-value}] ]
                         [ [key] ]))

                     (message-field? field-info)
                     (let [sub-paths (resource-field-paths (:type field-info))]
                       (when (seq sub-paths)
                         (let [prefix (if (= :repeated (:field-rule field-info))
                                        [[key]]
                                        [key])]
                           (mapv (partial into prefix) sub-paths)))))))
            cat
            (remove nil?))
          (field-infos class))))

(def resource-field-paths (memoize resource-field-paths-raw))

(declare get-field-fn)

(defn- get-field-fn-raw [path]
  (if (seq path)
    (let [elem (first path)
          sub-path-fn (get-field-fn (rest path))]
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

(def ^:private get-field-fn (memoize get-field-fn-raw))

(defn- get-fields-fn-raw [paths]
  (let [get-field-fns (map get-field-fn paths)]
    (fn [pb]
      (into []
            (mapcat (fn [get-fn]
                      (get-fn pb)))
            get-field-fns))))


(def get-fields-fn (memoize get-fields-fn-raw))

(defn- pb->clj-fn [fields]
  (fn pb->clj [pb]
    (->> fields
         (reduce (fn [pb-map [field-key pb->field-value]]
                   (if-some [field-value (pb->field-value pb)]
                     (assoc! pb-map field-key field-value)
                     pb-map))
                 (transient {}))
         (persistent!)
         (msg->clj pb))))

(declare ^:private pb->clj-with-defaults-fn)

(defn- pb->clj-with-defaults-fn-raw [^Class class]
  (pb->clj-fn
    (mapv (fn [[field-key {:keys [field-rule java-name] :as field-info}]]
            (let [pb-value->clj (pb-value->clj-fn field-info pb->clj-with-defaults-fn)
                  repeated (= :repeated field-rule)
                  ^Method field-get-method (field-get-method class java-name repeated)]
              (pair field-key
                    (if (= :required field-rule)
                      (let [field-has-value? (field-has-value-fn class java-name false)]
                        (fn pb->field-clj-value [pb]
                          (when (field-has-value? pb)
                            (let [field-pb-value (.invoke field-get-method pb j/no-args-array)]
                              (pb-value->clj field-pb-value)))))
                      (fn pb->field-clj-value-or-default [pb]
                        (let [field-pb-value (.invoke field-get-method pb j/no-args-array)]
                          (pb-value->clj field-pb-value)))))))
          (field-infos class))))

(def ^:private pb->clj-with-defaults-fn (memoize pb->clj-with-defaults-fn-raw))

(declare ^:private pb->clj-without-defaults-fn)

(defn- pb->clj-without-defaults-fn-raw [^Class class]
  (pb->clj-fn
    (mapv (fn [[field-key {:keys [field-rule java-name] :as field-info}]]
            (let [pb-value->clj (pb-value->clj-fn field-info pb->clj-without-defaults-fn)
                  repeated (= :repeated field-rule)
                  ^Method field-get-method (field-get-method class java-name repeated)
                  field-has-value? (field-has-value-fn class java-name repeated)]
              (pair field-key
                    (fn pb->field-clj-value [pb]
                      (when (field-has-value? pb)
                        (let [field-pb-value (.invoke field-get-method pb j/no-args-array)]
                          (pb-value->clj field-pb-value)))))))
          (field-infos class))))

(def ^:private pb->clj-without-defaults-fn (memoize pb->clj-without-defaults-fn-raw))

(declare ^:private clear-defaults-from-message)

(defn- clear-defaults-from-builder!
  ^Message$Builder [^Message$Builder builder]
  (let [descriptor (.getDescriptorForType builder)
        field-descs (.getFields descriptor)]
    (doseq [^Descriptors$FieldDescriptor field-desc field-descs]
      (cond
        (and (.isOptional field-desc)
             (.hasField builder field-desc))
        (cond
          (= Descriptors$FieldDescriptor$JavaType/MESSAGE (.getJavaType field-desc))
          (let [^Message without-defaults (clear-defaults-from-message (.getField builder field-desc))]
            (if (zero? (.getSerializedSize without-defaults))
              (.clearField builder field-desc)
              (.setField builder field-desc without-defaults)))

          (= (.getDefaultValue field-desc) (.getField builder field-desc))
          (.clearField builder field-desc))

        (and (.isRepeated field-desc)
             (= Descriptors$FieldDescriptor$JavaType/MESSAGE (.getJavaType field-desc)))
        (doseq [index (range (.getRepeatedFieldCount builder field-desc))]
          (let [item-with-defaults (.getRepeatedField builder field-desc index)
                item-without-defaults (clear-defaults-from-message item-with-defaults)]
            (.setRepeatedField builder field-desc index item-without-defaults)))))
    builder))

(defn- clear-defaults-from-message
  ^Message [^Message message]
  (-> message
      (.toBuilder)
      (clear-defaults-from-builder!)
      (.build)))

(defn pb->map-with-defaults
  [^Message pb]
  (let [pb->clj-with-defaults (pb->clj-with-defaults-fn (.getClass pb))]
    (pb->clj-with-defaults pb)))

(defn pb->map-without-defaults
  [^Message pb]
  (let [pb->clj-without-defaults (pb->clj-without-defaults-fn (.getClass pb))]
    (pb->clj-without-defaults (clear-defaults-from-message pb))))

(defn- default-map-raw [^Class cls]
  (-> cls
      (default-instance)
      (pb->map-with-defaults)))

(def ^:private default-map (memoize default-map-raw))

(defn default [^Class cls field]
  (get (default-map cls) field))

(def ^:private math-type-keys
  {"Point3" [:x :y :z]
   "Scale3" [:x :y :z]
   "Vector3" [:x :y :z]
   "Vector4" [:x :y :z :w]
   "Quat" [:x :y :z :w]
   "Matrix4" [:m00 :m01 :m02 :m03
              :m10 :m11 :m12 :m13
              :m20 :m21 :m22 :m23
              :m30 :m31 :m32 :m33]})

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
    (workspace/load-class! cls-name)))

(defn- primitive-builder [^Descriptors$FieldDescriptor desc]
  (let [type (.getJavaType desc)]
    (cond
      (= type Descriptors$FieldDescriptor$JavaType/INT) (fn [v]
                                                            (int (if (instance? Boolean v)
                                                                   (if v 1 0)
                                                                   v)))
      (= type Descriptors$FieldDescriptor$JavaType/LONG) long
      (= type Descriptors$FieldDescriptor$JavaType/FLOAT) float
      (= type Descriptors$FieldDescriptor$JavaType/DOUBLE) double
      (= type Descriptors$FieldDescriptor$JavaType/STRING) str
      ;; The reason we convert to Boolean object is for symmetry - the protobuf system do this when loading from protobuf files
      (= type Descriptors$FieldDescriptor$JavaType/BOOLEAN) (fn [v] (Boolean/valueOf (boolean v)))
      (= type Descriptors$FieldDescriptor$JavaType/BYTE_STRING) identity
      (= type Descriptors$FieldDescriptor$JavaType/ENUM) (let [enum-cls (desc->proto-cls (.getEnumType desc))]
                                                             (fn [v] (Enum/valueOf enum-cls (keyword->enum-name v))))
      :else nil)))

(def ^:private pb-builder)

(defn- pb-builder-raw [^Class class]
  (let [^Descriptors$Descriptor desc (j/invoke-no-arg-class-method class "getDescriptor")
        ^Method new-builder-method (j/get-declared-method class "newBuilder" [])
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
                      (j/get-declared-methods builder-class))
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
                                   value-fn (if repeated
                                              (partial mapv field-builder)
                                              field-builder)]
                               (pair (field->key fd)
                                     (fn [^Message$Builder b v]
                                       (let [value (value-fn v)]
                                         (.invoke field-set-method b (to-array [value]))))))))
                      field-descs)
        builder-fn (fn [m]
                     (let [b (new-builder class)]
                       (doseq [[k v] m
                               :when (some? v)
                               :let [setter! (get setters k)]
                               :when setter!]
                         (setter! b v))
                       (.build b)))
        pb-desc-name (desc-name desc)]
    (if (and (.startsWith (full-name desc) "dmMath") (contains? math-type-keys pb-desc-name))
      (comp builder-fn (partial zipmap (get math-type-keys pb-desc-name)))
      builder-fn)))

(def ^:private pb-builder (memoize pb-builder-raw))

(defn map->pb
  [^Class cls m]
  (when-let [builder (pb-builder cls)]
    (builder m)))

(defmacro str->pb [^Class cls str]
  (with-meta `(TextFormat/parse ~str ~cls)
             {:tag cls}))

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

(defn val->pb-enum [^Class enum-class val]
  (Enum/valueOf enum-class (keyword->enum-name val)))

(def ^:private ^:const f0 (float 0.0))
(def ^:private ^:const f1 (float 1.0))

(extend-protocol PbConverter
  DdfMath$Point3
  (msg->vecmath [_pb v] (Point3d. (:x v f0) (:y v f0) (:z v f0)))
  (msg->clj [_pb v] [(:x v f0) (:y v f0) (:z v f0)])

  DdfMath$Vector3
  (msg->vecmath [_pb v] (Vector3d. (:x v f0) (:y v f0) (:z v f0)))
  (msg->clj [_pb v] [(:x v f0) (:y v f0) (:z v f0)])

  DdfMath$Vector4
  (msg->vecmath [_pb v] (Vector4d. (:x v f0) (:y v f0) (:z v f0) (:w v f0)))
  (msg->clj [_pb v] [(:x v f0) (:y v f0) (:z v f0) (:w v f0)])

  DdfMath$Quat
  (msg->vecmath [_pb v] (Quat4d. (:x v f0) (:y v f0) (:z v f0) (:w v f1)))
  (msg->clj [_pb v] [(:x v f0) (:y v f0) (:z v f0) (:w v f1)])

  DdfMath$Matrix4
  (msg->vecmath [_pb v]
    (Matrix4d. (:m00 v f1) (:m01 v f0) (:m02 v f0) (:m03 v f0)
               (:m10 v f0) (:m11 v f1) (:m12 v f0) (:m13 v f0)
               (:m20 v f0) (:m21 v f0) (:m22 v f1) (:m23 v f0)
               (:m30 v f0) (:m31 v f0) (:m32 v f0) (:m33 v f1)))
  (msg->clj [_pb v]
    [(:m00 v f1) (:m01 v f0) (:m02 v f0) (:m03 v f0)
     (:m10 v f0) (:m11 v f1) (:m12 v f0) (:m13 v f0)
     (:m20 v f0) (:m21 v f0) (:m22 v f1) (:m23 v f0)
     (:m30 v f0) (:m31 v f0) (:m32 v f0) (:m33 v f1)])

  Message
  (msg->vecmath [_pb v] v)
  (msg->clj [_pb v] v))

(defprotocol VecmathConverter
  (vecmath->pb [v] "Return the Protocol Buffer equivalent for the given javax.vecmath value"))

(extend-protocol VecmathConverter
  Point3d
  (vecmath->pb [v]
    (-> (DdfMath$Point3/newBuilder)
        (.setX (.getX v))
        (.setY (.getY v))
        (.setZ (.getZ v))
        (.build)))

  Vector3d
  (vecmath->pb [v]
    (-> (DdfMath$Vector3/newBuilder)
        (.setX (.getX v))
        (.setY (.getY v))
        (.setZ (.getZ v))
        (.build)))

  Vector4d
  (vecmath->pb [v]
    (-> (DdfMath$Vector4/newBuilder)
        (.setX (.getX v))
        (.setY (.getY v))
        (.setZ (.getZ v))
        (.setW (.getW v))
        (.build)))

  Quat4d
  (vecmath->pb [v]
    (-> (DdfMath$Quat/newBuilder)
        (.setX (.getX v))
        (.setY (.getY v))
        (.setZ (.getZ v))
        (.setW (.getW v))
        (.build)))

  Matrix4d
  (vecmath->pb [v]
    (-> (DdfMath$Matrix4/newBuilder)
        (.setM00 (.getElement v 0 0)) (.setM01 (.getElement v 0 1)) (.setM02 (.getElement v 0 2)) (.setM03 (.getElement v 0 3))
        (.setM10 (.getElement v 1 0)) (.setM11 (.getElement v 1 1)) (.setM12 (.getElement v 1 2)) (.setM13 (.getElement v 1 3))
        (.setM20 (.getElement v 2 0)) (.setM21 (.getElement v 2 1)) (.setM22 (.getElement v 2 2)) (.setM23 (.getElement v 2 3))
        (.setM30 (.getElement v 3 0)) (.setM31 (.getElement v 3 1)) (.setM32 (.getElement v 3 2)) (.setM33 (.getElement v 3 3))
        (.build))))

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

(defn read-pb [^Class cls input]
  ;; TODO: Make into macro so we can tag the return type.
  (.build (read-pb-into! (new-builder cls) input)))

(defn str->map-with-defaults [^Class cls ^String str]
  (pb->map-with-defaults
    (with-open [reader (StringReader. str)]
      (read-pb cls reader))))

(defn str->map-without-defaults [^Class cls ^String str]
  (pb->map-without-defaults
    (with-open [reader (StringReader. str)]
      (read-pb cls reader))))

(defonce single-byte-array-args [j/byte-array-class])

(defn- parser-fn-raw [^Class cls]
  (let [^Method parse-method (j/get-declared-method cls "parseFrom" single-byte-array-args)]
    (fn parser-fn [^bytes bytes]
      (.invoke parse-method nil (object-array [bytes])))))

(def parser-fn (memoize parser-fn-raw))

(defmacro bytes->pb [^Class cls bytes]
  (with-meta `((parser-fn ~cls) ~bytes)
             {:tag cls}))

(defn bytes->map-with-defaults [^Class cls bytes]
  (let [parser (parser-fn cls)]
    (-> bytes
        parser
        pb->map-with-defaults)))

(defn- enum-values-raw [^Class cls]
  (let [^Method values-method (j/get-declared-method cls "values" [])
        values (.invoke values-method nil (object-array 0))]
    (mapv (fn [^ProtocolMessageEnum value]
            [(pb-enum->val value)
             {:display-name (-> (.getValueDescriptor value) (.getOptions) (.getExtension DdfExtensions/displayName))}])
          values)))

(def enum-values (memoize enum-values-raw))

(defn- fields-by-indices-raw [^Class cls]
  (let [^Descriptors$Descriptor desc (j/invoke-no-arg-class-method cls "getDescriptor")]
    (into {}
          (map (fn [^Descriptors$FieldDescriptor field]
                 (pair (.getNumber field)
                       (field->key field))))
          (.getFields desc))))

(def fields-by-indices (memoize fields-by-indices-raw))

(defn pb->hash
  ^bytes [^String algorithm ^Message pb]
  (with-open [digest-output-stream (digest/make-digest-output-stream algorithm)]
    (.writeTo pb digest-output-stream)
    (.digest (.getMessageDigest digest-output-stream))))

(defn map->sha1-hex
  ^String [^Class cls m]
  (digest/bytes->hex (pb->hash "SHA-1" (map->pb cls m))))

(defn default-read-scale-value? [value]
  ;; The default value of the Vector3 type is zero, and protobuf does not
  ;; support custom default values for message-type fields. That means
  ;; everything read from protobuf will be scaled down to zero. However, we
  ;; might change the behavior of the protobuf reader in the future to have it
  ;; return [1.0 1.0 1.0] or even nil for default scale fields. In some way, nil
  ;; would make sense as a default for message-type fields as that is what
  ;; protobuf does without our wrapper. We could then decide on sensible default
  ;; values on a case-by-case basis. Related to all this, there has been some
  ;; discussion around perhaps omitting default values from the project data.
  (= [0.0 0.0 0.0] value))

(defn make-map-with-defaults [^Class cls & kvs]
  (into (default-map cls)
        (comp (partition-all 2)
              (keep (fn [[key value]]
                      (cond
                        (nil? value)
                        nil

                        (sequential? value)
                        (when (pos? (coll/bounded-count 1 value))
                          (pair key (vec value)))

                        :else
                        (pair key value)))))
        kvs))

(defn make-map-without-defaults [^Class cls & kvs]
  (let [key->field-info (field-infos cls)
        key->default (default-map cls)]
    (into {}
          (comp (partition-all 2)
                (keep (fn [[key value]]
                        (when (some? value)
                          (let [field-info (key->field-info key)]
                            (case (:field-rule field-info)
                              :optional
                              (when-not (or (and (= :message (:java-type-key field-info))
                                                 (zero? (coll/bounded-count 1 value)))
                                            (= (key->default key) value))
                                (pair key value))

                              :repeated
                              (when (and (pos? (coll/bounded-count 1 value))
                                         (not= (key->default key) value))
                                (pair key (vec value)))

                              :required
                              (when-not (and (= :message (:java-type-key field-info))
                                             (zero? (coll/bounded-count 1 value)))
                                (pair key value))

                              (throw (ex-info (str "Invalid field " key " for protobuf class " (.getName cls))
                                              {:key key
                                               :pb-class cls}))))))))
          kvs)))

(defn read-map-with-defaults [^Class cls input]
  (pb->map-with-defaults
    (read-pb cls input)))

(defn read-map-without-defaults [^Class cls input]
  (pb->map-without-defaults
    (read-pb cls input)))

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
     (cond
       (= ::not-found value)
       pb-map

       (nil? value)
       (dissoc pb-map field-kw)

       :else
       (if-some [sanitized-value (sanitize-value-fn value)]
         (assoc pb-map field-kw sanitized-value)
         (dissoc pb-map field-kw))))))

(defn sanitize-repeated
  ([pb-map field-kw]
   {:pre [(map? pb-map)
          (keyword? field-kw)]}
   (let [items (get pb-map field-kw ::not-found)]
     (if (or (= ::not-found items)
             (seq items))
       pb-map
       (dissoc pb-map field-kw))))
  ([pb-map field-kw sanitize-item-fn]
   {:pre [(map? pb-map)
          (keyword? field-kw)
          (ifn? sanitize-item-fn)]}
   (let [items (get pb-map field-kw ::not-found)]
     (cond
       (= ::not-found items)
       pb-map

       (seq items)
       (assoc pb-map field-kw (mapv sanitize-item-fn items))

       :else
       (dissoc pb-map field-kw)))))
