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

(ns editor.tab-history
  "Most-recently-used order of editor tabs, with back and forward navigation.

  A history holds each item once, most recently used first. The cursor marks
  the item shown while the user steps with `back` and `forward`. Stepping keeps
  the order, so repeated steps reach older items. Visiting any other item first
  moves the item at the cursor to the front, then puts the visited item before
  it."
  (:require [util.coll :as coll]))

(set! *warn-on-reflection* true)

(def empty-history
  {:entries []
   :cursor 0})

(defn- commit [{:keys [entries ^long cursor] :as history}]
  (if (zero? cursor)
    history
    (let [current (entries cursor)]
      {:entries (coll/into-> entries [current] (remove #(= current %)))
       :cursor 0})))

(defn current
  "Returns the item at the cursor, or nil if the history is empty."
  [{:keys [entries cursor]}]
  (get entries cursor))

(defn visit
  "Records `item` as the most recently used item."
  [history item]
  (if (= item (current history))
    history
    {:entries (coll/into-> (:entries (commit history)) [item] (remove #(= item %)))
     :cursor 0}))

(defn back-target
  "Returns the item `back` steps to, or nil if there is none."
  [{:keys [entries ^long cursor]}]
  (get entries (inc cursor)))

(defn forward-target
  "Returns the item `forward` steps to, or nil if there is none."
  [{:keys [entries ^long cursor]}]
  (when (pos? cursor)
    (get entries (dec cursor))))

(defn back
  "Moves the cursor to the next older item, keeping the order."
  [history]
  (cond-> history
    (back-target history) (update :cursor inc)))

(defn forward
  "Moves the cursor to the next newer item, keeping the order."
  [history]
  (cond-> history
    (forward-target history) (update :cursor dec)))

(defn successor
  "Returns the most recently used item other than `item` that satisfies `pred`.

  Returns nil if there is none. The item at the cursor counts as the most
  recently used one."
  [history item pred]
  (coll/first-where #(and (not= item %)
                          (pred %))
                    (:entries (commit history))))

(defn forget
  "Removes `item` from the history.

  The cursor stays on the same item when it remains, or moves to the next older
  one when `item` was at the cursor."
  [{:keys [entries ^long cursor] :as history} item]
  (let [index (coll/first-index-where #(= item %) entries)]
    (if-not index
      history
      (let [entries (coll/remove-index entries index)]
        {:entries entries
         :cursor (if (< ^long index cursor)
                   (dec cursor)
                   (min cursor (max 0 (dec (count entries)))))}))))
