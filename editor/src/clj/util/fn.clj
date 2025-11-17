;; Copyright 2020-2025 The Defold Foundation
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
  (:refer-clojure :exclude [memoize partial]
                  :rename {and and*
                           or or*})
  (:require [internal.java :as java]
            [util.coll :as coll :refer [pair]])
  (:import [clojure.lang ArityException Compiler Fn IFn IHashEq MultiFn]
           [java.lang.reflect Method]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defonce constantly-false (constantly false))
(defonce constantly-true (constantly true))
(defonce constantly-nil (constantly nil))
(defonce constantly-constantly-nil (constantly constantly-nil))

(defmacro constantly-fail
  ([message-expr]
   `(fn ~'fail ~'[& args]
      (throw (ex-info ~message-expr
                      {:args ~'args}))))
  ([message-expr context-map-expr]
   `(fn ~'fail ~'[& args]
      (throw (ex-info ~message-expr
                      (merge {:args ~'args}
                             ~context-map-expr))))))

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

(defn- ifn-class-arities-raw
  [^Class ifn-class]
  (into #{}
        (keep (fn [^Method method]
                (case (.getName method)
                  "invoke" (.getParameterCount method)
                  "getRequiredArity" -1 ; The function is variadic.
                  nil)))
        (java/get-declared-methods ifn-class)))

(def ^:private ifn-class-arities (memoize-one ifn-class-arities-raw))

(defn- ifn-class-max-arity-raw
  ^long [^Class ifn-class]
  (reduce (fn [^long max-arity ^Method method]
            (case (.getName method)
              "invoke" (max max-arity (.getParameterCount method))
              "getRequiredArity" (reduced -1) ; The function is variadic.
              max-arity))
          0
          (java/get-declared-methods ifn-class)))

(def ^:private ifn-class-max-arity (memoize-one ifn-class-max-arity-raw))

(defn max-arity
  "Returns the maximum number of arguments the supplied function can accept, or
  -1 if the function is variadic."
  ^long [ifn-or-var]
  (let [ifn (if (var? ifn-or-var)
              (var-get ifn-or-var)
              ifn-or-var)
        ^long max-arity (ifn-class-max-arity (class ifn))]
    (if (and* (pos? max-arity)
              (var? ifn-or-var)
              (-> ifn-or-var meta :macro))
      (- max-arity 2) ; Subtract implicit arguments from macro.
      max-arity)))

(defn has-explicit-arity?
  "Returns true if the supplied function has a non-variadic implementation that
  takes exactly the specified number of arguments, or false if it does not. You
  can supply -1 for the arity to check if the function is variadic."
  [ifn-or-var ^long arity]
  (let [ifn (if (var? ifn-or-var)
              (var-get ifn-or-var)
              ifn-or-var)
        arities (ifn-class-arities (class ifn))
        adjusted-arity (if (and* (not (neg? arity))
                                 (var? ifn-or-var)
                                 (-> ifn-or-var meta :macro))
                         (+ arity 2) ; Adjust for implicit arguments to macro.
                         arity)]
    (contains? arities adjusted-arity)))

(defn memoize
  "Like core.memoize, but uses an optimized cache based on the number of
  arguments the original function accepts. Also enables you to evict entries
  from the cache using the evict-memoized! and clear-memoized! functions."
  [ifn]
  (if (::memoize-cache (meta ifn))
    ifn ; Already memoized.
    (let [arity (max-arity ifn)]
      (case arity
        1 (memoize-one ifn)
        2 (memoize-two ifn)
        (memoize-any ifn arity)))))

(defn clear-memoized!
  "Clear all previously cached results from the cache of a memoized function
  created by the functions in this module. Returns nil."
  [memoized-fn]
  (if-let [memoize-cache (::memoize-cache (meta memoized-fn))]
    (do
      (swap! memoize-cache coll/empty-with-meta)
      nil)
    (throw (IllegalArgumentException. "The function was not memoized by us."))))

(defn evict-memoized!
  "Evict a previously cached result from the cache of a memoized function
  created by the functions in this module, if present. Returns nil."
  [memoized-fn & args]
  (let [metadata (meta memoized-fn)
        memoize-cache (::memoize-cache metadata)
        memoize-arity (::memoize-arity metadata)]
    (if (and* memoize-cache memoize-arity)
      (let [arity (long memoize-arity)
            evicted-key (case arity
                          1 (first args)
                          2 (pair (first args) (second args))
                          args)]
        (if (and* (not= -1 arity) ; Variadic.
                  (not= arity (coll/bounded-count (inc arity) args)))
          (let [original-fn (::memoize-original metadata)
                original-class-name (-> original-fn class .getName)]
            (throw (ArityException. arity original-class-name))))
        (swap! memoize-cache dissoc evicted-key)
        nil)
      (throw (IllegalArgumentException. "The function was not memoized by us.")))))

(defn demunged-symbol
  "Given a munged name, such as the full name of a Clojure function class,
  return a demunged symbol."
  [^String munged-name]
  (let [namespaced-name (Compiler/demunge munged-name)
        separator-index (.indexOf namespaced-name (int \/))
        namespace (when (pos? separator-index)
                    (.intern (subs namespaced-name 0 separator-index)))
        name (if (neg? separator-index)
               namespaced-name
               (.intern (subs namespaced-name (inc separator-index))))]
    (symbol namespace name)))

(defn declared-symbol
  "Given a declared function, returns the symbol that resolves to the function."
  [declared-fn]
  (if-not (fn? declared-fn)
    (throw (IllegalArgumentException. "The argument must be a declared function."))
    (let [class-name (.getName (class declared-fn))]
      (if (.find (re-matcher #"(__|\$)\d+$" class-name))
        (throw (IllegalArgumentException.
                 (format "Unable to get declared symbol from anonymous function `%s`."
                         class-name)))
        (demunged-symbol class-name)))))

(defn make-case-fn
  "Given a collection of key-value pairs, return a function that returns the
  value for a key, or throws an IllegalArgumentException if the key does not
  match any entry. The behavior of the returned function should be functionally
  equivalent to a case expression."
  [key-value-pairs]
  ;; TODO: Reimplement as macro producing a case expression?
  (let [lookup (if (map? key-value-pairs)
                 key-value-pairs
                 (into {} key-value-pairs))]
    (fn key->value [key]
      (let [value (lookup key ::not-found)]
        (if (identical? ::not-found value)
          (throw (IllegalArgumentException.
                   (str "No matching clause: " key)
                   (ex-info "Key not found in lookup."
                            {:key key
                             :valid-keys (keys lookup)})))
          value)))))

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
        (and* (= fn (.-fn that))
              (= args (.-args that))))
      false)))

