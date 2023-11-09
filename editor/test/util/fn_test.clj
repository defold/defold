;; Copyright 2020-2023 The Defold Foundation
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

(defn- max-arity-general-test-fn
  ([_arg1 _arg2])
  ([_arg1 _arg2 _arg3]))

(defn- max-arity-variadic-test-fn
  ([_arg])
  ([_arg & _args]))

(defmacro ^:private max-arity-general-test-macro
  ([_arg1 _arg2])
  ([_arg1 _arg2 _arg3]))

(defmacro ^:private max-arity-variadic-test-macro
  ([_arg])
  ([_arg & _args]))

(deftest max-arity-test
  (is (= 3 (fn/max-arity max-arity-general-test-fn)))
  (is (= -1 (fn/max-arity max-arity-variadic-test-fn)))
  (is (= 3 (fn/max-arity #'max-arity-general-test-macro)))
  (is (= -1 (fn/max-arity #'max-arity-variadic-test-macro)))
  (is (= 0 (fn/max-arity (fn []))))
  (is (= 1 (fn/max-arity (fn ([]) ([_arg])))))
  (is (= -1 (fn/max-arity (fn [_arg & _args])))))

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

swap!
(fn/declared-symbol int)