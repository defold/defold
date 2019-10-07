(ns editor.buffer
  (:require [dynamo.graph :as g]
            [editor.code.resource :as r]
            [editor.json :as json]
            [editor.pipeline :as pipeline])
  (:import [com.dynamo.buffer.proto BufferProto$BufferDesc]))

(def ^:private buffer-icon "icons/32/Icons_61-Buffer.png")

(defn- type->pb-value-type [type]
  (case type
    "uint8" :value-type-uint8
    "uint16" :value-type-uint16
    "uint32" :value-type-uint32
    "uint64" :value-type-uint64
    "int8" :value-type-int8
    "int16" :value-type-int16
    "int32" :value-type-int32
    "int64" :value-type-int64
    "float32" :value-type-float32))

(defn- pb-value-type->pb-stream-field [pb-value-type]
  (case pb-value-type
    :value-type-float32 :f
    (:value-type-uint8 :value-type-uint16 :value-type-uint32) :ui
    (:value-type-int8 :value-type-int16 :value-type-int32) :i
    :value-type-uint64 :ui64
    :value-type-int64 :i64))

(defn- json-stream->pb-stream [{:keys [name type count data] :as _json-stream}]
  (assoc {:name name
          :value-type type
          :value-count count}
    (pb-value-type->pb-stream-field type) data))

(defn- lines->pb-msg [lines]
  (let [json-streams (json/lines->json lines
                                       :key-fn keyword
                                       :value-fn (fn [k v]
                                                   (case k
                                                     :type (type->pb-value-type v)
                                                     v)))]
    {:streams (map json-stream->pb-stream json-streams)}))

(g/defnk produce-build-targets [_node-id lines resource]
  (let [pb-msg (try
                 (lines->pb-msg lines)
                 (catch Exception error
                   error))]
    (if (instance? Exception pb-msg)
      (g/->error _node-id :lines :fatal lines "Syntax error in buffer file.")
      [(pipeline/make-protobuf-build-target resource nil
                                            BufferProto$BufferDesc
                                            pb-msg
                                            nil)])))

(g/defnode BufferNode
  (inherits r/CodeEditorResourceNode)

  (output build-targets g/Any :cached produce-build-targets))

(defn register-resource-types [workspace]
  (r/register-code-resource-type workspace
                                 :ext "buffer"
                                 :label "Buffer"
                                 :icon buffer-icon
                                 :view-types [:code :default]
                                 :view-opts {:code {:grammar json/json-grammar}}
                                 :node-type BufferNode
                                 :eager-loading? false))
