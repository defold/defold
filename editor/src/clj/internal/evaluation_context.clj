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

(ns internal.evaluation-context
  (:require [util.defonce :as defonce])
  (:import [clojure.lang Associative IFn IHashEq ILookup IMeta IObj IPersistentCollection IPersistentMap MapEntry SeqIterator Seqable Util]
           [java.util Map]))

(set! *warn-on-reflection* true)

;; Derived contexts (for example, :in-production scopes) share materialization
;; state. Their other entries retain ordinary persistent-map semantics.
(defonce/type EvaluationContext [data state metadata]
  ILookup
  (valAt [this key] (.valAt this key nil))
  (valAt [_ key not-found]
    (case key
      :basis (:basis @state)
      :initial-invalidate-counters (let [default (get data key ::not-found)]
                                     (if (= ::not-found default)
                                       not-found
                                       (get @state key default)))
      (get data key not-found)))

  Associative
  (containsKey [_ key] (or (= :basis key) (contains? data key)))
  (entryAt [this key]
    (when (.containsKey this key)
      (MapEntry/create key (.valAt this key))))
  (assoc [_ key value]
    (if (= :basis key)
      (EvaluationContext. data (atom (assoc @state :basis value :changes [])) metadata)
      (EvaluationContext. (assoc data key value) state metadata)))

  IPersistentMap
  (assocEx [this key value]
    (if (.containsKey this key)
      (throw (RuntimeException. "Key already present"))
      (.assoc this key value)))
  (without [_ key]
    (if (= :basis key)
      data
      (EvaluationContext. (dissoc data key) state metadata)))

  IPersistentCollection
  (count [_] (inc (count data)))
  (cons [this value]
    (reduce-kv (fn [result key value] (assoc result key value)) this (conj {} value)))
  (empty [_] {})
  (equiv [this other] (= (into {} this) other))

  Seqable
  (seq [this]
    (seq (cond-> (assoc data :basis (.valAt this :basis))
           (contains? data :initial-invalidate-counters)
           (assoc :initial-invalidate-counters (.valAt this :initial-invalidate-counters)))))

  Iterable
  (iterator [this] (SeqIterator. (.seq this)))

  IFn
  (invoke [this key] (.valAt this key))
  (invoke [this key not-found] (.valAt this key not-found))
  (applyTo [this args] (apply (into {} this) args))

  IMeta
  (meta [_] metadata)

  IObj
  (withMeta [_ value] (EvaluationContext. data state value))

  IHashEq
  (hasheq [this] (Util/hasheq (into {} this)))

  Map
  (size [this] (.count this))
  (isEmpty [_] false)
  (get [this key] (.valAt this key))
  (containsValue [this value] (.containsValue ^Map (into {} this) value))
  (keySet [_] (.keySet ^Map (assoc data :basis nil)))
  (values [this] (.values ^Map (into {} this)))
  (entrySet [this] (.entrySet ^Map (into {} this)))
  (put [_ _ _] (throw (UnsupportedOperationException.)))
  (putAll [_ _] (throw (UnsupportedOperationException.)))
  (remove [_ _] (throw (UnsupportedOperationException.)))
  (clear [_] (throw (UnsupportedOperationException.)))

  Object
  (equals [this other] (.equiv this other))
  (hashCode [this] (.hashCode ^Object (into {} this))))

(defn make-context [data]
  (EvaluationContext. (dissoc data :basis)
                      (atom {:basis (:basis data)
                             :initial-basis (:basis data)
                             :changes []
                             :user-data {}
                             :invalidated #{}})
                      (meta data)))

(defn state [^EvaluationContext evaluation-context]
  (.-state evaluation-context))
