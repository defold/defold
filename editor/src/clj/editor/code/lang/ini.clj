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

(ns editor.code.lang.ini)

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def grammar
  "Defold-style ini grammar"
  {:name "Ini"
   :scope-name "source.ini"
   :auto-insert {:characters {\[ \]}
                 :close-characters #{\]}
                 :exclude-scopes #{}
                 :open-scopes {\[ "punctuation.definition.entity.ini"}
                 :close-scopes {\] "punctuation.definition.entity.ini"}}
   :patterns [;; section headers like [foo]
              {:match #"^(\[)(.*?)(\])"
               :captures {1 {:name "punctuation.definition.entity.ini"}
                          2 {:name "entity.name.section.ini"}
                          3 {:name "punctuation.definition.entity.ini"}}
               :name "meta.section.ini"}
              ;; key = value
              {:match #"^\s*([^=]+?)(\s*=\s*)(.*)$"
               :captures {1 {:name "entity.name.key.ini"}
                          2 {:name "punctuation.separator.key-value.ini"}
                          3 {:name "string.unquoted.ini"}}
               :name "meta.key-value.ini"}]})