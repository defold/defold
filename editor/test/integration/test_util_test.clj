(ns integration.test-util-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.ui :as ui]
            [integration.test-util :as test-util]
            [support.test-support :refer [with-clean-system]]))

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
      (is (<= 300 (- (System/currentTimeMillis) begin)))))

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

(g/defnode TestNode
  (property title g/Str (default "untitled")))

(deftest graph-reverter-test

  (with-clean-system
    (let [graph-id (g/make-graph! :history true :volatility 1)
          node-tx-data (g/make-node graph-id TestNode)
          node-id (first (g/tx-data-nodes-added node-tx-data))]
      (with-open [_ (test-util/make-graph-reverter graph-id)]
        (is (not (g/node-instance? TestNode node-id)))
        (g/transact node-tx-data)
        (is (g/node-instance? TestNode node-id)))
      (is (not (g/node-instance? TestNode node-id)))))

  (with-clean-system
    (let [graph-id (g/make-graph! :history true :volatility 1)
          node-tx-data (g/make-node graph-id TestNode)
          node-id (first (g/tx-data-nodes-added node-tx-data))]
      (with-open [_ (test-util/make-graph-reverter graph-id)]
        (is (not (g/node-instance? TestNode node-id)))
        (g/transact node-tx-data)
        (is (g/node-instance? TestNode node-id))
        (with-open [_ (test-util/make-graph-reverter graph-id)]
          (is (= "untitled" (g/node-value node-id :title)))
          (g/set-property! node-id :title "titled")
          (is (= "titled" (g/node-value node-id :title))))
        (is (= "untitled" (g/node-value node-id :title))))
      (is (not (g/node-instance? TestNode node-id))))))
