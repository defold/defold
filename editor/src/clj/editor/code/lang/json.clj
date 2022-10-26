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

(ns editor.code.lang.json
  (:require [clojure.data.json :as json]
            [editor.code.data :as data])
  (:import [java.io PushbackReader]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def grammar
  {:name "JSON"
   :scope-name "source.json"
   ;; indent patterns shamelessly stolen from textmate:
   ;; https://github.com/textmate/json.tmbundle/blob/master/Preferences/Miscellaneous.tmPreferences
   :indent {:begin #"^.*(\{[^}]*|\[[^\]]*)$"
            :end #"^\s*[}\]],?\s*$"}
   :patterns [{:match #"\b(?:true|false|null)\b"
               :name "constant.language.json"}
              {:match #"(?x)-?(?:0|[1-9]\d*)(?:\n(?:\n\.\d+)?(?:[eE][+-]?\d+)?)?"
               :name "constant.numeric.json"}
              {:begin #"\""
               :begin-captures {0 {:name "punctuation.definition.string.begin.json"}}
               :end #"\""
               :end-captures {0 {:name "punctuation.definition.string.end.json"}}
               :name "string.quoted.double.json"}]})

(defn lines->json [lines & options]
  (with-open [lines-reader (data/lines-reader lines)
              pushback-reader (PushbackReader. lines-reader)]
    (apply json/read pushback-reader options)))
