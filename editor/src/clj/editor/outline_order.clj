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

(ns editor.outline-order
  (:require [dynamo.graph :as g]
            [schema.core :as s]
            [util.coll :as coll]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(g/deftype OrderPair [(s/one s/Int "node-id") (s/one s/Int "index")])

(defn next-index
  ^long [parent-node-id parent-node-child-order-pairs-label evaluation-context]
  (->> (g/node-value parent-node-id parent-node-child-order-pairs-label evaluation-context)
       (transduce (map second) max -1)
       (long)
       (inc)))

(defn ordered-node-ids
  [node-ids node-index-label evaluation-context]
  (vec (sort-by #(g/node-value % node-index-label evaluation-context) node-ids)))

(defn reorder-tx-data
  [reordered-node-index-label reordered-node-ids]
  (coll/mapcat-indexed
    #(g/set-property %2 reordered-node-index-label %1)
    reordered-node-ids))

(defn- find-move-neighbour
  [child-node-index parent-node-child-order-pairs offset]
  (let [include-index? (if (= offset -1)
                         (partial > child-node-index)
                         (partial < child-node-index))
        comparator (if (= offset -1)
                     coll/descending-order
                     coll/ascending-order)]
    (->> parent-node-child-order-pairs
         (filterv (comp include-index? second))
         (sort-by second comparator)
         (first))))

(defn can-move?
  [parent-node-id parent-node-child-order-pairs-label child-node-id child-node-index-label offset evaluation-context]
  (let [child-node-index (g/node-value child-node-id child-node-index-label evaluation-context)
        parent-node-child-order-pairs (g/node-value parent-node-id parent-node-child-order-pairs-label evaluation-context)]
    (some? (find-move-neighbour child-node-index parent-node-child-order-pairs offset))))

(defn move!
  [parent-node-id parent-node-child-order-pairs-label child-node-id child-node-index-label offset]
  (let [child-node-index (g/node-value child-node-id child-node-index-label)
        parent-node-child-order-pairs (g/node-value parent-node-id parent-node-child-order-pairs-label)]
    (when-let [[neighbour-node-id neighbour-node-index] (find-move-neighbour child-node-index parent-node-child-order-pairs offset)]
      (g/transact
        (concat
          (g/set-property child-node-id child-node-index-label neighbour-node-index)
          (g/set-property neighbour-node-id child-node-index-label child-node-index))))))
