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

(ns editor.tab-history-test
  (:require [clojure.test :refer :all]
            [editor.tab-history :as tab-history]))

(defn- visit-all [items]
  (reduce tab-history/visit tab-history/empty-history items))

;; Verifies that visiting orders items most recently used first and keeps each
;; item once, even when an item is visited again.
(deftest visit-orders-by-most-recent-use
  (is (= [:c :b :a] (:entries (visit-all [:a :b :c]))))
  (is (= [:a :c :b] (:entries (visit-all [:a :b :c :a]))))
  (is (= [:c :b :a] (:entries (visit-all [:a :b :c :c])))))

;; Verifies that back and forward walk the history without reordering it, so
;; repeated back steps reach older items instead of toggling between two.
(deftest back-and-forward-keep-order
  (let [history (visit-all [:a :b :c :d])
        stepped (-> history tab-history/back tab-history/back)]
    (is (= :b (tab-history/current stepped)))
    (is (= (:entries history) (:entries stepped)))
    (is (= :a (tab-history/back-target stepped)))
    (is (= :c (tab-history/forward-target stepped)))
    (is (= :c (tab-history/current (tab-history/forward stepped))))))

;; Verifies that stepping stops at both ends of the history.
(deftest back-and-forward-stop-at-ends
  (let [history (visit-all [:a :b])]
    (is (nil? (tab-history/forward-target history)))
    (is (= history (tab-history/forward history)))
    (let [oldest (tab-history/back history)]
      (is (= :a (tab-history/current oldest)))
      (is (nil? (tab-history/back-target oldest)))
      (is (= oldest (tab-history/back oldest))))))

;; Verifies that selecting the item at the cursor, as the editor does after a
;; back step, leaves the history unchanged.
(deftest visiting-current-item-is-a-no-op
  (let [stepped (tab-history/back (visit-all [:a :b :c]))]
    (is (= stepped (tab-history/visit stepped :b)))))

;; Verifies that visiting another item after stepping back first moves the
;; shown item to the front, since it was the last one the user looked at.
(deftest visit-after-back-commits-shown-item
  (let [stepped (-> (visit-all [:a :b :c :d]) tab-history/back tab-history/back)]
    (is (= [:e :b :d :c :a] (:entries (tab-history/visit stepped :e))))
    (is (= [:d :b :c :a] (:entries (tab-history/visit stepped :d))))))

;; Verifies that the successor of a closing item is the most recently used
;; other item that passes the filter, which is how a closed tab hands focus to
;; the tab used before it in the same tab pane.
(deftest successor-is-most-recent-other-item
  (let [history (visit-all [:a :b :c :d])]
    (is (= :c (tab-history/successor history :d any?)))
    (is (= :d (tab-history/successor history :a any?)))
    (is (= :b (tab-history/successor history :d #{:a :b})))
    (is (nil? (tab-history/successor history :d #{:d})))
    (is (nil? (tab-history/successor tab-history/empty-history :a any?)))))

;; Verifies that closing the item shown after stepping back hands focus to the
;; most recently used other item, not to the next item in the walk.
(deftest successor-after-back-counts-shown-item-as-most-recent
  (let [stepped (-> (visit-all [:a :b :c :d]) tab-history/back tab-history/back)]
    (is (= :d (tab-history/successor stepped :b any?)))
    (is (= :b (tab-history/successor stepped :d any?)))))

;; Verifies that forgetting an item keeps the cursor on the same item, or on
;; the next older one when the item at the cursor is removed.
(deftest forget-keeps-cursor-on-item
  (let [stepped (-> (visit-all [:a :b :c :d]) tab-history/back tab-history/back)]
    (is (= {:entries [:d :b :a] :cursor 1} (tab-history/forget stepped :c)))
    (is (= {:entries [:d :c :b] :cursor 2} (tab-history/forget stepped :a)))
    (is (= {:entries [:d :c :a] :cursor 2} (tab-history/forget stepped :b)))
    (is (= stepped (tab-history/forget stepped :missing))))
  (is (= {:entries [:b] :cursor 0} (tab-history/forget (tab-history/back (visit-all [:a :b])) :a)))
  (is (= tab-history/empty-history (tab-history/forget (visit-all [:a]) :a))))
