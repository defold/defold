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

(ns editor.async
  (:require [cljfx.api :as fx]
            [dynamo.graph :as g]))

(set! *warn-on-reflection* true)

(defmacro with-auto-evaluation-context
  "Evaluates the body on the calling thread and queues its materializations and
  cache entries for merging on the UI thread. Returns the body's result without
  waiting for the merge. The evaluation-context must not escape the body.
  Pass it explicitly to graph reads and helpers; calls without an
  evaluation-context create their own evaluation-context and commit on the
  calling thread."
  [evaluation-context & body]
  `(let [~evaluation-context (g/make-evaluation-context)
         result# (g/do-strict-evaluation-context-scope-body ~@body)]
     (fx/on-fx-thread (g/update-system-from-evaluation-context! ~evaluation-context))
     result#))

(defmacro let-ec
  "Like g/let-ec, but queues the evaluation-context merge on the UI thread.
  Bindings are evaluated on the calling thread. The implicit evaluation-context
  is only available in the bindings; the body runs after the merge is queued."
  [bindings & body]
  {:pre [(vector? bindings)
         (even? (count bindings))]}
  (let [private-bindings (into []
                               (comp (partition-all 2)
                                     (mapcat destructure))
                               bindings)
        public-symbols (into []
                             (comp (take-nth 2)
                                   (distinct))
                             private-bindings)]
    `(let [~public-symbols
           (with-auto-evaluation-context ~'evaluation-context
             (let* ~private-bindings
               ~public-symbols))]
       ~@body)))
