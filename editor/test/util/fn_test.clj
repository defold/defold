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
  (:import [clojure.lang ArityException ExceptionInfo]
           [com.github.benmanes.caffeine.cache Caffeine]
           [java.lang Thread$State]
           [java.util.concurrent Callable ExecutionException FutureTask TimeUnit]))

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

(defn- memoized-fn-cache [memoized-fn]
  (deref (::fn/memoize-cache (meta memoized-fn))))

(def defn-cached-call-count (atom 0))

(fn/defn-cached cached-test-fn
  "A cached function used by defn-cached-test."
  {:cache (-> (Caffeine/newBuilder)
              (.maximumSize 16)
              (.build))}
  ([{:keys [value]}]
   (swap! defn-cached-call-count inc)
   (case value
     ::throw (throw (Exception. "Failed"))
     ::nil nil
     (Object.)))
  ([prefix value & suffixes]
   (swap! defn-cached-call-count inc)
   (into [prefix value] suffixes)))

(deftest defn-cached-test
  (fn/clear-cached! cached-test-fn)
  (reset! defn-cached-call-count 0)
  (let [value (random-uuid)
        argument {:value value}
        result (cached-test-fn argument)]
    (is (identical? result (cached-test-fn argument)))
    (is (= 1 @defn-cached-call-count)))

  (let [value (random-uuid)]
    (is (= [:prefix value :a :b]
           (cached-test-fn :prefix value :a :b)))
    (is (= [:prefix value :a :b]
           (cached-test-fn :prefix value :a :b)))
    (is (= 2 @defn-cached-call-count)))

  (let [argument {:value ::throw}]
    (is (thrown? Exception (cached-test-fn argument)))
    (is (thrown? Exception (cached-test-fn argument)))
    (is (= 4 @defn-cached-call-count)))

  (let [argument {:value ::nil}]
    (is (nil? (cached-test-fn argument)))
    (is (nil? (cached-test-fn argument)))
    (is (= 5 @defn-cached-call-count)))

  (is (= "A cached function used by defn-cached-test."
         (:doc (meta #'cached-test-fn))))
  (is (= '([{:keys [value]}] [prefix value & suffixes])
         (:arglists (meta #'cached-test-fn))))

  (let [argument {:value (random-uuid)}
        first-result (cached-test-fn argument)]
    (is (nil? (fn/clear-cached! cached-test-fn)))
    (is (not (identical? first-result (cached-test-fn argument)))))

  (is (thrown? IllegalArgumentException
               (fn/clear-cached! identity))))

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
      (is (= "1, 2, 3, 4, 5, 6, 7, 8, 9" (memoized-fn 1 2 3 4 5 6 7 8 9)))))

  (testing "Limited unary function"
    (let [memoized-fn (fn/memoize {:limit 1} #(str ":" % "::"))]
      (is (= {} (memoized-fn-cache memoized-fn)))
      (memoized-fn :f)
      (is (= {:f "::f::"}
             (memoized-fn-cache memoized-fn)))
      (memoized-fn :i)
      (is (= {:i "::i::"}
             (memoized-fn-cache memoized-fn))))
    (let [memoized-fn (fn/memoize {:limit 3} #(str ":" % "::"))]
      (is (= {} (memoized-fn-cache memoized-fn)))
      (memoized-fn :f)
      (memoized-fn :i)
      (memoized-fn :f)
      (is (= {:f "::f::"
              :i "::i::"}
             (memoized-fn-cache memoized-fn)))
      (memoized-fn :o)
      (is (= {:f "::f::"
              :i "::i::"
              :o "::o::"}
             (memoized-fn-cache memoized-fn)))
      (memoized-fn :n)
      (is (= {:i "::i::"
              :o "::o::"
              :n "::n::"}
             (memoized-fn-cache memoized-fn)))))

  (testing "Limited binary function"
    (let [memoized-fn (fn/memoize {:limit 1} #(str ":" %1 ":" %2 "::"))]
      (memoized-fn :f 0)
      (is (= {[:f 0] "::f:0::"}
             (memoized-fn-cache memoized-fn)))
      (memoized-fn :i 1)
      (is (= {[:i 1] "::i:1::"}
             (memoized-fn-cache memoized-fn))))
    (let [memoized-fn (fn/memoize {:limit 3} #(str ":" %1 ":" %2 "::"))]
      (memoized-fn :f 0)
      (memoized-fn :i 1)
      (memoized-fn :f 0)
      (is (= {[:f 0] "::f:0::"
              [:i 1] "::i:1::"}
             (memoized-fn-cache memoized-fn)))
      (memoized-fn :o 2)
      (is (= {[:f 0] "::f:0::"
              [:i 1] "::i:1::"
              [:o 2] "::o:2::"}
             (memoized-fn-cache memoized-fn)))
      (memoized-fn :n 3)
      (is (= {[:i 1] "::i:1::"
              [:o 2] "::o:2::"
              [:n 3] "::n:3::"}
             (memoized-fn-cache memoized-fn)))))

  (testing "Limited general function"
    (let [memoized-fn (fn/memoize {:limit 1} #(str ":" %1 ":" %2 ":" %3 "::"))]
      (memoized-fn :f 0 "A")
      (is (= {[:f 0 "A"] "::f:0:A::"}
             (memoized-fn-cache memoized-fn)))
      (memoized-fn :i 1 "B")
      (is (= {[:i 1 "B"] "::i:1:B::"}
             (memoized-fn-cache memoized-fn))))
    (let [memoized-fn (fn/memoize {:limit 3} #(str ":" %1 ":" %2 ":" %3 "::"))]
      (memoized-fn :f 0 "A")
      (memoized-fn :i 1 "B")
      (memoized-fn :f 0 "A")
      (is (= {[:f 0 "A"] "::f:0:A::"
              [:i 1 "B"] "::i:1:B::"}
             (memoized-fn-cache memoized-fn)))
      (memoized-fn :o 2 "C")
      (is (= {[:f 0 "A"] "::f:0:A::"
              [:i 1 "B"] "::i:1:B::"
              [:o 2 "C"] "::o:2:C::"}
             (memoized-fn-cache memoized-fn)))
      (memoized-fn :n 3 "D")
      (is (= {[:i 1 "B"] "::i:1:B::"
              [:o 2 "C"] "::o:2:C::"
              [:n 3 "D"] "::n:3:D::"}
             (memoized-fn-cache memoized-fn)))))

  (testing "Limited variadic function"
    (let [memoized-fn (fn/memoize {:limit 1} str)]
      (memoized-fn :f 0 "A")
      (is (= {[:f 0 "A"] ":f0A"}
             (memoized-fn-cache memoized-fn)))
      (memoized-fn :i 1 "B")
      (is (= {[:i 1 "B"] ":i1B"}
             (memoized-fn-cache memoized-fn))))
    (let [memoized-fn (fn/memoize {:limit 3} str)]
      (memoized-fn :f 0 "A")
      (memoized-fn :i 1 "B")
      (memoized-fn :f 0 "A")
      (is (= {[:f 0 "A"] ":f0A"
              [:i 1 "B"] ":i1B"}
             (memoized-fn-cache memoized-fn)))
      (memoized-fn :o 2 "C")
      (is (= {[:f 0 "A"] ":f0A"
              [:i 1 "B"] ":i1B"
              [:o 2 "C"] ":o2C"}
             (memoized-fn-cache memoized-fn)))
      (memoized-fn :n 3 "D")
      (is (= {[:i 1 "B"] ":i1B"
              [:o 2 "C"] ":o2C"
              [:n 3 "D"] ":n3D"}
             (memoized-fn-cache memoized-fn))))))

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

;; region memoize-late-bound

;; Mutually recursive functions can capture each other during initialization.
;; With ordinary memoization, the nested lookup would overflow the stack.
(deftest memoize-late-bound-allows-memoized-fn-to-call-itself
  (let [memoized-fn-promise (promise)

        memoized-fn
        (fn/memoize-late-bound
          (fn higher-order-fn [fn-name]
            (let [memoized-fn @memoized-fn-promise

                  other-fn-name
                  (case fn-name
                    :outer :inner
                    :inner :outer)

                  other-lower-order-fn (memoized-fn other-fn-name)]
              (fn lower-order-fn [^long depth]
                (if (zero? depth)
                  [fn-name]
                  (let [remaining-path (other-lower-order-fn (dec depth))]
                    (into [fn-name] remaining-path)))))))]
    (deliver memoized-fn-promise memoized-fn)
    (let [path-future
          (future
            (let [outer-fn (memoized-fn :outer)]
              (outer-fn 2)))]
      (try
        (let [path (deref path-future 5000 ::timeout)]
          (is (= [:outer :inner :outer] path)))
        (finally
          (future-cancel path-future))))))

(defn- await-reader-wait [^Thread reader-thread]
  (let [start-time (System/nanoTime)
        timeout-nanos (.toNanos TimeUnit/SECONDS 5)]
    (loop []
      (let [state (.getState reader-thread)]
        (cond
          (or (= state Thread$State/BLOCKED)
              (= state Thread$State/WAITING)
              (= state Thread$State/TIMED_WAITING))
          true

          (and (.isAlive reader-thread)
               (< (- (System/nanoTime) start-time) timeout-nanos))
          (do
            (.join reader-thread 10)
            (recur))

          :else
          false)))))

;; A reader can look up and invoke a pending function on the same thread
;; even when initialization takes longer than the old one-second timeout.
(deftest memoize-late-bound-shares-slow-initialization-test
  (let [initialization-started-promise (promise)
        finish-initialization-promise (promise)
        reader-ready-promise (promise)
        initialization-count-atom (atom 0)

        memoized-fn
        (fn/memoize-late-bound
          (fn higher-order-fn [higher-order-fn-arg]
            (swap! initialization-count-atom inc)
            (deliver initialization-started-promise true)
            @finish-initialization-promise
            (fn lower-order-fn [lower-order-fn-arg]
              [higher-order-fn-arg lower-order-fn-arg])))

        initializer-future
        (future
          (let [lower-order-fn (memoized-fn :arg)]
            (lower-order-fn :first)))

        reader-task
        (FutureTask.
          ^Callable
          (fn read-pending-fn []
            @initialization-started-promise
            (let [lower-order-fn (memoized-fn :arg)]
              (deliver reader-ready-promise true)
              (lower-order-fn :second))))

        reader-thread (Thread/startVirtualThread reader-task)]
    (try
      (when (is (deref reader-ready-promise 5000 false)
                "The reader should obtain the pending function.")
        ;; After the lookup, the only remaining work on this thread is the call.
        ;; Start timing once the reader is blocked inside it.
        (when (is (await-reader-wait reader-thread)
                  "Invoking the pending function should block the reader.")
          (let [reader-result (deref reader-task 1500 ::pending)]
            (is (= ::pending reader-result)))))

      (deliver finish-initialization-promise true)
      (let [initializer-result (deref initializer-future 5000 ::timeout)
            reader-result (deref reader-task 5000 ::timeout)]
        (is (= [:arg :first] initializer-result))
        (when (is (= [:arg :second] reader-result))
          (let [cached-fn (memoized-fn :arg)]
            (is (= [:arg :cached] (cached-fn :cached)))))
        (is (= 1 @initialization-count-atom)))
      (finally
        (deliver finish-initialization-promise true)
        (future-cancel initializer-future)
        (future-cancel reader-task)))))

;; A recursive lookup returns the pending function. Invoking it during
;; initialization throws an exception with the recursive key.
(deftest memoize-late-bound-rejects-invocation-by-initializer-test
  (let [memoized-fn-promise (promise)
        recursive-lookup-result-atom (atom nil)

        memoized-fn
        (fn/memoize-late-bound
          (fn higher-order-fn [arg]
            (let [memoized-fn @memoized-fn-promise
                  lower-order-fn (memoized-fn arg)]
              (reset! recursive-lookup-result-atom lower-order-fn)
              (lower-order-fn))))]
    (deliver memoized-fn-promise memoized-fn)
    (let [initializer-future
          (future
            (try
              (memoized-fn :recursive)
              ::unexpected-success
              (catch ExceptionInfo exception
                (:cache-key (ex-data exception)))))]
      (try
        (let [cache-key (deref initializer-future 5000 ::timeout)]
          (is (ifn? @recursive-lookup-result-atom))
          (is (= [:recursive] cache-key)))
        (finally
          (future-cancel initializer-future))))))

(deftest memoize-late-bound-retries-if-higher-order-fn-throws-test
  (let [attempt-count-atom (atom 0)

        memoized-fn
        (fn/memoize-late-bound
          (fn higher-order-fn [arg]
            (if (= 1 (swap! attempt-count-atom inc))
              (throw (ex-info "Thrown from higher-order-fn." {:arg arg}))
              (fn lower-order-fn []
                arg))))]

    (is (thrown-with-msg? ExceptionInfo #"Thrown from higher-order-fn." (memoized-fn :arg)))
    (let [lower-order-fn (memoized-fn :arg)]
      (is (= :arg (lower-order-fn))))
    (is (= 2 @attempt-count-atom))))

;; A reader that observes an initialization failure can immediately retry, even
;; if it observes the failure while the initializer is removing its entry.
(deftest memoize-late-bound-reader-retries-immediately-after-failure-test
  (let [initialization-exception (ex-info "First initialization failed." {})
        attempt-count-atom (atom 0)
        pending-reader (volatile! nil)

        memoized-fn
        (fn/memoize-late-bound
          (fn higher-order-fn [_arg]
            (if (= 1 (swap! attempt-count-atom inc))
              (throw initialization-exception)
              (constantly :ready))))

        read-result
        (fn read-result [lower-order-fn]
          (try
            (lower-order-fn)
            (catch ExceptionInfo exception
              (ex-cause exception))))

        initializer-future
        (future
          ;; Probe the reader before removal commits. On the initializing
          ;; thread, invoking a pending function throws immediately.
          (set-validator!
            (::fn/memoize-cache (meta memoized-fn))
            (fn read-before-removal [next-cache]
              (when (and (= 1 @attempt-count-atom)
                         (zero? (count next-cache)))
                (let [reader (memoized-fn :arg)]
                  (vreset! pending-reader reader)
                  (when (identical? initialization-exception (read-result reader))
                    (let [retry-fn (memoized-fn :arg)
                          retry-result (read-result retry-fn)]
                      (is (= :ready retry-result))))))
              true))
          (let [initializer-failure
                (try
                  (memoized-fn :arg)
                  (catch ExceptionInfo exception
                    exception))

                reader-failure (read-result @pending-reader)
                retry-fn (memoized-fn :arg)]
            [initializer-failure reader-failure (retry-fn)]))]
    (try
      (let [result (deref initializer-future 5000 ::timeout)]
        (when (is (not= ::timeout result) "The initializer should finish.")
          (let [[initializer-failure reader-failure retry-result] result]
            (is (identical? initialization-exception initializer-failure))
            (is (identical? initialization-exception reader-failure))
            (is (= :ready retry-result)))))
      (finally
        (future-cancel initializer-future)))))

;; A caller holding the late-bound function receives the cache key and the
;; original initialization failure.
(deftest memoize-late-bound-delivers-throwing-lower-order-fn-if-higher-order-fn-throws-test
  (let [initialization-started-promise (promise)
        reader-ready-promise (promise)
        initialization-exception (ex-info "Thrown from higher-order-fn." {})

        memoized-fn
        (fn/memoize-late-bound
          (fn higher-order-fn [_arg]
            (deliver initialization-started-promise true)
            (is (deref reader-ready-promise 5000 false))
            (throw initialization-exception)))

        reader-future
        (future
          @initialization-started-promise
          (let [lower-order-fn (memoized-fn :arg)]
            (deliver reader-ready-promise true)
            (lower-order-fn)))]

    (try
      (is (thrown-with-msg?
            ExceptionInfo
            #"Thrown from higher-order-fn."
            (memoized-fn :arg)))
      (let [execution-exception
            (is (thrown-with-msg?
                  ExecutionException
                  #"An exception was thrown when invoking the higher-order-fn to produce this lower-order-fn."
                  (deref reader-future 5000 ::timeout)))

            reader-exception (ex-cause execution-exception)
            reader-cache-key (:cache-key (ex-data reader-exception))]
        (is (= [:arg] reader-cache-key))
        (is (identical? initialization-exception (ex-cause reader-exception))))
      (finally
        (future-cancel reader-future)))))

;; An evicted initializer still delivers its own outcome to existing readers.
;; Its success or failure must leave the replacement entry in the cache, even
;; when the replacement returns the original reader's pending function.
(deftest memoize-late-bound-preserves-replacement-after-eviction-test
  (doseq [[description original-exception]
          [["Original initialization succeeds" nil]
           ["Original initialization fails" (ex-info "Original initialization failed." {})]]

          replacement-kind [:fresh :captured]]
    (testing (str description " with a " (name replacement-kind) " replacement")
      (let [original-started-promise (promise)
            replacement-installed-promise (promise)
            replacement-fn-atom (atom nil)
            initialization-count-atom (atom 0)

            memoized-fn
            (fn/memoize-late-bound
              (fn higher-order-fn [_arg]
                (let [attempt (swap! initialization-count-atom inc)]
                  (if (not= 1 attempt)
                    @replacement-fn-atom
                    (do
                      (deliver original-started-promise true)
                      (let [installed-fn
                            (deref replacement-installed-promise 5000 ::timeout)]
                        (is (identical? @replacement-fn-atom installed-fn)))
                      (when original-exception
                        (throw original-exception))
                      (constantly attempt))))))

            reader-future
            (future
              @original-started-promise
              (let [original-reader (memoized-fn :arg)

                    replacement-fn
                    (case replacement-kind
                      :fresh (constantly 2)
                      :captured original-reader)]
                (reset! replacement-fn-atom replacement-fn)
                (fn/evict-memoized! memoized-fn :arg)
                (let [installed-fn (memoized-fn :arg)]
                  (deliver replacement-installed-promise installed-fn))
                (try
                  (original-reader)
                  (catch ExceptionInfo exception
                    (ex-cause exception)))))]
        (try
          (let [expected-original-result (or original-exception 1)

                original-result
                (try
                  (let [original-fn (memoized-fn :arg)]
                    (original-fn))
                  (catch ExceptionInfo exception
                    exception))

                reader-result (deref reader-future 5000 ::timeout)
                cached-fn (memoized-fn :arg)]
            (is (= expected-original-result original-result))
            (is (= expected-original-result reader-result))
            (is (identical? @replacement-fn-atom cached-fn))
            (is (= 2 @initialization-count-atom)))
          (finally
            (future-cancel reader-future)))))))

;; endregion

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
