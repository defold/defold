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

(ns util.task-test
  (:require [clojure.test :refer :all]
            [util.defonce :as defonce]
            [util.task :as task])
  (:import [clojure.lang Compiler$CompilerException]
           [java.lang AutoCloseable WrongThreadException]
           [java.util NoSuchElementException]
           [java.util.concurrent CountDownLatch Semaphore StructuredTaskScope$Subtask]))

(set! *warn-on-reflection* true)

(def ^:dynamic *binding-value* nil)

(defn- resource
  ^AutoCloseable [closed id exception]
  (reify AutoCloseable
    (close [_]
      (swap! closed conj id)
      (when exception
        (throw exception)))))

(defonce/interface CloseMethod
  (^void close []))

;; Objects with a close method need not implement AutoCloseable, guarding against
;; a forced interface cast rejecting values accepted by clojure.core/with-open.
(deftest with-open-close-method-test
  (let [closed (atom 0)
        value (reify CloseMethod
                (close [_] (swap! closed inc)))]
    (is (not (instance? AutoCloseable value)))
    (is (= :result
           (task/with-open [^CloseMethod opened value]
             (is (identical? value opened))
             :result)))
    (is (= 1 @closed))))

;; Multiple resources can depend on earlier bindings and close exactly once in
;; reverse order; empty bindings and falsey body values behave like with-open.
(deftest with-open-bindings-test
  (let [closed (atom [])]
    (is (= :result
           (task/with-open [first-resource (resource closed :first nil)
                            second-resource (do
                                              (is (instance? java.lang.AutoCloseable first-resource))
                                              (resource closed :second nil))]
             (is (instance? java.lang.AutoCloseable second-resource))
             :result)))
    (is (= [:second :first] @closed)))
  (is (nil? (task/with-open [])))
  (is (false? (task/with-open [] false))))

;; Failure to initialize a later resource closes earlier ones and preserves the
;; initialization exception even when cleanup throws.
(deftest with-open-initialization-failure-test
  (let [closed (atom [])
        initialization-error (ex-info "initialization" {})
        close-error (ex-info "close" {})
        outcome (try
                  (task/with-open [_first-resource (resource closed :first close-error)
                                   ^AutoCloseable _second-resource (throw initialization-error)]
                    (is false "The body must not run after initialization fails"))
                  (catch Throwable exception exception))]
    (is (identical? initialization-error outcome))
    (is (= [close-error] (vec (.getSuppressed ^Throwable outcome))))
    (is (= [:first] @closed))))

;; Every resource closes despite cleanup failures; a body failure remains primary,
;; otherwise the first cleanup failure remains primary and later ones are suppressed.
(deftest with-open-close-failures-test
  (doseq [body-fails [false true]]
    (let [closed (atom [])
          body-error (ex-info "body" {})
          first-close-error (ex-info "first close" {})
          second-close-error (ex-info "second close" {})
          outcome (try
                    (task/with-open [_first-resource (resource closed :first first-close-error)
                                     _second-resource (resource closed :second second-close-error)]
                      (when body-fails
                        (throw body-error))
                      :result)
                    (catch Throwable exception exception))]
      (is (identical? (if body-fails body-error second-close-error) outcome))
      (is (= (if body-fails [second-close-error first-close-error] [first-close-error])
             (vec (.getSuppressed ^Throwable outcome))))
      (is (= [:second :first] @closed)))))

(defn- await-cancellation! [finished]
  (deliver finished
           (try
             (.await (CountDownLatch. 1))
             false
             (catch InterruptedException _ true))))

