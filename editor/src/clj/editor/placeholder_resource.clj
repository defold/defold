;; Copyright 2020-2023 The Defold Foundation
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

(g/defnk produce-build-targets [_node-id resource]
  (g/error-fatal (format "Cannot build resource of type '%s'" (resource/ext resource))))

(g/defnode PlaceholderResourceNode
  (inherits r/CodeEditorResourceNode)
  (output build-targets g/Any produce-build-targets))

(defn load-node [project node-id resource]
  (when (and (resource/file-resource? resource)
             (resource/editable? resource)
             (not (text-util/binary? resource)))
    (g/connect node-id :save-data project :save-data)))

;; TODO(save-value): Get rid of these and go over any places where we handle nil resource types some special way.
(defn view-type [workspace]
  (workspace/get-view-type workspace :code))

(def search-fn r/search-fn)

(def search-value-fn r/search-value-fn)

(defn register-resource-types [workspace]
  (r/register-code-resource-type workspace
    :ext resource/placeholder-resource-type-ext
    :label "Unknown"
    :icon "icons/32/Icons_29-AT-Unknown.png"
    :node-type PlaceholderResourceNode
    :view-types [:code :default]
    :lazy-loaded true))
