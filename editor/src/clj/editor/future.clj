;; Copyright 2020-2024 The Defold Foundation
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

(ns editor.future
  (:refer-clojure :exclude [future])
  (:import [java.util.concurrent CompletableFuture CompletionException]
           [java.util.function Function]))

;; TODO: get rid of this namespace once we move to a JDK that supports virtual threads.

(defn make
  ^CompletableFuture []
  (CompletableFuture.))

(defn completed
  ^CompletableFuture [x]
  (CompletableFuture/completedFuture x))

(defn wrap [x]
  (if (instance? CompletableFuture x)
    x
    (completed x)))

(defn failed
  ^CompletableFuture [ex]
  (CompletableFuture/failedFuture ex))

(defn fail!
  [^CompletableFuture future ex]
  (doto future (.completeExceptionally ex)))

(defn done? [^CompletableFuture future]
  (.isDone future))

(defn complete!
  [^CompletableFuture future x]
  (doto future (.complete x)))

(defn then-async
  [^CompletableFuture future f]
  (let [f (bound-fn* f)]
    (.thenComposeAsync future (reify Function (apply [_ x] (wrap (f x)))))))

(defn then
  [^CompletableFuture future f]
  (let [f (bound-fn* f)]
    (.thenCompose future (reify Function (apply [_ x] (wrap (f x)))))))

(defn catch [^CompletableFuture future f]
  (let [f (bound-fn* f)]
    (.exceptionally
      future
      (reify Function
        (apply [_ ex]
          (if (instance? CompletionException ex)
            (f (.getCause ^CompletionException ex))
            (f ex)))))))