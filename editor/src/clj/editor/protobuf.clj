(ns editor.protobuf
  (:require [camel-snake-kebab :refer :all]
            [clojure.java.io :as io]
            [clojure.string :as s]
            [internal.java :as j])
  (:import [com.google.protobuf Message TextFormat ProtocolMessageEnum GeneratedMessage$Builder Descriptors$Descriptor
            Descriptors$FileDescriptor Descriptors$EnumDescriptor Descriptors$EnumValueDescriptor Descriptors$FieldDescriptor Descriptors$FieldDescriptor$Type Descriptors$FieldDescriptor$JavaType]
           [javax.vecmath Point3d Vector3d Vector4d Quat4d Matrix4d]
           [com.dynamo.proto DdfExtensions DdfMath$Point3 DdfMath$Vector3 DdfMath$Vector4 DdfMath$Quat DdfMath$Matrix4]
           [com.defold.editor.pipeline MurmurHash]
           [java.io Reader ByteArrayOutputStream]
           [org.apache.commons.io FilenameUtils]))

(set! *warn-on-reflection* true)

(defn- new-builder ^GeneratedMessage$Builder
  [class]
  (j/invoke-no-arg-class-method class "newBuilder"))

(defn- pb->str
  [^Message pb]
  (TextFormat/printToString pb))

(defn pb->bytes
  [^Message pb]
  (let [out (ByteArrayOutputStream. (* 4 1024))]
    (.writeTo pb out)
    (.close out)
    (.toByteArray out)))

(defn val->pb-enum
  [^Class enum-class val]
  (Enum/valueOf enum-class (s/replace (s/upper-case (name val)) "-" "_")))

(defn pb-enum->val
  [^Descriptors$EnumValueDescriptor val]
  (keyword (s/lower-case (s/replace (.getName val) "_" "-"))))

(defprotocol PbConverter
  (msg->vecmath [^Message pb v] "Return the javax.vecmath equivalent for the Protocol Buffer message")
  (msg->clj [^Message pb v]))

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

(defn- field->key [^Descriptors$FieldDescriptor field-desc]
  (keyword (s/replace (.getName field-desc) "_" "-")))

(defn pb->map
  [^Message pb]
  (let [fld->map (fn [^Descriptors$FieldDescriptor descriptor value]
                   (let [repeated? (.isRepeated descriptor)]
                     (cond
                       (= (.getType descriptor) Descriptors$FieldDescriptor$Type/MESSAGE)
                       (if repeated? (vec (map pb->map value)) (pb->map value))

                       (= (.getType descriptor) Descriptors$FieldDescriptor$Type/ENUM)
                       (if repeated? (vec (map pb-enum->val value)) (pb-enum->val value))

                       :else
                       (if repeated? (vec value) value))))
        all-fields (.getFields (.getDescriptorForType pb))]
    (let [m (into {} (map (fn [^Descriptors$FieldDescriptor field-desc]
                            (let [key (field->key field-desc)
                                  v (fld->map field-desc (.getField pb field-desc))]
                              [key v]))
                          all-fields))
          meta-defaults (into {} (map (fn [^Descriptors$FieldDescriptor field-desc]
                                       (let [k (field->key field-desc)]
                                         [k (get m k)]))
                                     (filter (fn [^Descriptors$FieldDescriptor field-desc]
                                               (and (not (.isRepeated field-desc)) (not (.hasField pb field-desc))))
                                             all-fields)))
          m (with-meta m {:proto-defaults meta-defaults})]
      (msg->clj pb m))))

(defn field-set? [m key]
  (not (identical? (get m key) (get-in (meta m) [:proto-defaults key]))))

(defn desc->cls [^Descriptors$FieldDescriptor desc val]
 (if (.isRepeated desc)
   Iterable
   (let [java-type (.getJavaType desc)]
    (cond
      (= java-type (Descriptors$FieldDescriptor$JavaType/INT)) Integer/TYPE
      (= java-type (Descriptors$FieldDescriptor$JavaType/LONG)) Long/TYPE
      (= java-type (Descriptors$FieldDescriptor$JavaType/FLOAT)) Float/TYPE
      (= java-type (Descriptors$FieldDescriptor$JavaType/DOUBLE)) Double/TYPE
      (= java-type (Descriptors$FieldDescriptor$JavaType/BOOLEAN)) Boolean/TYPE
      (= java-type (Descriptors$FieldDescriptor$JavaType/STRING)) String
      :else (type val)))))

(defprotocol GenericDescriptor
  (proto ^Message [this])
  (desc-name ^java.lang.String [this])
  (full-name ^java.lang.String [this])
  (file ^Descriptors$FileDescriptor [this])
  (containing-type ^Descriptors$Descriptor [this]))

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

