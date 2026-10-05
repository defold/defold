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

(ns util.task
  (:refer-clojure :exclude [with-open])
  (:import [clojure.lang Var]
           [java.util.concurrent StructuredTaskScope StructuredTaskScope$FailedException StructuredTaskScope$Joiner StructuredTaskScope$Subtask$State]))

(set! *warn-on-reflection* true)

(defmacro with-open
  "Bind resources, evaluate body, and close the resources in reverse order.

  Args:
    bindings    vector of binding names and resource initialization forms;
                each value must be non-null and have a zero-argument close method
    body        forms to evaluate with the resources bound

  Resources are initialized in order, with earlier bindings available to later
  initialization forms. Returns the body's value after closing the resources.

  Unlike clojure.core/with-open, a body or initialization failure remains
  the primary exception if closing also fails. Close failures are attached as
  suppressed exceptions. If only closing fails, the first close failure is
  thrown and later close failures are suppressed."
  [bindings & body]
  (letfn [(expand [bindings]
            (if (zero? (count bindings))
              `(do ~@body)
              (let [[resource init] bindings]
                `(let [~resource ~init
                       result# (try
                                 ~(expand (subvec bindings 2))
                                 (catch Throwable exception#
                                   (try
                                     (.close ~resource)
                                     (catch Throwable suppressed#
                                       (.addSuppressed exception# suppressed#)))
                                   (throw exception#)))]
                   (.close ~resource)
                   result#))))]
    (expand bindings)))

(defmacro scope
  "Evaluate body in a structured task scope and wait for its subtasks to finish.

  The body runs on the calling thread. Use fork to start concurrent subtasks.

  Policies:
    :all-successful   wait for every subtask to succeed and return the body's
                      value; a subtask failure cancels the remaining subtasks
                      and is thrown
    :first-completed  return a completed subtask's value or throw its exception,
                      cancelling the remaining subtasks; requires at least one
                      fork, otherwise throws NoSuchElementException

  If multiple completed subtasks are available, :first-completed selects the
  one forked earliest. The body's value is ignored for this policy.

  Every subtask finishes before scope returns or throws, including when the
  body throws or the calling thread is interrupted. Forked subtasks inherit
  the dynamic bindings captured when the scope opens."
  [policy & body]
  ;; Deliberately capture these reserved locals in fork, including in nested
  ;; macro expansions. They are literal symbols, not auto-gensyms.
  `(let [policy# ~policy
         joiner# (case policy#
                   :all-successful (StructuredTaskScope$Joiner/awaitAllSuccessfulOrThrow)
                   :first-completed (StructuredTaskScope$Joiner/allUntil (constantly true))
                   (throw (IllegalArgumentException. (str "Unknown task scope policy: " policy#))))
         ~'task-binding-frame# (Var/cloneThreadBindingFrame)]
     (with-open [~'task-scope# (StructuredTaskScope/open joiner#)]
       (let [result# (do ~@body)
             joined# (try
                       (.join ~'task-scope#)
                       (catch StructuredTaskScope$FailedException exception#
                         (throw (.getCause exception#))))]
         (if (= :all-successful policy#)
           result#
           (let [^java.util.concurrent.StructuredTaskScope$Subtask subtask#
                 (-> ^java.util.stream.Stream joined#
                     (.filter (fn [^java.util.concurrent.StructuredTaskScope$Subtask subtask#]
                                (not= StructuredTaskScope$Subtask$State/UNAVAILABLE
                                      (.state subtask#))))
                     (.findFirst)
                     (.orElseThrow))]
             (if (= StructuredTaskScope$Subtask$State/SUCCESS (.state subtask#))
               (.get subtask#)
               (throw (.exception subtask#)))))))))

(defmacro fork
  "Evaluate body concurrently in the enclosing scope.

  Starts a virtual thread with the dynamic bindings captured when scope opened.
  Returns a StructuredTaskScope.Subtask. After scope completes, its get method
  reads a successful result.

  Must appear lexically inside scope and execute on the thread that opened it.
  An ordinary helper function cannot fork into its caller's scope."
  [& body]
  (when-not (contains? &env 'task-scope#)
    (throw (IllegalArgumentException. "task/fork must appear inside task/scope")))
  `(let [^java.util.concurrent.StructuredTaskScope scope# ~'task-scope#]
     (.fork scope#
            ^java.util.concurrent.Callable
            (fn []
              (Var/resetThreadBindingFrame ~'task-binding-frame#)
              ~@body))))
