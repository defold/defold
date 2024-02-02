;; Copyright 2020-2024 The Defold Foundation
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

(ns editor.placeholder-resource
  (:require [dynamo.graph :as g]
            [editor.code.resource :as r]
            [editor.resource :as resource]
            [editor.workspace :as workspace]
            [util.text-util :as text-util]))

(g/defnk produce-undecorated-save-data [_node-id resource save-value]
  (cond-> {:node-id _node-id :resource resource :value save-value}
          (some? save-value) (assoc :content (r/write-fn save-value))))

(g/defnk produce-build-targets [_node-id resource]
  (g/error-fatal (format "Cannot build resource of type '%s'" (resource/ext resource))))

(g/defnode PlaceholderResourceNode
  (inherits r/CodeEditorResourceNode)
  (output build-targets g/Any produce-build-targets)
  (output undecorated-save-data g/Any produce-undecorated-save-data))

(defn load-node [project node-id resource]
  (if (or (not (resource/editable? resource))
          (text-util/binary? resource))
    (g/set-property node-id :editable false)
    (concat
      (g/set-property node-id :editable true)
      (when (resource/file-resource? resource)
        (g/connect node-id :save-data project :save-data)))))

(defn view-type [workspace]
  (workspace/get-view-type workspace :code))
