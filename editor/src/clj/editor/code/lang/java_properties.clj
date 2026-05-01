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

(ns editor.code.lang.java-properties
  (:import [java.io Reader]
           [java.util Properties]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def grammar
  {:name "Java Properties"
   :scope-name "source.properties"
   :line-comment "#"
   :patterns [;; comments (lines starting with #)
              {:match #"^\s*#.*$"
               :name "comment.line.number-sign.properties"
               :captures {0 {:name "punctuation.definition.comment.properties"}}}

              ;; key = value (ignores leading whitespace, allows spaces around =)
              {:match #"^\s*([^=]*)(\s*=\s*)(.*)$"
               :name "meta.key-value.properties"
               :captures {1 {:name "entity.name.key.properties"}
                          2 {:name "punctuation.separator.key-value.properties"}
                          3 {:name "string.unquoted.value.properties"}}}]})

(defn parse
  "Parses a reader of a properties file into a kv map"
  [^Reader reader]
  (with-open [reader reader]
    (into {} (doto (Properties.) (.load reader)))))
