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
