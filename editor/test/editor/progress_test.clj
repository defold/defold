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

(ns editor.progress-test
  (:refer-clojure :exclude [mapv])
  (:require [clojure.test :refer :all]
            [editor.localization :as localization]
            [editor.progress :as progress])
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

    {:message (localization/message "a") :size 1 :pos 0 :cancel-state :not-cancellable}
    (progress/make (localization/message "a"))

    {:message (localization/message "b") :size 10 :pos 0 :cancel-state :not-cancellable}
    (progress/make (localization/message "b") 10)

    {:message (localization/message "c") :size 15 :pos 5 :cancel-state :not-cancellable}
    (progress/make (localization/message "c") 15 5)

    {:message (localization/message "d") :size 20 :pos 10 :cancel-state :not-cancellable}
    (progress/make (localization/message "d") 20 10 false)

    {:message (localization/message "e") :size 25 :pos 15 :cancel-state :cancellable}
    (progress/make (localization/message "e") 25 15 true)))

(deftest with-message-test
  (are [expected-fields progress]
    (let [actual-fields (to-map progress)]
      (is (instance? Progress progress))
      (is (= expected-fields actual-fields)))

    {:message (localization/message "new message") :size 20 :pos 10 :cancel-state :not-cancellable}
    (progress/with-message (progress/make (localization/message "d") 20 10 false) (localization/message "new message"))

    {:message (localization/message "other message") :size 25 :pos 15 :cancel-state :cancellable}
    (progress/with-message (progress/make (localization/message "e") 25 15 true) (localization/message "other message"))))

(deftest advance-test
  (are [expected-fields progress]
    (let [actual-fields (to-map progress)]
      (is (instance? Progress progress))
      (is (= expected-fields actual-fields)))

    {:message (localization/message "single step") :size 20 :pos 11 :cancel-state :not-cancellable}
    (progress/advance (progress/make (localization/message "single step") 20 10 false))

    {:message (localization/message "delta supplied") :size 25 :pos 20 :cancel-state :cancellable}
    (progress/advance (progress/make (localization/message "delta supplied") 25 15 true) 5)

    {:message (localization/message "message replaced") :size 25 :pos 20 :cancel-state :cancellable}
    (progress/advance (progress/make (localization/message "message") 25 15 true) 5 (localization/message "message replaced"))

    {:message (localization/message "past end") :size 2 :pos 2 :cancel-state :not-cancellable}
    (progress/advance (progress/make (localization/message "past end") 2 0 false) 5)

    {:message (localization/message "at end") :size 2 :pos 2 :cancel-state :not-cancellable}
    (progress/advance (progress/make (localization/message "at end") 2 2 false) 5)))

(deftest jump-test
  (testing "jump beyond size"
    (is (= 100 (progress/percentage (progress/jump (progress/make (localization/message "") 100) 666)))))
  (is (= 50 (progress/percentage (progress/jump (progress/make (localization/message "") 100 0) 50)))))

(deftest fraction-test
  (is (nil? (progress/fraction (progress/make (localization/message "mess") 0 0))))
  (is (= 0 (progress/fraction (progress/make (localization/message "mess")))))
  (is (= 1 (progress/fraction (progress/advance (progress/make (localization/message "mess"))))))
  (is (= 1/2 (progress/fraction (progress/advance (progress/make (localization/message "mess") 2))))))

(deftest percentage-test
  (is (nil? (progress/percentage (progress/make (localization/message "") 0 0))))
  (is (= 1 (progress/percentage (progress/make (localization/message "1%") 200 2))))
  (is (= 99 (progress/percentage (progress/make (localization/message "99%") 100 99)))))

(deftest message-test
  (is (= (localization/message "mess") (progress/message (progress/make (localization/message "mess"))))))

(deftest cancelable?-test
  (is (false? (progress/cancellable? (progress/make (localization/message "mess") 0 0 false))))
  (is (true? (progress/cancellable? (progress/make (localization/message "mess") 0 0 true))))
  (is (false? (progress/cancellable? (progress/cancel (progress/make (localization/message "mess") 0 0 true))))))

(deftest cancel-test
  (is (false? (progress/cancelled? (progress/make (localization/message "mess") 0 0 false))))
  (is (false? (progress/cancelled? (progress/make (localization/message "mess") 0 0 true))))
  (is (true? (progress/cancelled? (progress/cancel (progress/make (localization/message "mess") 0 0 true)))))
  (is (thrown? AssertionError (progress/cancel (progress/make (localization/message "mess") 0 0 false)))))

