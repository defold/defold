;; Copyright 2020-2026 The Defold Foundation
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

(ns editor.data
  (:require [dynamo.graph :as g]
            [editor.build-target :as bt]
            [editor.code.data :as data]
            [editor.code.resource :as r]
            [editor.localization :as localization]
            [editor.protobuf :as protobuf]
            [editor.workspace :as workspace])
  (:import [com.dynamo.gamesys.proto DataProto$Data]
           [com.google.protobuf Message]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- build-data [build-resource _dep-resources user-data]
  (let [{:keys [lines]} user-data
        ^String text (data/lines->string lines)]
    (try
      (let [pb (protobuf/str->pb DataProto$Data text)
            content (protobuf/pb->bytes pb)]
        {:resource build-resource
         :content content})
      (catch Throwable e
        (throw (ex-info (str "Failed to compile .data file: " (.getMessage e))
                        {:build-resource build-resource}
                        e))))))

(g/defnk produce-build-targets [_node-id resource lines]
  (if (g/error? lines)
    (g/error-aggregate [lines] :_node-id _node-id :_label :build-targets)
    (let [build-resource (workspace/make-build-resource resource)]
      [(bt/with-content-hash
         {:node-id _node-id
          :resource build-resource
          :build-fn build-data
          :user-data {:lines lines}})])))

(g/defnode DataFileNode
  (inherits r/CodeEditorResourceNode)
  (output build-targets g/Any :cached produce-build-targets))

(defn register-resource-types [workspace]
  (r/register-code-resource-type workspace
    :ext "data"
    :node-type DataFileNode
    :label (localization/message "resource.type.data")
    :icon "icons/32/Icons_11-Script-general.png"
    :view-types [:code :default]
    :view-opts {}
    :build-ext "datac"
    :lazy-loaded true
    :built-pb-class DataProto$Data))