(defn partial
  "Like clojure.core/partial, but with equality semantics"
  [f & args]
  (PartialFn. (apply clojure.core/partial f args) f args))

(defn multi-responds?
  "Check if a multimethod has a matching method for the supplied arguments"
  [^MultiFn multi & args]
  (some? (.getMethod multi (apply (.-dispatchFn multi) args))))

(defn make-call-logger
  "Returns a function that keeps track of its invocations. Every
  time it is called, the call and its arguments are stored in the
  metadata associated with the returned function. If fn f is
  supplied, it will be invoked after the call is logged."
  ([]
   (make-call-logger constantly-nil))
  ([f]
   (let [calls (atom [])]
     (with-meta (fn [& args]
                  (swap! calls conj args)
                  (apply f args))
                {::calls calls}))))

(defn call-logger-calls
  "Given a function obtained from make-call-logger, returns a
  vector of sequences containing the arguments for every time it
  was called."
  [call-logger]
  (-> call-logger meta ::calls deref))

(defmacro with-logged-calls
  "Temporarily redefines the specified functions into call-loggers
  while executing the body. Returns a map of functions to the
  result of (call-logger-calls fn). Non-invoked functions will not
  be included in the returned map.

  Example:
  (with-logged-calls [print println]
    (println :a)
    (println :a :b))
  => {#object[clojure.core$println] [(:a)
                                     (:a :b)]}"
  [var-symbols & body]
  `(let [binding-map# ~(into {}
                             (map (fn [var-symbol]
                                    `[(var ~var-symbol) (make-call-logger)]))
                             var-symbols)]
     (with-redefs-fn binding-map# (fn [] ~@body))
     (into {}
           (keep (fn [[var# call-logger#]]
                   (let [calls# (call-logger-calls call-logger#)]
                     (when (seq calls#)
                       [(deref var#) calls#]))))
           binding-map#)))

(defmacro among-values-case-expr
  "Given a sequence of valid-values and a checked-value, returns a case
  expression that returns true for the valid-values and false otherwise."
  [valid-values checked-value]
  `(case ~checked-value
     ~(seq (eval valid-values)) true
     false))

(defmacro defamong
  "Defines a top-level single-argument function that returns true for the
  provided valid-values, and false otherwise. The valid-values must be a
  compile-time expression, since the predicate argument is checked against the
  valid-values in a case expression."
  [name valid-values]
  `(defn ~name [~'value]
     (among-values-case-expr ~valid-values ~'value)))

;; Keep at the bottom to avoid internal use. Code in this file should use and*.
(defn and
  "Like clojure.core/and, but is eager and can be used as a function."
  ([] true)
  ([a] a)
  ([a b] (if a b a))
  ([a b & more] (if a (reduce and b more) a)))

;; Keep at the bottom to avoid internal use. Code in this file should use or*.
(defn or
  "Like clojure.core/or, but is eager and can be used as a function."
  ([] nil)
  ([a] a)
  ([a b] (if a a b))
  ([a b & more] (reduce or (if a a b) more)))
