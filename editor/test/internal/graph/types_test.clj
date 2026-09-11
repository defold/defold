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

(ns internal.graph.types-test
  (:require [clojure.test :refer :all]
            [internal.graph.types :as gt]))

(deftest endpoint-comparable
  (is (thrown? NullPointerException
               (.compareTo nil
                           (gt/endpoint 0 :a))))
  (is (thrown? NullPointerException
               (.compareTo (gt/endpoint 0 :a)
                           nil)))
  (is (thrown? ClassCastException
               (.compareTo ""
                           (gt/endpoint 0 :a))))
  (is (thrown? ClassCastException
               (.compareTo (gt/endpoint 0 :a)
                           "")))

  (is (neg? (.compareTo (gt/endpoint 0 :a)
                        (gt/endpoint 1 :a))))
  (is (zero? (.compareTo (gt/endpoint 0 :a)
                         (gt/endpoint 0 :a))))
  (is (pos? (.compareTo (gt/endpoint 1 :a)
                        (gt/endpoint 0 :a))))
  (is (neg? (.compareTo (gt/endpoint 1 :a)
                        (gt/endpoint 1 :b))))
  (is (zero? (.compareTo (gt/endpoint 1 :a)
                         (gt/endpoint 1 :a))))
  (is (pos? (.compareTo (gt/endpoint 1 :b)
                        (gt/endpoint 1 :a))))

  (is (= [(gt/endpoint 0 :a)
          (gt/endpoint 0 :b)
          (gt/endpoint 1 :a)
          (gt/endpoint 1 :b)]
         (vec (into (sorted-set)
                    [(gt/endpoint 1 :b)
                     (gt/endpoint 0 :b)
                     (gt/endpoint 1 :a)
                     (gt/endpoint 0 :a)
                     (gt/endpoint 1 :b)
                     (gt/endpoint 0 :b)
                     (gt/endpoint 1 :a)
                     (gt/endpoint 0 :a)])))))
