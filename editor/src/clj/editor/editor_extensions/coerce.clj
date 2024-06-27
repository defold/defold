;; Copyright 2020-2024 The Defold Foundation
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
  "Define efficient LuaValue -> Clojure data structure conversions."
  (:refer-clojure :exclude [boolean integer hash-map vector-of])
  (:require [clojure.string :as string]
            [editor.editor-extensions.vm :as vm]
            [editor.util :as util]
            [util.coll :as coll])
  (:import [clojure.lang ITransientCollection]
           [org.luaj.vm2 LuaDouble LuaError LuaInteger LuaString LuaValue Varargs]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(deftype Failure [lua-value explain-fn])

(defmacro failure [lua-value-expr explain-expr]
  `(->Failure ~lua-value-expr (fn ~'explain-failure [] ~explain-expr)))

(defn failure? [x]
  (instance? Failure x))

(defn- failure-message
  ^String [vm ^Failure failure]
  (str (vm/lua-value->string vm (.-lua-value failure))
       " "
       ((.-explain-fn failure))))

(defn coerce
  "Coerce a LuaValue to a Clojure data structure according to coercer

  Either returns a Clojure data structure or throws a validation exception"
  [vm coercer lua-value]
  {:pre [(instance? LuaValue lua-value)]}
  (let [ret (coercer vm lua-value)]
    (if (failure? ret)
      (throw (LuaError. (failure-message vm ret)))
      ret)))

(defn enum
  "Coercer that deserializes a LuaValue into one of the provided constants

  Works with keywords too."
  [& values]
  (let [m (coll/pair-map-by vm/->lua values)]
    (fn coerce-enum [vm x]
      (let [v (m x ::not-found)]
        (if (identical? ::not-found v)
          (failure x (str "is not " (->> m
                                         keys
                                         (mapv #(vm/lua-value->string vm %))
                                         (sort)
                                         (util/join-words ", " " or "))))
          v)))))

(def string
  "Coercer the deserializes a Lua string into a string"
  (fn coerce-string
    [_ x]
    (if (instance? LuaString x)
      (str x)
      (failure x "is not a string"))))

(def integer
  "Coercer that deserializes a Lua integer into long"
  (fn coerce-integer [_ ^LuaValue x]
    (if (.isinttype x)
      (.tolong x)
      (failure x "is not an integer"))))

(def number
  "Coercer that deserializes Lua number either to long or to double"
  (fn coerce-number [_ ^LuaValue x]
    (condp instance? x
      LuaInteger (.tolong x)
      LuaDouble (.todouble x)
      (failure x "is not a number"))))

(defn tuple
  "Tuple coercer, deserializes Lua table into a tuple vector

  Does not enforce the absence of other keys in the table"
  [& coercers]
  {:pre [(not (empty? coercers))]}
  (let [coercers (vec coercers)]
    (fn coerce-tuple [vm ^LuaValue x]
      (if (.istable x)
        (let [acc (transduce
                    (comp
                      (map-indexed
                        (fn [i coercer]
                          (coercer vm (vm/with-lock vm (.rawget x (unchecked-inc-int i))))))
                      (halt-when failure?))
                    conj!
                    coercers)]
          (if (failure? acc)
            acc
            (persistent! acc)))
        (failure x "is not a tuple")))))

(def untouched
  "Pass-through coercer that does not perform any deserialized"
  (fn coerce-untouched [_ x]
    x))

(def null
  "Coercer that deserializes Lua nil into nil"
  (fn coerce-null [_ ^LuaValue x]
    (if (.isnil x)
      nil
      (failure x "is not nil"))))

(def boolean
  "Boolean coercer, requires the value to be a Lua boolean"
  (fn coerce-boolean [_ ^LuaValue x]
    (if (.isboolean x)
      (.toboolean x)
      (failure x "is not a boolean"))))

(def to-boolean
  "Boolean coercer, converts any LuaValue to boolean"
  (fn coerce-to-boolean [_ ^LuaValue x]
    (.toboolean x)))

(def any
  "Generic transformation of Lua to Clojure"
  (fn coerce-any [vm x]
    (vm/->clj x vm)))

(def userdata
  "Coercer that converts Lua userdata to the wrapped userdata object"
  (fn coerce-userdata [_ ^LuaValue x]
    (if (.isuserdata x)
      (.touserdata x)
      (failure x "is not a userdata"))))

(def function
  "Coercer that checks that the Lua value is a function"
  (fn coerce-function [_ ^LuaValue x]
    (if (.isfunction x)
      x
      (failure x "is not a function"))))

(defn wrap-with-pred
  "Wrap a coercer with an additional predicate that checks the coerced value"
  [coercer pred error-message]
  (fn coerce-pred [vm x]
    (let [ret (coercer vm x)]
      (cond
        (failure? ret) ret
        (not (pred ret)) (failure x error-message)
        :else ret))))

(defn vector-of
  "Collection coercer, converts LuaTable to a vector

  Optional kv-args:
    :min-count    minimum number of items in the collection
    :distinct     if true, ensures that the result does not have repeated items"
  [item-coercer & {:keys [min-count distinct]}]
  (let [f (fn coerce-coll-of [vm ^LuaValue x]
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
              (failure x "is not an array")))]
    (cond-> f

            min-count
            (wrap-with-pred
              #(<= ^long min-count (count %))
              (str "needs at least " min-count " element" (when (< 1 ^long min-count) "s")))

            distinct
            (wrap-with-pred #(apply distinct? %) "should not have repeated elements"))))

(defn hash-map
  "Coerces Lua table to a Clojure map with required and optional keys

  Optional kv-args:
    :req    a map from required key to a coercer for a value at that key
    :opt    a map from optional key to a coercer for a value at that key"
  [& {:keys [req opt]}]
  (let [reqs (mapv (fn [[k v]]
                     [(vm/->lua k) k v])
                   req)
        opts (mapv (fn [[k v]]
                     [(vm/->lua k) k v])
                   opt)]
    (fn coerce-record [vm ^LuaValue x]
      (if (.istable x)
        (let [acc (transient {})
              acc (reduce
                    (fn acc-req [acc [^LuaValue lua-key clj-key coerce-val]]
                      (let [^LuaValue lua-val (vm/with-lock vm (.rawget x lua-key))]
                        (if (.isnil lua-val)
                          (reduced (failure x (str "needs " (vm/lua-value->string vm lua-key) " key")))
                          (let [coerced-val (coerce-val vm lua-val)]
                            (if (failure? coerced-val)
                              (reduced coerced-val)
                              (assoc! acc clj-key coerced-val))))))
                    acc
                    reqs)]
          (if (failure? acc)
            acc
            (let [acc (reduce
                        (fn acc-opt [acc [^LuaValue lua-key clj-key coerce-val]]
                          (let [^LuaValue lua-val (vm/with-lock vm (.rawget x lua-key))]
                            (if (.isnil lua-val)
                              acc
                              (let [coerced-val (coerce-val vm lua-val)]
                                (if (failure? coerced-val)
                                  (reduced coerced-val)
                                  (assoc! acc clj-key coerced-val))))))
                        acc
                        opts)]
              (if (failure? acc)
                acc
                (persistent! acc)))))
        (failure x "is not a table")))))

(defn one-of
  "Tries several coercers in the provided order, and return first success result"
  [& coercers]
  {:pre [(not (empty? coercers))]}
  (let [v (vec coercers)]
    (fn coerce-one-of [vm x]
      (let [ret (transduce
                  (comp
                    (map #(% vm x))
                    (halt-when (complement failure?)))
                  conj!
                  v)]
        (if (instance? ITransientCollection ret)
          (let [failures (persistent! ret)]
            (failure x (str "does not satisfy any of its requirements:\n"
                            (->> failures
                                 (map (fn [failure]
                                        (string/join "\n"
                                                     (map str
                                                          (cons "- " (repeat "  "))
                                                          (string/split-lines (failure-message vm failure))))))
                                 (string/join "\n")))))
          ret)))))

(defn by-key
  "Coerce a Lua table where its key defines a shape of the table

  This is similar to spec's multi-spec, but specialized to tables where a single
  key defines the used coercer

  Args:
    k              key that should be present in a Lua table
    val->coerce    a map from value at the k to a LuaTable coercer

  The value at k is constrained to be enum of keys of val->coerce map"
  [k val->coerce]
  (let [lua-key (vm/->lua k)
        coerce-val (apply enum (keys val->coerce))]
    (fn coerce-by-key [vm ^LuaValue x]
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
        (failure x "is not a table")))))
