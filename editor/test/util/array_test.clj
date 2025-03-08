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

(deftest of-test
  (let [items (mapv #(str "item" %) (range 20))]
    (dotimes [length (count items)]
      (let [expected-items (take length items)]
        (is (object-arrays-equal?
              (object-array expected-items)
              (apply array/of expected-items)))))))
