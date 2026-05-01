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

(ns editor.util-test
  (:require [clojure.test :refer :all]
            [editor.util :as util]))

(deftest natural-order
  (are [unsorted sorted] (= sorted (sort util/natural-order unsorted))
       ;; edge cases
       [nil nil] [nil nil]
       ["" ""] ["" ""]
       ["a" ""] ["" "a"]
       ["1" "a"] ["1" "a"]
       ;; lexigraphically
       ["a" "b"] ["a" "b"]
       ["b" "a"] ["a" "b"]
       ;; case insensitive
       ["aa" "Ab"] ["aa" "Ab"]
       ["ab" "Aa"] ["Aa" "ab"]
       ;; numbers
       ["a1" "a"] ["a" "a1"]
       ["a1" "a10" "a2"] ["a1" "a2" "a10"]
       ["a2" "b" "a1"] ["a1" "a2" "b"]
       ;; really large numbers (these have one more digit than Long/MAX_VALUE)
       ["a10000000000000000001" "a10000000000000000000"] ["a10000000000000000000" "a10000000000000000001"]))

(deftest join-words
  (is (= "" (util/join-words ", " " or " nil)))
  (is (= "" (util/join-words ", " " or " [])))
  (is (= "a" (util/join-words ", " " or " ["a"])))
  (is (= "a or b" (util/join-words ", " " or " ["a" "b"])))
  (is (= "a, b or c" (util/join-words ", " " or " ["a" "b" "c"])))
  (is (= "a, b, c and d" (util/join-words ", " " and " ["a" "b" "c" "d"])))
  (is (= "1 or 2 or :c" (util/join-words " or " " or " [1 2 :c]))))
