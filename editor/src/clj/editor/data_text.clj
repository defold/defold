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

(ns editor.data-text
  (:require [dynamo.graph :as g]
            [editor.build-target :as bt]
            [editor.code.data :as code.data]
            [editor.code.resource :as code.resource]
            [editor.localization :as localization]
            [editor.protobuf :as protobuf]
            [editor.workspace :as workspace])
  (:import [com.dynamo.gamesys.proto DataProto$Data]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- build-data-lines [build-resource _dep-resources user-data]
  (let [{:keys [lines]} user-data
        text (code.data/lines->string lines)]
    (try
      (let [pb (protobuf/str->pb DataProto$Data text)
            content (protobuf/pb->bytes pb)]
        {:resource build-resource
         :content content})
      (catch Throwable exception
        (throw
          (ex-info
            (str "Failed to compile .data file: " (.getMessage exception))
            {:build-resource build-resource}
            exception))))))

(g/defnode CodeEditorDataResourceNode
  (inherits code.resource/CodeEditorResourceNode)

  (output build-targets g/Any :cached
          (g/fnk [_node-id lines resource]
            (let [build-resource (workspace/make-build-resource resource)]
              [(bt/with-content-hash
                 {:node-id _node-id
                  :resource build-resource
                  :build-fn build-data-lines
                  :user-data {:lines lines}})]))))

(defn register-resource-types [workspace]
  (code.resource/register-code-resource-type workspace
    :ext "data"
    :node-type CodeEditorDataResourceNode
    :built-pb-class DataProto$Data
    :label (localization/message "resource.type.data")
    :icon "icons/32/Icons_11-Script-general.png"
    :view-types [:code :default]
    :lazy-loaded true))
