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
