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

(ns util.fn
  (:refer-clojure :exclude [memoize partial])
  (:require [internal.java :as java]
            [util.coll :as coll :refer [pair]])
  (:import [clojure.lang ArityException Compiler Fn IFn IHashEq]
           [java.lang.reflect Method]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(definline ^:private with-memoize-info [memoized-fn cache arity]
  `(with-meta ~memoized-fn
              {::memoize-original ~memoized-fn
               ::memoize-arity ~arity
               ::memoize-cache ~cache}))

(defn- memoize-one [unary-fn]
  (let [cache (atom {})
        memoized-fn (fn memoized-fn [arg]
                      (let [cached-result (@cache arg cache)]
                        (if (identical? cache cached-result)
                          (let [result (unary-fn arg)]
                            (swap! cache assoc arg result)
                            result)
                          cached-result)))]
    (with-memoize-info memoized-fn cache 1)))

(defn- memoize-two [binary-fn]
  (let [cache (atom {})
        memoized-fn (fn memoized-fn [arg1 arg2]
                      (let [key (pair arg1 arg2)
                            cached-result (@cache key cache)]
                        (if (identical? cache cached-result)
                          (let [result (binary-fn arg1 arg2)]
                            (swap! cache assoc key result)
                            result)
                          cached-result)))]
    (with-memoize-info memoized-fn cache 2)))

(defn- memoize-any [any-fn ^long arity]
  (let [cache (atom {})
        memoized-fn (fn memoized-fn [& args]
                      (let [key (vec args)
                            cached-result (@cache key cache)]
                        (if (identical? cache cached-result)
                          (let [result (apply any-fn args)]
                            (swap! cache assoc key result)
                            result)
                          cached-result)))]
    (with-memoize-info memoized-fn cache arity)))

(defn- ifn-max-arity-raw
  ^long [ifn]
  (reduce (fn [^long max-arity ^Method method]
            (case (.getName method)
              "invoke" (max max-arity (.getParameterCount method))
              "getRequiredArity" (reduced -1) ; The function is variadic.
              max-arity))
          0
          (java/get-declared-methods (class ifn))))

(def ifn-max-arity (memoize-one ifn-max-arity-raw))

(defn max-arity
  "Returns the maximum number of arguments the supplied function can accept, or
  -1 if the function is variadic."
  ^long [ifn-or-var]
  (let [ifn (if (var? ifn-or-var)
              (var-get ifn-or-var)
              ifn-or-var)
        ^long max-arity (ifn-max-arity ifn)]
    (if (and (pos? max-arity)
             (var? ifn-or-var)
             (-> ifn-or-var meta :macro))
      (- max-arity 2) ; Subtract implicit arguments from macro.
      max-arity)))

(defn memoize
  "Like core.memoize, but uses an optimized cache based on the number of
  arguments the original function accepts. Also enables you to evict entries
  from the cache using the dememoize! function."
  [ifn]
  (if (::memoize-cache (meta ifn))
    ifn ; Already memoized.
    (let [arity (max-arity ifn)]
      (case arity
        1 (memoize-one ifn)
        2 (memoize-two ifn)
        (memoize-any ifn arity)))))

(defn evict-memoized!
  "Evict a previously cached result from the cache of a memoized function
  created by the functions in this module, if present. Returns nil."
  [memoized-fn & args]
  (let [metadata (meta memoized-fn)
        memoize-cache (::memoize-cache metadata)
        memoize-arity (::memoize-arity metadata)]
    (if (and memoize-cache memoize-arity)
      (let [arity (long memoize-arity)
            evicted-key (case arity
                          1 (first args)
                          2 (pair (first args) (second args))
                          args)]
        (if (and (not= -1 arity) ; Variadic.
                 (not= arity (coll/bounded-count (inc arity) args)))
          (let [original-fn (::memoize-original metadata)
                original-class-name (-> original-fn class .getName)]
            (throw (ArityException. arity original-class-name))))
        (swap! memoize-cache dissoc evicted-key)
        nil)
      (throw (IllegalArgumentException. "The function was not memoized by us.")))))

(defn declared-symbol [declared-fn]
  (if-not (fn? declared-fn)
    (throw (IllegalArgumentException. "The argument must be a declared function."))
    (let [class-name (.getName (class declared-fn))]
      (if (.find (re-matcher #"(__|\$)\d+$" class-name))
        (throw (IllegalArgumentException.
                 (format "Unable to get declared symbol from anonymous function `%s`."
                         class-name)))
        (let [namespaced-name (Compiler/demunge class-name)
              separator-index (.indexOf namespaced-name (int \/))
              namespace (when (pos? separator-index)
                          (.intern (subs namespaced-name 0 separator-index)))
              name (if (neg? separator-index)
                     namespaced-name
                     (.intern (subs namespaced-name (inc separator-index))))]
          (symbol namespace name))))))

(deftype PartialFn [pfn fn args]
  Fn
  IFn
  (invoke [_]
    (pfn))
  (invoke [_ a]
    (pfn a))
  (invoke [_ a b]
    (pfn a b))
  (invoke [_ a b c]
    (pfn a b c))
  (invoke [_ a b c d]
    (pfn a b c d))
  (invoke [_ a b c d e]
    (pfn a b c d e))
  (invoke [_ a b c d e f]
    (pfn a b c d e f))
  (invoke [_ a b c d e f g]
    (pfn a b c d e f g))
  (invoke [_ a b c d e f g h]
    (pfn a b c d e f g h))
  (invoke [_ a b c d e f g h i]
    (pfn a b c d e f g h i))
  (invoke [_ a b c d e f g h i j]
    (pfn a b c d e f g h i j))
  (invoke [_ a b c d e f g h i j k]
    (pfn a b c d e f g h i j k))
  (invoke [_ a b c d e f g h i j k l]
    (pfn a b c d e f g h i j k l))
  (invoke [_ a b c d e f g h i j k l m]
    (pfn a b c d e f g h i j k l m))
  (invoke [_ a b c d e f g h i j k l m n]
    (pfn a b c d e f g h i j k l m n))
  (invoke [_ a b c d e f g h i j k l m n o]
    (pfn a b c d e f g h i j k l m n o))
  (invoke [_ a b c d e f g h i j k l m n o p]
    (pfn a b c d e f g h i j k l m n o p))
  (invoke [_ a b c d e f g h i j k l m n o p q]
    (pfn a b c d e f g h i j k l m n o p q))
  (invoke [_ a b c d e f g h i j k l m n o p q r]
    (pfn a b c d e f g h i j k l m n o p q r))
  (invoke [_ a b c d e f g h i j k l m n o p q r s]
    (pfn a b c d e f g h i j k l m n o p q r s))
  (invoke [_ a b c d e f g h i j k l m n o p q r s t]
    (pfn a b c d e f g h i j k l m n o p q r s t))
  (invoke [_ a b c d e f g h i j k l m n o p q r s t rest]
    (apply pfn a b c d e f g h i j k l m n o p q r s t rest))
  (applyTo [_ arglist]
    (apply pfn arglist))
  IHashEq
  (hasheq [_]
    (hash [fn args]))
  Object
  (equals [_ obj]
    (if (instance? PartialFn obj)
      (let [^PartialFn that obj]
        (and (= fn (.-fn that))
             (= args (.-args that))))
      false)))

(defn partial
  "Like clojure.core/partial, but with equality semantics"
  [f & args]
  (PartialFn. (apply clojure.core/partial f args) f args))