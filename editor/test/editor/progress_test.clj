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

(ns editor.progress-test
  (:refer-clojure :exclude [mapv])
  (:require [clojure.test :refer :all]
            [editor.progress :refer :all])
  (:import [editor.progress Progress]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- to-map [^Progress progress]
  (into {} progress))

(deftest make-test
  (are [expected-fields progress]
    (let [actual-fields (to-map progress)]
      (is (instance? Progress progress))
      (is (= expected-fields actual-fields)))

    {:message "a" :size 1 :pos 0 :cancel-state :not-cancellable}
    (make "a")

    {:message "b" :size 10 :pos 0 :cancel-state :not-cancellable}
    (make "b" 10)

    {:message "c" :size 15 :pos 5 :cancel-state :not-cancellable}
    (make "c" 15 5)

    {:message "d" :size 20 :pos 10 :cancel-state :not-cancellable}
    (make "d" 20 10 false)

    {:message "e" :size 25 :pos 15 :cancel-state :cancellable}
    (make "e" 25 15 true)))

(deftest with-message-test
  (are [expected-fields progress]
    (let [actual-fields (to-map progress)]
      (is (instance? Progress progress))
      (is (= expected-fields actual-fields)))

    {:message "new message" :size 20 :pos 10 :cancel-state :not-cancellable}
    (with-message (make "d" 20 10 false) "new message")

    {:message "other message" :size 25 :pos 15 :cancel-state :cancellable}
    (with-message (make "e" 25 15 true) "other message")))

(deftest advance-test
  (are [expected-fields progress]
    (let [actual-fields (to-map progress)]
      (is (instance? Progress progress))
      (is (= expected-fields actual-fields)))

    {:message "single step" :size 20 :pos 11 :cancel-state :not-cancellable}
    (advance (make "single step" 20 10 false))

    {:message "delta supplied" :size 25 :pos 20 :cancel-state :cancellable}
    (advance (make "delta supplied" 25 15 true) 5)

    {:message "message replaced" :size 25 :pos 20 :cancel-state :cancellable}
    (advance (make "message" 25 15 true) 5 "message replaced")

    {:message "past end" :size 2 :pos 2 :cancel-state :not-cancellable}
    (advance (make "past end" 2 0 false) 5)

    {:message "at end" :size 2 :pos 2 :cancel-state :not-cancellable}
    (advance (make "at end" 2 2 false) 5)))

(deftest jump-test
  (testing "jump beyond size"
    (is (= 100 (percentage (jump (make "" 100) 666)))))
  (is (= 50 (percentage (jump (make "" 100 0) 50)))))

(deftest fraction-test
  (is (nil? (fraction (make "mess" 0 0))))
  (is (= 0 (fraction (make "mess"))))
  (is (= 1 (fraction (advance (make "mess")))))
  (is (= 1/2 (fraction (advance (make "mess" 2))))))

(deftest percentage-test
  (is (nil? (percentage (make "" 0 0))))
  (is (= 1 (percentage (make "1%" 200 2))))
  (is (= 99 (percentage (make "99%" 100 99)))))

(deftest message-test
  (is (= "mess" (message (make "mess")))))

(deftest cancelable?-test
  (is (false? (cancellable? (make "mess" 0 0 false))))
  (is (true? (cancellable? (make "mess" 0 0 true))))
  (is (false? (cancellable? (cancel (make "mess" 0 0 true))))))

(deftest cancel-test
  (is (false? (cancelled? (make "mess" 0 0 false))))
  (is (false? (cancelled? (make "mess" 0 0 true))))
  (is (true? (cancelled? (cancel (make "mess" 0 0 true)))))
  (is (thrown? AssertionError (cancel (make "mess" 0 0 false)))))

(deftest with-inherited-cancel-state-test
  (testing ":cancellable wins over :not-cancellable."
    (is (cancellable?
          (with-inherited-cancel-state
            (->progress "new" 1 0 :cancellable)
            (->progress "old" 1 0 :not-cancellable))))
    (is (cancellable?
          (with-inherited-cancel-state
            (->progress "new" 1 0 :not-cancellable)
            (->progress "old" 1 0 :cancellable)))))

  (testing ":cancelled wins over any state."
    (is (cancelled?
          (with-inherited-cancel-state
            (->progress "new" 1 0 :cancelled)
            (->progress "old" 1 0 :not-cancellable))))
    (is (cancelled?
          (with-inherited-cancel-state
            (->progress "new" 1 0 :cancelled)
            (->progress "old" 1 0 :cancellable))))
    (is (cancelled?
          (with-inherited-cancel-state
            (->progress "new" 1 0 :not-cancellable)
            (->progress "old" 1 0 :cancelled))))
    (is (cancelled?
          (with-inherited-cancel-state
            (->progress "new" 1 0 :cancellable)
            (->progress "old" 1 0 :cancelled)))))

  (testing "remaining field values are taken from the first Progress."
    (is (= {:message "new message" :size 20 :pos 10 :cancel-state :not-cancellable}
           (to-map
             (with-inherited-cancel-state
               (->progress "new message" 20 10 :not-cancellable)
               (->progress "old message" 1 0 :not-cancellable)))))
    (is (= {:message "other message" :size 25 :pos 15 :cancel-state :not-cancellable}
           (to-map
             (with-inherited-cancel-state
               (->progress "other message" 25 15 :not-cancellable)
               (->progress "old message" 1 0 :not-cancellable)))))))

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
      (is (thrown? AssertionError
                   (nest-render-progress render-progress!
                                         (make "parent" 4 2)
                                         ;; span too large, 2 + 3 > 4
                                         3))))))

(deftest progress-mapv-test
  (let [render-res (atom [])
        render-f   #(swap! render-res conj %)
        message-f  #(str "m" %)]

    (is (= (map inc (range 3))
           (progress-mapv (fn [^long d _]
                            (inc d))
                          (range 3)
                          render-f
                          message-f)))
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
