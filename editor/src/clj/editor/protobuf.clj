(ns editor.protobuf
  "This ns primarily converts between protobuf java objs and clojure maps.
It does this at runtime, which is why it relies on java reflection to
call the appropriate methods. Since this is very expensive (specifically
fetching the Method from the Class), it uses memoization wherever possible.
It should be possible to use macros instead and retain the same API.
Macros currently mean no foreseeable performance gain however."
  (:require [camel-snake-kebab :refer :all]
            [clojure.java.io :as io]
            [clojure.string :as s]
            [internal.java :as j]
            [editor.util :as util]
            [util.murmur :as murmur]
            [util.digest :as digest])
  (:import [com.google.protobuf Message TextFormat ProtocolMessageEnum GeneratedMessage$Builder Descriptors$Descriptor DescriptorProtos$FieldOptions
            Descriptors$FileDescriptor Descriptors$EnumDescriptor Descriptors$EnumValueDescriptor Descriptors$FieldDescriptor Descriptors$FieldDescriptor$Type Descriptors$FieldDescriptor$JavaType]
           [javax.vecmath Point3d Vector3d Vector4d Quat4d Matrix4d]
           [com.dynamo.proto DdfExtensions DdfMath$Point3 DdfMath$Vector3 DdfMath$Vector4 DdfMath$Quat DdfMath$Matrix4]
           [java.lang.reflect Method]
           [java.io Reader ByteArrayOutputStream]
           [org.apache.commons.io FilenameUtils]))

(set! *warn-on-reflection* true)

(defprotocol GenericDescriptor
  (proto ^Message [this])
  (desc-name ^java.lang.String [this])
  (full-name ^java.lang.String [this])
  (file ^Descriptors$FileDescriptor [this])
  (containing-type ^Descriptors$Descriptor [this]))

(defprotocol PbConverter
  (msg->vecmath [^Message pb v] "Return the javax.vecmath equivalent for the Protocol Buffer message")
  (msg->clj [^Message pb v]))

