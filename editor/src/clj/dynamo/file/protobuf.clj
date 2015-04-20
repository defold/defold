(ns dynamo.file.protobuf
  (:require [camel-snake-kebab :refer :all]
            [clojure.java.io :as io]
            [internal.java :as j])
  (:import [com.google.protobuf Message TextFormat GeneratedMessage$Builder Descriptors$EnumValueDescriptor Descriptors$FieldDescriptor Descriptors$FieldDescriptor$Type]
           [javax.vecmath Point3d Vector3d Vector4d Quat4d Matrix4d]
           [com.dynamo.proto DdfMath$Point3 DdfMath$Vector3 DdfMath$Vector4 DdfMath$Quat DdfMath$Matrix4]
           [java.io Reader]))

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

(defn val->pb-enum
  [^Class enum-class val]
  (assert (contains? (supers enum-class) com.google.protobuf.ProtocolMessageEnum))
  (assert (keyword? val))
  (j/invoke-class-method enum-class "valueOf" (name val)))

(defn pb-enum->val
  [^Descriptors$EnumValueDescriptor val]
  (keyword (.getName val)))

(defn- msg->vecmath [^Message pb v]
  (let [cls (class pb)]
    (cond
      (= cls DdfMath$Point3)
      (Point3d. (:x v) (:y v) (:z v))

      (= cls DdfMath$Vector3)
      (Vector3d. (:x v) (:y v) (:z v))

      (= cls DdfMath$Vector4)
      (Vector4d. (:x v) (:y v) (:z v) (:w v))

      (= cls DdfMath$Quat)
      (Quat4d. (:x v) (:y v) (:z v) (:w v))

      (= cls DdfMath$Matrix4)
      (Matrix4d. (:m00 v) (:m01 v) (:m02 v) (:m03 v)
                 (:m10 v) (:m11 v) (:m12 v) (:m13 v)
                 (:m20 v) (:m21 v) (:m22 v) (:m23 v)
                 (:m30 v) (:m31 v) (:m32 v) (:m33 v))

      :else v)))

(defmulti vecmath->pb (fn [v] (class v)))

(defmethod vecmath->pb Point3d [v]
  (->
    (doto (DdfMath$Point3/newBuilder)
      (.setX (.getX v))
      (.setY (.getY v))
      (.setZ (.getZ v)))
    (.build)))

(defmethod vecmath->pb Vector3d [v]
  (->
    (doto (DdfMath$Vector3/newBuilder)
      (.setX (.getX v))
      (.setY (.getY v))
      (.setZ (.getZ v)))
    (.build)))

(defmethod vecmath->pb Vector4d [v]
  (->
    (doto (DdfMath$Vector4/newBuilder)
      (.setX (.getX v))
      (.setY (.getY v))
      (.setZ (.getZ v))
      (.setW (.getW v)))
    (.build)))

(defmethod vecmath->pb Quat4d [v]
  (->
    (doto (DdfMath$Quat/newBuilder)
      (.setX (.getX v))
      (.setY (.getY v))
      (.setZ (.getZ v))
      (.setW (.getW v)))
    (.build)))

(defmethod vecmath->pb Matrix4d [v]
  (->
    (doto (DdfMath$Point3/newBuilder)
      (.setM00 (.getElement 0 0 v)) (.setM01 (.getElement 0 1 v)) (.setM02 (.getElement 0 2 v)) (.setM03 (.getElement 0 3 v))
      (.setM10 (.getElement 1 0 v)) (.setM11 (.getElement 1 1 v)) (.setM12 (.getElement 1 2 v)) (.setM13 (.getElement 1 3 v))
      (.setM20 (.getElement 2 0 v)) (.setM21 (.getElement 2 1 v)) (.setM22 (.getElement 2 2 v)) (.setM23 (.getElement 2 3 v))
      (.setM30 (.getElement 3 0 v)) (.setM31 (.getElement 3 1 v)) (.setM32 (.getElement 3 2 v)) (.setM33 (.getElement 3 3 v)))
    (.build)))

(defn pb->map
  [^Message pb]
  (let [fld->map (fn [m [^Descriptors$FieldDescriptor descriptor value]]
                   (let [prop      (keyword (->kebab-case (.getName descriptor)))
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
      (msg->vecmath pb))))
