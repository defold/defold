(ns editor.protobuf
  (:require [camel-snake-kebab :refer :all]
            [clojure.java.io :as io]
            [clojure.string :as s]
            [internal.java :as j])
  (:import [com.google.protobuf Message TextFormat GeneratedMessage$Builder Descriptors$Descriptor
            Descriptors$FileDescriptor Descriptors$EnumDescriptor Descriptors$EnumValueDescriptor Descriptors$FieldDescriptor Descriptors$FieldDescriptor$Type Descriptors$FieldDescriptor$JavaType]
           [javax.vecmath Point3d Vector3d Vector4d Quat4d Matrix4d]
           [com.dynamo.proto DdfMath$Point3 DdfMath$Vector3 DdfMath$Vector4 DdfMath$Quat DdfMath$Matrix4]
           [java.io Reader ByteArrayOutputStream]
           [org.apache.commons.io FilenameUtils]))

(set! *warn-on-reflection* true)

(defmacro set-if-present
  "Use this macro to set an optional field on a protocol buffer message.
props must be a maplike data structure, and k is a key into it. k will also
be turned into a setter method name on the protocol buffer class.

If provided, xform is a function of one argument. It will be called with the
value of the property. The return value from xform will be the actual value
placed into the protocol buffer."
  ([inst k props]
    `(set-if-present ~inst ~k ~props identity))
  ([inst k props xform]
    (let [setter (symbol (->camelCase (str "set-" (name k))))]
      `(when-let [value# (get ~props ~k)]
         (. ~inst ~setter (~xform value#))))))

(defn- new-builder ^GeneratedMessage$Builder
  [class]
  (j/invoke-no-arg-class-method class "newBuilder"))

(defn read-text
  [^java.lang.Class class input & {:as opts}]
  (let [input   (if (instance? Reader input) input (io/reader input))
        builder (new-builder class)]
    (TextFormat/merge ^Reader input builder)
    (.build builder)))

(defn pb->str
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
  (assert (contains? (supers enum-class) com.google.protobuf.ProtocolMessageEnum))
  (assert (keyword? val))
  (j/invoke-class-method enum-class "valueOf" (name val)))

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

(defn pb->map
  [^Message pb]
  (let [fld->map (fn [m [^Descriptors$FieldDescriptor descriptor value]]
                   (let [prop      (keyword (s/replace (.getName descriptor) "_" "-"))
                         repeated? (.isRepeated descriptor)]
                     (cond
                       (= (.getType descriptor) Descriptors$FieldDescriptor$Type/MESSAGE)
                       (assoc m prop (if repeated? (map pb->map value) (pb->map value)))

                       (= (.getType descriptor) Descriptors$FieldDescriptor$Type/ENUM)
                       (assoc m prop (if repeated? (map pb-enum->val value) (pb-enum->val value)))

                       :else
                       (assoc m prop (if repeated? (seq value) value)))))
        all-fields     (.getFields (.getDescriptorForType pb))]
    (->> (reduce fld->map {} (zipmap all-fields (map #(.getField pb %) all-fields)))
      (msg->clj pb))))

(defn- desc->cls [^Descriptors$FieldDescriptor desc val]
 (let [java-type (.getJavaType desc)]
   (cond
     (= java-type (Descriptors$FieldDescriptor$JavaType/INT)) Integer/TYPE
     (= java-type (Descriptors$FieldDescriptor$JavaType/FLOAT)) Float/TYPE
     (= java-type (Descriptors$FieldDescriptor$JavaType/BOOLEAN)) Boolean/TYPE
     (= java-type (Descriptors$FieldDescriptor$JavaType/STRING)) String
     :else (type val))))

(defprotocol GenericDescriptor
  (proto ^Message [this])
  (desc-name ^java.lang.String [this])
  (full-name ^java.lang.String [this])
  (file ^Descriptors$FileDescriptor [this]))

(extend-protocol GenericDescriptor
  Descriptors$Descriptor
  (proto [this] (.toProto this))
  (desc-name [this] (.getName this))
  (full-name [this] (.getFullName this))
  (file [this] (.getFile this))
  Descriptors$EnumDescriptor
  (proto [this] (.toProto this))
  (desc-name [this] (.getName this))
  (full-name [this] (.getFullName this))
  (file [this] (.getFile this)))

(defn- desc->proto-cls ^java.lang.Class [desc]
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
    (Class/forName (str package "." outer-cls "$" inner-cls))))

(defn- kw->enum [^Descriptors$FieldDescriptor desc val]
  (let [enum-desc (.getEnumType desc)
        ; Can't use ->SNAKE_CASE here because of inconsistencies with word-splitting and numerals
        enum-name (s/replace (s/upper-case (subs (str val) 1)) "-" "_")
        enum-cls (desc->proto-cls enum-desc)]
    (java.lang.Enum/valueOf enum-cls enum-name)))

(defn- desc->builder ^GeneratedMessage$Builder [^Descriptors$Descriptor desc]
  (let [cls (desc->proto-cls desc)]
    (new-builder cls)))

(def map->pb)

(defn- clj->java [^Descriptors$FieldDescriptor desc val]
  (let [type (.getJavaType desc)]
    (cond
      (= type (Descriptors$FieldDescriptor$JavaType/INT)) (int val)
      (= type (Descriptors$FieldDescriptor$JavaType/FLOAT)) (float val)
      (= type (Descriptors$FieldDescriptor$JavaType/STRING)) (str val)
      (= type (Descriptors$FieldDescriptor$JavaType/BOOLEAN)) (boolean val)
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

(defn map->pb
  [desc-or-cls m]
  (let [^Descriptors$Descriptor desc (if (class? desc-or-cls)
                                       (j/invoke-no-arg-class-method desc-or-cls "getDescriptor")
                                       desc-or-cls)
        m (if (.startsWith (full-name desc) "dmMath")
            (zipmap (get math-type-keys (desc-name desc)) m)
            m)
        set-fn (fn [m ^Descriptors$FieldDescriptor desc ^GeneratedMessage$Builder builder]
                 (when-let [clj-val (get m (keyword (s/replace (.getName desc) "_" "-")))]
                   (let [name (.getName desc)
                         set-name (str "set" (->CamelCase name))
                         type (.getJavaType desc)
                         value (clj->java desc clj-val)
                         cls (desc->cls desc value)]
                     (when (not (nil? value))
                       (do
                         (-> (.getClass builder) (.getDeclaredMethod set-name (into-array Class [cls]))
                           (.invoke builder (into-array Object [value]))))))))
        builder (desc->builder desc)
        all-fields (.getFields desc)]
    (doall (map #(set-fn m % builder) all-fields))
    (.build builder)))
