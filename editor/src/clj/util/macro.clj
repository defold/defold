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

(ns util.macro
  (:require [clojure.string :as string]
            [clojure.walk :as walk]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- claim-conformed-symbol-name!
  ^String [symbol-name name-without-suffix assigned-names-by-gensym-name-atom assigned-name-fn]
  {:pre [(string? symbol-name)
         (not-empty symbol-name)
         (string? name-without-suffix)
         (not-empty name-without-suffix)
         (ifn? assigned-name-fn)]}
  (-> assigned-names-by-gensym-name-atom
      (swap! (fn [assigned-names-by-gensym-name]
               (if (contains? assigned-names-by-gensym-name symbol-name)
                 assigned-names-by-gensym-name
                 (assoc assigned-names-by-gensym-name
                   symbol-name
                   (let [taken-name? (set (vals assigned-names-by-gensym-name))]
                     (loop [numeric-suffix 0]
                       (let [assigned-name (assigned-name-fn name-without-suffix numeric-suffix)]
                         (assert (string? assigned-name))
                         (if (taken-name? assigned-name)
                           (recur (inc numeric-suffix))
                           assigned-name))))))))
      (get symbol-name)))

(defn conform-gensym-name!
  "Strip any randomly generated gensym suffix from the supplied symbol-name.
  Call the assigned-name-fn with the stipped name string and the number
  of times the resulting stripped name has previously been seen in the supplied
  assigned-names-by-gensym-name-atom. The assigned-name-fn must return a unique
  string based on these arguments. Adds the resulting entry to the
  assigned-names-by-gensym-name-atom and returns the assigned name."
  ^String [symbol-name assigned-names-by-gensym-name-atom assigned-name-fn]
  (let [name-without-hash-auto-suffix (string/replace symbol-name #"__(\d+)__auto__$" "")]
    (if (not= symbol-name name-without-hash-auto-suffix)
      (claim-conformed-symbol-name! symbol-name name-without-hash-auto-suffix assigned-names-by-gensym-name-atom assigned-name-fn)
      (let [name-without-gensym-suffix (string/replace symbol-name #"__(\d+)$" "")]
        (if (not= symbol-name name-without-gensym-suffix)
          (claim-conformed-symbol-name! symbol-name name-without-gensym-suffix assigned-names-by-gensym-name-atom assigned-name-fn)
          symbol-name)))))

(defn conform-gensym!
  "If called with a symbol, strip any randomly generated gensym suffix from its
  name and call the assigned-name-fn with the stipped name string and the number
  of times the resulting stripped name has previously been seen in the supplied
  assigned-names-by-gensym-name-atom. The assigned-name-fn must return a unique
  string based on these arguments. Returns a new symbol with the assigned-name
  and the same namespace as the original symbol. If called with anything but a
  symbol, return it unaltered."
  [sub-form assigned-names-by-gensym-name-atom assigned-name-fn]
  (if (symbol? sub-form)
    (-> (symbol (namespace sub-form)
                (conform-gensym-name! (name sub-form) assigned-names-by-gensym-name-atom assigned-name-fn))
        (with-meta (meta sub-form)))
    sub-form))

(defn conform-gensyms
  "Remove any random number suffixes produced by gensym from the supplied form
  and replace them with consistent numbers based on the order they appear in the
  form. Useful when testing the output of macros.

  Args:
    form
      The form to replace gensyms in.

    assigned-name-fn
      Optional function that will be called for each unique gensym we encounter.
      Should take a name-without-suffix string and a numeric-suffix integer and
      return a name string to assign to the gensym."
  ([form]
   (conform-gensyms
     form
     (fn [name-without-suffix numeric-suffix]
       (str name-without-suffix "__" numeric-suffix))))
  ([form assigned-name-fn]
   (let [assigned-names-by-gensym-name-atom (atom {})]
     (walk/postwalk
       #(conform-gensym! % assigned-names-by-gensym-name-atom assigned-name-fn)
       form))))
