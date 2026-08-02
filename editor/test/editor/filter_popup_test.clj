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

(ns editor.filter-popup-test
  (:require [clojure.test :refer :all]
            [editor.filter-popup :as filter-popup]
            [util.fn :as fn]))

(set! *warn-on-reflection* true)

(deftest badge-count-test
  (testing "No exclusions active"
    (is (zero? (filter-popup/badge-count {:filter-popup-filtering-enabled true
                                           :exclude-patterns []
                                           :exclude-libraries false
                                           :exclude-hidden false}))))

  (testing "Counts only enabled patterns"
    (is (= 1 (filter-popup/badge-count {:filter-popup-filtering-enabled true
                                        :exclude-patterns [["a" true] ["b" false]]
                                        :exclude-libraries false
                                        :exclude-hidden false}))))

  (testing "Counts multiple enabled patterns"
    (is (= 2 (filter-popup/badge-count {:filter-popup-filtering-enabled true
                                        :exclude-patterns [["a" true] ["b" true] ["c" false]]
                                        :exclude-libraries false
                                        :exclude-hidden false}))))

  (testing "Libraries and hidden each contribute one"
    (is (= 2 (filter-popup/badge-count {:filter-popup-filtering-enabled true
                                        :exclude-patterns []
                                        :exclude-libraries true
                                        :exclude-hidden true}))))

  (testing "Patterns, libraries, and hidden all combine"
    (is (= 3 (filter-popup/badge-count {:filter-popup-filtering-enabled true
                                        :exclude-patterns [["a" true]]
                                        :exclude-libraries true
                                        :exclude-hidden true}))))

  (testing "Patterns don't count when filtering is disabled, but toggles still do"
    (is (= 2 (filter-popup/badge-count {:filter-popup-filtering-enabled false
                                        :exclude-patterns [["a" true] ["b" true]]
                                        :exclude-libraries true
                                        :exclude-hidden true})))))

(defn- initial-state []
  {:filter-popup-open false
   :filter-popup-text ""
   :filter-popup-filtering-enabled true
   :exclude-patterns [["a" true] ["b" false]]
   :exclude-libraries false
   :exclude-hidden false})

(defn- make-event-handler []
  (let [on-patterns-changed (fn/make-call-logger)
        on-filtering-changed (fn/make-call-logger)
        on-filter-changed (fn/make-call-logger)]
    {:event-handler (filter-popup/event-handler
                       on-patterns-changed on-filtering-changed on-filter-changed)
     :on-patterns-changed on-patterns-changed
     :on-filtering-changed on-filtering-changed
     :on-filter-changed on-filter-changed}))

(deftest event-handler-test
  (testing ":filter-popup/toggle-open flips :filter-popup-open"
    (let [{:keys [event-handler]} (make-event-handler)]
      (is (true? (:filter-popup-open (event-handler (initial-state) {:event-type :filter-popup/toggle-open}))))
      (is (false? (:filter-popup-open (event-handler (assoc (initial-state) :filter-popup-open true)
                                                      {:event-type :filter-popup/toggle-open}))))))

  (testing ":filter-popup/hide always closes the popup"
    (let [{:keys [event-handler]} (make-event-handler)]
      (is (false? (:filter-popup-open (event-handler (assoc (initial-state) :filter-popup-open true)
                                                      {:event-type :filter-popup/hide}))))))

  (testing ":filter-popup/set-text stores the new text from :fx/event"
    (let [{:keys [event-handler]} (make-event-handler)]
      (is (= "foo" (:filter-popup-text (event-handler (initial-state) {:event-type :filter-popup/set-text :fx/event "foo"}))))))

  (testing ":filter-popup/toggle-filtering flips the flag and calls on-filtering-changed"
    (let [{:keys [event-handler on-filtering-changed]} (make-event-handler)
          new-state (event-handler (initial-state) {:event-type :filter-popup/toggle-filtering})]
      (is (false? (:filter-popup-filtering-enabled new-state)))
      (is (= [[false]] (fn/call-logger-calls on-filtering-changed)))))

  (testing ":filter-popup/toggle-filter flips :exclude-libraries and calls on-filter-changed"
    (let [{:keys [event-handler on-filter-changed]} (make-event-handler)
          new-state (event-handler (initial-state) {:event-type :filter-popup/toggle-filter :key :exclude-libraries})]
      (is (true? (:exclude-libraries new-state)))
      (is (= [[:exclude-libraries true]] (fn/call-logger-calls on-filter-changed)))))

  (testing ":filter-popup/toggle-filter flips :exclude-hidden and calls on-filter-changed"
    (let [{:keys [event-handler on-filter-changed]} (make-event-handler)
          new-state (event-handler (initial-state) {:event-type :filter-popup/toggle-filter :key :exclude-hidden})]
      (is (true? (:exclude-hidden new-state)))
      (is (= [[:exclude-hidden true]] (fn/call-logger-calls on-filter-changed)))))

  (testing ":filter-popup/toggle flips the pattern at :index, calls on-patterns-changed, and opens the popup"
    (let [{:keys [event-handler on-patterns-changed]} (make-event-handler)
          new-state (event-handler (initial-state) {:event-type :filter-popup/toggle :index 1})]
      (is (= [["a" true] ["b" true]] (:exclude-patterns new-state)))
      (is (true? (:filter-popup-open new-state)))
      (is (= [[[["a" true] ["b" true]]]] (fn/call-logger-calls on-patterns-changed)))))

  (testing ":filter-popup/add appends a trimmed, non-blank, non-duplicate pattern as enabled"
    (let [{:keys [event-handler on-patterns-changed]} (make-event-handler)
          state (assoc (initial-state) :filter-popup-text "  c  ")
          new-state (event-handler state {:event-type :filter-popup/add})]
      (is (= [["a" true] ["b" false] ["c" true]] (:exclude-patterns new-state)))
      (is (= "" (:filter-popup-text new-state)))
      (is (= [[[["a" true] ["b" false] ["c" true]]]] (fn/call-logger-calls on-patterns-changed)))))

  (testing ":filter-popup/add on blank text is a no-op"
    (let [{:keys [event-handler on-patterns-changed]} (make-event-handler)
          state (assoc (initial-state) :filter-popup-text "   ")
          new-state (event-handler state {:event-type :filter-popup/add})]
      (is (= state new-state))
      (is (empty? (fn/call-logger-calls on-patterns-changed)))))

  (testing ":filter-popup/add on a duplicate pattern only clears the text field"
    (let [{:keys [event-handler on-patterns-changed]} (make-event-handler)
          state (assoc (initial-state) :filter-popup-text "a")
          new-state (event-handler state {:event-type :filter-popup/add})]
      (is (= (:exclude-patterns state) (:exclude-patterns new-state)))
      (is (= "" (:filter-popup-text new-state)))
      (is (empty? (fn/call-logger-calls on-patterns-changed)))))

  (testing ":filter-popup/remove drops the pattern at :index and calls on-patterns-changed"
    (let [{:keys [event-handler on-patterns-changed]} (make-event-handler)
          new-state (event-handler (initial-state) {:event-type :filter-popup/remove :index 0})]
      (is (= [["b" false]] (:exclude-patterns new-state)))
      (is (= [[[["b" false]]]] (fn/call-logger-calls on-patterns-changed)))))

  (testing "Unhandled event types return nil"
    (let [{:keys [event-handler]} (make-event-handler)]
      (is (nil? (event-handler (initial-state) {:event-type ::unknown}))))))
