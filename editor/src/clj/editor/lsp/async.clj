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

(ns editor.lsp.async
  (:require [cljfx.api :as fx]
            [clojure.core.async :as a :refer [<! >!]])
  (:import [clojure.core.async.impl.channels ManyToManyChannel]))

(set! *warn-on-reflection* true)

(defn chan? [x]
  (instance? ManyToManyChannel x))

(defn reduce-async
  "Reduce a channel ch with async function af

  Args:
    af      fn of 2 args (acc and input) that should return a channel; from this
            channel, a single value will be taken as the next acc
    init    initial value for reduction
    ch      channel to reduce over

  Returns a channel with a single result of reduction available after the input
  channel closes"
  [af init ch]
  (a/go-loop [ret init]
    (let [v (<! ch)]
      (if (nil? v)
        ret
        (let [ret' (<! (af ret v))]
          (if (reduced? ret')
            @ret'
            (recur ret')))))))

(defn drain-async
  "Consume all items from the channel and pass them to async function af

  Args:
    af    fn that will receive items from ch for the purpose of side effects,
          must return a channel that will be awaited before taking the next item
    ch    channel to consume

  Returns a channel that will close after all items are consumed"
  [af ch]
  (a/go-loop []
    (when-some [v (<! ch)]
      (<! (af v))
      (recur))))

(defn pipe
  "Take items from the source channel and put them to the sink channel

  Like clojure.core.async/pipe, but:
  - returns a channel that will close when the process of piping completes
  - from and to args are switched"
  [source sink close]
  (a/go-loop []
    (let [v (<! source)]
      (if (nil? v)
        (when close (a/close! sink))
        (when (>! sink v)
          (recur))))))

(defn drain-blocking
  "Consume all items from the channel and pass them to f

  Args:
    f     fn that will receive items from ch for the purpose of side effects,
          can block
    ch    channel to consume

  Returns a channel that will close after all items are consumed"
  [f ch]
  (drain-async #(a/thread (f %)) ch))

(defn supply-blocking
  "Repeatedly calls blocking function f and puts results onto a ch

  Stops calling f and closes ch when f returns nil

  Args:
    f    a blocking function of no args, presumably with side effects

  Returns a channel that will close when ch is closed"
  [f ch]
  (a/go-loop []
    (if-some [v (<! (a/thread (f)))]
      (when (>! ch v)
        (recur))
      (a/close! ch))))

(defmacro with-auto-evaluation-context
  "Like g/with-auto-evaluation-context, but for the background threads"
  [ec & body]
  `(let [~ec (g/make-evaluation-context)
         ret# (g/do-strict-evaluation-context-scope-body ~@body)]
     (fx/on-fx-thread (g/update-cache-from-evaluation-context! ~ec))
     ret#))