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

(ns editor.binary-file
  (:require [dynamo.graph :as g]
            [editor.localization :as localization]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.workspace :as workspace]))

(set! *warn-on-reflection* true)

(g/defnk produce-build-targets [resource]
  (g/error-fatal (format "Cannot build binary resource of type '%s'" (resource/ext resource))))

(g/defnode BinaryFileNode
  (inherits resource-node/ResourceNode)
  (output build-targets g/Any produce-build-targets))

(defn register-resource-types [workspace]
  (for [ext ["jar" "lib" "dylib"]]
    (workspace/register-resource-type workspace
      :ext ext
      :label (localization/message (str "resource.type." ext))
      :view-types [:default]
      :node-type BinaryFileNode
      :icon "icons/32/Icons_29-AT-Unknown.png")))