(defn- desc->proto-cls ^java.lang.Class [desc]
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

(defn- kw->enum [^Descriptors$FieldDescriptor desc val]
  (let [enum-desc (.getEnumType desc)
        ; TODO - Can't use ->SNAKE_CASE here because of inconsistencies with word-splitting and numerals
        enum-name (s/replace (s/upper-case (name val)) "-" "_")
        enum-cls (desc->proto-cls enum-desc)]
    (java.lang.Enum/valueOf enum-cls enum-name)))

(defn- desc->builder ^GeneratedMessage$Builder [^Descriptors$Descriptor desc]
  (let [cls (desc->proto-cls desc)]
    (new-builder cls)))

(def map->pb)

(defn- clj->java [^Descriptors$FieldDescriptor desc val]
  (let [type (.getJavaType desc)]
    (cond
      (= type (Descriptors$FieldDescriptor$JavaType/INT)) (let [val (if (instance? java.lang.Boolean val)
                                                                      (if val 1 0)
                                                                      val)]
                                                            (int val))
      (= type (Descriptors$FieldDescriptor$JavaType/LONG)) (long val)
      (= type (Descriptors$FieldDescriptor$JavaType/FLOAT)) (float val)
      (= type (Descriptors$FieldDescriptor$JavaType/DOUBLE)) (double val)
      (= type (Descriptors$FieldDescriptor$JavaType/STRING)) (str val)
      (= type (Descriptors$FieldDescriptor$JavaType/BOOLEAN)) (boolean val)
      (= type (Descriptors$FieldDescriptor$JavaType/BYTE_STRING)) val
      (= type (Descriptors$FieldDescriptor$JavaType/ENUM)) (kw->enum desc val)
      (= type (Descriptors$FieldDescriptor$JavaType/MESSAGE)) (map->pb (.getMessageType desc) val)
      :else nil)))

(def math-type-keys
  {"Point3" [:x :y :z]
   "Vector3" [:x :y :z]
   "Vector4" [:x :y :z :w]
   "Quat" [:x :y :z :w]
   "Matrix4" [:m00 :m01 :m02 :m03
              :m10 :m11 :m12 :m13
              :m20 :m21 :m22 :m23
              :m30 :m31 :m32 :m33]})

(defn- set-field [m ^Descriptors$FieldDescriptor desc ^GeneratedMessage$Builder builder]
  (let [key (field->key desc)]
    (when (and (contains? m key) (field-set? m key))
      (let [clj-val (get m key)
            repeated? (.isRepeated desc)
            name (.getName desc)
            method-prefix (if repeated? "addAll" "set")
            set-name (str method-prefix (->CamelCase name))
            type (.getJavaType desc)
            value (if repeated? (map #(clj->java desc %) clj-val) (clj->java desc clj-val))
            cls (desc->cls desc value)]
        (when (not (nil? value))
          (-> (.getClass builder)
            (.getDeclaredMethod set-name (into-array Class [cls]))
            (.invoke builder (into-array Object [value]))))))))

(defn map->pb
  [desc-or-cls m]
  (let [^Descriptors$Descriptor desc (if (class? desc-or-cls)
                                       (j/invoke-no-arg-class-method desc-or-cls "getDescriptor")
                                       desc-or-cls)
        m (if (.startsWith (full-name desc) "dmMath")
            (zipmap (get math-type-keys (desc-name desc)) m)
            m)
        builder (desc->builder desc)
        all-fields (.getFields desc)]
    (doall (map #(set-field m % builder) all-fields))
    (.build builder)))

(defn map->str [desc-or-cls m]
  (->
    (map->pb desc-or-cls m)
    (pb->str)))

(defn map->bytes [desc-or-cls m]
  (->
    (map->pb desc-or-cls m)
    (pb->bytes)))

(defn read-text
  [^java.lang.Class class input]
  (let [input   (if (instance? Reader input) input (io/reader input))
        builder (new-builder class)]
    (TextFormat/merge ^Reader input builder)
    (pb->map (.build builder))))

(defn bytes->map [^java.lang.Class cls bytes]
  (let [parse-method (.getMethod cls "parseFrom" (into-array Class [(.getClass (byte-array 0))]))
        pb (.invoke parse-method nil (into-array Object [bytes]))]
    (pb->map pb)))

(defn enum-values [^java.lang.Class cls]
  (let [values-method (.getMethod cls "values" (into-array Class []))
        value-descs (map #(.getValueDescriptor ^ProtocolMessageEnum %) (.invoke values-method nil (object-array 0)))]
    (map (fn [value-desc]
            [(pb-enum->val ^Descriptors$EnumValueDescriptor value-desc)
             {:display-name (-> ^Descriptors$EnumValueDescriptor value-desc (.getOptions) (.getExtension DdfExtensions/displayName))}])
          value-descs)))

(defn hash64 [v]
  (MurmurHash/hash64 v))
