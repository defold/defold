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

(ns integration.test-util-test
  (:require [clojure.test :refer :all]
            [editor.ui :as ui]
            [integration.test-util :as test-util]))

(deftest run-event-loop-test

  (testing "Exit function terminates event loop."
    (is (nil? (test-util/run-event-loop! (fn [exit-event-loop!]
                                           (exit-event-loop!)))))
    (is (nil? (test-util/run-event-loop! (fn [exit-event-loop!]
                                           (ui/run-now
                                             (exit-event-loop!))))))
    (is (nil? (test-util/run-event-loop! (fn [exit-event-loop!]
                                           (ui/run-later
                                             (exit-event-loop!))))))
    (is (nil? (test-util/run-event-loop! (fn [exit-event-loop!]
                                           (ui/run-later
                                             (ui/run-later
                                               (exit-event-loop!)))))))
    (is (nil? (test-util/run-event-loop! (fn [exit-event-loop!]
                                           (ui/run-later
                                             (ui/run-now
                                               (exit-event-loop!)))))))
    (is (nil? (test-util/run-event-loop! (fn [exit-event-loop!]
                                           (ui/run-now
                                             (ui/run-later
                                               (exit-event-loop!))))))))

  (testing "Throwing terminates event loop."
    (is (thrown? AssertionError (test-util/run-event-loop!
                                  (fn [_exit-event-loop!]
                                    (assert false)))))
    (is (thrown? AssertionError (test-util/run-event-loop!
                                  (fn [_exit-event-loop!]
                                    (ui/run-now
                                      (assert false))))))
    (is (thrown? AssertionError (test-util/run-event-loop!
                                  (fn [_exit-event-loop!]
                                    (ui/run-later
                                      (assert false))))))
    (is (thrown? AssertionError (test-util/run-event-loop!
                                  (fn [_exit-event-loop!]
                                    (ui/run-later
                                      (ui/run-later
                                        (assert false)))))))
    (is (thrown? AssertionError (test-util/run-event-loop!
                                  (fn [_exit-event-loop!]
                                    (ui/run-later
                                      (ui/run-now
                                        (assert false)))))))
    (is (thrown? AssertionError (test-util/run-event-loop!
                                  (fn [_exit-event-loop!]
                                    (ui/run-now
                                      (ui/run-later
                                        (assert false))))))))

  (testing "Blocks until terminated."
    (let [begin (System/currentTimeMillis)]
      (test-util/run-event-loop! (fn [exit-event-loop!]
                                   (Thread/sleep 300)
                                   (exit-event-loop!)))
      (is (<= 230 (- (System/currentTimeMillis) begin)))))

  (testing "Performs side-effects in order."
    (let [side-effects (atom [])]
      (test-util/run-event-loop! (fn [exit-event-loop!]
                                   (ui/run-later
                                     (swap! side-effects conj :b)
                                     (exit-event-loop!))
                                   (swap! side-effects conj :a)))
      (is (= [:a :b] @side-effects))))

  (testing "Executes actions on the calling thread."
    (let [calling-thread (Thread/currentThread)
          action-threads (atom [])]
      (test-util/run-event-loop! (fn [exit-event-loop!]
                                   (swap! action-threads conj (Thread/currentThread))
                                   (ui/run-now
                                     (swap! action-threads conj (Thread/currentThread)))
                                   (ui/run-later
                                     (swap! action-threads conj (Thread/currentThread))
                                     (exit-event-loop!))))
      (is (= [calling-thread calling-thread calling-thread] @action-threads))))

  (testing "UI thread checks."
    (let [errors (atom [])
          results (atom [])]
      (test-util/run-event-loop! (fn [exit-event-loop!]
                                   (swap! results conj (ui/on-ui-thread?))
                                   (ui/run-now
                                     (swap! results conj (ui/on-ui-thread?)))
                                   (ui/run-later
                                     (swap! results conj (ui/on-ui-thread?))
                                     (future
                                       (try
                                         (swap! results conj (ui/on-ui-thread?))
                                         (ui/run-now
                                           (swap! results conj (ui/on-ui-thread?))
                                           (future
                                             (try
                                               (swap! results conj (ui/on-ui-thread?))
                                               (catch Throwable error
                                                 (swap! errors conj error))
                                               (finally
                                                 (exit-event-loop!)))))
                                         (catch Throwable error
                                           (swap! errors conj error)
                                           (exit-event-loop!)))))))
      (is (empty? @errors))
      (is (= [true true true false true false] @results)))))
