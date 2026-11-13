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

(ns util.successors-test
  (:require [clojure.test :refer [deftest is]]
            [clojure.test.check.clojure-test :refer [defspec]]
            [clojure.test.check.generators :as gen]
            [clojure.test.check.properties :as prop]
            [internal.graph.types :as gt]
            [util.coll :as coll])
  (:import [clojure.lang Keyword Murmur3 Util]
           [com.defold.util Endpoint Successors]
           [java.util.concurrent Callable CountDownLatch ExecutorService Executors Future]))

(set! *warn-on-reflection* true)

(defn- ensure! [condition data]
  (when-not condition
    (throw (ex-info "Successor cache differs from the model." data))))

(def ^:private node-id-gen
  (gen/frequency [[9 gen/large-integer]
                  [1 (gen/elements [Long/MIN_VALUE Long/MAX_VALUE])]]))

(def ^:private label-gen
  (gen/one-of [gen/keyword gen/keyword-ns]))

(def ^:private node-ids-gen
  (gen/one-of
    [(gen/vector-distinct node-id-gen {:min-elements 1})
     (gen/let [origin node-id-gen
               offsets (gen/vector-distinct gen/nat {:min-elements 1})]
       (mapv #(unchecked-add (long origin) (long %)) offsets))
     (gen/let [origin node-id-gen]
       (let [page-start (bit-and (long origin) -64)]
         (mapv #(unchecked-add page-start (long %)) [0 1 64 65])))]))

(defn- subset-gen [values]
  (gen/fmap
    (fn select-subset [selected]
      (coll/into-> values []
        (keep-indexed (fn [index value] (when (selected index) value)))))
    (gen/vector gen/boolean (count values))))

(def ^:private history-gen
  (gen/let [ids node-ids-gen
            initial (subset-gen ids)
            labels (gen/vector-distinct label-gen {:min-elements 1})
            edits (let [label (gen/elements labels)
                        nodes (subset-gen ids)]
                    (gen/vector
                      (gen/one-of
                        [(gen/tuple (gen/return :put) gen/nat label (gen/vector label))
                         (gen/tuple (gen/elements [:get :toggle])
                                    gen/nat label (gen/return nil))
                         (gen/tuple (gen/elements [:invalidate-label :invalidate-node])
                                    gen/nat label nodes)
                         (gen/tuple (gen/return :reset) gen/nat label (gen/tuple nodes nodes))])))]
    {:ids ids :initial initial :edits edits}))

(defn- choose-key [op selector label ids nodes rows]
  (if (and (= :invalidate-label op) (coll/not-empty rows))
    (nth (coll/into-> (coll/keys rows) []) (mod selector (count rows)))
    (let [live-ids (when (#{:put :invalidate-node} op) (filterv nodes ids))
          candidates (or (coll/not-empty live-ids) ids)]
      [(candidates (mod selector (count candidates))) label])))

(defn- drop-node-rows [rows node-ids]
  (coll/into-> rows {}
    (remove (fn [[[source _]]] (contains? node-ids source)))))

(defn- check-history! [{:keys [ids initial edits]}]
  (loop [^Successors cache (Successors.)
         nodes (reduce #(assoc %1 %2 true) {} initial)
         rows {}
         index 0]
    (if (= index (count edits))
      true
      (let [[op selector output data :as edit] (edits index)
            [id label :as key] (choose-key op selector output ids nodes rows)
            live (contains? nodes id)
            [added deleted] (when (= :reset op) data)
            deleted (if (and (= :toggle op) live) [id] deleted)
            invalidated-ids (case op
                              (:invalidate-label :invalidate-node) (into #{id} data)
                              :toggle #{id}
                              :reset (set ids)
                              nil)
            before-endpoints (coll/into-> ids {}
                               (comp (filter nodes)
                                     (map (fn [source] [source (.endpoint cache source label)]))))
            [^Successors next-cache next-nodes next-rows]
            (case op
              :get [cache nodes rows]
              :put (if-not live
                     [cache nodes rows]
                     (let [row (into-array Endpoint (mapv #(.endpoint cache id %) data))
                           previous (get rows key)]
                       (ensure! (identical? previous (.putIfAbsent cache id label row))
                                {:edit edit :key key :reason :wrong-previous-row})
                       [cache nodes (assoc rows key (or previous row))]))
              (:invalidate-label :invalidate-node)
              (let [labels (when (= :invalidate-label op) #{label})
                    changes (coll/into-> invalidated-ids {}
                              (map (fn [source] [source labels])))
                    next-rows (if-not labels
                                (drop-node-rows rows invalidated-ids)
                                (reduce (fn [rows source] (dissoc rows [source label]))
                                        rows
                                        invalidated-ids))]
                [(.invalidate cache changes nodes) nodes next-rows])
              :toggle (let [next-nodes (if live (dissoc nodes id) (assoc nodes id true))]
                        [(.invalidate cache {id nil} next-nodes) next-nodes (drop-node-rows rows invalidated-ids)])
              :reset [(Successors.) (reduce #(assoc %1 %2 true) (reduce dissoc nodes deleted) added) {}])]
        (when invalidated-ids
          (ensure! (not (identical? cache next-cache))
                   {:edit edit :key key :reason :cache-reused})
          (let [deleted-ids (set deleted)]
            (doseq [source ids
                    :when (contains? next-nodes source)]
              (let [endpoint (.endpoint next-cache source label)]
                (ensure! (= (gt/endpoint source label) endpoint)
                         {:edit edit :source source :reason :wrong-endpoint})
                (ensure! (identical? endpoint (.endpoint next-cache source label))
                         {:edit edit :source source :reason :endpoint-not-interned})))
            (doseq [[source endpoint] before-endpoints]
              (ensure! (= (and (not= :reset op) (not (contains? deleted-ids source)))
                          (identical? endpoint (.endpoint next-cache source label)))
                       {:edit edit :source source :reason :wrong-endpoint-lifetime})
              (ensure! (identical? endpoint (.endpoint cache source label))
                       {:edit edit :source source :reason :old-endpoint-changed})))
          (doseq [[[source output] row] rows]
            (ensure! (identical? row (.get cache source output))
                     {:edit edit :source source :output output :reason :old-row-changed})
            (ensure! (identical? (get next-rows [source output]) (.get next-cache source output))
                     {:edit edit :source source :output output :reason :wrong-cached-row}))
          (doseq [source invalidated-ids
                  :when (and (contains? nodes source) (not (contains? next-rows [source label])))]
            (.putIfAbsent cache source label (into-array Endpoint []))
            (ensure! (nil? (.get next-cache source label))
                     {:edit edit :source source :output label :reason :late-write-shared})))
        (ensure! (identical? (get next-rows key) (.get next-cache id label))
                 {:edit edit :key key :reason :wrong-cached-row})
        (recur next-cache next-nodes next-rows (inc index))))))

;; Checks cached rows and endpoint identity across generated snapshots,
;; including batches of invalidations within a page and across pages.
(defspec generated-successor-histories 180
  (prop/for-all [history history-gen]
    (check-history! history)))

;; Checks that late writes cannot fill invalidated rows across snapshots,
;; including nodes and pages that had no cached rows at invalidation time.
(deftest invalidate-uncached-successor-row-test
  (let [previous (Successors.)
        changes {1 #{:dirty} 2 #{:dirty} 64 #{:dirty} 128 #{:dirty}}
        nodes {1 true 2 true 64 true 128 true}
        populated-row (into-array Endpoint [(gt/endpoint 1 :successor)])
        empty-row (into-array Endpoint [])]
    (.putIfAbsent previous 1 :clean populated-row)
    (.endpoint previous 64 :dirty)
    (let [current (.invalidate previous changes nodes)]
      (is (identical? populated-row (.get current 1 :clean)))
      (doseq [[node-id _] changes]
        (.putIfAbsent previous node-id :dirty populated-row)
        (is (nil? (.get current node-id :dirty)) (str node-id))

        (.putIfAbsent current node-id :dirty empty-row)
        (is (identical? populated-row (.get previous node-id :dirty)) (str node-id))
        (is (identical? empty-row (.get current node-id :dirty)) (str node-id))))))

;; Checks that one deletion batch clears rows and interned endpoints across
;; block and page boundaries while preserving the old snapshot and nearby nodes.
(deftest delete-nodes-across-successor-storage-boundaries-test
  (let [previous (Successors.)
        changes {15 nil 16 nil 63 nil 64 nil}
        nodes {14 true 17 true 62 true 65 true}
        endpoints (coll/into-> (coll/keys (merge changes nodes)) {}
                    (map (fn [node-id]
                           (let [endpoint (.endpoint previous node-id :output)]
                             (.putIfAbsent previous node-id :output (into-array Endpoint [endpoint]))
                             [node-id endpoint]))))
        current (.invalidate previous changes nodes)]
    (doseq [[node-id endpoint] endpoints]
      (let [current-endpoint (.endpoint current node-id :output)
            previous-row (.get previous node-id :output)
            current-row (.get current node-id :output)]
        (is (= endpoint current-endpoint) (str node-id))
        (is (identical? endpoint (.endpoint previous node-id :output)) (str node-id))
        (is (= [endpoint] (vec previous-row)) (str node-id))
        (if (contains? nodes node-id)
          (do
            (is (identical? endpoint current-endpoint) (str node-id))
            (is (identical? previous-row current-row) (str node-id)))
          (do
            (is (not (identical? endpoint current-endpoint)) (str node-id))
            (is (nil? current-row) (str node-id))))))))

(defn- check-concurrent-publication! [^ExecutorService pool node-id labels]
  (let [previous (Successors.)
        retained-label (peek labels)
        labels (pop labels)
        fork (delay (.invalidate previous {node-id (set labels)} {node-id true}))
        reversed-labels (into [] (rseq labels))
        empty-row (into-array Endpoint [])
        retained-row (into-array Endpoint [(.endpoint previous node-id retained-label)])]
    (.putIfAbsent previous node-id retained-label retained-row)
    (let [label (labels 0)]
      (.putIfAbsent previous node-id label (into-array Endpoint [(.endpoint previous node-id label)])))
    (let [gate (CountDownLatch. 1)
          jobs (mapv (fn [worker]
                       (.submit pool
                                ^Callable
                                (fn publish-rows []
                                  (.await gate)
                                  (let [^Successors cache (if (even? worker) previous @fork)
                                        endpoints (mapv #(.endpoint cache node-id %) labels)
                                        by-label (zipmap labels endpoints)
                                        write-order (if (zero? (bit-and worker 2)) labels reversed-labels)
                                        rows (coll/into-> write-order {}
                                               (map (fn [label]
                                                      (let [row (if (even? worker)
                                                                  (into-array Endpoint [(by-label label)])
                                                                  empty-row)]
                                                        [label (or (.putIfAbsent cache node-id label row) row)]))))]
                                    [cache endpoints rows]))))
                     (range 6))
          results (try
                    (.countDown gate)
                    (mapv deref jobs)
                    (finally
                      (doseq [^Future job jobs]
                        (.cancel job true))))
          ^Successors current @fork
          endpoints (second (results 0))]
      (and (identical? retained-row (.get current node-id retained-label))
           (coll/every? (fn [[^Successors cache candidate-endpoints rows]]
                          (and (coll/every? true? (mapv identical? endpoints candidate-endpoints))
                               (coll/every? (fn [[label row]] (identical? row (.get cache node-id label))) rows)))
                        results)
           (coll/every? (fn [index]
                          (let [label (labels index)
                                previous-row (.get previous node-id label)
                                current-row (.get current node-id label)]
                            (and previous-row
                                 current-row
                                 (= 1 (alength previous-row))
                                 (identical? (endpoints index) (aget previous-row 0))
                                 (zero? (alength current-row)))))
                        (range (count labels)))))))

;; Checks that a fork preserves unchanged rows and shares endpoints while
;; workers publish dirty rows on both bases.
(defspec generated-concurrent-successor-publication 60
  (prop/for-all [node-id node-id-gen
                 labels (gen/vector-distinct label-gen {:min-elements 3})]
    (let [thread-factory (-> (Thread/ofPlatform) (.daemon true) .factory)
          pool (Executors/newFixedThreadPool 6 thread-factory)]
      (try
        (check-concurrent-publication! pool node-id labels)
        (finally
          (.shutdownNow pool))))))

;; Checks the Java endpoint hash, equality, and sort order.
(defspec generated-endpoint-values 180
  (prop/for-all [node-id node-id-gen
                 label label-gen
                 other-id node-id-gen
                 other-label label-gen]
    (let [endpoint (gt/endpoint node-id label)
          equal-endpoint (Endpoint. node-id label)
          other (gt/endpoint other-id other-label)
          expected-hash (Util/hashCombine (Murmur3/hashLong node-id) (.hasheq ^Keyword label))]
      (and (= node-id (gt/endpoint-node-id endpoint))
           (= label (gt/endpoint-label endpoint))
           (= endpoint equal-endpoint)
           (= (= [node-id label] [other-id other-label]) (= endpoint other))
           (= (= node-id other-id) (= endpoint (Endpoint. other-id label)))
           (= (= label other-label) (= endpoint (Endpoint. node-id other-label)))
           (= expected-hash (hash endpoint) (.hashCode ^Endpoint endpoint))
           (= (compare [node-id label] [other-id other-label]) (compare endpoint other))))))
