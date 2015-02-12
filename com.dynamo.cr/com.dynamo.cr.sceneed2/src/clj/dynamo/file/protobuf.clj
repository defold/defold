(ns dynamo.file.protobuf
  (:require [clojure.java.io :as io]
            [clojure.string :as str]
            [dynamo.node :as n]
            [dynamo.file :as f]
            [dynamo.system :as ds]
            [internal.java :as j]
            [camel-snake-kebab :refer :all])
  (:import [java.io Reader]
           [com.google.protobuf Message TextFormat GeneratedMessage$Builder Descriptors$EnumValueDescriptor Descriptors$FieldDescriptor Descriptors$FieldDescriptor$Type]))

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

(defmulti message->node
  "This is an extensible function that you implement to help load a specific file
type. Most of the time, these will be created for you by the
dynamo.file.protobuf/protocol-buffer-converter macro.

Create an implementation by adding something like this to your namespace:

    (defmethod message->node message-classname
      [message-instance]
      (,,,) ;; implementation
    )

You'll replace _message-classname_ with the Java class that matches the message
type to convert. The _message-instance_ argument will contain an instance of the
class specified in _message-classname_.

Your implementation is responsible for calling `dyanmo.system/add` on any nodes
it creates. Likewise, it is responsible for making connections as desired."
  (fn [message & _] (class message)))

(defn- new-builder ^GeneratedMessage$Builder
  [class]
  (j/invoke-no-arg-class-method class "newBuilder"))

(defn read-text
  [^java.lang.Class class input & {:as opts}]
  (let [input   (if (instance? Reader input) input (io/reader input))
        builder (new-builder class)]
    (TextFormat/merge ^Reader input builder)
    (.build builder)))

(defn getter
  [fld]
  (symbol (->camelCase (str "get-" (name fld)))))

(defn message-mapper
  [class props]
  `(fn [~'protobuf]
     (hash-map
       ~@(mapcat (fn [k getter] [k (list '. (with-meta 'protobuf {:tag class}) (symbol getter))])
           props (map getter props)))))

(defn connection
  [[source-label _ target-label]]
  (list `ds/connect 'node source-label 'this target-label))

(defn subordinate-mapper
  [class [from-property connections]]
  `(doseq [~'node (map message->node (. ~(with-meta 'protobuf {:tag class}) ~(getter from-property)))]
     ~@(map connection (partition-all 3 connections))))

(defn callback-field-mapper
  [class [from-property callback]]
  `(doseq [~'msg (. ~(with-meta 'protobuf {:tag class}) ~(getter from-property))]
      (~callback ~'this ~'msg)))

(defmacro protocol-buffer-converter
  [class spec]
  `(defmethod message->node ~class
     [~'protobuf & {:as ~'overrides}]
     (let [~'message-mapper ~(message-mapper class (:basic-properties spec))
           ~'basic-props    (~'message-mapper ~'protobuf)
           ~'this           (apply n/construct ~(:node-type spec) (mapcat identity (merge ~'basic-props ~'overrides)))]
       (ds/add ~'this)
       ~@(map (partial subordinate-mapper class) (:node-properties spec))
       ~@(map (partial class callback-field-mapper) (:field-mappers spec))
       ~'this)))

(defmacro protocol-buffer-converters
  [& class+specs]
  (let [converters (map (partial cons `protocol-buffer-converter) (partition 2 class+specs))]
    (list* 'do converters)))

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
