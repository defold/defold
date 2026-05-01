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

(ns potemkin.imports-test)

(defn multi-arity-fn
  "Here is a doc-string."
  ([x])
  ([x y]))

(defprotocol TestProtocol
  (protocol-function [x a b c] "This is a protocol function."))

(defmacro multi-arity-macro
  "I am described."
  ([a])
  ([a b]))

(defn inlined-fn
  "Faster than the average invocation."
  {:inline (fn [x] x)}
  [x]
  x)

(def some-value 1)
