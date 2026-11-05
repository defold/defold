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

(ns internal.invalidate-counters
  (:require [internal.graph.types :as gt])
  (:import [com.defold.util InvalidationCounters InvalidationCounters$Editor]
           [java.util.function Function ToLongFunction]))

(set! *warn-on-reflection* true)

(defn counters? [value]
  (instance? InvalidationCounters value))

(defn make []
  InvalidationCounters/EMPTY)

(def ^:private endpoint-node-id
  (reify ToLongFunction
    (applyAsLong [_ endpoint]
      (gt/endpoint-node-id endpoint))))

(def ^:private endpoint-label
  (reify Function
    (apply [_ endpoint]
      (gt/endpoint-label endpoint))))

(defn edit
  [^InvalidationCounters counters]
  (.edit counters))

(defn commit!
  [^InvalidationCounters$Editor editor]
  (.commit editor))

(defn add-nodes!
  [^InvalidationCounters$Editor editor node-ids ^long lifetime]
  (.addNodes editor node-ids lifetime))

(defn remove-nodes!
  [^InvalidationCounters$Editor editor node-ids]
  (.removeNodes editor node-ids))

(defn bump!
  [^InvalidationCounters$Editor editor endpoints]
  (.bump editor endpoints endpoint-node-id endpoint-label))

(defn full-invalidate!
  [^InvalidationCounters$Editor editor]
  (.fullInvalidate editor))

(defn add-nodes
  [^InvalidationCounters counters node-ids ^long lifetime]
  (.addNodes counters node-ids lifetime))

(defn remove-nodes
  [^InvalidationCounters counters node-ids]
  (.removeNodes counters node-ids))

(defn bump
  [^InvalidationCounters counters endpoints]
  (.bump counters endpoints endpoint-node-id endpoint-label))

(defn counter
  [^InvalidationCounters counters endpoint]
  (.counter counters (gt/endpoint-node-id endpoint) (gt/endpoint-label endpoint)))

(defn full-invalidate
  [^InvalidationCounters counters]
  (.fullInvalidate counters))

(defn full-invalidation-since?
  [^InvalidationCounters snapshot ^InvalidationCounters current]
  (.fullInvalidatedSince current snapshot))

(defn endpoint-invalidated-since?
  [endpoint ^InvalidationCounters snapshot ^InvalidationCounters current]
  (.changedSince current snapshot (gt/endpoint-node-id endpoint) (gt/endpoint-label endpoint)))
