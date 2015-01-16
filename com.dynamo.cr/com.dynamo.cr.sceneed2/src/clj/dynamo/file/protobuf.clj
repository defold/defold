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

(set! *warn-on-reflection* true)

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
  [cls props]
  `(fn [~'protobuf]
     (hash-map
       ~@(mapcat (fn [k getter] [k (list '. 'protobuf (symbol getter))])
           props (map getter props)))))

(defn connection
  [[source-label _ target-label]]
  (list `ds/connect 'node source-label 'this target-label))

(defn subordinate-mapper
  [[from-property connections]]
  `(doseq [~'node (map message->node (. ~'protobuf ~(getter from-property)))]
     ~@(map connection (partition-all 3 connections))))

(defn callback-field-mapper
  [[from-property callback]]
  `(doseq [~'msg (. ~'protobuf ~(getter from-property))]
      (~callback ~'this ~'msg)))

(defmacro protocol-buffer-converter
  [class spec]
  `(defmethod message->node ~class
     [~'protobuf & {:as ~'overrides}]
     (let [~'message-mapper ~(message-mapper class (:basic-properties spec))
           ~'basic-props    (~'message-mapper ~'protobuf)
           ~'this           (apply n/construct ~(:node-type spec) (mapcat identity (merge ~'basic-props ~'overrides)))]
       (ds/add ~'this)
       ~@(map subordinate-mapper (:node-properties spec))
       ~@(map callback-field-mapper (:field-mappers spec))
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
