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

(ns util.id-vec
  (:require [util.coll :as coll :refer [pair]]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defrecord IdVec [^long next-id entries])

(defn iv-conj
  ^IdVec [^IdVec id-vec value]
  (let [id (.-next-id id-vec)
        entries (.-entries id-vec)
        metadata (meta id-vec)]
    (cond-> (IdVec. (inc id)
                    (conj entries (pair id value)))
            metadata (with-meta metadata))))

(defn iv-into
  ^IdVec [^IdVec id-vec coll]
  (let [first-id (.-next-id id-vec)
        old-entries (.-entries id-vec)
        new-entries (into old-entries
                          (map-indexed (fn [^long index value]
                                         (let [id (+ first-id index)]
                                           (pair id value))))
                          coll)
        next-id (+ first-id (- (count new-entries) (count old-entries)))
        metadata (meta id-vec)]
    (cond-> (IdVec. next-id new-entries)
            metadata (with-meta metadata))))

(defn iv-vec
  ^IdVec [coll]
  (iv-into (IdVec. 1 []) coll))

(defn iv-count
  ^long [^IdVec id-vec]
  (count (.-entries id-vec)))

(defn iv-added-id
  ^long [^IdVec id-vec]
  (key (peek (.-entries id-vec))))

(defn iv-entries [^IdVec id-vec]
  (.-entries id-vec))

(defn iv-ids [^IdVec id-vec]
  (mapv first (.-entries id-vec)))

(defn iv-vals [^IdVec id-vec]
  (mapv second (.-entries id-vec)))

(defn- xform-entries
  ^IdVec [^IdVec id-vec xform]
  (update id-vec :entries
          (fn [entries]
            (into (coll/empty-with-meta entries)
                  xform
                  entries))))

(defn iv-map-vals
  ^IdVec [f ^IdVec id-vec]
  (xform-entries id-vec (map (fn [[id value]]
                               (pair id (f value))))))

(defn iv-filter-ids
  ^IdVec [^IdVec id-vec ids]
  (if-some [kept-ids (some-> ids not-empty set)]
    (xform-entries id-vec (filter (comp kept-ids first)))
    (update id-vec :entries coll/empty-with-meta)))

(defn iv-update-ids
  ^IdVec [f ^IdVec id-vec ids]
  (let [updated-ids (some-> ids not-empty set)]
    (cond-> id-vec
            updated-ids
            (xform-entries
              (map (fn [entry]
                     (let [id (first entry)]
                       (if (updated-ids id)
                         (let [value (second entry)]
                           (pair id (f value)))
                         entry))))))))

(defn iv-remove-ids
  ^IdVec [^IdVec id-vec ids]
  (if-some [removed-ids (some-> ids not-empty set)]
    (xform-entries id-vec (remove (comp removed-ids first)))
    id-vec))
