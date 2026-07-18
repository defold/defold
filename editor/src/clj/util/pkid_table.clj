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

(ns util.pkid-table
  (:refer-clojure :exclude [vals])
  (:require [clojure.data.int-map :as int-map]
            [util.coll :as coll]
            [util.defonce :as defonce])
  (:import [clojure.data.int_map PersistentIntSet]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defonce/type PkidTable [vals missing-pkids ^long next-pkid])

(def ^:private empty-missing-pkids
  (int-map/dense-int-set))

(def ^:private empty-pkid-table
  (PkidTable. [] empty-missing-pkids 0))

(defn pkid-table
  "Returns an empty pkid table."
  ^PkidTable []
  empty-pkid-table)

(definline vals
  "Returns the table values in ascending pkid order."
  [^PkidTable table]
  `(.-vals ~(with-meta table {:tag `PkidTable})))

(definline next-pkid
  "Returns the pkid that will be assigned by the next append."
  [^PkidTable table]
  `(.-next-pkid ~(with-meta table {:tag `PkidTable})))

(definline ^:private missing-pkids [^PkidTable table]
  `(.-missing-pkids ~(with-meta table {:tag `PkidTable})))

(defn- missing-pkid-count-in-range
  "Counts missing pkids in the half-open interval. Sorted batch operations use
  disjoint intervals so previously counted prefixes are not counted again."
  ^long [missing-pkids ^long start-pkid ^long end-pkid]
  (if (= start-pkid end-pkid)
    0
    (long (count (int-map/range missing-pkids start-pkid (dec end-pkid))))))

(defn- missing-pkid-count-before
  ^long [missing-pkids ^long pkid]
  (missing-pkid-count-in-range missing-pkids 0 pkid))

(defn- pkid-at-index
  ^long [missing-pkids ^long next-pkid ^long index]
  (loop [low index
         high (dec next-pkid)]
    (if (= low high)
      low
      (let [mid (+ low (quot (- high low) 2))
            live-count-through-mid (- (inc mid)
                                      (missing-pkid-count-before missing-pkids (inc mid)))]
        (if (< index live-count-through-mid)
          (recur low mid)
          (recur (inc mid) high))))))

(defn- conj-pkid-range!
  [missing-pkids ^long first-pkid ^long end-pkid]
  (loop [missing-pkids missing-pkids
         missing-pkid first-pkid]
    (if (= missing-pkid end-pkid)
      missing-pkids
      (recur (conj! missing-pkids missing-pkid)
             (inc missing-pkid)))))

(defn- conj-vals!
  [result table-vals ^long start-index ^long end-index]
  (loop [result result
         index start-index]
    (if (= index end-index)
      result
      (recur (conj! result (nth table-vals index))
             (inc index)))))

(defn- persistent-missing-pkids
  [missing-pkids]
  (let [missing-pkids (persistent! missing-pkids)]
    (if (zero? (count missing-pkids))
      empty-missing-pkids
      missing-pkids)))

(defn- normalize-assoc-pkids
  [pkids table-missing-pkids ^long table-next-pkid]
  ;; Sorting lets the update paths merge with the compact value vector once.
  ;; Deduplication is safe because every supplied pkid receives the same value.
  (let [restores-missing (volatile! false)
        int-set-input (instance? PersistentIntSet pkids)
        normalized-pkids-result
        (reduce (fn [normalized-pkids pkid]
                  (assert (nat-int? pkid))
                  (let [pkid (long pkid)]
                    (when (and (not @restores-missing)
                               (< pkid table-next-pkid)
                               (contains? table-missing-pkids pkid))
                      (vreset! restores-missing true))
                    (if int-set-input
                      normalized-pkids
                      (conj! normalized-pkids pkid))))
                (when-not int-set-input
                  (transient (int-map/int-set)))
                pkids)
        normalized-pkids (if int-set-input
                           pkids
                           (persistent! normalized-pkids-result))]
    [normalized-pkids @restores-missing]))

