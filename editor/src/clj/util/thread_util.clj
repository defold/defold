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

(ns util.thread-util)

(defn throw-if-interrupted!
  "Checks if the current thread has been interrupted, and if so throws an
  InterruptedException. Calling this function clears the interrupted status
  of the executing thread."
  []
  (if (Thread/interrupted)
    (throw (InterruptedException.))))

(defn abortable-identity!
  "Like core.identity, but throws an InterruptedException if the executing
  thread was interrupted. Suitable for use with map in a transducer or lazy
  sequence transformation."
  [value]
  (throw-if-interrupted!)
  value)

(defn thread-uninterrupted-predicate
  "Returns a predicate function that takes a single value, ignores it, and
  returns false if the specified thread has been interrupted. If no thread is
  supplied, defaults to the calling thread."
  ([]
   (thread-uninterrupted-predicate (Thread/currentThread)))
  ([^Thread thread]
   {:pre [(instance? Thread thread)]}
   (fn thread-uninterrupted? [_]
     (not (.isInterrupted thread)))))

(defn cancel-future!
  "Cancels the supplied future. Does nothing if called with nil or a future that
  was already cancelled. Always returns nil."
  [future]
  (when future
    (future-cancel future)
    nil))

(defn preset!
  "Sets the value of atom to newval without regard for the current value.
  Returns the previous value that was in atom before newval was swapped in."
  [atom newval]
  (let [*prev (volatile! nil)]
    (swap! atom (fn [prev]
                  (vreset! *prev prev)
                  newval))
    (deref *prev)))

(defn swap-rest!
  "Similar to core.swap!, but f is expected to return a sequence. The first
  element is swapped into the atom. Returns the remaining elements."
  [atom f & args]
  (let [*rest (volatile! nil)]
    (swap! atom (fn [val]
                  (let [[newval & rest] (apply f val args)]
                    (vreset! *rest rest)
                    newval)))
    (deref *rest)))
