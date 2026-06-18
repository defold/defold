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

(ns internal.paper-tape)

(set! *warn-on-reflection* true)

(defprotocol Iterator
  (inext  [this] "Advance cursor to the next-most-recent item")
  (iprev  [this] "Step cursor backward to the previous-to-current item")
  (ivalue [this] "Return the current item")
  (before [this] "Return a seq of the items up and including the current value")
  (after  [this] "Return a seq of the items after the current value"))

(defprotocol Truncate
  (truncate [this] "Drop all values after the current one"))

(defprotocol Drop
  (drop-current [this] "Drops the current value"))

(deftype PaperTape [limit limiter on-drop left right]
  clojure.lang.IPersistentCollection
  (seq [_this]     (concat left right))
  (count [_this]   (+ (count left) (count right)))
  (cons  [_this o] (PaperTape. limit limiter on-drop (limiter (conj left o)) []))
  (empty [_this]   (PaperTape. limit limiter on-drop [] []))
  (equiv [_this o] (and (instance? PaperTape o)
                        (let [^PaperTape that o]
                          (and (= limit (.limit that))
                               (= left (.left that))
                               (= right (.right that))))))

  Iterator
  ;; Move one from right to left
  (inext [this]
    (if-let [v (peek right)]
      (PaperTape. limit limiter on-drop (conj left v) (pop right))
      this))

  ;; Move one from left to right
  (iprev [this]
    (if-let [v (peek left)]
      (PaperTape. limit limiter on-drop (pop left) (conj right v))
      this))

  (ivalue [_this] (peek left))
  (before [_this] left)
  (after  [_this] right)

  Truncate
  (truncate [_this]
    (PaperTape. limit limiter on-drop left []))

  Drop
  (drop-current [_this]
    (PaperTape. limit limiter on-drop (pop left) right)))

(defn- make-limiter
  [limit]
  (if-not limit
    identity
    (fn [v]
      (if (> (count v) limit)
        (subvec v (- (count v) limit))
        v))))

(defn paper-tape
  ([limit]
   (paper-tape limit (fn [_v])))
  ([limit on-drop]
   (PaperTape. limit (make-limiter limit) on-drop [] [])))
