(ns editor.progress-test
  (:refer-clojure :exclude [mapv])
  (:require [clojure.test :refer :all]
            [editor.progress :refer :all]))

(deftest make-test
  (is (= {:message "mess" :size 10 :pos 0}
         (make "mess" 10)))
  (is (= {:message "mess" :size 10 :pos 5}
         (make "mess" 10 5))))

(deftest message-test
  (is (= {:message "mess2" :size 1 :pos 0}
         (message (make "mess") "mess2"))))

(deftest advance-test
  (is (= {:message "mess" :size 1 :pos 1}
         (advance (make "mess"))))

  (is (= {:message "mess2" :size 2 :pos 2}
         (advance (make "mess" 2) 2 "mess2")))

  (testing "advancing beyond size"
    (is (= {:message "mess2" :size 2 :pos 2}
           (advance (make "mess" 2) 3 "mess2")))))

(deftest jump-test
  (testing "jump beyond size"
    (is (= 100 (percentage (jump (make "" 100) 666)))))
  (is (= 50 (percentage (jump (make "" 100 0) 50)))))

(deftest fraction-test
  (is (= 0 (fraction (make "mess"))))
  (is (= 1 (fraction (advance (make "mess")))))
  (is (= 1/2 (fraction (advance (make "mess" 2))))))

(deftest percentage-test
  (is (= 1 (percentage (make "1%" 200 2))))
  (is (= 99 (percentage (make "99%" 100 99)))))

(deftest message-test
  (is (= "mess" (message (make "mess")))))

(deftest nest-render-progress-test
  (let [last-progress (atom nil)
        render-progress! (fn [p] (reset! last-progress p))]
    (testing "plain nest"
      (let [nested-render-progress!
            (nest-render-progress render-progress!
                                  (make "parent" 4 2))]
        ;; parent is at 2/4, child at 1/2 with the child
        ;; spanning 1 of parents progress
        ;; => effectively at 2/4 + 1 * 1/2 = 5/8
        (nested-render-progress! (make "child" 2 1))
        (is (= 5/8 (fraction @last-progress)))))
    (testing "span nest"
      (let [nested-render-progress!
            (nest-render-progress render-progress!
                                  (make "parent" 4 2)
                                  2)]
        ;; parent is at 2/4, child at 1/2 with the child
        ;; spanning 2 of parents progress
        ;; effectively at 2/4 + 2 * 1/2 = 3/4
        (nested-render-progress! (make "child" 2 1))
        (is (= 3/4 (fraction @last-progress)))))
    (testing "precond failure"
      (is (thrown? java.lang.AssertionError
                   (nest-render-progress render-progress!
                                         (make "parent" 4 2)
                                         ;; span too large, 2 + 3 > 4
                                         3))))))

(deftest progress-mapv-test
  (let [render-res (atom [])
        render-f   #(swap! render-res conj %)
        message-f  #(str "m" %)]

    (is (= (map inc (range 3))
           (progress-mapv (fn [d _ ] (inc d)) (range 3) render-f message-f)))
    (is (= [(make "m0" 3 0)
            (make "m1" 3 1)
            (make "m2" 3 2)]
           @render-res))

    (reset! render-res [])

    (testing "empty col"
      (is (= [] (progress-mapv inc [] render-f)))
      (is (= [] @render-res)))

    (testing "nil"
      (is (= [] (progress-mapv inc nil render-f)))
      (is (= [] @render-res)))))

(deftest test-done?
  (is (done? done))
  (is (not (done? (make "job"))))
  (is (done? (advance (make "job") 1)))
  (is (not (done? (advance (make "job" 2) 1))))
  (is (done? (make "job" 2 2)))
  (is (not (done? (make "job" 2 1)))))
