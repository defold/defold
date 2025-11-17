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

(ns editor.sentry-test
  (:require [clojure.data.json :as json]
            [clojure.test :refer :all]
            [editor.sentry :as sentry]
            [schema.core :as s])
  (:import (java.time LocalDateTime ZoneOffset)))

(def ^:private sentry-options
  {:project-id "my-project-id"
   :key "my-sentry-key"
   :secret "my-sentry-secret"})

(deftest to-safe-json-value-passthrough
  (let [safe-values [nil true false 1.0 "a" :a 'a [true] {:key true}]]
    (is (= safe-values (sentry/to-safe-json-value safe-values)))))

(deftest to-safe-json-value-conversion
  (is (= (str Object) (sentry/to-safe-json-value Object)))
  (is (= (str cons) (sentry/to-safe-json-value cons)))
  (is (= '(not (number? :a)) (sentry/to-safe-json-value (s/check (s/pred number?) :a)))))

(deftest to-safe-json-value-converts-sets-to-vectors
  (is (= [0 1 2 3 4] (sentry/to-safe-json-value #{2 1 3 0 4})))
  (is (= [:a :b :c] (sentry/to-safe-json-value (sorted-set :c :a :b)))))

(deftest to-safe-json-value-collection-recursion
  (let [value Object
        json-value (str value)]
    (is (= [json-value] (sentry/to-safe-json-value [value])))
    (is (= [json-value] (sentry/to-safe-json-value #{value})))
    (is (= {:key json-value} (sentry/to-safe-json-value {:key value})))
    (is (= [[json-value]] (sentry/to-safe-json-value [[value]])))
    (is (= [[json-value]] (sentry/to-safe-json-value #{#{value}})))
    (is (= {:key {:key json-value}} (sentry/to-safe-json-value {:key {:key value}})))))

(deftest to-safe-json-value-validation-error-does-not-require-recursion
  (is (= '(not (number? a-java.lang.Class))
         (sentry/to-safe-json-value (s/check (s/pred number?) Object)))))

(deftest make-event-converts-unrepresentable-values-in-ex-data
  (let [class Object
        json-class (str class)
        fn cons
        json-fn (str fn)
        event (sentry/make-event (ex-info "Failure"
                                          {:class class
                                           :class-list [class]
                                           :class-map {:key class}
                                           :fn fn
                                           :fn-list [fn]
                                           :fn-map {:key fn}})
                                 (Thread/currentThread)
                                 {:id "test-user"})
        extra (:extra event)]
    (is (= json-class (:class extra)))
    (is (= [json-class] (:class-list extra)))
    (is (= {:key json-class} (:class-map extra)))
    (is (= json-fn (:fn extra)))
    (is (= [json-fn] (:fn-list extra)))
    (is (= {:key json-fn} (:fn-map extra)))))

(deftest make-request-data-succeeds-even-if-event-extra-data-contains-unrepresentable-values
  (let [invalid-event {:timestamp (LocalDateTime/now ZoneOffset/UTC)
                       :extra {:unrepresentable-values [Object cons]}}
        request (sentry/make-request-data sentry-options invalid-event)
        data (json/read-str (:body request))]
    (testing "Extra data contains java-home"
      (is (string? (get-in data ["extra" "java-home"]))))
    (testing "Extra data contains info about conversion-failure"
      (let [[conversion-failure] (get-in data ["extra" "conversion-failure"])]
        (is (contains? conversion-failure "type"))
        (is (contains? conversion-failure "stacktrace"))
        (is (contains? conversion-failure "thread_id"))))))