(defn- assoc-pkids-without-restoration
  "Updates existing positions in a transient copy and appends new positions.
  This avoids copying all existing values for replacement-only batches."
  ^PkidTable [^PkidTable table normalized-pkids val]
  (let [table-vals (vals table)
        table-missing-pkids (missing-pkids table)
        table-next-pkid (next-pkid table)
        new-missing-pkids (volatile! nil)
        new-next-pkid (volatile! table-next-pkid)
        missing-count-before (volatile! 0)
        range-start-pkid (volatile! 0)
        new-table-vals
        (reduce (fn [new-table-vals ^long pkid]
                  (if (< pkid table-next-pkid)
                    (let [missing-count-before-pkid
                          (+ (long @missing-count-before)
                             (missing-pkid-count-in-range table-missing-pkids
                                                          (long @range-start-pkid)
                                                          pkid))
                          index (- pkid missing-count-before-pkid)]
                      (vreset! missing-count-before missing-count-before-pkid)
                      (vreset! range-start-pkid pkid)
                      (assoc! new-table-vals index val))
                    (let [current-next-pkid (long @new-next-pkid)]
                      (when (< current-next-pkid pkid)
                        (vreset! new-missing-pkids
                                 (conj-pkid-range! (or @new-missing-pkids
                                                       (transient table-missing-pkids))
                                                   current-next-pkid
                                                   pkid)))
                      (vreset! new-next-pkid (inc pkid))
                      (conj! new-table-vals val))))
                (transient table-vals)
                normalized-pkids)]
    (PkidTable. (persistent! new-table-vals)
                (if-let [new-missing-pkids @new-missing-pkids]
                  (persistent-missing-pkids new-missing-pkids)
                  table-missing-pkids)
                @new-next-pkid)))

(defn- assoc-pkids-with-restoration
  "Merges sorted updates with the existing values, copying each old value at
  most once."
  ^PkidTable [^PkidTable table normalized-pkids val]
  (let [table-vals (vals table)
        table-val-count (count table-vals)
        table-missing-pkids (missing-pkids table)
        table-next-pkid (next-pkid table)
        new-missing-pkids (volatile! (transient table-missing-pkids))
        new-next-pkid (volatile! table-next-pkid)
        source-index (volatile! 0)
        missing-count-before (volatile! 0)
        range-start-pkid (volatile! 0)
        new-table-vals
        (reduce (fn [new-table-vals ^long pkid]
                  (if (< pkid table-next-pkid)
                    (let [missing-count-before-pkid
                          (+ (long @missing-count-before)
                             (missing-pkid-count-in-range table-missing-pkids
                                                          (long @range-start-pkid)
                                                          pkid))
                          index (- pkid missing-count-before-pkid)
                          new-table-vals (conj-vals! new-table-vals
                                                     table-vals
                                                     (long @source-index)
                                                     index)]
                      (vreset! missing-count-before missing-count-before-pkid)
                      (vreset! range-start-pkid pkid)
                      (vreset! new-missing-pkids (disj! @new-missing-pkids pkid))
                      (vreset! source-index
                               (if (contains? table-missing-pkids pkid)
                                 index
                                 (inc index)))
                      (conj! new-table-vals val))
                    (let [current-next-pkid (long @new-next-pkid)
                          new-table-vals (conj-vals! new-table-vals
                                                     table-vals
                                                     (long @source-index)
                                                     table-val-count)]
                      (vreset! source-index table-val-count)
                      (vreset! new-missing-pkids
                               (conj-pkid-range! @new-missing-pkids
                                                 current-next-pkid
                                                 pkid))
                      (vreset! new-next-pkid (inc pkid))
                      (conj! new-table-vals val))))
                (transient [])
                normalized-pkids)]
    (PkidTable. (-> new-table-vals
                    (conj-vals! table-vals (long @source-index) table-val-count)
                    (persistent!))
                (persistent-missing-pkids @new-missing-pkids)
                @new-next-pkid)))

(defn append
  "Appends a value at the next pkid."
  ^PkidTable [^PkidTable table val]
  (PkidTable. (conj (vals table) val)
              (missing-pkids table)
              (inc (next-pkid table))))

(defn assoc-pkids
  "Associates a value with every supplied pkid."
  ^PkidTable [^PkidTable table pkids val]
  (let [table-missing-pkids (missing-pkids table)
        table-next-pkid (next-pkid table)
        [normalized-pkids restores-missing]
        (normalize-assoc-pkids pkids table-missing-pkids table-next-pkid)]
    (cond
      (coll/empty? normalized-pkids)
      table

      restores-missing
      (assoc-pkids-with-restoration table normalized-pkids val)

      :else
      (assoc-pkids-without-restoration table normalized-pkids val))))

(defn- normalize-dissoc-pkids
  [pkids table-missing-pkids ^long table-next-pkid]
  (let [int-set-input (instance? PersistentIntSet pkids)]
    (-> (reduce (fn [normalized-pkids pkid]
                  (assert (nat-int? pkid))
                  (let [pkid (long pkid)
                        ineffective-pkid (or (<= table-next-pkid pkid)
                                             (contains? table-missing-pkids pkid))]
                    (cond
                      ineffective-pkid
                      (if int-set-input
                        (disj! normalized-pkids pkid)
                        normalized-pkids)

                      int-set-input
                      normalized-pkids

                      :else
                      (conj! normalized-pkids pkid))))
                (transient (if int-set-input
                             pkids
                             (int-map/int-set)))
                pkids)
        (persistent!))))

