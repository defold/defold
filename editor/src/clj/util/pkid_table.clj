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
            [util.defonce :as defonce]))

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

(defn- missing-pkid-count-before
  ^long [missing-pkids ^long pkid]
  (long (count (int-map/range missing-pkids 0 (dec pkid)))))

(defn- realized-index
  ^long [missing-pkids ^long pkid]
  (- pkid (missing-pkid-count-before missing-pkids pkid)))

(defn- pkid-at-index
  ^long [missing-pkids ^long next-pkid ^long index]
  (if (coll/empty? missing-pkids)
    index
    (loop [low index
           high (dec next-pkid)]
      (if (= low high)
        low
        (let [mid (+ low (quot (- high low) 2))
              live-count-through-mid (- (inc mid)
                                        (missing-pkid-count-before missing-pkids (inc mid)))]
          (if (< index live-count-through-mid)
            (recur low mid)
            (recur (inc mid) high)))))))

(defn- insert-val
  [table-vals ^long index val]
  (-> (reduce conj!
              (conj! (reduce conj!
                             (transient [])
                             (subvec table-vals 0 index))
                     val)
              (subvec table-vals index))
      (persistent!)))

(defn- add-missing-pkids
  [missing-pkids ^long first-pkid ^long end-pkid]
  (loop [missing-pkids (transient missing-pkids)
         missing-pkid first-pkid]
    (if (= missing-pkid end-pkid)
      (persistent! missing-pkids)
      (recur (conj! missing-pkids missing-pkid)
             (inc missing-pkid)))))

(defn- remove-missing-pkid
  [missing-pkids ^long pkid]
  (if (= 1 (count missing-pkids))
    empty-missing-pkids
    (disj missing-pkids pkid)))

(defn- assoc-pkid
  ^PkidTable [^PkidTable table ^long pkid val]
  (let [table-vals (vals table)
        table-missing-pkids (missing-pkids table)
        table-next-pkid (next-pkid table)]
    (cond
      (= pkid table-next-pkid)
      (PkidTable. (conj table-vals val)
                  table-missing-pkids
                  (inc table-next-pkid))

      (< pkid table-next-pkid)
      (let [index (realized-index table-missing-pkids pkid)]
        (if (contains? table-missing-pkids pkid)
          (PkidTable. (insert-val table-vals index val)
                      (remove-missing-pkid table-missing-pkids pkid)
                      table-next-pkid)
          (PkidTable. (assoc table-vals index val)
                      table-missing-pkids
                      table-next-pkid)))

      :else
      (PkidTable. (conj table-vals val)
                  (add-missing-pkids table-missing-pkids table-next-pkid pkid)
                  (inc pkid)))))

(defn append
  "Appends a value at the next pkid."
  ^PkidTable [^PkidTable table val]
  (PkidTable. (conj (vals table) val)
              (missing-pkids table)
              (inc (next-pkid table))))

(defn assoc-pkids
  "Associates a value with every supplied pkid."
  ^PkidTable [^PkidTable table pkids val]
  {:pre [(coll/every? nat-int? pkids)]}
  (reduce (fn [table pkid]
            (assoc-pkid table pkid val))
          table
          pkids))

(defn- dissoc-pkid
  ^PkidTable [^PkidTable table ^long pkid]
  (let [table-missing-pkids (missing-pkids table)
        table-next-pkid (next-pkid table)]
    (if (or (<= table-next-pkid pkid)
            (contains? table-missing-pkids pkid))
      table
      (PkidTable. (coll/remove-index (vals table)
                                     (realized-index table-missing-pkids pkid))
                  (conj table-missing-pkids pkid)
                  table-next-pkid))))

(defn dissoc-pkids
  "Dissociates the values at the supplied pkids."
  ^PkidTable [^PkidTable table pkids]
  {:pre [(coll/every? nat-int? pkids)]}
  (reduce dissoc-pkid table pkids))

(defn find-pkids
  "Returns the pkids whose values equal the supplied value."
  [^PkidTable table val]
  (let [table-vals (vals table)
        table-missing-pkids (missing-pkids table)
        table-next-pkid (next-pkid table)]
    (coll/into-> table-vals (int-map/int-set)
      (keep-indexed (fn [^long index table-val]
                      (when (= val table-val)
                        (pkid-at-index table-missing-pkids table-next-pkid index)))))))
