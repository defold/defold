;; Copyright 2020-2022 The Defold Foundation
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

(ns editor.code.text-file
  (:require [dynamo.graph :as g]
            [editor.code.lang.cish :as cish]
            [editor.code.lang.json :as json]
            [editor.code.resource :as r]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def ^:private cish-opts {:code {:grammar cish/grammar}})

(def ^:private json-opts {:code {:grammar json/grammar}})

(def ^:private text-file-defs
  [{:ext "cpp"
    :label "C++"
    :icon "icons/32/Icons_12-Script-type.png"
    :view-opts cish-opts}
   {:ext "cxx"
    :label "C++"
    :icon "icons/32/Icons_12-Script-type.png"
    :view-opts cish-opts}
   {:ext "C"
    :label "C++"
    :icon "icons/32/Icons_12-Script-type.png"
    :view-opts cish-opts}
   {:ext "cc"
    :label "C++"
    :icon "icons/32/Icons_12-Script-type.png"
    :view-opts cish-opts}
   {:ext "hpp"
    :label "C++"
    :icon "icons/32/Icons_12-Script-type.png"
    :view-opts cish-opts}
   {:ext "h"
    :label "C/C++ Header"
    :icon "icons/32/Icons_12-Script-type.png"
    :view-opts cish-opts}
   {:ext "mm"
    :label "Objective-C"
    :icon "icons/32/Icons_12-Script-type.png"
    :view-opts cish-opts}
   {:ext "c"
    :label "C"
    :icon "icons/32/Icons_12-Script-type.png"
    :view-opts cish-opts}
   {:ext "json"
    :label "JSON"
    :icon "icons/32/Icons_29-AT-Unknown.png"
    :view-opts json-opts}
   {:ext "manifest"
    :label "Manifest"
    :icon "icons/32/Icons_11-Script-general.png"}
   {:ext "defignore"
    :label "Defignore"
    :icon "icons/32/Icons_11-Script-general.png"}])

(g/defnode TextNode
  (inherits r/CodeEditorResourceNode))

(defn register-resource-types [workspace]
  (for [def text-file-defs
        :let [args (assoc def
                          :node-type TextNode
                          :view-types [:code :default])]]
    (apply r/register-code-resource-type workspace (mapcat identity args))))
