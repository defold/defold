(ns dynamo.condition-test
  (:require [dynamo.condition :refer :all]
            [clojure.test :refer :all]))

(defn simulate-file-read [f] f)

(defn low-level
  [force xtra]
  (case force
    :signal-one    (signal :test-condition-one)
    :signal-other  (signal :test-other-condition)
    :signal-three  (signal :test-condition-three)
    :signal-four   (signal :conditional-logic :val xtra)
    force))

(defn medium-level
  [force xtra]
  (restart-case
   (:test-condition-one
    (:use-value [v] v)
    (:retry     [f] (simulate-file-read f)))
   (:conditional-logic
    (:use-value [v] v))
   (low-level force xtra)))

(defn use-replacement-value
  [_]
  (invoke-restart :use-value 42))

(defn high-level
  [force xtra]
  (handler-bind
   (:test-condition-one #'use-replacement-value)
   (:test-other-condition (fn [x] x))
   (:conditional-logic    (fn [{:keys [val]}]
                            (if (< 0 val)
                              (invoke-restart :use-value 0)
                              (invoke-restart :use-value val))))
   (medium-level force xtra)))

(deftest signal-with-restart
  (is (= 42 (high-level :signal-one 0))))

(deftest signal-with-bounce-and-return
  (is (= {:condition :test-other-condition} (high-level :signal-other 0))))

(deftest signal-without-handler
  (is (thrown? clojure.lang.ExceptionInfo (high-level :signal-three 0))))

(deftest signal-with-logic-in-handler
  (is (= 0 (high-level :signal-four 100))))
