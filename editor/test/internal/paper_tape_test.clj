(ns internal.paper-tape-test
  (:require [internal.history :refer :all]
            [clojure.test :refer :all]))

(deftest shuffling-items
  (testing "cursor back and forth"
    (let [tape (into (paper-tape nil) (range 10))]
      (is (= 9 (-> tape ivalue)))
      (is (= 8 (-> tape iprev ivalue)))
      (is (= 7 (-> tape iprev iprev ivalue)))
      (is (= 8 (-> tape iprev iprev inext ivalue)))
      (is (= 9 (-> tape iprev iprev inext inext ivalue)))))

  (testing "bumping against tail end"
    (let [tape (into (paper-tape nil) (range 10))]
      (is (= 9 (-> tape inext ivalue)))
      (is (= 9 (-> tape inext inext ivalue)))
      (is (= 9 (-> tape inext inext inext ivalue)))
      (is (= 9 (-> tape iprev inext inext inext ivalue)))
      (is (= 9 (-> tape iprev iprev inext inext inext ivalue)))))

  (testing "bumping against head end"
    (let [tape (into (paper-tape nil) (range 10))
          tape (-> tape iprev iprev iprev iprev iprev iprev iprev iprev iprev iprev)]
      (is (nil? (-> tape ivalue)))
      (is (= 0  (-> tape inext ivalue)))
      (is (= 1  (-> tape inext inext ivalue)))
      (is (= 0  (-> tape inext inext iprev ivalue)))
      (is (nil? (-> tape inext inext iprev iprev ivalue)))
      (is (nil? (-> tape inext inext iprev iprev iprev ivalue)))
      (is (nil? (-> tape inext inext iprev iprev iprev iprev ivalue))))))

(deftest truncation
  (let [tape (into (paper-tape nil) (range 10))
        short-tape (truncate (-> tape iprev iprev iprev iprev))]
    (is (= 5 (-> short-tape ivalue)))
    (is (= 5 (-> short-tape inext ivalue)))))

(deftest dropping
  (let [tape (into (paper-tape nil) (range 10))
        drop-tape (drop-current (-> tape iprev iprev iprev iprev))]
    (is (= 4 (-> drop-tape ivalue)))
    (is (= 6 (-> drop-tape inext ivalue)))))

(deftest truncate-on-write
  (let [tape (into (paper-tape nil) (range 10))
        tape (-> tape iprev iprev iprev iprev)]
    (is (= 10 (count tape)))
    (let [tape (conj tape 99)]
      (is (= 99 (ivalue tape)))
      (is (= 7 (count tape))))))

(deftest size-limit
  (let [tape (into (paper-tape 5) (range 10))]
    (is (= '(5 6 7 8 9) (seq tape)))))