(def ^:private upper-pattern (re-pattern #"\p{javaUpperCase}"))

(defn- new-builder ^GeneratedMessage$Builder
  [class]
  (j/invoke-no-arg-class-method class "newBuilder"))

(defn- field->key [^Descriptors$FieldDescriptor field-desc]
  (let [field-name (.getName field-desc)]
    (keyword (if (re-find upper-pattern field-name)
               (->kebab-case field-name)
               (s/replace field-name "_" "-")))))

(defn pb-enum->val
  [val-or-desc]
  (let [^Descriptors$EnumValueDescriptor desc (if (instance? ProtocolMessageEnum val-or-desc)
                                                (.getValueDescriptor ^ProtocolMessageEnum val-or-desc)
                                                val-or-desc)]
    (keyword (util/lower-case* (s/replace (.getName desc) "_" "-")))))

(declare pb-accessor)

(defn- field-accessor-fn [{:keys [field-type-key java-type-key type]}]
  (cond
    (= field-type-key :message)
    (pb-accessor type)

    (= field-type-key :enum)
    pb-enum->val

    (= java-type-key :boolean)
    boolean

    :else
    identity))

(def ^:private methods-by-name
  (memoize
    (fn methods-by-name [^Class class]
      (into {}
            (map (fn [^Method m] [(.getName m) m]))
            (.getDeclaredMethods class)))))

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
(defn underscores-to-camel-case
  [^String s]
  (loop [i 0
         sb (StringBuilder.)
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
      (cond-> sb
        (and (pos? (.length s)) (= (int \#) (.codePointAt s (dec (.length s)))))
        (.appendCodePoint (int \_))

        true
        (.toString)))))

(defn- options [^DescriptorProtos$FieldOptions field-options]
  (let [resource-desc (.getDescriptor DdfExtensions/resource)]
    (cond-> {}
      (.getField field-options resource-desc)
      (assoc :resource true))))

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

(defn- field-desc-default ^Object [^Descriptors$FieldDescriptor fd ^Object builder]
  (when (.isOptional fd)
    (if (.hasDefaultValue fd)
      (.getDefaultValue fd)
      ;; Protobuf does not support default values for messages, e.g. a dmMath.Quat
      ;; However, when such a field is optional, and dmMath.Quat happens to specify the defaults 0.0, 0.0, 0.0, 1.0
      ;; for *its* individual fields, it's still possible to retrieve that complex message as a default value.
      (let [java-name (underscores-to-camel-case (.getName fd))
            ^Method field-get-method (j/get-declared-method (.getClass builder) (str "get" java-name) [])]
        (.invoke field-get-method builder j/no-args-array)))))

(defn- field-info-raw [^Class cls]
  (let [^Descriptors$Descriptor desc (j/invoke-no-arg-class-method cls "getDescriptor")
        builder (new-builder cls)]
    (into {}
          (map (fn [^Descriptors$FieldDescriptor field-desc]
                 (let [default-value (field-desc-default field-desc builder)
                       info (cond-> {:java-name (underscores-to-camel-case (.getName field-desc))
                                     :type (field-type cls field-desc)
                                     :java-type-key (java-type->key (.getJavaType field-desc))
                                     :field-type-key (field-type->key (.getType field-desc))
                                     :repeated? (.isRepeated field-desc)
                                     :optional? (.isOptional field-desc)
                                     :options (options (.getOptions field-desc))}
                              (some? default-value)
                              (assoc :default default-value))]
                   [(field->key field-desc) info]))
               (.getFields desc)))))

(defn- field-get-method ^Method [{:keys [java-name repeated?]} ^Class cls]
  (let [get-method-name (str "get" java-name (if repeated? "List" ""))]
    (lookup-method cls get-method-name)))

(def field-info (memoize field-info-raw))

(declare resource-field-paths)

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
  (let [resource-field? (fn [field] (and (= (:type field) java.lang.String) (:resource (:options field))))
        message? (fn [field] (= (:field-type-key field) :message))]
    (into []
          (comp
            (map (fn [[key field]]
                   (cond
                     (resource-field? field)
                     (if (:repeated? field)
                       [ [[key]] ]
                       [ [key] ])

                     (message? field)
                     (let [sub-paths (resource-field-paths (:type field))]
                       (when (seq sub-paths)
                         (let [prefix (if (:repeated? field) [[key]] [key])]
                           (mapv (partial into prefix) sub-paths)))))))
            cat
            (remove nil?))
          (field-info class))))

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

(defn- pb-accessor-raw [^Class class]
  (let [fields (mapv (fn [[key {:keys [java-name repeated? type] :as field-info}]]
                       (let [get-method (field-get-method field-info class)
                             field-accessor (field-accessor-fn field-info)
                             field-accessor (if repeated? (partial mapv field-accessor) field-accessor)]
                         [key (fn [pb] (field-accessor (.invoke get-method pb j/no-args-array)))]))
                     (field-info class))]
    (fn [pb]
      (->> (reduce (fn [m [field get-fn]] (assoc m field (get-fn pb)))
                   {}
                   fields)
           (msg->clj pb)))))

(def ^:private pb-accessor (memoize pb-accessor-raw))

(defn pb->map
  [^Message pb]
  (let [accessor (pb-accessor (.getClass pb))]
    (accessor pb)))

(defn- default-vals-raw [^Class cls]
  (into {} (keep (fn [[key info]]
                     (when-some [default (:default info)]
                       [key ((field-accessor-fn info) default)])))
        (field-info cls)))

(def ^:private default-vals (memoize default-vals-raw))

(defn default [^Class cls field]
  (get (default-vals cls) field))

(def ^:private math-type-keys
  {"Point3" [:x :y :z]
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
    (Class/forName cls-name)))

(defn- primitive-builder [^Descriptors$FieldDescriptor desc]
  (let [type (.getJavaType desc)]
    (cond
      (= type (Descriptors$FieldDescriptor$JavaType/INT)) (fn [v]
                                                            (int (if (instance? java.lang.Boolean v)
                                                                   (if v 1 0)
                                                                   v)))
      (= type (Descriptors$FieldDescriptor$JavaType/LONG)) long
      (= type (Descriptors$FieldDescriptor$JavaType/FLOAT)) float
      (= type (Descriptors$FieldDescriptor$JavaType/DOUBLE)) double
      (= type (Descriptors$FieldDescriptor$JavaType/STRING)) str
      ;; The reason we convert to Boolean object is for symmetry - the protobuf system do this when loading from protobuf files
      (= type (Descriptors$FieldDescriptor$JavaType/BOOLEAN)) (fn [v] (java.lang.Boolean. (boolean v)))
      (= type (Descriptors$FieldDescriptor$JavaType/BYTE_STRING)) identity
      (= type (Descriptors$FieldDescriptor$JavaType/ENUM)) (let [enum-cls (desc->proto-cls (.getEnumType desc))]
                                                             (fn [v] (java.lang.Enum/valueOf enum-cls (s/replace (util/upper-case* (name v)) "-" "_"))))
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
                                                   (some-> m (.getParameterTypes) ^Class first (.getName) (.endsWith "$Builder")))]
                            (when (not set-via-builder?)
                              [m-name m]))))
                  (.getDeclaredMethods builder-class))
        field-descs (.getFields ^Descriptors$Descriptor (j/invoke-no-arg-class-method class "getDescriptor"))
        setters (into {}
                  (map (fn [^Descriptors$FieldDescriptor fd]
                         (let [j-name (->CamelCase (.getName fd))
                               repeated? (.isRepeated fd)
                               ^Method field-set-method (get methods (str (if repeated? "addAll" "set") j-name))
                               field-builder (if (= (.getJavaType fd) (Descriptors$FieldDescriptor$JavaType/MESSAGE))
                                               (let [^Method field-get-method (get methods (str "get" j-name))]
                                                 (pb-builder (.getReturnType field-get-method)))
                                               (primitive-builder fd))
                               value-fn (if repeated?
                                          (partial mapv field-builder)
                                          field-builder)]
                           [(field->key fd) (fn [^GeneratedMessage$Builder b v]
                                              (.invoke field-set-method b (to-array [(value-fn v)])))])))
                  field-descs)
        builder-fn (fn [m]
                     (let [b ^GeneratedMessage$Builder (new-builder class)]
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

(defn- break-embedded-newlines
  [^String pb-str]
  (.replace pb-str "\\n" "\\n\"\n  \""))

(defn- pb->str [^Message pb format-newlines?]
  (cond-> (TextFormat/printToString pb)
    format-newlines?
    (break-embedded-newlines)))

(defn pb->bytes [^Message pb]
  (let [out (ByteArrayOutputStream. (* 4 1024))]
    (.writeTo pb out)
    (.close out)
    (.toByteArray out)))

(defn val->pb-enum [^Class enum-class val]
  (Enum/valueOf enum-class (s/replace (util/upper-case* (name val)) "-" "_")))

(extend-protocol PbConverter
  DdfMath$Point3
  (msg->vecmath [pb v] (Point3d. (:x v) (:y v) (:z v)))
  (msg->clj [pb v] [(:x v) (:y v) (:z v)])

  DdfMath$Vector3
  (msg->vecmath [pb v] (Vector3d. (:x v) (:y v) (:z v)))
  (msg->clj [pb v] [(:x v) (:y v) (:z v)])

  DdfMath$Vector4
  (msg->vecmath [pb v] (Vector4d. (:x v) (:y v) (:z v) (:w v)))
  (msg->clj [pb v] [(:x v) (:y v) (:z v) (:w v)])

  DdfMath$Quat
  (msg->vecmath [pb v] (Quat4d. (:x v) (:y v) (:z v) (:w v)))
  (msg->clj [pb v] [(:x v) (:y v) (:z v) (:w v)])

  DdfMath$Matrix4
  (msg->vecmath [pb v]
    (Matrix4d. (:m00 v) (:m01 v) (:m02 v) (:m03 v)
               (:m10 v) (:m11 v) (:m12 v) (:m13 v)
               (:m20 v) (:m21 v) (:m22 v) (:m23 v)
               (:m30 v) (:m31 v) (:m32 v) (:m33 v)))
  (msg->clj [pb v]
    [(:m00 v) (:m01 v) (:m02 v) (:m03 v)
     (:m10 v) (:m11 v) (:m12 v) (:m13 v)
     (:m20 v) (:m21 v) (:m22 v) (:m23 v)
     (:m30 v) (:m31 v) (:m32 v) (:m33 v)])

  Message
  (msg->vecmath [pb v] v)
  (msg->clj [pb v] v))

(defprotocol VecmathConverter
  (vecmath->pb [v] "Return the Protocol Buffer equivalent for the given javax.vecmath value"))

(extend-protocol VecmathConverter
  Point3d
  (vecmath->pb [v]
    (->
     (doto (DdfMath$Point3/newBuilder)
       (.setX (.getX v))
       (.setY (.getY v))
       (.setZ (.getZ v)))
     (.build)))

  Vector3d
  (vecmath->pb [v]
    (->
     (doto (DdfMath$Vector3/newBuilder)
       (.setX (.getX v))
       (.setY (.getY v))
       (.setZ (.getZ v)))
     (.build)))

  Vector4d
  (vecmath->pb [v]
    (->
     (doto (DdfMath$Vector4/newBuilder)
       (.setX (.getX v))
       (.setY (.getY v))
       (.setZ (.getZ v))
       (.setW (.getW v)))
     (.build)))

  Quat4d
  (vecmath->pb [v]
    (->
     (doto (DdfMath$Quat/newBuilder)
       (.setX (.getX v))
       (.setY (.getY v))
       (.setZ (.getZ v))
       (.setW (.getW v)))
     (.build)))

  Matrix4d
  (vecmath->pb [v]
    (->
     (doto (DdfMath$Matrix4/newBuilder)
       (.setM00 (.getElement v 0 0)) (.setM01 (.getElement v 0 1)) (.setM02 (.getElement v 0 2)) (.setM03 (.getElement v 0 3))
       (.setM10 (.getElement v 1 0)) (.setM11 (.getElement v 1 1)) (.setM12 (.getElement v 1 2)) (.setM13 (.getElement v 1 3))
       (.setM20 (.getElement v 2 0)) (.setM21 (.getElement v 2 1)) (.setM22 (.getElement v 2 2)) (.setM23 (.getElement v 2 3))
       (.setM30 (.getElement v 3 0)) (.setM31 (.getElement v 3 1)) (.setM32 (.getElement v 3 2)) (.setM33 (.getElement v 3 3)))
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

(defn- desc->builder ^GeneratedMessage$Builder [^Descriptors$Descriptor desc]
  (let [cls (desc->proto-cls desc)]
    (new-builder cls)))

(defn map->str
  ([^Class cls m] (map->str cls m true))
  ([^Class cls m format-newlines?]
    (->
      (map->pb cls m)
      (pb->str format-newlines?))))

(defn map->bytes [^Class cls m]
  (->
    (map->pb cls m)
    (pb->bytes)))

(defn read-text [^Class cls input]
  (with-open [reader (io/reader input)]
    (let [builder (new-builder cls)]
      (TextFormat/merge ^Reader reader builder)
      (pb->map (.build builder)))))

(defn- parser-fn-raw [^Class cls]
  (let [parse-method (.getMethod cls "parseFrom" (into-array Class [(.getClass (byte-array 0))]))]
    (fn [^bytes bytes]
      (.invoke parse-method nil (into-array Object [bytes])))))

(def ^:private parser-fn (memoize parser-fn-raw))

(defn bytes->map [^Class cls bytes]
  (let [parser (parser-fn cls)]
    (-> bytes
      parser
      pb->map)))

(defn- enum-values-raw [^Class cls]
  (let [values-method (.getMethod cls "values" (into-array Class []))
        values (.invoke values-method nil (object-array 0))]
    (mapv (fn [^ProtocolMessageEnum value]
            [(pb-enum->val value)
             {:display-name (-> (.getValueDescriptor value) (.getOptions) (.getExtension DdfExtensions/displayName))}])
      values)))

(def enum-values (memoize enum-values-raw))

(defn- fields-by-indices-raw [^Class cls]
  (let [^Descriptors$Descriptor desc (j/invoke-no-arg-class-method cls "getDescriptor")]
    (into {} (map (fn [^Descriptors$FieldDescriptor field] [(.getNumber field) (field->key field)]) (.getFields desc)))))

(def fields-by-indices (memoize fields-by-indices-raw))

(defn pb->hash
  ^bytes [^String algorithm ^Message pb]
  (.digest (with-open [digest-output-stream (digest/make-digest-output-stream algorithm)]
             (.writeTo pb digest-output-stream)
             (.getMessageDigest digest-output-stream))))

(defn map->sha1-hex
  ^String [^Class cls m]
  (digest/bytes->hex (pb->hash "SHA-1" (map->pb cls m))))
