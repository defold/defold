;; Copyright 2020 The Defold Foundation
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

(ns editor.code.util-test
  (:require [clojure.test :refer :all]
            [editor.code.util :as util]
            [integration.test-util :as test-util]))

(deftest last-index-where-test
  (is (= 3 (util/last-index-where even? (range 1 6))))
  (is (= 4 (util/last-index-where nil? [:a :b nil :d nil :f])))
  (is (= 4 (util/last-index-where #(= :a %) (list :a nil :a nil :a))))
  (is (= 5 (util/last-index-where #(Character/isDigit %) "abc123def")))
  (is (thrown? UnsupportedOperationException (util/last-index-where (fn [[k _]] (= :d k)) (sorted-map :a 1 :b 2 :c 3 :d 4))))
  (is (thrown? UnsupportedOperationException (util/last-index-where #(= "f" %) (sorted-set "f" "e" "d" "c" "b" "a"))))
  (is (nil? (util/last-index-where nil? nil)))
  (is (nil? (util/last-index-where even? nil)))
  (is (nil? (util/last-index-where even? [])))
  (is (nil? (util/last-index-where even? [1 3 5])))

  (testing "stops calling pred after first true"
    (let [pred (test-util/make-call-logger (constantly true))]
      (is (= 9 (util/last-index-where pred (range 10))))
      (is (= 1 (count (test-util/call-logger-calls pred)))))))
