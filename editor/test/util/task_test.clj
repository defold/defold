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
           [java.util.concurrent CountDownLatch StructuredTaskScope$Subtask TimeUnit]))

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

(defn- await! [^CountDownLatch latch]
  (when-not (.await latch 10 TimeUnit/SECONDS)
    (throw (ex-info "Timed out waiting for task" {}))))

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
                         (await! started)
                         (is (.isVirtual (Thread/currentThread)))
                         (is (not (identical? owner (Thread/currentThread))))
                         value))
                     [:first nil false])))))))

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

;; Child bindings are isolated and restore the scope's shared binding snapshot
;; after pop, guarding against sibling or owner bindings being overwritten.
(deftest binding-isolation-test
  (binding [*binding-value* :outer]
    (is (= [[:child :outer] :outer]
           (results
             (task/scope :all-successful
               [(task/fork
                  [(binding [*binding-value* :child]
                     *binding-value*)
                   *binding-value*])
                (task/fork *binding-value*)]))))
    (is (= :outer *binding-value*))))

;; A child failure is propagated unchanged and siblings finish before throwing,
;; guarding against a scope returning while interrupted children are still alive.
(deftest all-successful-failure-test
  (let [exception (ex-info "child failure" {:original true})
        started (CountDownLatch. 1)
        finished (promise)
        outcome
        (try
          (task/scope :all-successful
            (task/fork
              (await! started)
              (throw exception))
            (task/fork
              (.countDown started)
              (try
                (.await (CountDownLatch. 1))
                (finally
                  (deliver finished true)))))
          (catch Throwable exception exception))]
    (is (identical? exception outcome))
    (is (realized? finished))))

;; First completion accepts nil and false results and joins cancelled siblings,
;; guarding against treating a completed falsey result as an unfinished scope.
(deftest first-completed-result-test
  (doseq [value [nil false :result]]
    (let [started (CountDownLatch. 1)
          finished (promise)]
      (is (= value
             (task/scope :first-completed
               (task/fork
                 (await! started)
                 value)
               (task/fork
                 (.countDown started)
                 (try
                   (.await (CountDownLatch. 1))
                   (finally
                     (deliver finished true)))))))
      (is (realized? finished)))))

;; A failed first completion cancels the remaining task immediately rather than
;; waiting for a successful result that may never arrive.
(deftest first-completed-failure-test
  (let [exception (ex-info "first failure" {})
        started (CountDownLatch. 1)
        finished (promise)
        outcome
        (try
          (task/scope :first-completed
            (task/fork
              (await! started)
              (throw exception))
            (task/fork
              (.countDown started)
              (try
                (.await (CountDownLatch. 1))
                (finally
                  (deliver finished true)))))
          (catch Throwable exception exception))]
    (is (identical? exception outcome))
    (is (realized? finished))))

;; An owner failure before join still cancels and joins children and keeps the
;; original exception with the scope's missing-join error suppressed onto it.
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
                (try
                  (.await (CountDownLatch. 1))
                  (finally
                    (deliver finished true))))
              (await! started)
              (throw exception))
            (catch Throwable exception exception))]
      (is (identical? exception outcome))
      (let [suppressed (.getSuppressed ^Throwable outcome)]
        (is (= 1 (count suppressed)))
        (is (instance? IllegalStateException (aget suppressed 0))))
      (is (realized? finished)))))

;; Interrupting the owner releases its join and waits for every child to exit,
;; guarding against cancellation leaving nested transport work running.
(deftest owner-interruption-test
  (doseq [policy [:all-successful :first-completed]]
    (let [started (CountDownLatch. 2)
          release (CountDownLatch. 1)
          finished (CountDownLatch. 2)
          outcome (promise)
          owner
          (.start (Thread/ofVirtual)
                  ^Runnable
                  (fn []
                    (try
                      (task/scope policy
                        (dotimes [_ 2]
                          (task/fork
                            (.countDown started)
                            (try
                              (.await release)
                              (finally
                                (.countDown finished))))))
                      (deliver outcome :completed)
                      (catch Throwable exception
                        (deliver outcome exception)))))]
      (try
        (await! started)
        (.interrupt owner)
        (.join owner 10000)
        (is (not (.isAlive owner)))
        (is (instance? InterruptedException (deref outcome 1000 ::timeout)))
        (is (zero? (.getCount finished)))
        (finally
          (.countDown release)
          (.interrupt owner)
          (.join owner 10000))))))

;; Leaving a nested owner scope restores the outer scope used by later forks.
(deftest nested-scope-test
  (let [inner-result (atom nil)]
    (is (= [:before :after]
           (results
             (task/scope :all-successful
               (let [before (task/fork :before)
                     inner (task/scope :all-successful
                             (task/fork :inner))]
                 (reset! inner-result (.get ^StructuredTaskScope$Subtask inner))
                 [before (task/fork :after)])))))
    (is (= :inner @inner-result))))

;; A child must open its own scope before forking, guarding against an
;; accidentally captured outer scope bypassing the JDK owner-thread check.
(deftest fork-owner-test
  (doseq [policy [:all-successful :first-completed]]
    (is (thrown? WrongThreadException
                 (task/scope policy
                   (task/fork
                     (task/fork :wrong-owner)))))))

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
                 (is false "Invalid policies must not run the body")))))

;; A policy expression is evaluated once before the body and selects the same
;; behavior as a literal policy, guarding against duplicated macro evaluation.
(deftest scope-policy-expression-test
  (doseq [policy [:all-successful :first-completed]]
    (let [evaluations (atom 0)]
      (is (= (case policy :all-successful :owner :first-completed false)
             (task/scope (do
                           (swap! evaluations inc)
                           policy)
               (task/fork false)
               :owner)))
      (is (= 1 @evaluations))))
  (let [evaluations (atom 0)]
    (is (thrown? IllegalArgumentException
                 (task/scope (do
                               (swap! evaluations inc)
                               :unknown)
                   (is false "Invalid policies must not run the body"))))
    (is (= 1 @evaluations))))
