;; Copyright 2020 The Defold Foundation
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

(ns editor.placeholder-resource
  (:require [dynamo.graph :as g]
            [editor.code.resource :as r]
            [editor.resource :as resource]
            [util.text-util :as text-util]
            [editor.workspace :as workspace]))

(g/defnk produce-save-data [_node-id dirty? resource save-value]
  (cond-> {:dirty? dirty? :node-id _node-id :resource resource :value save-value}
          (some? save-value) (assoc :content (r/write-fn save-value))))

(g/defnk produce-build-targets [_node-id resource]
  (g/error-fatal (format "Cannot build resource of type '%s'" (resource/ext resource))))

(g/defnode PlaceholderResourceNode
  (inherits r/CodeEditorResourceNode)
  (output build-targets g/Any produce-build-targets)
  (output save-data g/Any produce-save-data))

(defn load-node [project node-id resource]
  (if (text-util/binary? resource)
    (g/set-property node-id :editable? false)
    (concat
      (g/set-property node-id :editable? true)
      (when (resource/file-resource? resource)
        (g/connect node-id :save-data project :save-data)))))

(defn view-type [workspace]
  (workspace/get-view-type workspace :code))
