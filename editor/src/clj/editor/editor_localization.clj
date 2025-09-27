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
            [editor.defold-project :as project]
            [editor.resource :as resource]
            [util.coll :as coll]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(g/defnode EditorLocalizationNode
  (inherits r/CodeEditorResourceNode)
  (output resource-path+reader-fn g/Any :cached
          (g/fnk [resource lines]
            (coll/pair
              (resource/proj-path resource)
              #(data/lines-reader lines)))))

(defn- additional-load-fn [project self _resource]
  (g/with-auto-evaluation-context evaluation-context
    (let [bundle (project/editor-localization-bundle project evaluation-context)]
      (g/connect self :resource-path+reader-fn bundle :resource-path+reader-fns))))

(defn register-resource-types [workspace]
  (r/register-code-resource-type
    workspace
    :ext "editor_localization"
    :node-type EditorLocalizationNode
    :label "Editor Localization"
    :icon "icons/32/Icons_05-Project-info.png"
    :language "properties"
    :additional-load-fn additional-load-fn
    :view-types [:code :default]
    :view-opts {:code {:grammar java-properties/grammar}}))
