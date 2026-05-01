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

(ns editor.editor-localization-bundle
  (:require [dynamo.graph :as g]
            [editor.graph-util :as gu]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(g/defnode EditorLocalizationBundle
  (input resource-path+reader-fns g/Any :array :substitute gu/array-subst-remove-errors)
  (output bundle g/Any :cached (g/fnk [resource-path+reader-fns]
                                 (into {} resource-path+reader-fns))))

(defn bundle [editor-localization-bundle evaluation-context]
  (g/node-value editor-localization-bundle :bundle evaluation-context))