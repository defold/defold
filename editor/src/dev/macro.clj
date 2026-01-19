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

(ns macro
  (:require [clojure.pprint :as pprint]
            [clojure.walk :as walk]
            [util.coll :as coll :refer [pair]]
            [util.macro :as macro]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- ns->namespace-name
  ^String [ns]
  (name (ns-name ns)))

(defn- class->canonical-symbol [^Class class]
  (symbol (.getName class)))

(defn- make-alias-names-by-namespace-name [ns]
  (into {(ns->namespace-name 'clojure.core) nil
         (ns->namespace-name ns) nil}
        (map (fn [[alias-symbol referenced-ns]]
               (pair (ns->namespace-name referenced-ns)
                     (name alias-symbol))))
        (ns-aliases ns)))

(defn- make-simple-symbols-by-canonical-symbol [ns]
  (into {}
        (map (fn [[alias-symbol imported-class]]
               (pair (class->canonical-symbol imported-class)
                     alias-symbol)))
        (ns-imports ns)))

(defn- simplify-namespace-name
  ^String [namespace-name alias-names-by-namespace-name]
  {:pre [(or (nil? namespace-name) (string? namespace-name))
         (map? alias-names-by-namespace-name)]}
  (let [alias-name (get alias-names-by-namespace-name namespace-name ::not-found)]
    (case alias-name
      ::not-found namespace-name
      alias-name)))

(defn- assigned-gensym-name
  ^String [name-without-suffix ^long numeric-suffix]
  (case numeric-suffix
    0 (str name-without-suffix \#)
    (str name-without-suffix \# numeric-suffix)))

(defn- simplify-symbol [expression alias-names-by-namespace-name simple-names-by-gensym-name-atom]
  (-> expression
      (namespace)
      (simplify-namespace-name alias-names-by-namespace-name)
      (symbol (-> expression name (macro/conform-gensym-name! simple-names-by-gensym-name-atom assigned-gensym-name)))
      (with-meta (meta expression))))

(defn- simplify-keyword [expression alias-names-by-namespace-name]
  (-> expression
      (namespace)
      (simplify-namespace-name alias-names-by-namespace-name)
      (keyword (name expression))))

(defn- simplify-expression-impl [expression alias-names-by-namespace-name simple-symbols-by-canonical-symbol simple-names-by-gensym-name-atom]
  (cond
    (record? expression)
    expression

    (map? expression)
    (into (coll/empty-with-meta expression)
          (map (fn [[key value]]
                 (pair (simplify-expression-impl key alias-names-by-namespace-name simple-symbols-by-canonical-symbol simple-names-by-gensym-name-atom)
                       (simplify-expression-impl value alias-names-by-namespace-name simple-symbols-by-canonical-symbol simple-names-by-gensym-name-atom))))
          expression)

    (or (vector? expression)
        (set? expression))
    (into (coll/empty-with-meta expression)
          (map #(simplify-expression-impl % alias-names-by-namespace-name simple-symbols-by-canonical-symbol simple-names-by-gensym-name-atom))
          expression)

    (coll/list-or-cons? expression)
    (into (coll/empty-with-meta expression)
          (map #(simplify-expression-impl % alias-names-by-namespace-name simple-symbols-by-canonical-symbol simple-names-by-gensym-name-atom))
          (reverse expression))

    (symbol? expression)
    (or (get simple-symbols-by-canonical-symbol expression)
        (simplify-symbol expression alias-names-by-namespace-name simple-names-by-gensym-name-atom))

    (keyword? expression)
    (simplify-keyword expression alias-names-by-namespace-name)

    :else
    expression))

(defmacro simplify-expression
  ([expression]
   `(simplify-expression *ns* ~expression))
  ([ns expression]
   `(let [ns# ~ns]
      (#'simplify-expression-impl
        ~expression
        (#'make-alias-names-by-namespace-name ns#)
        (#'make-simple-symbols-by-canonical-symbol ns#)
        (atom {})))))

(defn- pprint-code-impl [expression]
  (binding [pprint/*print-suppress-namespaces* false
            pprint/*print-right-margin* 100
            pprint/*print-miser-width* 60]
    (pprint/with-pprint-dispatch
      pprint/code-dispatch
      (pprint/pprint expression))))

(defmacro pprint-code
  "Pretty-print the supplied code expression while attempting to retain readable
  formatting. Useful when developing macros."
  ([expression]
   `(#'pprint-code-impl (simplify-expression ~expression)))
  ([ns expression]
   `(#'pprint-code-impl (simplify-expression ~ns ~expression))))

(defmacro pprint-macroexpanded
  "Recursively macroexpand and pretty-print the supplied code expression while
  attempting to retain readable formatting. Useful when developing macros."
  [expression]
  `(pprint-code
     ~(walk/macroexpand-all expression)))

(defmacro macroexpanded
  "Recursively macroexpand the supplied code expression while trying to keep the
  output as human-readable as possible. Useful when developing macros."
  ([expression]
   `(simplify-expression ~(walk/macroexpand-all expression)))
  ([ns expression]
   `(simplify-expression ~ns ~(walk/macroexpand-all expression))))
