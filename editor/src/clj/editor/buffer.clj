;; Copyright 2020-2022 The Defold Foundation
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

(ns editor.buffer
  (:require [dynamo.graph :as g]
            [editor.buffers :as buffers]
            [editor.code.lang.json :as json]
            [editor.code.resource :as r]
            [editor.pipeline :as pipeline])
  (:import [com.dynamo.gamesys.proto BufferProto$BufferDesc]))

(def ^:private buffer-icon "icons/32/Icons_61-Buffer.png")

(def ^:private put-functions-by-key
  (into {}
        (for [component-count (range 1 5)
              [source-data-tag-sym put-method-sym pb-value-types]
              [['bytes 'put [:value-type-uint8 :value-type-int8]]
               ['shorts 'putShort [:value-type-uint16 :value-type-int16]]
               ['ints 'putInt [:value-type-uint32 :value-type-int32]]
               ['longs 'putLong [:value-type-uint64 :value-type-int64]]
               ['floats 'putFloat [:value-type-float32]]]
              :let [put-fn (eval (buffers/make-put-fn-form component-count source-data-tag-sym put-method-sym))]
              pb-value-type pb-value-types]
          [[component-count pb-value-type] put-fn])))

(defn get-put-fn [pb-value-type ^long component-count]
  (get put-functions-by-key [component-count pb-value-type]))

(defn stream-data->array [stream-data pb-value-type length]
  (case pb-value-type
    :value-type-float32 (float-array length stream-data)
    (:value-type-uint8 :value-type-int8) (byte-array length stream-data)
    (:value-type-uint16 :value-type-int16) (short-array length stream-data)
    (:value-type-uint32 :value-type-int32) (int-array length stream-data)
    (:value-type-uint64 :value-type-int64) (long-array length stream-data)))

(defn stream->array-stream [vertex-count stream]
  (update stream :data stream-data->array
          (:type stream)
          (* (:count stream)
             vertex-count)))

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

(defn- lines->streams [lines]
  (json/lines->json lines
                    :key-fn keyword
                    :value-fn (fn [k v]
                                (case k
                                  :type (type->pb-value-type v)
                                  v))))

(g/defnk produce-build-targets [streams resource]
  [(pipeline/make-protobuf-build-target resource nil
                                        BufferProto$BufferDesc
                                        {:streams (map json-stream->pb-stream streams)}
                                        nil)])

(g/defnk produce-streams [_node-id lines]
  (let [streams (try
                  (lines->streams lines)
                  (catch Exception error
                    error))]
    (if (instance? Exception streams)
      (g/->error _node-id :lines :fatal lines "Syntax error in buffer file.")
      streams)))

(g/defnk produce-stream-ids [streams]
  (into (sorted-set)
        (map :name)
        streams))

(g/defnode BufferNode
  (inherits r/CodeEditorResourceNode)

  (output build-targets g/Any produce-build-targets)
  (output streams g/Any :cached produce-streams)
  (output stream-ids g/Any produce-stream-ids))

(defn register-resource-types [workspace]
  (r/register-code-resource-type workspace
                                 :ext "buffer"
                                 :label "Buffer"
                                 :icon buffer-icon
                                 :view-types [:code :default]
                                 :view-opts {:code {:grammar json/grammar}}
                                 :node-type BufferNode
                                 :eager-loading? false))
