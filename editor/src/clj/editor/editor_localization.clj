;; Copyright 2020-2025 The Defold Foundation
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

(ns editor.editor-localization
  (:require [dynamo.graph :as g]
            [editor.code.data :as data]
            [editor.code.lang.java-properties :as java-properties]
            [editor.code.resource :as r]
            [editor.resource :as resource]
            [util.coll :as coll]))

(g/defnode EditorLocalizationNode
  (inherits r/CodeEditorResourceNode)
  (output locale+k->v g/Any :cached
          (g/fnk [resource lines]
            ;; todo probably wrong, we probably want to produce resource path + fn that takes
            ;;      evaluation context and returns a reader, and parse it in another place...
            ;;      or maybe not? maybe take evaluation context and return parsed data... but
            ;;      the parser should be used both here and in editor.localization! move the
            ;;      parser to java-properties ns?
            (coll/pair
              (resource/base-name resource)
              (java-properties/parse (data/lines-reader lines))))))

(defn register-resource-types [workspace]
  (r/register-code-resource-type
    workspace
    :ext "editor_localization"
    :node-type EditorLocalizationNode
    :label "Editor Localization"
    :icon "icons/32/Icons_05-Project-info.png"
    :language "properties"
    :view-types [:code :default]
    :view-opts {:code {:grammar java-properties/grammar}}))