(defn- results [subtasks]
  (mapv #(.get ^StructuredTaskScope$Subtask %) subtasks))

;; All children run concurrently on virtual threads, and the returned body
;; value is available only after its children have completed, including nil/false.
(deftest all-successful-test
  (let [started (CountDownLatch. 3)
        owner (Thread/currentThread)]
    (is (= [:first nil false]
           (results
             (task/scope :all-successful
               (mapv (fn [value]
                       (task/fork
                         (.countDown started)
                         (.await started)
                         (is (.isVirtual (Thread/currentThread)))
                         (is (not (identical? owner (Thread/currentThread))))
                         value))
                     [:first nil false])))))))

;; The body value is returned only after a held child finishes, guarding against
;; returning before the scope joins successful work.
(deftest body-result-test
  (let [started (CountDownLatch. 1)
        release (CountDownLatch. 1)
        outcome (future
                  (task/scope :all-successful
                    (task/fork
                      (.countDown started)
                      (.await release))
                    :body))]
    (try
      (.await started)
      (is (not (realized? outcome)))
      (finally (.countDown release)))
    (is (= :body @outcome))))

;; Forks inherit scope-entry bindings, while a nested scope captures the
;; bindings active at its own entry rather than the enclosing scope's snapshot.
(deftest bindings-test
  (binding [*binding-value* :outer]
    (is (= [:outer :outer :inner]
           (results
             (task/scope :all-successful
               (let [outer (task/fork *binding-value*)
                     inner (binding [*binding-value* :inner]
                             [(task/fork *binding-value*)
                              (task/scope :all-successful
                                (task/fork *binding-value*))])]
                 [outer (inner 0) (inner 1)])))))))

;; A child binding stays local while another child reads the captured frame.
(deftest binding-isolation-test
  (let [bound (CountDownLatch. 1)
        observed (CountDownLatch. 1)]
    (binding [*binding-value* :outer]
      (is (= [[:child :outer] :outer]
             (results
               (task/scope :all-successful
                 [(task/fork
                    [(binding [*binding-value* :child]
                       (.countDown bound)
                       (.await observed)
                       *binding-value*)
                     *binding-value*])
                  (task/fork
                    (.await bound)
                    (let [value *binding-value*]
                      (.countDown observed)
                      value))]))))
      (is (= :outer *binding-value*)))))

;; Child failure propagates unchanged only after cancelled siblings finish cleanup.
;; Holding cleanup catches scopes that interrupt their children but return early.
(deftest child-failure-joins-cleanup-test
  (doseq [policy [:all-successful :first-completed]]
    (let [exception (ex-info "child failure" {})
          started (CountDownLatch. 1)
          cancelled (CountDownLatch. 1)
          release (Semaphore. 0)
          child (promise)
          outcome (future
                    (try
                      (task/scope policy
                        (task/fork
                          (.await started)
                          (throw exception))
                        (task/fork
                          (deliver child (Thread/currentThread))
                          (.countDown started)
                          (try
                            (.await (CountDownLatch. 1))
                            (finally
                              (.countDown cancelled)
                              (.acquireUninterruptibly release)))))
                      (catch Throwable exception exception)))]
      (try
        (.await cancelled)
        (is (not (realized? outcome)))
        (finally
          (.release release)
          (.interrupt ^Thread @child)
          @outcome))
      (is (identical? exception @outcome)))))

;; First completion accepts nil and false results and interrupts remaining work,
;; guarding against treating a completed falsey result as an unfinished scope.
(deftest first-completed-result-test
  (doseq [value [nil false :result]]
    (let [started (CountDownLatch. 1)
          finished (promise)]
      (is (= value
             (task/scope :first-completed
               (task/fork
                 (.await started)
                 value)
               (task/fork
                 (.countDown started)
                 (await-cancellation! finished)))))
      (is (and (realized? finished) @finished)))))

;; An owner failure before join interrupts children and keeps the
;; original exception when cancellation also closes the scope.
(deftest owner-failure-test
  (doseq [policy [:all-successful :first-completed]]
    (let [exception (ex-info "owner failure" {})
          started (CountDownLatch. 1)
          finished (promise)
          outcome
          (try
            (task/scope policy
              (task/fork
                (.countDown started)
                (await-cancellation! finished))
              (.await started)
              (throw exception))
            (catch Throwable exception exception))]
      (is (identical? exception outcome))
      (is (and (realized? finished) @finished)))))

;; Interrupting the owner waits for every child's held cleanup before throwing,
;; guarding against cancellation leaving transport work running.
(deftest owner-interruption-test
  (doseq [policy [:all-successful :first-completed]]
    (let [started (CountDownLatch. 2)
          cancelled (CountDownLatch. 2)
          release (Semaphore. 0)
          children (atom [])
          outcome (promise)
          owner (.start (Thread/ofVirtual)
                        ^Runnable
                        (fn []
                          (try
                            (task/scope policy
                              (dotimes [_ 2]
                                (task/fork
                                  (swap! children conj (Thread/currentThread))
                                  (.countDown started)
                                  (try
                                    (.await (CountDownLatch. 1))
                                    (finally
                                      (.countDown cancelled)
                                      (.acquireUninterruptibly release))))))
                            (deliver outcome :completed)
                            (catch Throwable exception
                              (deliver outcome exception)))))]
      (try
        (.await started)
        (.interrupt owner)
        (.await cancelled)
        (is (not (realized? outcome)))
        (finally
          (.release release 2)
          (run! Thread/.interrupt @children)
          (.interrupt owner)
          (.join owner)))
      (is (not (.isAlive owner)))
      (is (instance? InterruptedException @outcome)))))

;; Leaving a nested scope preserves the outer scope used by subsequent forks.
(deftest nested-scope-test
  (is (= [:before :inner :after]
         (results
           (task/scope :all-successful
             (let [before (task/fork :before)
                   inner (task/scope :all-successful (task/fork :inner))]
               [before inner (task/fork :after)]))))))

;; A child must open its own scope before forking, guarding against an
;; accidentally captured outer scope bypassing the JDK owner-thread check.
(deftest fork-owner-test
  (is (thrown? WrongThreadException
               (task/scope :all-successful
                 (task/fork (task/fork :wrong-owner))))))

;; Empty scopes return the body value or reject a missing first completion.
;; Invalid policies and forks outside a lexical scope cannot start work.
(deftest scope-boundaries-test
  (let [exception (try
                    (macroexpand '(util.task/fork :outside))
                    (catch Compiler$CompilerException exception exception))]
    (is (instance? IllegalArgumentException (.getCause ^Throwable exception)))
    (is (re-find #"inside task/scope" (.getMessage (.getCause ^Throwable exception)))))
  (is (nil? (task/scope :all-successful)))
  (is (false? (task/scope :all-successful false)))
  (is (= :body (task/scope :all-successful :body)))
  (is (thrown? NoSuchElementException (task/scope :first-completed)))
  (is (thrown? IllegalArgumentException
               (task/scope :unknown
                 (is false "Invalid policies must not run the body"))))
  (testing "Computed policies are evaluated once"
    (doseq [policy [:all-successful :first-completed :unknown]]
      (let [evaluations (atom 0)
            result (try
                     (task/scope (do (swap! evaluations inc) policy)
                       (task/fork false)
                       :owner)
                     (catch IllegalArgumentException exception exception))]
        (is (= 1 @evaluations))
        (case policy
          :all-successful (is (= :owner result))
          :first-completed (is (false? result))
          :unknown (is (instance? IllegalArgumentException result)))))))
