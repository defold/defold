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

(ns internal.invalidate-counters-test
  (:require [clojure.test :refer [deftest is]]
            [clojure.test.check.clojure-test :refer [defspec]]
            [clojure.test.check.generators :as gen]
            [clojure.test.check.properties :as prop]
            [dynamo.graph :as g]
            [internal.graph.types :as gt]
            [internal.invalidate-counters :as ic]
            [support.test-support :as ts]
            [util.coll :as coll])
  (:import [clojure.lang IReduceInit]
           [com.defold.util InvalidationCounters]
           [java.util ArrayList]))

(set! *warn-on-reflection* true)

(defn- stored-counts [^InvalidationCounters snapshot]
  {:lifetimes (.lifetimeCount snapshot)
   :nodes (.outputNodeCount snapshot)
   :outputs (.outputCount snapshot)})

(def ^:private label-gen
  (gen/one-of [gen/keyword gen/keyword-ns]))

(def ^:private endpoint-gen
  (gen/fmap (fn make-endpoint [[node-id label]]
              (gt/endpoint node-id label))
            (gen/tuple gen/large-integer label-gen)))

(def ^:private history-block-gen
  ;; Bind generated values once so commands retain concrete IDs and labels when
  ;; shrinking. Each block has its own saved root; no numeric snapshot selectors
  ;; can silently point at another root after a command is removed.
  (gen/let [ids (gen/vector gen/large-integer 1 5)
            labels (gen/vector label-gen 1 4)
            birth gen/large-integer
            operations (gen/vector
                         (gen/fmap
                           (fn make-operation [[kind ids endpoints birth fork query]]
                             {:kind kind
                              :ids ids
                              :endpoints endpoints
                              :birth birth
                              :fork fork
                              :query query})
                           (gen/tuple
                             (gen/frequency [[2 (gen/return :add)]
                                             [2 (gen/return :delete)]
                                             [6 (gen/return :bump)]
                                             [1 (gen/return :full)]])
                             (gen/vector (gen/elements ids) 0 8)
                             (gen/vector
                               (gen/fmap (fn reuse-endpoint [[id label]]
                                           (gt/endpoint id label))
                                         (gen/tuple (gen/elements ids) (gen/elements labels)))
                               0 16)
                             gen/large-integer
                             (gen/frequency [[9 (gen/return false)]
                                             [1 (gen/return true)]])
                             endpoint-gen))
                         0 16)]
    {:ids ids
     :labels labels
     :birth birth
     :operations operations}))

(defn- ensure! [condition data]
  (when-not condition
    (throw (ex-info "Invalidation contract mismatch" data))))

