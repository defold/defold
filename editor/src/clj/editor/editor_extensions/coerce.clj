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

(ns editor.editor-extensions.coerce
  "Define efficient Varargs (0 or more LuaValues, typically 1) to Clojure
  conversions."
  (:refer-clojure :exclude [boolean integer hash-map vector-of])
  (:require [clojure.string :as string]
            [editor.editor-extensions.vm :as vm]
            [editor.util :as util]
            [util.coll :as coll]
            [util.eduction :as e]
            [util.fn :as fn])
  (:import [org.luaj.vm2 LuaDouble LuaError LuaInteger LuaString LuaValue Varargs]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(deftype Failure [lua-varargs explain-fn])

(defmacro failure [lua-varargs-expr explain-expr]
  `(->Failure ~lua-varargs-expr (fn ~'explain-failure [] ~explain-expr)))

(defn failure? [x]
  (instance? Failure x))

(defn- failure-message
  ^String [vm ^Failure failure]
  (let [^Varargs varargs (.-lua-varargs failure)
        n (.narg varargs)]
    (if (zero? n)
      ((.-explain-fn failure))
      (str (->> (range (.narg varargs))
                (map #(vm/lua-value->string vm (.arg varargs (inc (int %)))))
                (string/join ", "))
           " "
           ((.-explain-fn failure))))))

(defn coerce
  "Coerce Varargs to a Clojure data structure according to coercer

  Typically, coerces the first arg of Varargs

  Either returns a Clojure data structure or throws a validation exception"
  [vm coercer varargs]
  {:pre [(instance? Varargs varargs)]}
  (let [ret (coercer vm varargs)]
    (if (failure? ret)
      (throw (LuaError. (failure-message vm ret)))
      ret)))

(def enum-lua-value-cache
  "A memoized function that converts a Clojure value to Lua value"
  (fn/memoize vm/->lua))

(defn schema
  "Given a coercer, returns a schema, i.e. the expected data shape description

  The returned map has a :type key that defines the rest of the map. The list of
  possible types:
    :nil         the expected Lua value is nil
    :any         there are no expectations about the Lua value
    :boolean     a boolean
    :string      a string
    :number      a number
    :integer     an integer
    :function    a function
    :table       a table
    :userdata    an userdata
    :array       homogeneous array; extra keys:
                   :item    schema of the expected item type
    :tuple       heterogeneous array of a fixed size; extra keys:
                   :items    a vector of schemas that describe expected Lua
                             values
    :one-of      union type, extra keys:
                   :schemas    alternative schemas vector, guaranteed to have at
                               least 2 schemas
    :enum        the expected Lua value is defined by a fixed set of values;
                 extra keys:
                   :values    a vector of allowed values (JVM data structures,
                              e.g. keywords, strings, number constants)"
  [coercer]
  {:post [(some? %)]}
  (:schema (meta coercer)))

(defn enum
  "Coercer that deserializes 1st Varargs arg into one of the provided constants

  Works with keywords too."
  [& values]
  (let [m (coll/pair-map-by enum-lua-value-cache values)
        ks (mapv enum-lua-value-cache values)]
    ^{:schema {:type :enum :values (vec values)}}
    (fn coerce-enum [vm ^Varargs x]
      (let [x (.arg1 x)
            v (m x ::not-found)]
        (if (identical? ::not-found v)
          (failure x (str "is not " (->> ks
                                         (mapv #(vm/lua-value->string vm %))
                                         (util/join-words ", " " or "))))
          v)))))

(defn const
  "Coercer that deserializes 1st Varargs arg into a predefined expected constant"
  [value]
  (let [expected-lua-value (enum-lua-value-cache value)]
    ^{:schema {:type :enum :values [value]}}
    (fn coerce-const [vm ^Varargs x]
      (let [x (.arg1 x)]
        (if (vm/with-lock vm (.eq_b x expected-lua-value))
          value
          (failure x (str "is not " (vm/lua-value->string vm expected-lua-value))))))))

(def string
  "Coercer that deserializes 1st Varargs arg into a string"
  ^{:schema {:type :string}}
  (fn coerce-string
    [_ ^Varargs x]
    (let [x (.arg1 x)]
      (if (instance? LuaString x)
        (str x)
        (failure x "is not a string")))))

(def to-string
  "Coercer that deserializes any 1st Varargs arg to string"
  ^{:schema {:type :any}}
  (fn coerce-to-string [_ ^Varargs x]
    ;; This coercer is equivalent to a simple toString call because default
    ;; toString implementation in LuaJ is thread-safe. It's useful to have such
    ;; a coercer still: any code that does Lua->Clojure transformation can use
    ;; this namespace without concern if some functions are thread-safe and some
    ;; are not
    (str (.arg1 x))))

(def integer
  "Coercer that deserializes 1st Varargs arg into long"
  ^{:schema {:type :integer}}
  (fn coerce-integer [_ ^Varargs x]
    (let [x (.arg1 x)]
      (if (.isinttype x)
        (.tolong x)
        (failure x "is not an integer")))))

(def number
  "Coercer that deserializes 1st Varargs arg either to long or to double"
  ^{:schema {:type :number}}
  (fn coerce-number [_ ^Varargs x]
    (let [x (.arg1 x)]
      (condp instance? x
        LuaInteger (.tolong x)
        LuaDouble (.todouble x)
        (failure x "is not a number")))))

(defn tuple
  "Tuple coercer, deserializes 1st Varargs arg Lua table into a tuple vector

  Does not enforce the absence of other keys in the table"
  [& coercers]
  {:pre [(not (empty? coercers))]}
  (let [coercers (vec coercers)]
    ^{:schema {:type :tuple
               :items (mapv schema coercers)}}
    (fn coerce-tuple [vm ^Varargs x]
      (let [x (.arg1 x)]
        (if (.istable x)
          (let [acc (vm/with-lock vm
                      (transduce
                        (comp
                          (map-indexed
                            (fn [i coercer]
                              (coercer vm (.rawget x (unchecked-inc-int i)))))
                          (halt-when failure?))
                        conj!
                        coercers))]
            (if (failure? acc)
              acc
              (persistent! acc)))
          (failure x "is not a tuple"))))))

(def untouched
  "Pass-through coercer that returns 1st Varargs arg without any deserialization"
  ^{:schema {:type :any}}
  (fn coerce-untouched [_ ^Varargs x]
    (.arg1 x)))

(def null
  "Coercer that deserializes 1st Varargs arg into nil"
  ^{:schema {:type :nil}}
  (fn coerce-null [_ ^Varargs x]
    (let [x (.arg1 x)]
      (if (.isnil x)
        nil
        (failure x "is not nil")))))

(def boolean
  "Boolean coercer, requires the 1st Varargs arg to be a Lua boolean"
  ^{:schema {:type :boolean}}
  (fn coerce-boolean [_ ^Varargs x]
    (let [x (.arg1 x)]
      (if (.isboolean x)
        (.toboolean x)
        (failure x "is not a boolean")))))

(def to-boolean
  "Boolean coercer, converts 1st Varargs arg to boolean"
  ^{:schema {:type :any}}
  (fn coerce-to-boolean [_ ^Varargs x]
    (let [x (.arg1 x)]
      (.toboolean x))))

(def any
  "Generic transformation of 1st Varargs arg to Clojure"
  ^{:schema {:type :any}}
  (fn coerce-any [vm ^Varargs x]
    (vm/->clj (.arg1 x) vm)))

(def userdata
  "Coercer that converts 1st Varargs arg userdata to the wrapped userdata object"
  ^{:schema {:type :userdata}}
  (fn coerce-userdata [_ ^Varargs x]
    (let [x (.arg1 x)]
      (if (.isuserdata x)
        (.touserdata x)
        (failure x "is not a userdata")))))

(def function
  "Coercer that checks that the 1st Varargs arg is a function"
  ^{:schema {:type :function}}
  (fn coerce-function [_ ^Varargs x]
    (let [x (.arg1 x)]
      (if (.isfunction x)
        x
        (failure x "is not a function")))))

(defn wrap-with-pred
  "Wrap a coercer with an additional predicate that checks the coerced value"
  [coercer pred error-message]
  ^{:schema (schema coercer)}
  (fn coerce-pred [vm x]
    (let [ret (coercer vm x)]
      (cond
        (failure? ret) ret
        (not (pred ret)) (failure x error-message)
        :else ret))))

(defn wrap-transform
  "Wrap a coercer with a transformation step that changes the coerced value"
  [coercer f & args]
  ^{:schema (schema coercer)}
  (fn coerce-transform [vm x]
    (let [ret (coercer vm x)]
      (if (failure? ret)
        ret
        (apply f ret args)))))

(defn vector-of
  "Collection coercer, converts 1st Varargs arg LuaTable to a vector

  Optional kv-args:
    :min-count    minimum number of items in the collection
    :distinct     if true, ensures that the result does not have repeated items"
  [item-coercer & {:keys [min-count distinct]}]
  (let [f ^{:schema {:type :array :item (schema item-coercer)}}
          (fn coerce-coll-of [vm ^Varargs x]
            (let [x (.arg1 x)]
              (if (.istable x)
                (let [acc (vm/with-lock vm
                            (let [len (.rawlen x)]
                              (transduce
                                (comp
                                  (map (fn [i]
                                         (item-coercer vm (.rawget x (unchecked-inc-int i)))))
                                  (halt-when failure?))
                                conj!
                                (range len))))]
                  (if (failure? acc)
                    acc
                    (persistent! acc)))
                (failure x "is not an array"))))]
    (cond-> f

            min-count
            (wrap-with-pred
              #(<= ^long min-count (count %))
              (str "needs at least " min-count " element" (when (< 1 ^long min-count) "s")))

            distinct
            (wrap-with-pred #(apply distinct? %) "should not have repeated elements"))))

(defn hash-map
  "Coerces 1st Varargs arg to a Clojure map with required and optional keys

  Optional kv-args:
    :req           a map from required key to a coercer for a value at that key
    :opt           a map from optional key to a coercer for a value at that key
    :extra-keys    boolean, whether to allow unspecified keys (default true)"
  [& {:keys [req opt extra-keys]
      :or {extra-keys true}}]
  (let [required-keys (mapv key req)
        lua-key->clj-key+coerce-val (coll/pair-map-by (comp enum-lua-value-cache key) (e/concat req opt))]
    ^{:schema {:type :table}}
    (fn coerce-hash-map [vm ^Varargs x]
      (let [x (.arg1 x)]
        (if (.istable x)
          (let [acc (reduce
                      (fn [acc ^Varargs varargs]
                        (let [lua-key (.arg1 varargs)]
                          (if-let [[clj-key coerce-val] (lua-key->clj-key+coerce-val lua-key)]
                            (let [coerced-val (coerce-val vm (.arg varargs 2))]
                              (if (failure? coerced-val)
                                (reduced coerced-val)
                                (assoc! acc clj-key coerced-val)))
                            (if extra-keys
                              acc
                              (reduced (failure x (str "cannot have the " (vm/lua-value->string vm lua-key) " key")))))))
                      (transient {})
                      (vm/lua-table-reducer vm x))]
            (if (failure? acc)
              acc
              (let [acc (reduce
                          (fn [acc k]
                            (if (contains? acc k)
                              acc
                              (reduced (failure x (str "must have the " (vm/lua-value->string vm (enum-lua-value-cache k)) " key")))))
                          acc
                          required-keys)]
                (if (failure? acc)
                  acc
                  (persistent! acc)))))
          (failure x "is not a table"))))))

(defn map-of
  "Coerces 1st Varargs arg to a homogeneous Clojure map"
  [key-coercer val-coercer]
  ^{:schema {:type :table}}
  (fn coerce-map-of [vm ^Varargs x]
    (let [x (.arg1 x)]
      (if (.istable x)
        (let [acc (transient {})
              acc (reduce
                    (fn acc-kv [acc ^Varargs varargs]
                      (let [k (key-coercer vm (.arg1 varargs))]
                        (if (failure? k)
                          (reduced k)
                          (let [v (val-coercer vm (.arg varargs 2))]
                            (if (failure? v)
                              (reduced v)
                              (assoc! acc k v))))))
                    acc
                    (vm/lua-table-reducer vm x))]
          (if (failure? acc)
            acc
            (persistent! acc)))
        (failure x "is not a table")))))

(defn one-of
  "Tries several coercers in the provided order, returns first success result"
  [& coercers]
  {:pre [(not (empty? coercers))]}
  (let [v (vec coercers)
        schemas (into []
                      (comp
                        (map schema)
                        (mapcat #(if (identical? :one-of (:type %)) (:schemas %) [%]))
                        (distinct))
                      coercers)]
    ^{:schema (if (= 1 (count schemas)) (schemas 0) {:type :one-of :schemas schemas})}
    (fn coerce-one-of [vm x]
      (let [ret (reduce
                  (fn [_ coercer]
                    (let [v (coercer vm x)]
                      (if (failure? v)
                        v
                        (reduced v))))
                  nil
                  v)]
        (if (failure? ret)
          (failure x (str "does not satisfy any of its requirements:\n"
                          (->> coercers
                               (map (fn [coercer]
                                      (let [failure (coercer vm x)]
                                        (assert (failure? failure))
                                        (string/join "\n"
                                                     (map str
                                                          (cons "- " (repeat "  "))
                                                          (string/split-lines (failure-message vm failure)))))))
                               (string/join "\n"))))
          ret)))))

(defn by-key
  "Coerce a 1st Varargs arg where its key defines a shape of the table

  This is similar to spec's multi-spec, but specialized to tables where a single
  key defines the used coercer

  Args:
    k              key that should be present in a Lua table
    val->coerce    a map from value at the k to a LuaTable coercer

  The value at k is constrained to be enum of keys of val->coerce map"
  [k val->coerce]
  (let [lua-key (vm/->lua k)
        coerce-val (apply enum (keys val->coerce))]
    ^{:schema {:type :table}}
    (fn coerce-by-key [vm ^Varargs x]
      (let [x (.arg1 x)]
        (if (.istable x)
          (let [^LuaValue lua-val (vm/with-lock vm (.rawget x ^LuaValue lua-key))]
            (if (.isnil lua-val)
              (failure x (str "needs " (vm/lua-value->string vm lua-key) " key"))
              (let [v (coerce-val vm lua-val)]
                (if (failure? v)
                  v
                  (let [m ((val->coerce v) vm x)]
                    (if (failure? m)
                      m
                      (assoc m k v)))))))
          (failure x "is not a table"))))))

(defn recursive
  "Create a recursive coercer that can refer to itself

  Args:
    coercer-fn    fn that produces the final coercer, will receive a reference
                  coercer as an argument, the reference coercer should be used
                  to refer to the coercer returned by the function"
  [coercer-fn]
  (let [vol (volatile! nil)
        coerce-recursive ^{:schema {:type :any}} (fn coerce-recursive [vm x]
                                                   (@vol vm x))]
    (vreset! vol (coercer-fn coerce-recursive))
    coerce-recursive))

(defn- regex-failure [vm failures final-failure]
  (if (coll/empty? failures)
    final-failure
    (failure
      LuaValue/NONE
      (str "Invalid argument:\n"
           (->> (conj failures final-failure)
                (map (fn [failure]
                       (string/join
                         "\n"
                         (map str
                              (cons "- " (repeat "  "))
                              (string/split-lines (failure-message vm failure))))))
                (string/join "\n"))))))

(defn regex
  "Create a regex-like coercer that coerces Varargs to a clojure map

  Returns a coerced map of identifiers to coerced values

  Args:
    ops    regex ops specification, a flat sequence of following values:
             keyword       required, subsequence identifier
             quantifier    optional, currently only :? and :1 are supported
             coercer       required, subsequence LuaValue coercer

  Examples:
    ;; Coerce Varargs with 0-3 LuaValues into an HTTP response map
    (coerce/regex :status :? coerce/integer
                  :headers :? (coerce/map-of coerce/string coerce/string)
                  :body :? coerce/string)
    ;; Coerce 2-element Varargs into person map (:1 can be omitted)
    (coerce/regex :first-name :1 coerce/string
                  :last-name :1 coerce/string)"
  [& ops]
  (let [ops (reduce
              (fn [acc el]
                (let [last-op (peek acc)]
                  (cond
                    (not (:key last-op))
                    (do
                      (when-not (keyword? el)
                        (throw (IllegalArgumentException. "op key must be a keyword")))
                      (conj acc (assoc last-op :key el)))

                    (and (not (:quantifier last-op)) (#{:? :1} el))
                    (assoc acc (dec (count acc)) (assoc last-op :quantifier el))

                    (and (not (:coerce last-op)) (fn? el))
                    (assoc acc (dec (count acc)) (-> last-op (update :quantifier #(or % :1)) (assoc :coerce el)))

                    (and (keyword? el) (:coerce last-op))
                    (conj acc {:key el})

                    :else
                    (throw (IllegalArgumentException. (str "unexpected input: " el))))))
              []
              ops)
        ops-len (count ops)]
    ^{:schema {:type :any}}
    (fn coerce-regex [vm ^Varargs inputs]
      (let [inputs-len (.narg inputs)]
        (loop [acc {}
               input-index 0
               op-index 0
               failures []]
          (cond
            (= input-index inputs-len) ; finished input
            (if (= op-index ops-len) ; also finished ops
              acc
              (case (:quantifier (ops op-index)) ; check if remaining ops are all optional
                :1 (regex-failure vm failures (failure LuaValue/NONE "more arguments expected"))
                :? (recur acc input-index (inc op-index) failures)))

            (= op-index ops-len) ; finished ops, but there is more input
            (let [extra (.subargs inputs (inc input-index))]
              (regex-failure vm failures (failure extra (if (= 1 (.narg extra)) "is unexpected" "are unexpected"))))

            :else
            (let [{:keys [key quantifier coerce]} (ops op-index)
                  lua-value (.arg inputs (inc input-index))
                  v (coerce vm lua-value)]
              (case quantifier
                :1 (if (failure? v)
                     (regex-failure vm failures v)
                     (recur (assoc acc key v) (inc input-index) (inc op-index) []))
                :? (if (failure? v)
                     (recur acc input-index (inc op-index) (conj failures v))
                     (recur (assoc acc key v) (inc input-index) (inc op-index) []))))))))))
