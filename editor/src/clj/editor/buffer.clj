(ns editor.buffer
  (:require [editor.resource-node :as resource-node]
            [dynamo.graph :as g]
            [editor.pipeline :as pipeline]
            [editor.workspace :as workspace])
  (:import [com.dynamo.buffer.proto BufferProto$BufferDesc]))

(def ^:private buffer-icon "icons/32/Icons_61-Buffer.png")

(g/defnk produce-build-targets [resource]
  [(pipeline/make-protobuf-build-target resource nil
                                        BufferProto$BufferDesc
                                        {}
                                        nil)])

(g/defnode BufferNode
  (inherits resource-node/ResourceNode)

  (property json-data g/Any)
  (output build-targets g/Any :cached produce-build-targets))

(defn- load-buffer [project self resource])

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace
                                    :ext "buffer"
                                    :label "Buffer"
                                    :build-ext "bufferc"
                                    :node-type BufferNode
                                    :read-fn
                                    :load-fn load-buffer
                                    :icon buffer-icon
                                    :view-types [:text]
                                    :textual? true)
  (resource-node/register-ddf-resource-type workspace
                                            :ext "buffer"
                                            :label "Buffer"
                                            :build-ext "bufferc"
                                            :node-type BufferNode
                                            :ddf-type BufferProto$BufferDesc
                                            :load-fn load-buffer
                                            :icon buffer-icon
                                            :view-types [:text]))

