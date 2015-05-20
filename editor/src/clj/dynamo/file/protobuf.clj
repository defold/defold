(ns dynamo.file.protobuf
  (:require [camel-snake-kebab :refer :all]
            [clojure.java.io :as io]
            [internal.java :as j])
  (:import [com.google.protobuf Message TextFormat GeneratedMessage$Builder Descriptors$EnumValueDescriptor Descriptors$FieldDescriptor Descriptors$FieldDescriptor$Type]
           [javax.vecmath Point3d Vector3d Vector4d Quat4d Matrix4d]
           [com.dynamo.proto DdfMath$Point3 DdfMath$Vector3 DdfMath$Vector4 DdfMath$Quat DdfMath$Matrix4]
           [java.io Reader]))

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

(defn val->pb-enum
  [^Class enum-class val]
  (assert (contains? (supers enum-class) com.google.protobuf.ProtocolMessageEnum))
  (assert (keyword? val))
  (j/invoke-class-method enum-class "valueOf" (name val)))

(defn pb-enum->val
  [^Descriptors$EnumValueDescriptor val]
  (keyword (.getName val)))

(defprotocol PbConverter
  (msg->vecmath [^Message pb v] "Return the javax.vecmath equivalent for the Protocol Buffer message"))

(extend-protocol PbConverter
  DdfMath$Point3
  (msg->vecmath [pb v] (Point3d. (:x v) (:y v) (:z v)))

  DdfMath$Vector3
  (msg->vecmath [pb v] (Vector3d. (:x v) (:y v) (:z v)))

  DdfMath$Vector4
  (msg->vecmath [pb v] (Vector4d. (:x v) (:y v) (:z v) (:w v)))

  DdfMath$Quat
  (msg->vecmath [pb v] (Quat4d. (:x v) (:y v) (:z v) (:w v)))

  DdfMath$Matrix4
  (msg->vecmath [pb v]
    (Matrix4d. (:m00 v) (:m01 v) (:m02 v) (:m03 v)
               (:m10 v) (:m11 v) (:m12 v) (:m13 v)
               (:m20 v) (:m21 v) (:m22 v) (:m23 v)
               (:m30 v) (:m31 v) (:m32 v) (:m33 v)))

  Message
  (msg->vecmath [pb v] v))

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