(deftest with-inherited-cancel-state-test
  (testing ":cancellable wins over :not-cancellable."
    (is (progress/cancellable?
          (progress/with-inherited-cancel-state
            (progress/->progress (localization/message "new") 1 0 :cancellable)
            (progress/->progress (localization/message "old") 1 0 :not-cancellable))))
    (is (progress/cancellable?
          (progress/with-inherited-cancel-state
            (progress/->progress (localization/message "new") 1 0 :not-cancellable)
            (progress/->progress (localization/message "old") 1 0 :cancellable)))))

  (testing ":cancelled wins over any state."
    (is (progress/cancelled?
          (progress/with-inherited-cancel-state
            (progress/->progress (localization/message "new") 1 0 :cancelled)
            (progress/->progress (localization/message "old") 1 0 :not-cancellable))))
    (is (progress/cancelled?
          (progress/with-inherited-cancel-state
            (progress/->progress (localization/message "new") 1 0 :cancelled)
            (progress/->progress (localization/message "old") 1 0 :cancellable))))
    (is (progress/cancelled?
          (progress/with-inherited-cancel-state
            (progress/->progress (localization/message "new") 1 0 :not-cancellable)
            (progress/->progress (localization/message "old") 1 0 :cancelled))))
    (is (progress/cancelled?
          (progress/with-inherited-cancel-state
            (progress/->progress (localization/message "new") 1 0 :cancellable)
            (progress/->progress (localization/message "old") 1 0 :cancelled)))))

  (testing "remaining field values are taken from the first Progress."
    (is (= {:message (localization/message "new message") :size 20 :pos 10 :cancel-state :not-cancellable}
           (to-map
             (progress/with-inherited-cancel-state
               (progress/->progress (localization/message "new message") 20 10 :not-cancellable)
               (progress/->progress (localization/message "old message") 1 0 :not-cancellable)))))
    (is (= {:message (localization/message "other message") :size 25 :pos 15 :cancel-state :not-cancellable}
           (to-map
             (progress/with-inherited-cancel-state
               (progress/->progress (localization/message "other message") 25 15 :not-cancellable)
               (progress/->progress (localization/message "old message") 1 0 :not-cancellable)))))))

(deftest nest-render-progress-test
  (let [last-progress (atom nil)
        render-progress! (fn [p] (reset! last-progress p))]
    (testing "plain nest"
      (let [nested-render-progress!
            (progress/nest-render-progress render-progress!
                                  (progress/make (localization/message "parent") 4 2))]
        ;; parent is at 2/4, child at 1/2 with the child
        ;; spanning 1 of parents progress
        ;; => effectively at 2/4 + 1 * 1/2 = 5/8
        (nested-render-progress! (progress/make (localization/message "child") 2 1))
        (is (= 5/8 (progress/fraction @last-progress)))))
    (testing "span nest"
      (let [nested-render-progress!
            (progress/nest-render-progress render-progress!
                                  (progress/make (localization/message "parent") 4 2)
                                  2)]
        ;; parent is at 2/4, child at 1/2 with the child
        ;; spanning 2 of parents progress
        ;; effectively at 2/4 + 2 * 1/2 = 3/4
        (nested-render-progress! (progress/make (localization/message "child") 2 1))
        (is (= 3/4 (progress/fraction @last-progress)))))
    (testing "precond failure"
      (is (thrown? AssertionError
                   (progress/nest-render-progress render-progress!
                                         (progress/make (localization/message "parent") 4 2)
                                         ;; span too large, 2 + 3 > 4
                                         3))))))

(deftest progress-mapv-test
  (let [render-res (atom [])
        render-f   #(swap! render-res conj %)
        message-f  #(localization/message (str "m" %))]

    (is (= (map inc (range 3))
           (progress/progress-mapv (fn [^long d _]
                                     (inc d))
                                   (range 3)
                                   render-f
                                   message-f)))
    (is (= [(progress/make (localization/message "m0") 3 0)
            (progress/make (localization/message "m1") 3 1)
            (progress/make (localization/message "m2") 3 2)]
           @render-res))

    (reset! render-res [])

    (testing "empty col"
      (is (= [] (progress/progress-mapv inc [] render-f)))
      (is (= [] @render-res)))

    (testing "nil"
      (is (= [] (progress/progress-mapv inc nil render-f)))
      (is (= [] @render-res)))))

(deftest test-done?
  (is (progress/done? progress/done))
  (is (not (progress/done? (progress/make (localization/message "job")))))
  (is (progress/done? (progress/advance (progress/make (localization/message "job")) 1)))
  (is (not (progress/done? (progress/advance (progress/make (localization/message "job") 2) 1))))
  (is (progress/done? (progress/make (localization/message "job") 2 2)))
  (is (not (progress/done? (progress/make (localization/message "job") 2 1)))))