(defn- model-step [model operation stamp]
  (let [{:keys [kind ids endpoints birth]} operation
        {:keys [lifetimes outputs]} model]
    (case kind
      :add
      (assoc model :lifetimes
             (coll/into-> ids lifetimes
               (coll/pair-map-by identity (constantly birth))))

      :delete
      (let [deleted (set ids)]
        (-> model
            (assoc :lifetimes (coll/reduce-> ids lifetimes dissoc))
            (update :outputs coll/transform->
                    (remove (fn [[endpoint _]]
                              (contains? deleted (gt/endpoint-node-id endpoint)))))))

      :full
      (-> model
          (update :generation inc)
          (assoc :outputs {}))

      :bump
      (assoc model :outputs
             (coll/into-> endpoints outputs
               (filter #(contains? lifetimes (gt/endpoint-node-id %)))
               (coll/pair-map-by identity (constantly stamp)))))))

(defn- model-changed [endpoint before after]
  (let [node-id (gt/endpoint-node-id endpoint)
        before-lifetime (get (:lifetimes before) node-id)
        after-lifetime (get (:lifetimes after) node-id)]
    (or (nil? before-lifetime)
        (nil? after-lifetime)
        (not= before-lifetime after-lifetime)
        (not= (get (:outputs before) endpoint) (get (:outputs after) endpoint)))))

(defn- capture [snapshot model endpoints]
  (-> (set endpoints)
      (into (coll/keys (:outputs model)))
      (coll/into-> {}
        (coll/pair-map-by identity #(ic/counter snapshot %)))))

(defn- check-snapshot! [{:keys [snapshot model captured]} node-ids endpoints]
  (let [outputs (:outputs model)
        expected-counts {:lifetimes (count (:lifetimes model))
                         :nodes (count (coll/into-> (coll/keys outputs) #{}
                                         (map gt/endpoint-node-id)))
                         :outputs (count outputs)}]
    (ensure! (= expected-counts (stored-counts snapshot))
             {:expected expected-counts
              :actual (stored-counts snapshot)})
    (doseq [node-id node-ids]
      (let [expected (get (:lifetimes model) node-id ::missing)
            actual (if (.contains ^InvalidationCounters snapshot node-id)
                     (.birth ^InvalidationCounters snapshot node-id)
                     ::missing)]
        (ensure! (= expected actual)
                 {:reason :wrong-lifetime
                  :node-id node-id
                  :expected expected
                  :actual actual})))
    (doseq [[endpoint previous] captured]
      (ensure! (identical? previous (ic/counter snapshot endpoint))
               {:reason :mutated-snapshot
                :endpoint endpoint}))
    (doseq [endpoint endpoints]
      (ensure! (= (contains? outputs endpoint) (some? (ic/counter snapshot endpoint)))
               {:reason :wrong-counter
                :endpoint endpoint
                :expected (get outputs endpoint)}))))

(defn- check-pair! [endpoint before after]
  (ensure! (= (model-changed endpoint (:model before) (:model after))
              (ic/endpoint-invalidated-since? endpoint (:snapshot before) (:snapshot after)))
           {:reason :endpoint-change
            :endpoint endpoint
            :before-lifetime (get-in before [:model :lifetimes (gt/endpoint-node-id endpoint)])
            :after-lifetime (get-in after [:model :lifetimes (gt/endpoint-node-id endpoint)])
            :before-output (get-in before [:model :outputs endpoint])
            :after-output (get-in after [:model :outputs endpoint])}))

(defn- advance-history [state before sibling operation]
  (let [{:keys [history node-ids endpoints]} state
        {:keys [kind ids birth query]} operation
        endpoints (cond-> (into endpoints (:endpoints operation))
                    query (conj query))
        node-ids (into (into node-ids ids) (map gt/endpoint-node-id) endpoints)
        model (model-step (:model before) operation (count history))
        snapshot (case kind
                   :add (ic/add-nodes (:snapshot before) ids birth)
                   :delete (ic/remove-nodes (:snapshot before) ids)
                   :bump (ic/bump (:snapshot before) (:endpoints operation))
                   :full (ic/full-invalidate (:snapshot before)))
        after {:snapshot snapshot
               :model model
               :captured (capture snapshot model endpoints)}]
    (doseq [previous [before sibling]]
      (check-snapshot! previous node-ids endpoints)
      (ensure! (= (not= (:generation (:model previous)) (:generation model))
                  (ic/full-invalidation-since? (:snapshot previous) snapshot))
               {:reason :full-reset
                :operation operation})
      (doseq [endpoint endpoints]
        (check-pair! endpoint previous after)))
    (check-snapshot! after node-ids endpoints)
    (assoc state
      :history (conj history after)
      :node-ids node-ids
      :endpoints endpoints)))

(defn- run-history!
  "Compare linear edits and explicit saved-root branches with a plain map model."
  [blocks]
  (let [initial {:snapshot (ic/make)
                 :model {:generation 0
                         :lifetimes {}
                         :outputs {}}
                 :captured {}}
        state (coll/reduce-> blocks
                {:history [initial]
                 :node-ids #{}
                 :endpoints #{}}
                (fn run-block [state {:keys [ids labels birth operations]}]
                  (let [block-endpoints (coll/into-> ids []
                                          (mapcat (fn [id]
                                                    (eduction (map #(gt/endpoint id %)) labels))))
                        state (update state :endpoints into block-endpoints)
                        latest (peek (:history state))
                        state (advance-history state latest latest {:kind :add
                                                                    :ids ids
                                                                    :birth birth})
                        created (peek (:history state))
                        state (advance-history state created latest {:kind :bump
                                                                     :endpoints block-endpoints})
                        saved (peek (:history state))]
                    (coll/reduce-> operations state
                      (fn run-command [state operation]
                        (let [before (if (:fork operation) saved (peek (:history state)))]
                          (advance-history state before saved operation)))))))]
    (doseq [entry (:history state)]
      (check-snapshot! entry (:node-ids state) (:endpoints state)))
    true))

;; Adding, deleting and restoring nodes, invalidating outputs and clearing all
;; counters must agree with a simple model and preserve earlier snapshots.
(defspec generated-persistent-histories 400
  (prop/for-all [blocks (gen/vector history-block-gen 0 6)]
    (run-history! blocks)))

(defn- apply-edit! [editor {:keys [kind ids endpoints birth]}]
  (case kind
    :add (ic/add-nodes! editor ids birth)
    :delete (ic/remove-nodes! editor ids)
    :bump (ic/bump! editor endpoints)
    :full (ic/full-invalidate! editor)))

(defn- run-edit-sessions! [blocks]
  (let [initial {:snapshot (ic/make)
                 :model {:generation 0
                         :lifetimes {}
                         :outputs {}}
                 :captured {}}
        state (coll/reduce-> blocks
                {:history [initial]
                 :node-ids #{}
                 :endpoints #{}}
                (fn run-session [{:keys [history node-ids endpoints]} {:keys [ids labels birth operations]}]
                  (let [block-endpoints (coll/into-> ids []
                                          (mapcat (fn [id]
                                                    (eduction (map #(gt/endpoint id %)) labels))))
                        operations (into [{:kind :add
                                           :ids ids
                                           :birth birth}
                                          {:kind :bump
                                           :endpoints block-endpoints}]
                                         operations)
                        endpoints (coll/into-> operations endpoints (mapcat :endpoints))
                        node-ids (-> node-ids
                                     (into (mapcat :ids) operations)
                                     (into (map gt/endpoint-node-id) endpoints))
                        before (if (:fork (peek operations)) initial (peek history))
                        editor (ic/edit (:snapshot before))
                        model (coll/reduce-> operations (:model before)
                                (fn apply-command [model operation]
                                  (apply-edit! editor operation)
                                  (model-step model operation (count history))))
                        snapshot (ic/commit! editor)
                        after {:snapshot snapshot
                               :model model
                               :captured (capture snapshot model endpoints)}]
                    (check-snapshot! after node-ids endpoints)
                    (doseq [previous history]
                      (check-snapshot! previous node-ids endpoints)
                      (ensure! (= (not= (:generation (:model previous)) (:generation model))
                                  (ic/full-invalidation-since? (:snapshot previous) snapshot))
                               {:reason :edit-session-full-reset})
                      (doseq [endpoint endpoints]
                        (check-pair! endpoint previous after)))
                    {:history (conj history after)
                     :node-ids node-ids
                     :endpoints endpoints})))]
    (doseq [entry (:history state)]
      (check-snapshot! entry (:node-ids state) (:endpoints state)))
    true))

;; Several node changes and output invalidations committed together must agree
;; with the graph model, including restores and full invalidation mid-session.
(defspec generated-edit-sessions 400
  (prop/for-all [blocks (gen/vector history-block-gen 0 6)]
    (run-edit-sessions! blocks)))

;; Committing a session must close it so later edits cannot change counters
;; already published to the graph or retained by an evaluation context.
(deftest edit-session-closes-on-commit
  (let [endpoint (gt/endpoint 1000 :value)
        initial (ic/add-nodes (ic/make) [1000] 1)
        editor (ic/edit initial)
        published (-> editor
                      (ic/bump! [endpoint])
                      ic/commit!)]
    (is (nil? (ic/counter initial endpoint)))
    (is (some? (ic/counter published endpoint)))
    (is (thrown? IllegalStateException (ic/add-nodes! editor [] 2)))
    (is (thrown? IllegalStateException (ic/remove-nodes! editor [])))
    (is (thrown? IllegalStateException (ic/bump! editor [])))
    (is (thrown? IllegalStateException (ic/full-invalidate! editor)))
    (is (thrown? IllegalStateException (ic/commit! editor)))
    (is (= {:lifetimes 1
            :nodes 1
            :outputs 1}
           (stored-counts published)))))

;; Separate edit sessions from one snapshot must leave each other's outputs
;; unchanged. Output invalidations within one session must share a stamp.
(deftest independent-edit-sessions
  (let [a (gt/endpoint 1000 :a)
        b (gt/endpoint 1000 :b)
        initial (ic/add-nodes (ic/make) [1000] 1)
        left (ic/edit initial)
        right (ic/edit initial)
        _ (ic/bump! left [a])
        _ (ic/bump! right [b])
        published-right (ic/commit! right)
        published-left (-> left
                           (ic/bump! [b])
                           ic/commit!)]
    (is (nil? (ic/counter initial a)))
    (is (nil? (ic/counter initial b)))
    (is (nil? (ic/counter published-right a)))
    (is (identical? (ic/counter published-left a) (ic/counter published-left b)))
    (is (not (identical? (ic/counter published-left b) (ic/counter published-right b))))))

;; Invalidating many outputs on a node, then invalidating some again, must
;; preserve old snapshots and leave the other outputs unchanged.
(defspec generated-dense-label-batches 200
  (prop/for-all [data (gen/let [id gen/large-integer
                                labels (gen/vector label-gen 1 100)
                                repeated (gen/vector (gen/elements labels) 0 80)]
                        {:id id
                         :labels labels
                         :repeated repeated})]
    (let [{:keys [id labels repeated]} data
          endpoints (mapv #(gt/endpoint id %) labels)
          repeats (mapv #(gt/endpoint id %) repeated)
          repeated-set (set repeats)
          initial (ic/add-nodes (ic/make) [id] 1)
          first (ic/bump initial endpoints)
          second (ic/bump first repeats)
          sibling (ic/bump first endpoints)
          captured (capture first
                            {:outputs (coll/pair-map-by identity (constantly 1) endpoints)}
                            endpoints)]
      (doseq [endpoint endpoints]
        (let [a (ic/counter first endpoint)]
          (ensure! (identical? (captured endpoint) a)
                   {:reason :dense-persistence})
          (ensure! (= (contains? repeated-set endpoint)
                      (ic/endpoint-invalidated-since? endpoint first second))
                   {:reason :dense-change})
          (ensure! (ic/endpoint-invalidated-since? endpoint first sibling)
                   {:reason :sibling-change})))
      (ensure! (= {:lifetimes 1
                   :nodes 1
                   :outputs (count (set labels))}
                  (stored-counts second))
               {:reason :dense-count})
      true)))

;; Two output names with the same hash must stay independent when one output is
;; invalidated again. Deleting the node must remove counters for both outputs.
(deftest colliding-labels-test
  (let [a (gt/endpoint 0 :invalidation-collision-4899)
        b (gt/endpoint 0 :invalidation-collision-30757)
        initial (ic/add-nodes (ic/make) [0] 1)
        populated (ic/bump initial [a b])
        changed (ic/bump populated [a])
        deleted (ic/remove-nodes changed [0])]
    (is (= (.hasheq :invalidation-collision-4899)
           (.hasheq :invalidation-collision-30757)))
    (is (= 2 (:outputs (stored-counts populated))))
    (is (ic/endpoint-invalidated-since? a populated changed))
    (is (not (ic/endpoint-invalidated-since? b populated changed)))
    (is (identical? (ic/counter populated b) (ic/counter changed b)))
    (is (some? (ic/counter populated a)))
    (is (some? (ic/counter populated b)))
    (is (zero? (:outputs (stored-counts deleted))))))

;; Node tracking and output invalidation must work at the smallest and largest
;; node IDs and where adjacent IDs cross storage boundaries.
(deftest signed-id-boundaries-test
  (doseq [id [Long/MIN_VALUE Long/MAX_VALUE -1 0 1 31 32 33 63 64 65
              1023 1024 1025 4294967295 4294967296 4294967297 281474976710656]]
    (let [endpoint (gt/endpoint id :value)
          created (ic/add-nodes (ic/make) [id] 0)
          populated (ic/bump created [endpoint])
          deleted (ic/remove-nodes populated [id])
          restored (ic/add-nodes deleted [id] Long/MIN_VALUE)]
      (is (.contains ^InvalidationCounters created id))
      (is (= 0 (.birth ^InvalidationCounters created id)))
      (is (some? (ic/counter populated endpoint)))
      (is (nil? (ic/counter deleted endpoint)))
      (is (ic/endpoint-invalidated-since? endpoint populated restored)))))

;; Creating, invalidating and deleting a node must leave earlier counter
;; snapshots unchanged. Invalidating no outputs must leave counters unchanged.
(deftest persistent-api-test
  (let [endpoint (gt/endpoint 1000 :a)
        empty (ic/make)
        created (ic/add-nodes empty [1000] 1)
        populated (ic/bump created [endpoint])
        deleted (ic/remove-nodes populated [1000])
        unchanged (ic/bump populated [])]
    (is (= {:lifetimes 0
            :nodes 0
            :outputs 0}
           (stored-counts empty)))
    (is (= {:lifetimes 1
            :nodes 0
            :outputs 0}
           (stored-counts created)))
    (is (nil? (ic/counter created endpoint)))
    (is (some? (ic/counter populated endpoint)))
    (is (nil? (ic/counter deleted endpoint)))
    (is (.contains ^InvalidationCounters created 1000))
    (is (not (.contains ^InvalidationCounters deleted 1000)))
    (is (not (ic/endpoint-invalidated-since? endpoint populated unchanged)))))

;; Empty node and output batches must leave persistent snapshots and open edit
;; sessions unchanged. Even empty batches must reject an already closed editor.
(deftest empty-batches-test
  (let [endpoint (gt/endpoint 1000 :value)
        initial (-> (ic/make)
                    (ic/add-nodes [1000] 1)
                    (ic/bump [endpoint]))]
    (doseq [batch [nil [] () (ArrayList.) (long-array 0) (object-array 0) (coll/keys {})]]
      (is (identical? initial (ic/add-nodes initial batch 2)))
      (is (identical? initial (ic/remove-nodes initial batch)))
      (is (identical? initial (ic/bump initial batch)))
      (let [editor (ic/edit initial)]
        (is (identical? editor (ic/add-nodes! editor batch 2)))
        (is (identical? editor (ic/remove-nodes! editor batch)))
        (is (identical? editor (ic/bump! editor batch)))
        (is (identical? initial (ic/commit! editor)))
        (is (thrown? IllegalStateException (ic/add-nodes! editor batch 2)))
        (is (thrown? IllegalStateException (ic/remove-nodes! editor batch)))
        (is (thrown? IllegalStateException (ic/bump! editor batch)))))))

;; Node and output batches that support reduction without count or seq must
;; still work; checking for empty input must not consume them before reduction.
(deftest uncounted-reducible-batches-test
  (let [endpoint (gt/endpoint 1000 :value)
        ids (reify IReduceInit
              (reduce [_ f init]
                (f init 1000)))
        endpoints (reify IReduceInit
                    (reduce [_ f init]
                      (f init endpoint)))
        initial (ic/make)
        persistent (-> initial
                       (ic/add-nodes ids 1)
                       (ic/bump endpoints))
        edited (-> initial
                   ic/edit
                   (ic/add-nodes! ids 1)
                   (ic/bump! endpoints)
                   ic/commit!)]
    (doseq [snapshot [persistent edited]]
      (is (= {:lifetimes 1
              :nodes 1
              :outputs 1}
             (stored-counts snapshot)))
      (is (some? (ic/counter snapshot endpoint)))
      (is (= {:lifetimes 0
              :nodes 0
              :outputs 0}
             (stored-counts (ic/remove-nodes snapshot ids))))
      (is (= {:lifetimes 0
              :nodes 0
              :outputs 0}
             (stored-counts (-> snapshot
                                ic/edit
                                (ic/remove-nodes! ids)
                                ic/commit!)))))))

;; Deleting nodes must remove their counters. Restoring them must mark their
;; outputs as invalidated since the old snapshot. Full invalidation must clear
;; all output counters.
(defspec generated-deletion-and-reset-contract 200
  (prop/for-all [node-ids (gen/vector gen/large-integer 1 30)
                 labels (gen/vector label-gen 1 12)]
    (let [endpoints (coll/into-> node-ids []
                      (mapcat (fn [id]
                                (eduction (map #(gt/endpoint id %)) labels))))
          initial (ic/add-nodes (ic/make) node-ids 1)
          populated (ic/bump initial endpoints)
          deleted (ic/remove-nodes populated node-ids)
          restored (ic/add-nodes deleted node-ids 2)
          reset (ic/full-invalidate populated)]
      (ensure! (= {:lifetimes 0
                   :nodes 0
                   :outputs 0}
                  (stored-counts deleted))
               {:reason :deleted-storage})
      (ensure! (= {:lifetimes (count (set node-ids))
                   :nodes 0
                   :outputs 0}
                  (stored-counts reset))
               {:reason :reset-storage})
      (ensure! (ic/full-invalidation-since? populated reset)
               {:reason :full-reset})
      (doseq [endpoint endpoints]
        (ensure! (some? (ic/counter populated endpoint))
                 {:reason :retained-counter})
        (ensure! (nil? (ic/counter deleted endpoint))
                 {:reason :deleted-counter})
        (ensure! (ic/endpoint-invalidated-since? endpoint populated restored)
                 {:reason :restored-lifetime
                  :endpoint endpoint}))
      true)))

;; Deleting some nodes and restoring them must invalidate only their outputs.
;; This must work for both consecutive and widely separated node IDs.
(defspec generated-node-layouts 200
  (prop/for-all [ids (gen/one-of
                       [(gen/vector gen/large-integer 0 150)
                        (gen/fmap (fn adjacent-ids [[base size]]
                                    (mapv #(unchecked-add (long base) (long %)) (range size)))
                                  (gen/tuple gen/large-integer (gen/choose 0 257)))])
                 label label-gen]
    (let [endpoints (mapv #(gt/endpoint % label) ids)
          deleted-ids (coll/into-> ids [] (take-nth 2))
          operations [{:kind :bump
                       :endpoints endpoints}
                      {:kind :delete
                       :ids deleted-ids}
                      {:kind :add
                       :ids deleted-ids
                       :birth 2}
                      {:kind :bump
                       :endpoints endpoints}
                      {:kind :bump
                       :endpoints endpoints
                       :fork true}
                      {:kind :full}
                      {:kind :bump
                       :endpoints endpoints}]]
      (run-history! [{:ids ids
                      :labels [label]
                      :birth 1
                      :operations operations}]))))

;; Invalidating the same output on thousands of nodes must preserve earlier
;; snapshots. Deleting and restoring one node must discard its old counters.
(deftest large-label-batch-persistence-test
  (let [ids (vec (range 12000))
        empty (ic/make)
        created (ic/add-nodes empty ids 1)
        endpoints (coll/into-> [:a :b :c] []
                    (mapcat (fn [label]
                              (eduction (map #(gt/endpoint % label)) ids))))
        before (ic/bump created endpoints)
        edited (ic/bump before (mapv #(gt/endpoint % :a) ids))
        deleted (ic/remove-nodes edited [42])
        restored (ic/add-nodes deleted [42] 2)
        after (ic/bump restored (mapv #(gt/endpoint % :b) ids))]
    (is (= 12000 (:nodes (stored-counts after))))
    (is (= (- (* 12000 3) 2) (:outputs (stored-counts after))))
    (is (some? (ic/counter before (gt/endpoint 42 :a))))
    (is (nil? (ic/counter after (gt/endpoint 42 :a))))
    (is (some? (ic/counter after (gt/endpoint 42 :b))))
    (is (ic/endpoint-invalidated-since? (gt/endpoint 42 :a) before after))
    (is (ic/endpoint-invalidated-since? (gt/endpoint 43 :b) before after))
    (is (not (ic/endpoint-invalidated-since? (gt/endpoint 43 :c) before after)))
    (is (zero? (:nodes (stored-counts empty))))))

;; Deleting most nodes, then restoring and invalidating them, must preserve
;; earlier snapshots and distinguish restored nodes from nodes that stayed live.
(deftest branch-representation-persistence-test
  (let [ids (vec (range 32))
        endpoints (mapv #(gt/endpoint % :scene) ids)
        before (ic/bump (ic/add-nodes (ic/make) ids 1) endpoints)
        captured (mapv #(ic/counter before %) endpoints)
        deleted (ic/remove-nodes before (subvec ids 0 28))
        restored (ic/add-nodes deleted (subvec ids 0 28) 2)
        after (ic/bump restored endpoints)]
    (is (= {:lifetimes 4
            :nodes 4
            :outputs 4}
           (stored-counts deleted)))
    (is (= {:lifetimes 32
            :nodes 32
            :outputs 32}
           (stored-counts after)))
    (doseq [id ids]
      (let [endpoint (endpoints id)]
        (is (identical? (captured id) (ic/counter before endpoint)))
        (is (= (< id 28) (nil? (ic/counter deleted endpoint))))
        (is (= (< id 28) (ic/endpoint-invalidated-since? endpoint before restored)))
        (is (ic/endpoint-invalidated-since? endpoint before after))))))

;; Creating many nodes together, then deleting and restoring some, must
;; invalidate only restored nodes. Deleting all nodes must remove all counters.
(deftest dense-region-test
  (let [node-ids (vec (range 8192))
        initial (ic/make)
        created (ic/add-nodes initial node-ids 42)
        endpoints (mapv #(gt/endpoint % :undeclared) node-ids)
        populated (ic/bump created endpoints)
        deleted-ids (coll/into-> node-ids [] (take-nth 3))
        deleted-set (set deleted-ids)
        deleted (ic/remove-nodes populated deleted-ids)
        restored (ic/add-nodes deleted deleted-ids Long/MIN_VALUE)
        cleared (ic/remove-nodes restored node-ids)
        reset (ic/full-invalidate restored)]
    (is (= {:lifetimes 0
            :nodes 0
            :outputs 0}
           (stored-counts cleared)))
    (is (= {:lifetimes 8192
            :nodes 0
            :outputs 0}
           (stored-counts reset)))
    (doseq [endpoint endpoints]
      (is (some? (ic/counter populated endpoint)))
      (is (= (contains? deleted-set (gt/endpoint-node-id endpoint))
             (ic/endpoint-invalidated-since? endpoint populated restored))))))

;; Nodes created separately must keep their own lifetime values. Updating one
;; node's lifetime must leave the others and earlier snapshots unchanged.
(deftest mixed-lifetime-persistence-test
  (let [empty (ic/make)
        mixed (coll/reduce-> (range 64) empty
                (fn [snapshot id]
                  (ic/add-nodes snapshot [id] id)))
        replaced (ic/add-nodes mixed [32] 999)]
    (is (= 64 (:lifetimes (stored-counts mixed))))
    (dotimes [id 64]
      (is (.contains ^InvalidationCounters mixed id))
      (is (= id (.birth ^InvalidationCounters mixed id))))
    (is (= 999 (.birth ^InvalidationCounters replaced 32)))
    (is (= 32 (.birth ^InvalidationCounters mixed 32)))
    (is (zero? (:lifetimes (stored-counts empty))))))

;; Creating nodes in large batches, deleting some and restoring them must
;; preserve the correct lifetimes and leave earlier snapshots unchanged.
(defspec generated-batched-lifetimes 40
  (prop/for-all [base gen/large-integer
                 size (gen/choose 0 8000)
                 stride (gen/choose 0 64)
                 birth gen/large-integer]
    (let [node-ids (mapv #(unchecked-add (long base)
                                         (unchecked-multiply (long stride) (long %)))
                         (range size))
          known-node-ids (set node-ids)
          deleted-ids (coll/into-> node-ids [] (take-nth 2))
          restored-birth (unchecked-inc (long birth))
          empty (ic/make)
          created (ic/add-nodes empty node-ids birth)
          created-from-primitive-array (ic/add-nodes empty (long-array node-ids) birth)
          created-from-object-array (-> empty
                                        ic/edit
                                        (ic/add-nodes! (object-array node-ids) birth)
                                        ic/commit!)
          repeated (ic/add-nodes created node-ids birth)
          deleted (ic/remove-nodes repeated deleted-ids)
          restored (ic/add-nodes deleted deleted-ids restored-birth)
          model (coll/pair-map-by identity (constantly birth) node-ids)
          deleted-model (coll/reduce-> deleted-ids model dissoc)
          restored-model (coll/into-> deleted-ids deleted-model
                           (coll/pair-map-by identity (constantly restored-birth)))]
      (doseq [[snapshot lifetimes] [[empty {}]
                                    [created model]
                                    [created-from-primitive-array model]
                                    [created-from-object-array model]
                                    [repeated model]
                                    [deleted deleted-model]
                                    [restored restored-model]]]
        (check-snapshot! {:snapshot snapshot
                          :model {:lifetimes lifetimes
                                  :outputs {}}
                          :captured {}}
                         known-node-ids
                         #{}))
      true)))

;; Creating 70,000 nodes must track every ID. Replacing, deleting and restoring
;; individual nodes must preserve the other nodes and the original snapshot.
(deftest batch-flush-and-operation-order-test
  (let [empty (ic/make)
        ids (mapv #(* 64 (long %)) (range 70000))
        created (ic/add-nodes empty ids 42)
        replaced (ic/add-nodes created [0] Long/MIN_VALUE)
        deleted (ic/remove-nodes replaced [64])
        snapshot (ic/add-nodes deleted [64] Long/MAX_VALUE)]
    (is (= 70000 (:lifetimes (stored-counts snapshot))))
    (is (zero? (:lifetimes (stored-counts empty))))
    (is (= 42 (.birth ^InvalidationCounters created 0)))
    (is (= 42 (.birth ^InvalidationCounters created 64)))
    (is (= Long/MIN_VALUE (.birth ^InvalidationCounters snapshot 0)))
    (is (= Long/MAX_VALUE (.birth ^InvalidationCounters snapshot 64)))
    (doseq [id (subvec ids 2)]
      (when-not (and (.contains ^InvalidationCounters snapshot id)
                     (= 42 (.birth ^InvalidationCounters snapshot id)))
        (throw (ex-info "Flushed page was lost" {:id id}))))))

(g/defnode IndependentOutputs
  (property left g/Str)
  (property right g/Str)
  (output left-output g/Str :cached (g/fnk [left] left))
  (output right-output g/Str :cached (g/fnk [right] right)))

(g/defnode Passthrough
  (input value g/Str)
  (output result g/Str :cached (g/fnk [value] value)))

;; After edits, full invalidation, undo or redo, only output values that are
;; still valid may be added to the graph cache.
(defspec generated-cache-publication-histories 60
  (prop/for-all [initial (gen/tuple gen/string-alphanumeric gen/string-alphanumeric)
                 operations (gen/vector
                              (gen/tuple (gen/elements [:left :right :full :restore :redo])
                                         gen/string-alphanumeric)
                              1 15)]
    (ts/with-clean-system
      (let [node (g/make-node! IndependentOutputs :left (initial 0) :right (initial 1))
            endpoints [(gt/endpoint node :left-output) (gt/endpoint node :right-output)]]
        (coll/reduce-> operations initial
          (fn check-publication [values [kind value :as operation]]
            (g/clear-system-cache!)
            (let [ec (g/make-evaluation-context)
                  evaluated [(g/node-value node :left-output ec)
                             (g/node-value node :right-output ec)]
                  updated (case kind
                            (:left :full) (assoc values 0 value)
                            :right (assoc values 1 value)
                            (:restore :redo) values)
                  invalidated (if (contains? #{:full :restore :redo} kind)
                                [true true]
                                (mapv not= values updated))]
              (ensure! (= values evaluated)
                       {:reason :evaluated-value
                        :operation operation})
              (case kind
                :left (g/transact {:undoable false} (g/set-property node :left value))
                :right (g/transact {:undoable false} (g/set-property node :right value))
                :full (g/transact {:undoable false
                                   :full-invalidation true}
                        (g/set-property node :left value))
                (:restore :redo) (do
                                   (g/transact {:undo-key ::generated-delete} (g/delete-node node))
                                   (g/undo! ::generated-delete)
                                   (when (= :redo kind)
                                     (g/redo! ::generated-delete)
                                     (g/undo! ::generated-delete))))
              (let [changed (g/endpoint-invalidated-pred (g/evaluation-context-invalidate-counters ec))]
                (ensure! (= invalidated (mapv changed endpoints))
                         {:reason :graph-invalidation
                          :operation operation}))
              (g/update-cache-from-evaluation-context! ec)
              (doseq [index (range 2)]
                (ensure! (= (if (invalidated index) ::miss (values index))
                            (get (g/cache) (endpoints index) ::miss))
                         {:reason :cache-publication
                          :operation operation
                          :endpoint (endpoints index)}))
              updated))))
      true)))

;; Nodes created in the same transaction must share a lifetime value and start
;; without output counters.
(deftest initial-counter-storage-is-shared
  (ts/with-clean-system
    (let [[a b] (ts/tx-nodes (g/make-nodes [_a IndependentOutputs
                                            _b IndependentOutputs]))
          ^InvalidationCounters counters (g/invalidate-counters)]
      (is (= (.birth counters a) (.birth counters b)))
      (is (= 2 (.lifetimeCount counters)))
      (is (zero? (.outputNodeCount counters))))))

;; Editing one property must reject the old value of its output, while allowing
;; an unchanged output on the same node to enter the cache.
(deftest precise-output-invalidation
  (ts/with-clean-system
    (let [node (g/make-node! IndependentOutputs :left "left" :right "right")
          left-endpoint (gt/endpoint node :left-output)
          right-endpoint (gt/endpoint node :right-output)
          ec (g/make-evaluation-context)]
      (is (= "left" (g/node-value node :left-output ec)))
      (is (= "right" (g/node-value node :right-output ec)))
      (g/transact (g/set-property node :left "changed"))
      (let [invalidated (g/endpoint-invalidated-pred (g/evaluation-context-invalidate-counters ec))]
        (is (invalidated left-endpoint))
        (is (not (invalidated right-endpoint))))
      (g/update-cache-from-evaluation-context! ec)
      (is (= ::miss (get (g/cache) left-endpoint ::miss)))
      (is (= "right" (get (g/cache) right-endpoint))))))

;; Deleting a node must discard its counters and reject its earlier evaluated
;; values, even after clearing undo history. Repeated creation and deletion
;; must leave no counters or cached values behind.
(deftest deletion-reclaims-invalidation-metadata
  (ts/with-clean-system
    (let [node (g/make-node! IndependentOutputs :left "old")
          endpoint (gt/endpoint node :left-output)
          ec (g/make-evaluation-context)]
      (g/node-value node :left-output ec)
      (g/transact (g/set-property node :left "changed"))
      (g/transact (g/delete-node node))
      (g/reset-undo! :undo/global)
      (is (zero? (.lifetimeCount ^InvalidationCounters (g/invalidate-counters))))
      (is (zero? (.outputNodeCount ^InvalidationCounters (g/invalidate-counters))))
      (is ((g/endpoint-invalidated-pred (g/evaluation-context-invalidate-counters ec)) endpoint))
      (g/update-cache-from-evaluation-context! ec)
      (is (= ::miss (get (g/cache) endpoint ::miss))))
    (dotimes [_ 100]
      (let [node (first (g/tx-nodes-added
                          (g/transact {:undoable false}
                            (g/make-node IndependentOutputs :left "temporary"))))]
        (g/node-value node :left-output)
        (g/transact {:undoable false} (g/delete-node node))))
    (is (zero? (.lifetimeCount ^InvalidationCounters (g/invalidate-counters))))
    (is (zero? (.outputNodeCount ^InvalidationCounters (g/invalidate-counters))))
    (is (zero? (count (g/cache))))))

;; Undoing a deletion must reject the node's old output value if its source
;; changed while it was deleted. Repeating redo and undo must do the same.
(deftest deletion-undo-assigns-new-lifetimes
  (ts/with-clean-system
    (let [[source target] (ts/tx-nodes
                            (g/make-nodes [source [IndependentOutputs :left "old"]
                                           target Passthrough]
                              (g/connect source :left-output target :value)))
          endpoint (gt/endpoint target :result)
          ec (g/make-evaluation-context)]
      (is (= "old" (g/node-value target :result ec)))
      (g/transact {:undo-key ::delete} (g/delete-node target))
      (g/transact {:undoable false} (g/set-property source :left "changed"))
      (g/undo! ::delete)
      (is (g/node-by-id target))
      (g/update-cache-from-evaluation-context! ec)
      (is (= ::miss (get (g/cache) endpoint ::miss)))
      (let [restored-ec (g/make-evaluation-context)]
        (is (= "changed" (g/node-value target :result restored-ec)))
        (g/redo! ::delete)
        (g/undo! ::delete)
        (g/update-cache-from-evaluation-context! restored-ec)
        (is (= ::miss (get (g/cache) endpoint ::miss)))
        (is (= "changed" (g/node-value target :result)))))))

;; Undoing and redoing node creation must reject values evaluated before undo
;; and preserve connections that still refer to the original node ID.
(deftest creation-redo-assigns-new-lifetime
  (ts/with-clean-system
    (let [target (first (g/tx-nodes-added
                          (g/transact {:undoable false}
                            (g/make-node Passthrough))))
          source (first (g/tx-nodes-added
                          (g/transact {:undo-key ::create}
                            (g/make-node IndependentOutputs :left "value"))))
          endpoint (gt/endpoint source :left-output)]
      (g/transact {:undoable false} (g/connect source :left-output target :value))
      (let [ec (g/make-evaluation-context)]
        (g/node-value source :left-output ec)
        (g/undo! ::create)
        (is (nil? (g/node-by-id source)))
        (g/redo! ::create)
        (is ((g/endpoint-invalidated-pred (g/evaluation-context-invalidate-counters ec)) endpoint))
        (g/update-cache-from-evaluation-context! ec)
        (is (= ::miss (get (g/cache) endpoint ::miss)))
        (is (= "value" (g/node-value target :result)))))))

;; Clearing all output counters must also reject output values evaluated before
;; the full invalidation.
(deftest full-invalidation-clears-output-counters
  (ts/with-clean-system
    (let [node (g/make-node! IndependentOutputs :left "old")
          endpoint (gt/endpoint node :left-output)]
      (g/transact (g/set-property node :left "changed"))
      (is (some? (ic/counter (g/invalidate-counters) endpoint)))
      (let [ec (g/make-evaluation-context)]
        (g/node-value node :left-output ec)
        (g/transact {:full-invalidation true} (g/set-property node :left "final"))
        (is (nil? (ic/counter (g/invalidate-counters) endpoint)))
        (g/update-cache-from-evaluation-context! ec)
        (is (= ::miss (get (g/cache) endpoint ::miss)))
        (is (= "final" (g/node-value node :left-output)))))))

;; Invalidating some outputs must leave the others and earlier snapshots
;; unchanged. Full invalidation must clear output counters but keep live nodes.
(deftest batched-version-updates-preserve-snapshots
  (let [labels (mapv #(keyword (str "output-" %)) (range 32))
        endpoints (coll/into-> [1000 1001] []
                    (mapcat (fn [node-id]
                              (eduction (map #(gt/endpoint node-id %)) labels))))
        initial (ic/add-nodes (ic/make) [1000 1001] 1000)
        ^InvalidationCounters first-batch (ic/bump initial endpoints)
        ^InvalidationCounters second-batch (ic/bump first-batch [(endpoints 0) (endpoints 1)])
        ^InvalidationCounters reset (ic/full-invalidate second-batch)]
    (is (= 64 (.outputCount first-batch)))
    (is (= 2 (.outputNodeCount first-batch)))
    (is (nil? (ic/counter initial (endpoints 0))))
    (is (ic/endpoint-invalidated-since? (endpoints 0) first-batch second-batch))
    (is (ic/endpoint-invalidated-since? (endpoints 1) first-batch second-batch))
    (doseq [endpoint (subvec endpoints 2)]
      (is (not (ic/endpoint-invalidated-since? endpoint first-batch second-batch)))
      (is (identical? (ic/counter first-batch endpoint) (ic/counter second-batch endpoint))))
    (is (ic/full-invalidation-since? second-batch reset))
    (is (zero? (.outputCount reset)))
    (doseq [node-id [1000 1001]]
      (is (.contains reset node-id))
      (is (= (.birth second-batch node-id) (.birth reset node-id))))
    (doseq [endpoint endpoints]
      (is (some? (ic/counter second-batch endpoint)))
      (is (nil? (ic/counter reset endpoint))))))

;; Outputs invalidated together must share one stamp to save memory. A later
;; invalidation must replace the stamp only on the affected outputs.
(deftest identity-stamps-are-shared-per-batch
  (let [initial (ic/add-nodes (ic/make) [1000 1001] 1000)
        a (gt/endpoint 1000 :a)
        b (gt/endpoint 1001 :b)
        first-batch (ic/bump initial [a b])
        second-batch (ic/bump first-batch [a])]
    (is (identical? (ic/counter first-batch a) (ic/counter first-batch b)))
    (is (not (identical? (ic/counter first-batch a) (ic/counter second-batch a))))
    (is (identical? (ic/counter first-batch b) (ic/counter second-batch b)))))

;; Deleting a node must remove counters for all its labels, including inputs
;; and undeclared labels, while leaving other nodes' counters unchanged.
(deftest deletion-reclaims-all-labels
  (ts/with-clean-system
    (let [node (g/make-node! Passthrough)
          endpoints [(gt/endpoint node :value) (gt/endpoint node ::undeclared)]]
      (g/invalidate-outputs! endpoints)
      (is (coll/every? #(some? (ic/counter (g/invalidate-counters) %)) endpoints))
      (g/transact (g/delete-node node))
      (is (zero? (.lifetimeCount ^InvalidationCounters (g/invalidate-counters))))
      (is (zero? (.outputNodeCount ^InvalidationCounters (g/invalidate-counters))))))
  (let [a (gt/endpoint 1000 :a)
        b (gt/endpoint 1001 :b)
        ^InvalidationCounters initial (-> (ic/make)
                                          (ic/add-nodes [1000 1001] 1000)
                                          (ic/bump [a b]))
        ^InvalidationCounters deleted (ic/remove-nodes initial [1000])]
    (is (identical? (ic/counter initial b) (ic/counter deleted b)))
    (is (not (.contains deleted 1000)))
    (is (.contains deleted 1001))
    (is (= 1 (.lifetimeCount deleted)))
    (is (= 1 (.outputNodeCount deleted)))
    (is (nil? (ic/counter deleted a)))))

;; A transaction that adds and deletes nodes while invalidating everything
;; must track the remaining nodes, clear cached values and output counters,
;; and reject output values evaluated before the transaction.
(deftest full-invalidation-with-node-changes
  (ts/with-clean-system
    (let [[node deleted-node] (ts/tx-nodes
                                (g/make-nodes [_node [IndependentOutputs :left "old"]
                                               _deleted-node [IndependentOutputs :left "deleted"]]))
          endpoints [(gt/endpoint node :left-output)
                     (gt/endpoint deleted-node :left-output)]]
      (g/invalidate-outputs! endpoints)
      (let [ec (g/make-evaluation-context)
            snapshot (g/evaluation-context-invalidate-counters ec)]
        (is (= "old" (g/node-value node :left-output ec)))
        (is (= "deleted" (g/node-value deleted-node :left-output ec)))
        (g/update-cache-from-evaluation-context! ec)
        (is (coll/every? #(contains? (g/cache) %) endpoints))

        (let [tx-result (g/transact {:full-invalidation true}
                          [(g/delete-node deleted-node)
                           (g/set-property node :left "changed")
                           (g/make-node IndependentOutputs :left "added")])
              added-node (first (g/tx-nodes-added tx-result))]
          (is (coll/empty? (g/cache)))
          (is (g/node-by-id node))
          (is (nil? (g/node-by-id deleted-node)))
          (is (g/node-by-id added-node))
          (is (= 2 (.lifetimeCount ^InvalidationCounters (g/invalidate-counters))))
          (is (zero? (.outputCount ^InvalidationCounters (g/invalidate-counters))))
          (is (g/full-invalidation-since? snapshot (g/invalidate-counters)))
          (is (coll/every? (g/endpoint-invalidated-pred snapshot) endpoints))

          (g/update-cache-from-evaluation-context! ec)
          (is (coll/empty? (g/cache)))
          (is (= "changed" (g/node-value node :left-output)))
          (is (= "added" (g/node-value added-node :left-output))))))))
