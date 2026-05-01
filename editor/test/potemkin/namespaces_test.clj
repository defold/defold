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

(ns potemkin.namespaces-test
  (:require
   [clojure.repl :as repl]
   [clojure.string :as str]
   [clojure.test :refer :all]
   [potemkin.imports-test :as i]
   [potemkin.namespaces :as pn]))

(pn/import-macro i/multi-arity-macro)
(pn/import-macro i/multi-arity-macro alt-macro-name)
(pn/import-fn i/multi-arity-fn)
(pn/import-fn i/multi-arity-fn alt-name)
(pn/import-fn i/protocol-function)
(pn/import-fn i/inlined-fn)
(pn/import-def i/some-value)

(defn drop-lines [n s]
  (->> s str/split-lines (drop n) (interpose "\n") (apply str)))

(defmacro out= [& args]
  `(= ~@(map (fn [x] `(with-out-str ~x)) args)))

(defmacro rest-out= [& args]
  `(= ~@(map (fn [x] `(drop-lines 2 (with-out-str ~x))) args)))

(deftest test-import-macro
  (is (out= (repl/source i/multi-arity-macro) (repl/source multi-arity-macro)))
  (is (rest-out= (repl/doc i/multi-arity-macro) (repl/doc multi-arity-macro)))
  (is (out= (repl/source i/multi-arity-macro) (repl/source alt-macro-name)))
  (is (rest-out= (repl/doc i/multi-arity-macro) (repl/doc alt-macro-name))))

(deftest test-import-fn
  (is (= 1 (inlined-fn 1)))
  (is (= 1 (apply inlined-fn [1])))
  (is (out= (repl/source i/multi-arity-fn) (repl/source multi-arity-fn)))
  (is (rest-out= (repl/doc i/multi-arity-fn) (repl/doc multi-arity-fn)))
  (is (out= (repl/source i/multi-arity-fn) (repl/source alt-name)))
  (is (rest-out= (repl/doc i/multi-arity-fn) (repl/doc alt-name)))
  (is (rest-out= (repl/doc i/protocol-function) (repl/doc protocol-function))))

(deftest test-points-to-the-value-after-reload
  (is (= 1 some-value))
  (require 'potemkin.imports-test :reload)
  (is (= 1 some-value)))
