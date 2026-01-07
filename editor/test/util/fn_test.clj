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

(ns util.fn-test
  (:require [clojure.test :refer :all]
            [util.fn :as fn])
  (:import [clojure.lang ArityException]))

(defn- arity-void-test-fn
  ([]))

(defn- arity-general-test-fn
  ([_arg1 _arg2])
  ([_arg1 _arg2 _arg3]))

(defn- arity-variadic-test-fn
  ([_arg])
  ([_arg & _args]))

(defmacro ^:private arity-void-test-macro
  ([]))

(defmacro ^:private arity-general-test-macro
  ([_arg1 _arg2])
  ([_arg1 _arg2 _arg3]))

(defmacro ^:private arity-variadic-test-macro
  ([_arg])
  ([_arg & _args]))

(deftest max-arity-test
  (is (= 0 (fn/max-arity arity-void-test-fn)))
  (is (= 3 (fn/max-arity arity-general-test-fn)))
  (is (= -1 (fn/max-arity arity-variadic-test-fn)))
  (is (= 0 (fn/max-arity #'arity-void-test-macro)))
  (is (= 3 (fn/max-arity #'arity-general-test-macro)))
  (is (= -1 (fn/max-arity #'arity-variadic-test-macro)))
  (is (= 0 (fn/max-arity (fn []))))
  (is (= 1 (fn/max-arity (fn ([]) ([_arg])))))
  (is (= -1 (fn/max-arity (fn [_arg & _args])))))

(deftest has-explicit-arity?-test
  (is (false? (fn/has-explicit-arity? arity-void-test-fn -1)))
  (is (true? (fn/has-explicit-arity? arity-void-test-fn 0)))
  (is (false? (fn/has-explicit-arity? arity-void-test-fn 1)))

  (is (false? (fn/has-explicit-arity? arity-general-test-fn -1)))
  (is (false? (fn/has-explicit-arity? arity-general-test-fn 0)))
  (is (false? (fn/has-explicit-arity? arity-general-test-fn 1)))
  (is (true? (fn/has-explicit-arity? arity-general-test-fn 2)))
  (is (true? (fn/has-explicit-arity? arity-general-test-fn 3)))
  (is (false? (fn/has-explicit-arity? arity-general-test-fn 4)))

  (is (true? (fn/has-explicit-arity? arity-variadic-test-fn -1)))
  (is (false? (fn/has-explicit-arity? arity-variadic-test-fn 0)))
  (is (true? (fn/has-explicit-arity? arity-variadic-test-fn 1)))
  (is (false? (fn/has-explicit-arity? arity-variadic-test-fn 2)))

  (is (false? (fn/has-explicit-arity? #'arity-void-test-macro -1)))
  (is (true? (fn/has-explicit-arity? #'arity-void-test-macro 0)))
  (is (false? (fn/has-explicit-arity? #'arity-void-test-macro 1)))

  (is (false? (fn/has-explicit-arity? #'arity-general-test-macro -1)))
  (is (false? (fn/has-explicit-arity? #'arity-general-test-macro 0)))
  (is (false? (fn/has-explicit-arity? #'arity-general-test-macro 1)))
  (is (true? (fn/has-explicit-arity? #'arity-general-test-macro 2)))
  (is (true? (fn/has-explicit-arity? #'arity-general-test-macro 3)))
  (is (false? (fn/has-explicit-arity? #'arity-general-test-macro 4)))

  (is (true? (fn/has-explicit-arity? #'arity-variadic-test-macro -1)))
  (is (false? (fn/has-explicit-arity? #'arity-variadic-test-macro 0)))
  (is (true? (fn/has-explicit-arity? #'arity-variadic-test-macro 1)))
  (is (false? (fn/has-explicit-arity? #'arity-variadic-test-macro 2)))

  (letfn [(arity-void-test-anonymous-fn
            ([]))

          (arity-general-test-anonymous-fn
            ([_arg1 _arg2])
            ([_arg1 _arg2 _arg3]))

          (arity-variadic-test-anonymous-fn
            ([_arg])
            ([_arg & _args]))]

    (is (false? (fn/has-explicit-arity? arity-void-test-anonymous-fn -1)))
    (is (true? (fn/has-explicit-arity? arity-void-test-anonymous-fn 0)))
    (is (false? (fn/has-explicit-arity? arity-void-test-anonymous-fn 1)))

    (is (false? (fn/has-explicit-arity? arity-general-test-anonymous-fn -1)))
    (is (false? (fn/has-explicit-arity? arity-general-test-anonymous-fn 0)))
    (is (false? (fn/has-explicit-arity? arity-general-test-anonymous-fn 1)))
    (is (true? (fn/has-explicit-arity? arity-general-test-anonymous-fn 2)))
    (is (true? (fn/has-explicit-arity? arity-general-test-anonymous-fn 3)))
    (is (false? (fn/has-explicit-arity? arity-general-test-anonymous-fn 4)))

    (is (true? (fn/has-explicit-arity? arity-variadic-test-anonymous-fn -1)))
    (is (false? (fn/has-explicit-arity? arity-variadic-test-anonymous-fn 0)))
    (is (true? (fn/has-explicit-arity? arity-variadic-test-anonymous-fn 1)))
    (is (false? (fn/has-explicit-arity? arity-variadic-test-anonymous-fn 2)))))

(deftest memoize-test
  (testing "Returns unique instances"
    (is (not (identical? (fn/memoize identity) (fn/memoize identity)))))

  (testing "Doesn't memoize already memoized function"
    (let [memoized-fn (fn/memoize identity)]
      (is (identical? memoized-fn (fn/memoize memoized-fn)))))

  (testing "Unary function"
    (let [call-count-atom (atom 0)
          memoized-fn (fn/memoize
                        (fn unary-fn [arg]
                          (swap! call-count-atom inc)
                          (str arg)))]
      (is (= "1" (memoized-fn 1)))
      (is (= 1 @call-count-atom))
      (is (identical? (memoized-fn 2) (memoized-fn 2)))
      (is (= 2 @call-count-atom))
      (is (thrown? ArityException (memoized-fn 1 2 3 4 5 6 7 8 9)))))

  (testing "Binary function"
    (let [call-count-atom (atom 0)
          memoized-fn (fn/memoize
                        (fn binary-fn [arg1 arg2]
                          (swap! call-count-atom inc)
                          (str arg1 ", " arg2)))]
      (is (= "1, 2" (memoized-fn 1 2)))
      (is (= 1 @call-count-atom))
      (is (identical? (memoized-fn 2 3) (memoized-fn 2 3)))
      (is (= 2 @call-count-atom))
      (is (thrown? ArityException (memoized-fn 1 2 3 4 5 6 7 8 9)))))

  (testing "General function"
    (let [call-count-atom (atom 0)
          memoized-fn (fn/memoize
                        (fn general-fn [arg1 arg2 arg3 arg4]
                          (swap! call-count-atom inc)
                          (str arg1 ", " arg2 ", " arg3 ", " arg4)))]
      (is (= "1, 2, 3, 4" (memoized-fn 1 2 3 4)))
      (is (= 1 @call-count-atom))
      (is (identical? (memoized-fn 2 3 4 5) (memoized-fn 2 3 4 5)))
      (is (= 2 @call-count-atom))
      (is (thrown? ArityException (memoized-fn 1 2 3 4 5 6 7 8 9)))))

  (testing "Variadic function"
    (let [call-count-atom (atom 0)
          memoized-fn (fn/memoize
                        (fn variadic-fn [arg & args]
                          (swap! call-count-atom inc)
                          (apply str (interpose ", " (cons arg args)))))]
      (is (= "1, 2, 3, 4, 5" (memoized-fn 1 2 3 4 5)))
      (is (= 1 @call-count-atom))
      (is (identical? (memoized-fn 2 3 4 5 6) (memoized-fn 2 3 4 5 6)))
      (is (= 2 @call-count-atom))
      (is (= "1, 2, 3, 4, 5, 6, 7, 8, 9" (memoized-fn 1 2 3 4 5 6 7 8 9))))))

(deftest clear-memoized!-test
  (testing "Non-memoized function"
    (is (thrown-with-msg? IllegalArgumentException #"The function was not memoized by us." (fn/clear-memoized! compare))))

  (testing "Unary function"
    (let [call-count-atom (atom 0)
          memoized-fn (fn/memoize
                        (fn unary-fn [arg]
                          (swap! call-count-atom inc)
                          (str arg)))]
      (is (= "1" (memoized-fn 1)))
      (is (= 1 @call-count-atom))
      (is (= "1" (memoized-fn 1)))
      (is (= 1 @call-count-atom))
      (is (= "2" (memoized-fn 2)))
      (is (= 2 @call-count-atom))
      (is (nil? (fn/clear-memoized! memoized-fn)))
      (is (= "1" (memoized-fn 1)))
      (is (= 3 @call-count-atom))
      (is (= "2" (memoized-fn 2)))
      (is (= 4 @call-count-atom))))

  (testing "Binary function"
    (let [call-count-atom (atom 0)
          memoized-fn (fn/memoize
                        (fn binary-fn [arg1 arg2]
                          (swap! call-count-atom inc)
                          (str arg1 ", " arg2)))]
      (is (= "1, 2" (memoized-fn 1 2)))
      (is (= 1 @call-count-atom))
      (is (= "1, 2" (memoized-fn 1 2)))
      (is (= 1 @call-count-atom))
      (is (= "2, 3" (memoized-fn 2 3)))
      (is (= 2 @call-count-atom))
      (is (nil? (fn/clear-memoized! memoized-fn)))
      (is (= "1, 2" (memoized-fn 1 2)))
      (is (= 3 @call-count-atom))
      (is (= "2, 3" (memoized-fn 2 3)))
      (is (= 4 @call-count-atom))))

  (testing "General function"
    (let [call-count-atom (atom 0)
          memoized-fn (fn/memoize
                        (fn general-fn [arg1 arg2 arg3 arg4]
                          (swap! call-count-atom inc)
                          (str arg1 ", " arg2 ", " arg3 ", " arg4)))]
      (is (= "1, 2, 3, 4" (memoized-fn 1 2 3 4)))
      (is (= 1 @call-count-atom))
      (is (= "1, 2, 3, 4" (memoized-fn 1 2 3 4)))
      (is (= 1 @call-count-atom))
      (is (= "2, 3, 4, 5" (memoized-fn 2 3 4 5)))
      (is (= 2 @call-count-atom))
      (is (nil? (fn/clear-memoized! memoized-fn)))
      (is (= "1, 2, 3, 4" (memoized-fn 1 2 3 4)))
      (is (= 3 @call-count-atom))
      (is (= "2, 3, 4, 5" (memoized-fn 2 3 4 5)))
      (is (= 4 @call-count-atom))))

  (testing "Variadic function"
    (let [call-count-atom (atom 0)
          memoized-fn (fn/memoize
                        (fn variadic-fn [arg & args]
                          (swap! call-count-atom inc)
                          (apply str (interpose ", " (cons arg args)))))]
      (is (= "1, 2, 3, 4, 5" (memoized-fn 1 2 3 4 5)))
      (is (= 1 @call-count-atom))
      (is (= "1, 2, 3, 4, 5" (memoized-fn 1 2 3 4 5)))
      (is (= 1 @call-count-atom))
      (is (= "2, 3, 4, 5, 6" (memoized-fn 2 3 4 5 6)))
      (is (= 2 @call-count-atom))
      (is (nil? (fn/clear-memoized! memoized-fn)))
      (is (= "1, 2, 3, 4, 5" (memoized-fn 1 2 3 4 5)))
      (is (= 3 @call-count-atom))
      (is (= "2, 3, 4, 5, 6" (memoized-fn 2 3 4 5 6)))
      (is (= 4 @call-count-atom)))))

(deftest evict-memoized!-test
  (testing "Non-memoized function"
    (is (thrown-with-msg? IllegalArgumentException #"The function was not memoized by us." (fn/evict-memoized! compare 1 2))))

  (testing "Unary function"
    (let [call-count-atom (atom 0)
          memoized-fn (fn/memoize
                        (fn unary-fn [arg]
                          (swap! call-count-atom inc)
                          (str arg)))]
      (is (= "1" (memoized-fn 1)))
      (is (= 1 @call-count-atom))
      (is (= "1" (memoized-fn 1)))
      (is (= 1 @call-count-atom))
      (is (= "2" (memoized-fn 2)))
      (is (= 2 @call-count-atom))
      (is (nil? (fn/evict-memoized! memoized-fn 1)))
      (is (= "1" (memoized-fn 1)))
      (is (= 3 @call-count-atom))
      (is (= "2" (memoized-fn 2)))
      (is (= 3 @call-count-atom))
      (is (thrown? ArityException (fn/evict-memoized! memoized-fn 1 2 3 4 5 6 7 8 9)))))

  (testing "Binary function"
    (let [call-count-atom (atom 0)
          memoized-fn (fn/memoize
                        (fn binary-fn [arg1 arg2]
                          (swap! call-count-atom inc)
                          (str arg1 ", " arg2)))]
      (is (= "1, 2" (memoized-fn 1 2)))
      (is (= 1 @call-count-atom))
      (is (= "1, 2" (memoized-fn 1 2)))
      (is (= 1 @call-count-atom))
      (is (= "2, 3" (memoized-fn 2 3)))
      (is (= 2 @call-count-atom))
      (is (nil? (fn/evict-memoized! memoized-fn 1 2)))
      (is (= "1, 2" (memoized-fn 1 2)))
      (is (= 3 @call-count-atom))
      (is (= "2, 3" (memoized-fn 2 3)))
      (is (= 3 @call-count-atom))
      (is (thrown? ArityException (fn/evict-memoized! memoized-fn 1 2 3 4 5 6 7 8 9)))))

  (testing "General function"
    (let [call-count-atom (atom 0)
          memoized-fn (fn/memoize
                        (fn general-fn [arg1 arg2 arg3 arg4]
                          (swap! call-count-atom inc)
                          (str arg1 ", " arg2 ", " arg3 ", " arg4)))]
      (is (= "1, 2, 3, 4" (memoized-fn 1 2 3 4)))
      (is (= 1 @call-count-atom))
      (is (= "1, 2, 3, 4" (memoized-fn 1 2 3 4)))
      (is (= 1 @call-count-atom))
      (is (= "2, 3, 4, 5" (memoized-fn 2 3 4 5)))
      (is (= 2 @call-count-atom))
      (is (nil? (fn/evict-memoized! memoized-fn 1 2 3 4)))
      (is (= "1, 2, 3, 4" (memoized-fn 1 2 3 4)))
      (is (= 3 @call-count-atom))
      (is (= "2, 3, 4, 5" (memoized-fn 2 3 4 5)))
      (is (= 3 @call-count-atom))
      (is (thrown? ArityException (fn/evict-memoized! memoized-fn 1 2 3 4 5 6 7 8 9)))))

  (testing "Variadic function"
    (let [call-count-atom (atom 0)
          memoized-fn (fn/memoize
                        (fn variadic-fn [arg & args]
                          (swap! call-count-atom inc)
                          (apply str (interpose ", " (cons arg args)))))]
      (is (= "1, 2, 3, 4, 5" (memoized-fn 1 2 3 4 5)))
      (is (= 1 @call-count-atom))
      (is (= "1, 2, 3, 4, 5" (memoized-fn 1 2 3 4 5)))
      (is (= 1 @call-count-atom))
      (is (= "2, 3, 4, 5, 6" (memoized-fn 2 3 4 5 6)))
      (is (= 2 @call-count-atom))
      (is (nil? (fn/evict-memoized! memoized-fn 1 2 3 4 5)))
      (is (= "1, 2, 3, 4, 5" (memoized-fn 1 2 3 4 5)))
      (is (= 3 @call-count-atom))
      (is (= "2, 3, 4, 5, 6" (memoized-fn 2 3 4 5 6)))
      (is (= 3 @call-count-atom))
      (is (nil? (fn/evict-memoized! memoized-fn 1 2 3 4 5 6 7 8 9))))))

(def ^:private defined-constant :default)
(defn- defined-function [_arg])
(defn- defined-function0 [_arg])
(defn- defined-function? [_arg])
(defn- defined-function! [_arg])

(deftest declared-symbol-test
  (testing "Returns namespaced symbols"
    (is (= `defined-function (fn/declared-symbol defined-function)))
    (is (= `defined-function0 (fn/declared-symbol defined-function0)))
    (is (= `defined-function? (fn/declared-symbol defined-function?)))
    (is (= `defined-function! (fn/declared-symbol defined-function!))))

  (testing "Throws for non-functions"
    (is (thrown-with-msg?
          IllegalArgumentException
          #"The argument must be a declared function"
          (fn/declared-symbol defined-constant)))
    (is (thrown-with-msg?
          IllegalArgumentException
          #"The argument must be a declared function"
          (fn/declared-symbol "other value"))))

  (testing "Throws for anonymous functions"
    (is (thrown-with-msg?
          IllegalArgumentException
          #"Unable to get declared symbol from anonymous function"
          (fn/declared-symbol (fn []))))
    (is (thrown-with-msg?
          IllegalArgumentException
          #"Unable to get declared symbol from anonymous function"
          (fn/declared-symbol (fn named []))))
    (is (thrown-with-msg?
          IllegalArgumentException
          #"Unable to get declared symbol from anonymous function"
          (fn/declared-symbol (memoize defined-function))))
    (is (thrown-with-msg?
          IllegalArgumentException
          #"Unable to get declared symbol from anonymous function"
          (fn/declared-symbol (fn/memoize defined-function))))
    (is (thrown-with-msg?
          IllegalArgumentException
          #"Unable to get declared symbol from anonymous function"
          (fn/declared-symbol (partial defined-function :arg))))
    (is (thrown-with-msg?
          IllegalArgumentException
          #"Unable to get declared symbol from anonymous function"
          (fn/declared-symbol (comp identity defined-function)))))

  (testing "Strings are interned"
    (is (identical? (name `defined-function) (name (fn/declared-symbol defined-function))))
    (is (identical? (namespace `defined-function) (namespace (fn/declared-symbol defined-function))))))


(deftest make-case-fn-test
  (let [case-fn (fn/make-case-fn {:a 1 :b 2 nil nil})]

    (testing "Returned fn resolves keys to values"
      (is (= 1 (case-fn :a)))
      (is (= 2 (case-fn :b)))
      (is (nil? (case-fn nil))))

    (testing "Returned fn throws on invalid key"
      (is (thrown-with-msg? IllegalArgumentException #"No matching clause: :non-existing-key" (case-fn :non-existing-key)))))

  (is (= 1 ((fn/make-case-fn [[:a 1]]) :a)) "Accepts sequence of pairs")
  (is (= 1 ((fn/make-case-fn {:a 1}) :a)) "Accepts map"))

(def ^:private color-keywords
  (into (sorted-set)
        (map keyword)
        ["red" "green" "blue"]))

(fn/defamong ^:private color-keyword? color-keywords)

(def ^:private known-greetings
  (mapv #(str "Hello " %)
        ["Jane" "John"]))

(fn/defamong ^:private known-greeting? known-greetings)

(deftest defamong-test
  (is (true? (color-keyword? :red)))
  (is (true? (color-keyword? :green)))
  (is (true? (color-keyword? :blue)))
  (is (false? (color-keyword? :unknown)))
  (is (false? (color-keyword? nil)))
  (is (false? (color-keyword? 1)))
  (is (false? (color-keyword? (Object.))))
  (is (true? (known-greeting? "Hello Jane")))
  (is (true? (known-greeting? "Hello John")))
  (is (false? (known-greeting? "Hello Sam")))
  (is (false? (known-greeting? 0.0))))

(deftest and-test
  (testing "No arguments."
    (is (true? (fn/and))))

  (testing "Single argument."
    (is (false? (fn/and false)))
    (is (nil? (fn/and nil)))
    (is (true? (fn/and true))))

  (testing "Two arguments."
    (is (false? (fn/and false nil)))
    (is (nil? (fn/and nil false)))
    (is (false? (fn/and false true)))
    (is (nil? (fn/and nil true)))
    (is (false? (fn/and true false)))
    (is (nil? (fn/and true nil))))

  (testing "Many arguments."
    (is (false? (fn/and false nil nil)))
    (is (nil? (fn/and nil false false)))
    (is (false? (fn/and false true true)))
    (is (nil? (fn/and nil true true)))
    (is (false? (fn/and true true false)))
    (is (nil? (fn/and true true nil)))
    (is (false? (fn/and true false true)))
    (is (nil? (fn/and true nil true)))
    (is (false? (fn/and true false nil)))
    (is (nil? (fn/and true nil false))))

  (testing "Returns final argument when all are true."
    (doseq [arg-count (range 1 32)]
      (let [args (repeatedly arg-count #(Object.))]
        (is (identical? (last args) (apply fn/and args))))))

  (testing "Returns first false argument found."
    (let [arg-count 32]
      (doseq [false-value [nil false]]
        (doseq [false-arg-index (range 0 arg-count)]
          (let [args (-> (repeatedly arg-count #(Object.))
                         (vec)
                         (assoc false-arg-index false-value))]
            (is (identical? false-value (apply fn/and args))))))))

  (testing "Boolean false instance handling matches core implementation."
    (let [false-a (Boolean. false)
          false-b (Boolean. false)]
      (is (identical? (and true false-b)
                      (fn/and true false-b)))
      (is (identical? (and false-a true)
                      (fn/and false-a true)))
      (is (identical? (and false-a false-b)
                      (fn/and false-a false-b))))))

(deftest or-test
  (testing "No arguments."
    (is (nil? (fn/or))))

  (testing "Single argument."
    (is (false? (fn/or false)))
    (is (nil? (fn/or nil)))
    (is (true? (fn/or true))))

  (testing "Two arguments."
    (is (nil? (fn/or false nil)))
    (is (false? (fn/or nil false)))
    (is (true? (fn/or false true)))
    (is (true? (fn/or nil true)))
    (is (true? (fn/or true false)))
    (is (true? (fn/or true nil))))

  (testing "Many arguments."
    (is (true? (fn/or true false false)))
    (is (true? (fn/or true nil nil)))
    (is (true? (fn/or false true false)))
    (is (true? (fn/or nil true nil)))
    (is (true? (fn/or false false true)))
    (is (true? (fn/or nil nil true)))
    (is (nil? (fn/or false false nil)))
    (is (false? (fn/or nil nil false))))

  (testing "Returns final argument when all are false."
    (doseq [arg-count (range 1 32)]
      (doseq [false-value [nil false]]
        (let [args (repeat arg-count false-value)]
          (is (identical? (last args) (apply fn/or args)))))))

  (testing "Returns first true argument found."
    (let [arg-count 32]
      (doseq [false-value [nil false]]
        (doseq [true-arg-index (range 0 arg-count)]
          (let [true-value (Object.)
                args (-> (repeat arg-count false-value)
                         (vec)
                         (assoc true-arg-index true-value))]
            (is (identical? true-value (apply fn/or args))))))))

  (testing "Boolean false instance handling matches core implementation."
    (let [false-a (Boolean. false)
          false-b (Boolean. false)]
      (is (identical? (or true false-b)
                      (fn/or true false-b)))
      (is (identical? (or false-a true)
                      (fn/or false-a true)))
      (is (identical? (or false-a false-b)
                      (fn/or false-a false-b))))))
