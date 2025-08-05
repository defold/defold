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

(ns util.array-test
  (:require [clojure.test :refer :all]
            [util.array :as array])
  (:import [java.util Arrays]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- object-arrays-equal? [^"[Ljava.lang.Object;" a ^"[Ljava.lang.Object;" b]
  (Arrays/equals a b))

(deftest primitive-type-test
  (is (= :boolean (array/primitive-type (boolean-array 0))))
  (is (= :char (array/primitive-type (char-array 0))))
  (is (= :byte (array/primitive-type (byte-array 0))))
  (is (= :short (array/primitive-type (short-array 0))))
  (is (= :int (array/primitive-type (int-array 0))))
  (is (= :long (array/primitive-type (long-array 0))))
  (is (= :float (array/primitive-type (float-array 0))))
  (is (= :double (array/primitive-type (double-array 0))))
  (is (nil? (array/primitive-type (object-array 0))))
  (is (nil? (array/primitive-type nil)))
  (is (nil? (array/primitive-type 0)))
  (is (nil? (array/primitive-type [])))
  (is (nil? (array/primitive-type "")))
  (is (nil? (array/primitive-type (Object.)))))

(deftest array?-test
  (is (true? (array/array? (boolean-array 0))))
  (is (true? (array/array? (char-array 0))))
  (is (true? (array/array? (byte-array 0))))
  (is (true? (array/array? (short-array 0))))
  (is (true? (array/array? (int-array 0))))
  (is (true? (array/array? (long-array 0))))
  (is (true? (array/array? (float-array 0))))
  (is (true? (array/array? (double-array 0))))
  (is (true? (array/array? (object-array 0))))
  (is (false? (array/array? nil)))
  (is (false? (array/array? 0)))
  (is (false? (array/array? [])))
  (is (false? (array/array? "")))
  (is (false? (array/array? (Object.)))))

(deftest primitive-array?-test
  (is (true? (array/primitive-array? (boolean-array 0))))
  (is (true? (array/primitive-array? (char-array 0))))
  (is (true? (array/primitive-array? (byte-array 0))))
  (is (true? (array/primitive-array? (short-array 0))))
  (is (true? (array/primitive-array? (int-array 0))))
  (is (true? (array/primitive-array? (long-array 0))))
  (is (true? (array/primitive-array? (float-array 0))))
  (is (true? (array/primitive-array? (double-array 0))))
  (is (false? (array/primitive-array? (object-array 0))))
  (is (false? (array/primitive-array? nil)))
  (is (false? (array/primitive-array? 0)))
  (is (false? (array/primitive-array? [])))
  (is (false? (array/primitive-array? "")))
  (is (false? (array/primitive-array? (Object.)))))

(deftest of-test
  (is (= Object/1 (class (array/of))))
  (is (= Object/1 (class (array/of "item"))))
  (is (= Object/1 (class (array/of 1))))
  (is (identical? array/empty-object-array (array/of)))
  (let [items (mapv #(str "item" %) (range 20))]
    (dotimes [length (count items)]
      (let [expected-items (take length items)
            actual-items (apply array/of expected-items)]
        (is (= Object/1 (class actual-items)))
        (is (object-arrays-equal?
              (object-array expected-items)
              actual-items))))))

(deftest of-type-test
  (is (= String/1 (class (array/of-type String))))
  (is (= String/1 (class (array/of-type String "item"))))
  (is (= Long/1 (class (array/of-type Long 1))))
  (is (= long/1 (class (array/of-type Long/TYPE 1))))
  (is (thrown? IllegalArgumentException (array/of-type String 0)))
  (is (thrown? IllegalArgumentException (array/of-type Long "item")))
  (is (thrown? IllegalArgumentException (array/of-type Long/TYPE "item")))
  (is (identical? array/empty-object-array (array/of-type Object)))
  (is (identical? (array/of-type String) (array/of-type String)))
  (is (identical? (array/of-type Long) (array/of-type Long)))
  (is (not= (array/of-type String) (array/of-type Long)))
  (let [items (mapv #(str "item" %) (range 20))]
    (dotimes [length (count items)]
      (let [expected-items (take length items)
            actual-items (apply array/of-type String expected-items)]
        (is (= String/1 (class actual-items)))
        (is (object-arrays-equal?
              (object-array expected-items)
              actual-items))))))

(deftest from-test
  (is (= Object/1 (class (array/from nil))))
  (is (= Object/1 (class (array/from ["item"]))))
  (is (= Object/1 (class (array/from [1]))))
  (is (identical? array/empty-object-array (array/from [])))
  (is (identical? array/empty-object-array (array/from nil)))
  (let [items (mapv #(str "item" %) (range 20))]
    (dotimes [length (count items)]
      (let [expected-items (take length items)
            actual-items (array/from expected-items)]
        (is (= Object/1 (class actual-items)))
        (is (object-arrays-equal?
              (object-array expected-items)
              actual-items))))))

(deftest from-type-test
  (is (= String/1 (class (array/from-type String nil))))
  (is (= String/1 (class (array/from-type String ["item"]))))
  (is (= Long/1 (class (array/from-type Long [1]))))
  (is (= long/1 (class (array/from-type Long/TYPE [1]))))
  (is (thrown? IllegalArgumentException (array/from-type String [0])))
  (is (thrown? IllegalArgumentException (array/from-type Long ["item"])))
  (is (thrown? IllegalArgumentException (array/from-type Long/TYPE ["item"])))
  (is (identical? array/empty-object-array (array/from-type Object [])))
  (is (identical? array/empty-object-array (array/from-type Object nil)))
  (is (identical? (array/from-type String nil) (array/from-type String [])))
  (is (identical? (array/from-type String nil) (array/from-type String nil)))
  (is (identical? (array/from-type Long nil) (array/from-type Long nil)))
  (is (not= (array/from-type String nil) (array/from-type Long nil)))
  (let [items (mapv #(str "item" %) (range 20))]
    (dotimes [length (count items)]
      (let [expected-items (take length items)
            actual-items (array/from-type String expected-items)]
        (is (= String/1 (class actual-items)))
        (is (object-arrays-equal?
              (object-array expected-items)
              actual-items))))))