(defn dissoc-pkids
  "Dissociates the values at the supplied pkids."
  ^PkidTable [^PkidTable table pkids]
  (let [table-vals (vals table)
        table-missing-pkids (missing-pkids table)
        table-next-pkid (next-pkid table)
        normalized-pkids (normalize-dissoc-pkids pkids
                                                 table-missing-pkids
                                                 table-next-pkid)]
    (if (coll/empty? normalized-pkids)
      table
      (let [source-index (volatile! 0)
            missing-count-before (volatile! 0)
            range-start-pkid (volatile! 0)
            new-missing-pkids (volatile! (transient table-missing-pkids))
            new-table-vals
            (reduce (fn [new-table-vals ^long pkid]
                      (let [missing-count-before-pkid
                            (+ (long @missing-count-before)
                               (missing-pkid-count-in-range table-missing-pkids
                                                            (long @range-start-pkid)
                                                            pkid))
                            index (- pkid missing-count-before-pkid)
                            new-table-vals (conj-vals! new-table-vals
                                                       table-vals
                                                       (long @source-index)
                                                       index)]
                        (vreset! missing-count-before missing-count-before-pkid)
                        (vreset! range-start-pkid pkid)
                        (vreset! source-index (inc index))
                        (vreset! new-missing-pkids (conj! @new-missing-pkids pkid))
                        new-table-vals))
                    (transient [])
                    normalized-pkids)]
        (PkidTable. (-> new-table-vals
                        (conj-vals! table-vals
                                    (long @source-index)
                                    (count table-vals))
                        (persistent!))
                    (persistent! @new-missing-pkids)
                    table-next-pkid)))))

(defn- binary-search-height
  ^long [^long item-count]
  (loop [item-count item-count
         height 0]
    (if (<= item-count 1)
      height
      (recur (- item-count (quot item-count 2))
             (inc height)))))

(defn- matching-indexes->pkids-by-merging
  [matching-indexes table-missing-pkids]
  (let [matching-index-count (count matching-indexes)
        matching-index-position (volatile! 0)
        missing-pkid-count (volatile! 0)
        result
        (reduce (fn [result ^long missing-pkid]
                  (let [missing-pkid-count-value (long @missing-pkid-count)]
                    (loop [result result
                           matching-index-position-value (long @matching-index-position)]
                      (if (= matching-index-position-value matching-index-count)
                        (do
                          (vreset! matching-index-position matching-index-position-value)
                          (reduced result))
                        (let [pkid (+ (long (nth matching-indexes
                                                 matching-index-position-value))
                                      missing-pkid-count-value)]
                          (if (< pkid missing-pkid)
                            (recur (conj! result pkid)
                                   (inc matching-index-position-value))
                            (do
                              (vreset! matching-index-position
                                       matching-index-position-value)
                              (vreset! missing-pkid-count
                                       (inc missing-pkid-count-value))
                              result)))))))
                (transient (int-map/int-set))
                table-missing-pkids)
        missing-pkid-count-value (long @missing-pkid-count)]
    (loop [result result
           matching-index-position-value (long @matching-index-position)]
      (if (= matching-index-position-value matching-index-count)
        (persistent! result)
        (recur (conj! result
                      (+ (long (nth matching-indexes matching-index-position-value))
                         missing-pkid-count-value))
               (inc matching-index-position-value))))))

(defn- matching-indexes->pkids-by-searching
  [matching-indexes table-missing-pkids ^long table-next-pkid]
  (-> (reduce (fn [result ^long matching-index]
                (conj! result
                       (pkid-at-index table-missing-pkids
                                      table-next-pkid
                                      matching-index)))
              (transient (int-map/int-set))
              matching-indexes)
      (persistent!)))

(defn find-pkids
  "Returns the pkids whose values equal the supplied value."
  [^PkidTable table val]
  (let [table-vals (vals table)
        table-missing-pkids (missing-pkids table)
        table-next-pkid (next-pkid table)]
    (if (coll/empty? table-missing-pkids)
      (coll/into-> table-vals (int-map/int-set)
        (keep-indexed (fn [^long index table-val]
                        (when (= val table-val)
                          index))))
      (let [matching-indexes
            (coll/into-> table-vals []
              (keep-indexed (fn [^long index table-val]
                              (when (= val table-val)
                                index))))
            matching-index-count (count matching-indexes)]
        (cond
          (zero? matching-index-count)
          (int-map/int-set)

          ;; Merge all results with the holes when that takes less work than an
          ;; independent binary rank search for every matching value.
          (<= (count table-missing-pkids)
              (* matching-index-count (binary-search-height table-next-pkid)))
          (matching-indexes->pkids-by-merging matching-indexes table-missing-pkids)

          :else
          (matching-indexes->pkids-by-searching matching-indexes
                                                table-missing-pkids
                                                table-next-pkid))))))
