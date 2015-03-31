(ns dynamo.file.protobuf
  (:require [camel-snake-kebab :refer :all]
            [clojure.java.io :as io]
            [internal.java :as j])
  (:import [com.google.protobuf Message TextFormat GeneratedMessage$Builder Descriptors$EnumValueDescriptor Descriptors$FieldDescriptor Descriptors$FieldDescriptor$Type]
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
                       (assoc m prop (if repeated? (seq value) value)))))]
    (reduce fld->map {} (.getAllFields pb))))
