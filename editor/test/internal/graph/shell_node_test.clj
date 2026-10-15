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

(ns internal.graph.shell-node-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [internal.graph.types :as gt]
            [internal.node :as in]
            [internal.transaction :as it]
            [support.test-support :refer [with-clean-system]]
            [util.coll :as coll])
  (:import [clojure.lang ExceptionInfo]
           [java.util.concurrent CountDownLatch TimeUnit]))

(g/defnode ShellTestNode
  (property identity g/Str :unjammable)
  (property value g/Int (default 0))
  (input dependency g/Int :cascade-delete)
  (output result g/Int :cached (g/fnk [value dependency] (+ value (or dependency 0)))))

(g/defnode ShellConsumer
  (property value g/Int)
  (property source g/NodeID
            (set (fn [evaluation-context self _old source]
                   (g/set-property self :value (g/node-value source :result evaluation-context))))))

;; Committing against the original basis must reuse evaluated nodes and update
;; the transaction revision, cached values, and downstream invalidations.
(deftest evaluation-context-direct-commit-test
  (doseq [custom-context [false true]]
    (with-clean-system
      (let [consumer (g/make-node! ShellTestNode :value 3)
            [shell child] (g/take-node-ids 2)]
        (g/transact
          (g/add-node
            (g/construct-shell ShellTestNode {:_node-id shell}
              (fn materialize-fn [self _evaluation-context]
                [(g/set-property self :value 7)
                 (g/add-node (g/construct ShellTestNode :_node-id child :value 5))
                 (g/connect child :result self :dependency)
                 (g/connect self :result consumer :dependency)]))))
        (is (= 3 (g/node-value consumer :result)))
        (let [initial-basis (g/now)
              evaluation-context (if custom-context
                                   (g/make-evaluation-context {:basis initial-basis})
                                   (g/make-evaluation-context))
              initial-invalidate-counters (g/invalidate-counters)]
          (is (= 12 (g/node-value shell :result evaluation-context)))
          (is (identical? initial-basis (g/now)))
          (let [evaluated-basis @(:basis-ref evaluation-context)]
            (g/update-system-from-evaluation-context! evaluation-context)
            (doseq [node-id [shell child]]
              (is (identical? (g/node-by-id evaluated-basis node-id)
                              (g/node-by-id (g/now) node-id)))))
          (is (= (inc (long (gt/tx-id initial-basis))) (gt/tx-id (g/now))))
          (is (= 12 (get (g/cache) (gt/endpoint shell :result))))
          (is (not (contains? (g/cache) (gt/endpoint consumer :result))))
          (is (g/endpoint-invalidated-since? (gt/endpoint consumer :result)
                                             initial-invalidate-counters
                                             (g/invalidate-counters)))
          (is (= 15 (g/node-value consumer :result))))))))

;; Deletions from successive evaluation transactions must all clean up caches,
;; user data, and materialization records when the evaluated basis is committed.
(deftest evaluation-context-deletions-clean-up-system-test
  (doseq [custom-context [false true]]
    (with-clean-system
      (let [shells (vec (g/take-node-ids 3))]
        (g/transact
          (mapv (fn [shell]
                  (g/add-node
                    (g/construct-shell ShellTestNode {:_node-id shell}
                      (fn materialize-fn [self _evaluation-context]
                        (g/set-property self :value 7)))))
                shells))
        (doseq [shell shells]
          (is (= 7 (g/node-value shell :result)))
          (g/user-data! shell :marker :retained))
        (let [[first-shell second-shell retained-shell] shells
              evaluation-context (if custom-context
                                   (g/make-evaluation-context {:basis (g/now)})
                                   (g/make-evaluation-context))]
          (it/transact-in-evaluation-context! evaluation-context (g/delete-node first-shell))
          (it/transact-in-evaluation-context! evaluation-context (g/delete-node second-shell))
          (g/update-system-from-evaluation-context! evaluation-context)
          (let [committed-context (g/make-evaluation-context)]
            (doseq [shell [first-shell second-shell]]
              (is (nil? (g/node-by-id (g/now) shell)))
              (is (nil? (g/user-data shell :marker)))
              (is (not (contains? (g/cache) (gt/endpoint shell :result))))
              (is (not (contains? (:initial-materialization-results committed-context) shell)))
              (is (not (contains? (:initial-materialization-invalidate-counters committed-context) shell)))))
          (is (= :retained (g/user-data retained-shell :marker)))
          (is (= 7 (g/node-value retained-shell :result))))))))

;; Commits must reject full invalidation and output invalidations, including
;; output invalidations that leave the system basis unchanged.
(deftest evaluation-context-commit-rejects-invalidations-test
  (doseq [invalidation [:output :full]]
    (with-clean-system
      (let [shell (first (g/take-node-ids 1))]
        (g/transact
          (g/add-node
            (g/construct-shell ShellTestNode {:_node-id shell}
              (fn materialize-fn [self _evaluation-context]
                (g/set-property self :value 7)))))
        (let [evaluation-context (g/make-evaluation-context)]
          (is (= 7 (g/node-value shell :result evaluation-context)))
          (case invalidation
            :output (do
                      (g/invalidate-outputs! [(gt/endpoint shell :result)])
                      (is (identical? (:initial-basis evaluation-context) (g/now))))
            :full (g/transact {:full-invalidation true} (g/invalidate-output shell :result)))
          (g/update-system-from-evaluation-context! evaluation-context)
          (is (nil? @(:basis-ref evaluation-context)))
          (is (in/shell-node? (g/node-by-id (g/now) shell)))
          (is (not (contains? (g/cache) (gt/endpoint shell :result)))))))))

;; A failed loader's deletions must not remove user data for a node that is
;; successfully recreated by a retry in the same evaluation context.
(deftest failed-materialization-restores-deletion-tracking-test
  (with-clean-system
    (let [[shell child] (g/take-node-ids 2)
          fail (atom true)]
      (g/transact
        (g/add-node
          (g/construct-shell ShellTestNode {:_node-id shell}
            (fn materialize-fn [self evaluation-context]
              (if-not @fail
                [(g/add-node (g/construct ShellTestNode :_node-id child :value 5))
                 (g/set-property self :value 7)]
                (g/callback
                  (fn delete-child-and-fail []
                    (it/transact-in-evaluation-context!
                      evaluation-context
                      (g/add-node (g/construct ShellTestNode :_node-id child)))
                    (it/transact-in-evaluation-context! evaluation-context (g/delete-node child))
                    (throw (ex-info "load failed" {})))))))))
      (let [evaluation-context (g/make-evaluation-context)]
        (is (thrown-with-msg? ExceptionInfo #"load failed"
                              (g/materialize-node! shell evaluation-context)))
        (reset! fail false)
        (is (= 7 (g/node-value shell :result evaluation-context)))
        (g/user-data! child :marker :retained)
        (g/update-system-from-evaluation-context! evaluation-context)
        (is (= :retained (g/user-data child :marker)))
        (is (= 5 (g/node-value child :result)))))))

(deftest deferred-materialization-test
  (with-clean-system
    (let [[node-id child-id] (g/take-node-ids 2)
          calls (atom 0)]
      (g/transact
        (g/add-node
          (g/construct-shell ShellTestNode
            {:_node-id node-id
             :identity "shell"}
            (fn [self _evaluation-context]
              (swap! calls inc)
              [(g/set-property self :value 7)
               (g/add-node (g/construct ShellTestNode :_node-id child-id :value 5))
               (g/connect child-id :result self :dependency)]))))
      (let [evaluation-context (g/make-evaluation-context)
            shell (g/node-by-id @(:basis-ref evaluation-context) node-id)]
        (is (= "shell" (g/node-value node-id :identity evaluation-context)))
        (is (zero? @calls))
        (is (thrown? ExceptionInfo (gt/get-property shell @(:basis-ref evaluation-context) :value)))
        (is (thrown? ExceptionInfo (g/own-property-values @(:basis-ref evaluation-context) shell)))
        (is (= 12 (g/node-value node-id :result evaluation-context)))
        (is (= 12 (g/node-value node-id :result evaluation-context)))
        (is (= 1 @calls))
        (is (in/shell-node? (g/node-by-id (g/now) node-id)))
        (is (nil? (g/node-by-id (g/now) child-id)))
        (is (instance? internal.node.NodeImpl (g/node-by-id @(:basis-ref evaluation-context) node-id)))
        (g/update-system-from-evaluation-context! evaluation-context)
        (is (not (in/shell-node? (g/node-by-id (g/now) node-id))))
        (is (= 12 (g/node-value node-id :result)))
        (is (thrown-with-msg? AssertionError #"Evaluation context is closed"
                              (g/update-system-from-evaluation-context! evaluation-context)))
        (is (= 1 @calls))))))

(deftest shell-property-access-uses-supplied-basis-test
  (with-clean-system
    (let [node-id (first (g/take-node-ids 1))]
      (g/transact
        (g/add-node
          (g/construct-shell
            ShellTestNode
            {:_node-id node-id
             :identity "shell"}
            (fn materialize-fn [self _evaluation-context]
              [(g/set-property self :identity "materialized")
               (g/set-property self :value 7)]))))
      (let [evaluation-context (g/make-evaluation-context)
            initial-basis @(:basis-ref evaluation-context)
            shell (g/node-by-id initial-basis node-id)]
        (is (= 7 (g/node-value node-id :result evaluation-context)))
        (let [materialized-basis @(:basis-ref evaluation-context)
              materialized-node (g/node-by-id materialized-basis node-id)]
          (doseq [node [shell materialized-node]]
            (testing "Property reads use the materialized node in the supplied basis."
              (is (= "materialized" (gt/get-property node materialized-basis :identity)))
              (is (= 7 (gt/get-property node materialized-basis :value)))
              (is (= {:identity "materialized" :value 7}
                     (g/own-property-values materialized-basis node)))))
          (testing "A stale shell delegates mutations to the resolved NodeImpl."
            (is (instance? internal.node.NodeImpl (gt/set-property shell materialized-basis :value 8))))
          (testing "The shell retains its guards in the original basis."
            (is (= "shell" (gt/get-property shell initial-basis :identity)))
            (is (thrown? ExceptionInfo (gt/get-property shell initial-basis :value)))
            (is (thrown? ExceptionInfo (gt/set-property shell initial-basis :identity "changed")))
            (is (thrown? ExceptionInfo (g/own-property-values initial-basis shell)))))))))

(deftest materialization-in-property-setter-test
  (with-clean-system
    (let [[source-id consumer-id] (g/take-node-ids 2)

          materialize-fn
          (fn materialize-fn [self _evaluation-context]
            (g/set-property self :value 42))]

      (g/transact
        (concat
          (g/add-node (g/construct-shell ShellTestNode {:_node-id source-id} materialize-fn))
          (g/add-node (g/construct ShellConsumer :_node-id consumer-id))))
      (g/transact
        (g/set-property consumer-id :source source-id))
      (is (= 42 (g/node-value consumer-id :value)))
      (is (= 42 (g/node-value source-id :result)))
      (is (not (in/shell-node? (g/node-by-id (g/now) source-id)))))))

(deftest nested-materialization-test
  (with-clean-system
    (let [[parent child unrelated] (g/take-node-ids 3)
          loaded (atom [])

          materialize-fn
          (fn materialize-fn [self _evaluation-context]
            (swap! loaded conj self)
            (g/set-property self :value 10))]

      (g/transact
        (concat
          (g/add-node (g/construct-shell ShellTestNode {:_node-id parent} materialize-fn))
          (g/add-node (g/construct-shell ShellTestNode {:_node-id child} materialize-fn))
          (g/add-node (g/construct-shell ShellTestNode {:_node-id unrelated} materialize-fn))
          (g/connect child :result parent :dependency)))
      (is (= 20 (g/node-value parent :result)))
      (is (= [parent child] @loaded))
      (is (in/shell-node? (g/node-by-id (g/now) unrelated))))))

;; Verifies rollback and retry of materialized properties, including
;; non-undoable state that must remain isolated until the load succeeds.
(deftest failed-materialization-can-be-retried-test
  (with-clean-system
    (let [node-id (first (g/take-node-ids 1))
          fail (atom true)]
      (g/transact
        (g/add-node
          (g/construct-shell ShellTestNode
            {:_node-id node-id}
            (fn materialize-fn [self _evaluation-context]
              [(g/non-undoable (g/set-property self :identity "loaded"))
               (g/set-property self :value 9)
               (g/callback #(when @fail (throw (ex-info "load failed" {}))))]))))
      (let [{:keys [basis-ref changes-atom materialization-results-atom invalidated-endpoints-atom] :as evaluation-context} (g/make-evaluation-context)
            initial-basis @basis-ref
            initial-materialization-results @materialization-results-atom]
        (is (thrown-with-msg? ExceptionInfo #"load failed" (g/node-value node-id :result evaluation-context)))
        (is (identical? initial-basis @basis-ref))
        (is (identical? initial-materialization-results @materialization-results-atom))
        (is (coll/empty? @changes-atom))
        (is (coll/empty? @invalidated-endpoints-atom))
        (is (in/shell-node? (g/node-by-id @basis-ref node-id)))
        (is (nil? (g/node-value node-id :identity evaluation-context)))
        (is (nil? (g/node-value node-id :identity)))
        (reset! fail false)
        (is (= 9 (g/node-value node-id :result evaluation-context)))
        (is (= "loaded" (g/node-value node-id :identity evaluation-context)))
        (is (nil? (g/node-value node-id :identity)))
        (g/update-system-from-evaluation-context! evaluation-context)
        (is (= "loaded" (g/node-value node-id :identity)))))))

;; Failed materialization must restore evicted entries along with the basis,
;; so subsequent reads can reuse the original cached values.
(deftest failed-materialization-restores-context-cache-test
  (with-clean-system
    (let [cached-node (g/make-node! ShellTestNode :value 7)
          source (g/make-node! ShellTestNode :value 4)
          shell (first (g/take-node-ids 1))]
      (g/transact
        (g/add-node
          (g/construct-shell ShellTestNode {:_node-id shell}
            (fn materialize-fn [_self evaluation-context]
              (g/callback
                (fn invalidate-cached-output []
                  (it/transact-in-evaluation-context!
                    evaluation-context
                    (g/connect source :result cached-node :dependency))
                  (is (= 11 (g/node-value cached-node :result evaluation-context)))
                  (throw (ex-info "load failed" {}))))))))
      (is (= 7 (g/node-value cached-node :result)))
      (let [trace (atom nil)
            evaluation-context (g/make-evaluation-context {:tracer (g/make-tree-tracer trace)})]
        (is (thrown-with-msg? ExceptionInfo #"load failed"
                              (g/materialize-node! shell evaluation-context)))
        (reset! trace nil)
        (is (= 7 (g/node-value cached-node :result evaluation-context)))
        (is (= :cache (:output-type @trace)))
        (is (= 7 (g/node-value cached-node :result)))))))

(deftest materialization-replays-non-undoable-changes-test
  (with-clean-system
    (let [node-id (first (g/take-node-ids 1))]
      (g/transact
        (g/add-node
          (g/construct-shell ShellTestNode
            {:_node-id node-id}
            (fn [self _evaluation-context]
              (g/non-undoable (g/set-property self :value 13))))))
      (is (= 13 (g/node-value node-id :result)))
      (is (= 13 (g/raw-property-value (g/now) node-id :value))))))

(deftest stale-materialization-does-not-resurrect-deleted-nodes-test
  (with-clean-system
    (let [node-id (first (g/take-node-ids 1))

          materialize-fn
          (fn materialize-fn [self _evaluation-context]
            (g/set-property self :value 1))]

      (g/transact
        (g/add-node
          (g/construct-shell ShellTestNode {:_node-id node-id} materialize-fn)))
      (let [evaluation-context (g/make-evaluation-context)]
        (is (= 1 (g/node-value node-id :result evaluation-context)))
        (g/transact (g/delete-node node-id))
        (g/update-system-from-evaluation-context! evaluation-context)
        (is (nil? (g/node-by-id (g/now) node-id)))))))

;; A retained basis-ref observes successive materializations through derived
;; contexts and is cleared on commit. Dereferenced bases remain immutable
;; snapshots.
(deftest retained-evaluation-context-basis-ref-test
  (doseq [custom-context [false true]]
    (with-clean-system
      (let [[a b] (g/take-node-ids 2)
            materialize-fn (fn [self _evaluation-context] (g/set-property self :value 17))]
        (g/transact
          [(g/add-node (g/construct-shell ShellTestNode {:_node-id a} materialize-fn))
           (g/add-node (g/construct-shell ShellTestNode {:_node-id b} materialize-fn))])
        (let [{:keys [basis-ref] :as evaluation-context} (if custom-context
                                                           (g/make-evaluation-context {:no-local-temp true})
                                                           (g/make-evaluation-context))
              initial-basis @basis-ref
              independent-context (g/make-evaluation-context)
              derived-context (g/pruned-evaluation-context evaluation-context (constantly true))]
          (is (identical? basis-ref (:basis-ref derived-context)))
          (is (= 17 (g/node-value a :result derived-context)))
          (is (= 17 (g/raw-property-value @basis-ref a :value)))
          (let [first-materialized-basis @basis-ref]
            (is (= 17 (g/node-value b :result evaluation-context)))
            (is (= 17 (g/raw-property-value @basis-ref b :value)))
            (is (in/shell-node? (g/node-by-id first-materialized-basis b))))
          (is (in/shell-node? (g/node-by-id initial-basis a)))
          (is (identical? initial-basis @(:basis-ref independent-context)))
          (g/update-system-from-evaluation-context! derived-context)
          (is (nil? @basis-ref))
          (is (identical? initial-basis @(:basis-ref independent-context)))
          (is (= 17 (g/node-value a :result)))
          (is (= 17 (g/node-value b :result))))))))

;; Committing a derived context closes every context sharing its mutable state.
(deftest committed-evaluation-context-is-closed-test
  (with-clean-system
    (let [[a b] (g/take-node-ids 2)
          materialize-fn (fn [self _evaluation-context] (g/set-property self :value 17))]
      (g/transact
        (concat
          (g/add-node (g/construct-shell ShellTestNode {:_node-id a} materialize-fn))
          (g/add-node (g/construct-shell ShellTestNode {:_node-id b} materialize-fn))))
      (let [evaluation-context (g/make-evaluation-context)
            derived-context (assoc evaluation-context :tracer nil)
            initial-basis @(:basis-ref evaluation-context)]
        (is (g/evaluation-context? evaluation-context))
        (is (not (contains? evaluation-context :basis)))
        (is (= 17 (g/node-value a :result derived-context)))
        (is (identical? initial-basis (:initial-basis evaluation-context)))
        (is (not (identical? initial-basis @(:basis-ref evaluation-context))))
        (is (identical? @(:basis-ref evaluation-context) @(:basis-ref derived-context)))
        (g/update-system-from-evaluation-context! derived-context)
        (doseq [context [evaluation-context derived-context]]
          (is (nil? @(:basis-ref context)))
          (is (thrown-with-msg? AssertionError #"Evaluation context is closed"
                                (g/node-value b :result context)))
          (is (thrown-with-msg? AssertionError #"Evaluation context is closed"
                                (g/maybe-node-value b :result context)))
          (is (thrown-with-msg? AssertionError #"Evaluation context is closed"
                                (g/materialize-node! b context)))
          (is (thrown-with-msg? AssertionError #"Evaluation context is closed"
                                (g/update-system-from-evaluation-context! context))))
        (is (in/shell-node? (g/node-by-id (g/now) b)))
        (g/with-auto-evaluation-context fresh-context
          (is (= 17 (g/node-value b :result fresh-context))))))))

(g/defnode SharedMaterializationState
  (property values g/Any (default {})))

(deftest restore-overridden-properties-keeps-evaluation-in-context-test
  (with-clean-system
    (with-redefs [g/node-key (fn [node-id evaluation-context]
                               (g/node-value node-id :identity evaluation-context))]
      (let [source (g/make-node! ShellTestNode :identity "source")]
        (g/transact (g/override source))
        (let [override-node-id (first (g/overrides source))]
          (g/transact (g/set-property override-node-id :value 7))
          (let [collected-properties (g/collect-overridden-properties override-node-id)]
            (g/transact (g/set-property override-node-id :value 19))
            (g/transact (g/restore-overridden-properties override-node-id collected-properties))
            (is (= 7 (g/node-value override-node-id :value)))))))))

;; A loader must not overwrite existing shared properties when its result is replayed.
(deftest materialization-rejects-external-property-mutation-test
  (with-clean-system
    (let [[shell shared] (g/take-node-ids 2)]
      (g/transact
        [(g/add-node (g/construct SharedMaterializationState :_node-id shared))
         (g/add-node
           (g/construct-shell ShellTestNode {:_node-id shell}
             (fn [_self _evaluation-context]
               (g/update-property shared :values assoc shell :loaded))))])
      (is (thrown-with-msg? ExceptionInfo #"cannot modify an existing external node"
                            (g/node-value shell :result)))
      (is (= {} (g/node-value shared :values)))
      (is (in/shell-node? (g/node-by-id (g/now) shell))))))

(deftest tracing-materializes-with-real-values-test
  (with-clean-system
    (let [[parent child] (g/take-node-ids 2)]
      (g/transact
        (concat
          (g/add-node
            (g/construct-shell ShellTestNode
              {:_node-id parent}
              (fn materialize-fn [self evaluation-context]
                (g/materialize-prerequisites! self #{child} evaluation-context)
                (g/set-property self :value (g/node-value child :result evaluation-context)))))
          (g/add-node
            (g/construct-shell ShellTestNode
              {:_node-id child}
              (fn materialize-fn [self _evaluation-context]
                (g/set-property self :value 23))))))
      (let [evaluation-context (g/make-evaluation-context)]
        (g/node-value parent :result (assoc evaluation-context :dry-run true))
        (is (= 23 (g/node-value parent :result evaluation-context)))
        (g/update-system-from-evaluation-context! evaluation-context)
        (is (= 23 (g/node-value parent :result)))
        (is (= 23 (g/node-value child :result)))))))

;; Editing even an unjammable property materializes first, so shells remain immutable.
(deftest editing-unjammable-property-materializes-shell-test
  (with-clean-system
    (let [node-id (first (g/take-node-ids 1))
          calls (atom 0)]
      (g/transact
        (g/add-node
          (g/construct-shell ShellTestNode
            {:_node-id node-id
             :identity "before"}
            (fn materialize-fn [self _evaluation-context]
              (swap! calls inc)
              (g/set-property self :value 7)))))
      (g/transact (g/set-property node-id :identity "after"))
      (is (= "after" (g/node-value node-id :identity)))
      (is (= 1 @calls))
      (is (instance? internal.node.NodeImpl (g/node-by-id (g/now) node-id)))
      (g/undo! :undo/global)
      (is (= "before" (g/node-value node-id :identity)))
      (is (= 7 (g/node-value node-id :result))))))

(deftest materialization-while-producing-transaction-steps-test
  (with-clean-system
    (let [[source target] (g/take-node-ids 2)]
      (g/transact
        (concat
          (g/add-node
            (g/construct-shell ShellTestNode
              {:_node-id source}
              (fn materialize-fn [self _evaluation-context]
                (g/set-property self :value 31))))
          (g/add-node
            (g/construct ShellTestNode :_node-id target))))
      (g/transact
        (g/set-property target :value (g/node-value source :result)))
      (is (= 31 (g/node-value target :result)))
      (is (= 31 (g/node-value source :result))))))

(deftest overriding-unmaterialized-shell-includes-substructure-test
  (with-clean-system
    (let [[source child] (g/take-node-ids 2)
          overrides (atom nil)]
      (g/transact
        (g/add-node
          (g/construct-shell ShellTestNode
            {:_node-id source}
            (fn materialize-fn [self _evaluation-context]
              [(g/add-node (g/construct ShellTestNode :_node-id child :value 19))
               (g/connect child :result self :dependency)]))))
      (g/transact
        (g/override source {}
          (fn init-fn [_evaluation-context original->override]
            (reset! overrides original->override)
            (g/set-property (original->override child) :value 29))))
      (is (= #{source child} (into #{} (map key) @overrides)))
      (is (= 19 (g/node-value source :result)))
      (is (= 29 (g/node-value (@overrides source) :result))))))

(deftest evaluation-transaction-producers-retain-execution-order-test
  (with-clean-system
    (let [parent (g/make-node! ShellTestNode)
          child (atom nil)]
      (g/transact
        (g/expand-ec
          (fn [_evaluation-context]
            (eduction
              (map (fn [step]
                     (case step
                       :create
                       (g/expand
                         (fn []
                           (let [node-id (first (g/take-node-ids 1))]
                             (reset! child node-id)
                             (g/add-node
                               (g/construct ShellTestNode :_node-id node-id :value 37)))))

                       :connect
                       (g/connect @child :result parent :dependency))))
              [:create :connect]))))
      (is (= 37 (g/node-value parent :result))))))

(g/defnode ShellWithChild
  (property child g/NodeID)
  (property source-value g/Any :unjammable))

(deftest competing-contexts-share-materialization-test
  ;; Verifies that competing contexts share one materialization and replay its
  ;; non-undoable properties through the same transactions as its child nodes.
  (doseq [commit-before-second-evaluation [false true]]
    (with-clean-system
      (let [shell (first (g/take-node-ids 1))
            calls (atom 0)]
        (g/transact
          (g/add-node
            (g/construct-shell ShellWithChild
              {:_node-id shell}
              (fn [self _evaluation-context]
                (swap! calls inc)
                (let [child (first (g/take-node-ids 1))]
                  [(g/add-node (g/construct ShellTestNode :_node-id child :value 42))
                   (g/non-undoable (g/set-property self :source-value :loaded))
                   (g/set-property self :child child)])))))
        (let [first-context (g/make-evaluation-context)
              second-context (g/make-evaluation-context)
              child (g/node-value shell :child first-context)]
          (is (= :loaded (g/node-value shell :source-value first-context)))
          (is (nil? (g/node-value shell :source-value second-context)))
          (is (nil? (g/node-value shell :source-value)))
          (when commit-before-second-evaluation
            (g/update-system-from-evaluation-context! first-context))
          (is (= child (g/node-value shell :child second-context)))
          (is (= 42 (g/node-value child :result second-context)))
          (is (= :loaded (g/node-value shell :source-value second-context)))
          (when-not commit-before-second-evaluation
            (g/update-system-from-evaluation-context! first-context))
          (g/update-system-from-evaluation-context! second-context)
          (is (= child (g/node-value shell :child)))
          (is (= 42 (g/node-value child :result)))
          (is (= :loaded (g/node-value shell :source-value)))
          (is (= 1 @calls)))))))

(deftest shared-materialization-retains-prerequisites-test
  (doseq [load-prerequisite-first [false true]]
    (with-clean-system
      (let [[parent prerequisite] (g/take-node-ids 2)
            calls (atom [])]
        (g/transact
          [(g/add-node
             (g/construct-shell ShellWithChild
               {:_node-id parent}
               (fn [self evaluation-context]
                 (g/materialize-prerequisites! self #{prerequisite} evaluation-context)
                 (swap! calls conj self)
                 (g/set-property self :child (g/node-value prerequisite :child evaluation-context)))))
           (g/add-node
             (g/construct-shell ShellWithChild
               {:_node-id prerequisite}
               (fn [self _evaluation-context]
                 (swap! calls conj self)
                 (let [child (first (g/take-node-ids 1))]
                   [(g/add-node (g/construct ShellTestNode :_node-id child :value 37))
                    (g/set-property self :child child)]))))])
        (let [first-context (g/make-evaluation-context)
              second-context (g/make-evaluation-context)
              third-context (g/make-evaluation-context)]
          (when load-prerequisite-first
            (g/node-value prerequisite :child first-context))
          (let [child (g/node-value parent :child first-context)]
            (is (= child (g/node-value parent :child second-context)))
            (is (= 37 (g/node-value child :result second-context)))
            (is (= child (g/node-value prerequisite :child third-context)))
            (g/update-system-from-evaluation-context! third-context)
            (g/update-system-from-evaluation-context! second-context)
            (g/update-system-from-evaluation-context! first-context)
            (is (= child (g/node-value parent :child)))
            (is (= 37 (g/node-value child :result)))
            (is (= {parent 1 prerequisite 1} (frequencies @calls)))))))))

;; Competing claims must share one loader result, and contexts can be handed back for commit.
(deftest simultaneous-contexts-materialize-once-test
  (with-clean-system
    (let [shell (first (g/take-node-ids 1))
          context-count 8
          calls (atom 0)
          ready (CountDownLatch. context-count)
          start (promise)
          entered (promise)
          release (promise)]
      (g/transact
        (g/add-node
          (g/construct-shell ShellWithChild
            {:_node-id shell}
            (fn [self _evaluation-context]
              (swap! calls inc)
              (deliver entered true)
              (when (= ::timeout (deref release 10000 ::timeout))
                (throw (ex-info "Timed out waiting to finish load" {})))
              (let [child (first (g/take-node-ids 1))]
                [(g/add-node (g/construct ShellTestNode :_node-id child :value 11))
                 (g/set-property self :child child)])))))
      (let [contexts (mapv (fn [_] (g/make-evaluation-context)) (range context-count))
            results (mapv (fn [context]
                            (future
                              (.countDown ready)
                              (when (= ::timeout (deref start 10000 ::timeout))
                                (throw (ex-info "Timed out starting competing materializations." {})))
                              (g/node-value shell :child context)))
                          contexts)]
        (try
          (is (.await ready 10 TimeUnit/SECONDS))
          (deliver start true)
          (is (= true (deref entered 10000 ::timeout)))
          (deliver release true)
          (let [children (mapv #(deref % 10000 ::timeout) results)
                child (first children)]
            (is (g/node-id? child))
            (is (coll/every? #(= child %) children))
            (doseq [context contexts]
              (g/update-system-from-evaluation-context! context))
            (is (= 11 (g/node-value child :result)))
            (is (= 1 @calls)))
          (finally
            (deliver start true)
            (deliver release true)
            (doseq [result results]
              (future-cancel result))))))))

;; Independent loaders must run concurrently without losing another shell's registry entry.
(deftest independent-contexts-materialize-concurrently-test
  (with-clean-system
    (let [shells (vec (g/take-node-ids 4))
          entered (CountDownLatch. (count shells))
          calls (atom [])]
      (g/transact
        (mapv (fn [shell]
                (g/add-node
                  (g/construct-shell ShellTestNode {:_node-id shell}
                    (fn [self _evaluation-context]
                      (swap! calls conj self)
                      (.countDown entered)
                      (when-not (.await entered 10 TimeUnit/SECONDS)
                        (throw (ex-info "Timed out starting independent materializations." {})))
                      (g/set-property self :value 7)))))
              shells))
      (let [contexts (mapv (fn [_] (g/make-evaluation-context)) shells)
            results (mapv (fn [shell context]
                            (future (g/node-value shell :result context)))
                          shells contexts)]
        (try
          (is (= [7 7 7 7] (mapv #(deref % 10000 ::timeout) results)))
          (is (= (into {} (map #(vector % 1)) shells) (frequencies @calls)))
          (let [shared-materialization-registry @(:shared-materialization-registry-atom (first contexts))]
            (is (= (set shells) (set (coll/keys shared-materialization-registry))))
            (is (coll/every? #(realized? (:promise %)) (coll/vals shared-materialization-registry))))
          (doseq [context contexts]
            (g/update-system-from-evaluation-context! context))
          (is (= [7 7 7 7] (mapv #(g/node-value % :result) shells)))
          (finally
            (doseq [result results]
              (future-cancel result))))))))

(deftest shared-materialization-does-not-overwrite-later-edit-test
  (with-clean-system
    (let [shell (first (g/take-node-ids 1))]
      (g/transact
        (g/add-node
          (g/construct-shell ShellTestNode
            {:_node-id shell}
            (fn [self _evaluation-context]
              (g/set-property self :value 1)))))
      (let [first-context (g/make-evaluation-context)
            second-context (g/make-evaluation-context)]
        (is (= 1 (g/node-value shell :result first-context)))
        (is (= 1 (g/node-value shell :result second-context)))
        (g/update-system-from-evaluation-context! first-context)
        (g/transact (g/set-property shell :value 99))
        (g/update-system-from-evaluation-context! second-context)
        (is (= 99 (g/node-value shell :result)))))))

(deftest shared-materialization-can-replay-into-explicit-old-basis-test
  (with-clean-system
    (let [shell (first (g/take-node-ids 1))
          calls (atom 0)]
      (g/transact
        (g/add-node
          (g/construct-shell ShellTestNode
            {:_node-id shell}
            (fn [self _evaluation-context]
              (swap! calls inc)
              (g/set-property self :value 5)))))
      (let [old-basis (g/now)]
        (is (= 5 (g/node-value shell :result)))
        (let [evaluation-context (g/make-evaluation-context {:basis old-basis})]
          (is (= 5 (g/node-value shell :result evaluation-context)))
          (is (= 1 @calls)))))))

;; Failure of a dependent must preserve a successfully published prerequisite.
(deftest failed-materialization-retains-completed-prerequisites-test
  (doseq [preload-child [false true]]
    (with-clean-system
      (let [[parent child] (g/take-node-ids 2)
            fail (atom true)
            child-calls (atom 0)]
        (g/transact
          [(g/add-node
             (g/construct-shell ShellTestNode
               {:_node-id parent}
               (fn [self evaluation-context]
                 (g/materialize-prerequisites! self #{child} evaluation-context)
                 (let [value (g/node-value child :result evaluation-context)]
                   (when @fail
                     (throw (ex-info "outer load failed" {})))
                   (g/set-property self :value value)))))
           (g/add-node
             (g/construct-shell ShellTestNode
               {:_node-id child}
               (fn [self _evaluation-context]
                 (swap! child-calls inc)
                 (g/set-property self :value 23))))])
        (let [first-context (g/make-evaluation-context)
              second-context (g/make-evaluation-context)]
          (when preload-child
            (g/node-value child :result second-context))
          (is (thrown-with-msg? ExceptionInfo #"outer load failed"
                                (g/node-value parent :result first-context)))
          (is (instance? internal.node.NodeImpl (g/node-by-id @(:basis-ref first-context) child)))
          (reset! fail false)
          (is (= 23 (g/node-value parent :result second-context)))
          (is (= 23 (g/node-value parent :result first-context)))
          (is (= 1 @child-calls))
          (g/update-system-from-evaluation-context! second-context)
          (g/update-system-from-evaluation-context! first-context)
          (is (= 23 (g/node-value parent :result))))))))

(deftest cloned-systems-have-independent-materializations-test
  (with-clean-system
    (let [shell (first (g/take-node-ids 1))
          calls (atom 0)]
      (g/transact
        (g/add-node
          (g/construct-shell ShellTestNode
            {:_node-id shell}
            (fn [self _evaluation-context]
              (g/set-property self :value (swap! calls inc))))))
      (let [clone (atom (g/clone-system))]
        (is (= 1 (g/node-value shell :result)))
        (binding [g/*the-system* clone]
          (is (= 2 (g/node-value shell :result))))
        (is (= 1 (g/node-value shell :result)))
        (is (= 2 @calls))))))

(deftest shared-committed-materialization-does-not-resurrect-deleted-node-test
  (with-clean-system
    (let [shell (first (g/take-node-ids 1))]
      (g/transact
        (g/add-node
          (g/construct-shell ShellTestNode
            {:_node-id shell}
            (fn [self _evaluation-context]
              (g/set-property self :value 1)))))
      (let [evaluation-context (g/make-evaluation-context)]
        (is (= 1 (g/node-value shell :result evaluation-context)))
        (is (= 1 (g/node-value shell :result)))
        (g/transact (g/delete-node shell))
        (g/update-system-from-evaluation-context! evaluation-context)
        (is (nil? (g/node-by-id (g/now) shell)))))))

(deftest shared-materialization-survives-prerequisite-commit-test
  (with-clean-system
    (let [[parent prerequisite] (g/take-node-ids 2)
          calls (atom 0)]
      (g/transact
        [(g/add-node
           (g/construct-shell ShellWithChild
             {:_node-id parent}
             (fn [self evaluation-context]
               (g/materialize-prerequisites! self #{prerequisite} evaluation-context)
               (swap! calls inc)
               (let [value (g/node-value prerequisite :result evaluation-context)
                     child (first (g/take-node-ids 1))]
                 [(g/add-node (g/construct ShellTestNode :_node-id child :value value))
                  (g/set-property self :child child)]))))
         (g/add-node
           (g/construct-shell ShellTestNode
             {:_node-id prerequisite}
             (fn [self _evaluation-context]
               (g/set-property self :value 17))))])
      (let [first-context (g/make-evaluation-context)
            child (g/node-value parent :child first-context)]
        (is (= 17 (g/node-value prerequisite :result)))
        (let [second-context (g/make-evaluation-context)]
          (is (= child (g/node-value parent :child second-context)))
          (is (= 1 @calls))
          (g/update-system-from-evaluation-context! second-context)
          (g/update-system-from-evaluation-context! first-context)
          (is (= 17 (g/node-value child :result))))))))

;; Setters must not start undeclared loads while a resource's changes are collected.
(deftest materialization-rejects-undeclared-setter-load-test
  (with-clean-system
    (let [[parent prerequisite consumer] (g/take-node-ids 3)]
      (g/transact
        [(g/add-node
           (g/construct-shell ShellTestNode {:_node-id parent}
             (fn [_self _evaluation-context]
               [(g/add-node (g/construct ShellConsumer :_node-id consumer))
                (g/set-property consumer :source prerequisite)])))
         (g/add-node
           (g/construct-shell ShellTestNode {:_node-id prerequisite}
             (fn [self _evaluation-context]
               (g/set-property self :value 11))))])
      (let [evaluation-context (g/make-evaluation-context)]
        (let [error (try
                      (g/materialize-node! parent evaluation-context)
                      (catch Exception exception exception))]
          (is (= :undeclared-materialization (-> error ex-cause ex-data :ex-type))))
        (is (nil? (g/node-by-id @(:basis-ref evaluation-context) consumer)))
        (is (in/shell-node? (g/node-by-id @(:basis-ref evaluation-context) prerequisite)))))))

;; Independent contexts on the same thread must not inherit another load's
;; owner, while the caller's context remains usable after materialization.
(deftest materialization-state-is-local-to-evaluation-context-test
  (with-clean-system
    (let [[parent independent] (g/take-node-ids 2)
          independent-context (atom nil)]
      (g/transact
        [(g/add-node
           (g/construct-shell ShellTestNode {:_node-id parent}
             (fn materialize-parent [self _evaluation-context]
               (g/set-property self :value (g/node-value independent :result @independent-context)))))
         (g/add-node
           (g/construct-shell ShellTestNode {:_node-id independent}
             (fn materialize-independent [self _evaluation-context]
               (g/set-property self :value 11))))])
      (reset! independent-context (g/make-evaluation-context))
      (let [evaluation-context (g/make-evaluation-context)]
        (is (= 11 (g/node-value parent :result evaluation-context)))
        (is (in/shell-node? (g/node-by-id @(:basis-ref evaluation-context) independent)))
        (is (in/shell-node? (g/node-by-id @(:basis-ref @independent-context) parent)))
        (is (= 11 (g/node-value independent :result evaluation-context)))))))

;; Expansion callbacks, including deferred step producers, must retain the
;; materialization owner through transaction and evaluation context conversion.
(deftest materialization-rejects-undeclared-expansion-load-test
  (doseq [deferred [false true]]
    (testing (str "Deferred transaction step production: " deferred)
      (with-clean-system
        (let [[parent prerequisite] (g/take-node-ids 2)]
          (g/transact
            [(g/add-node
               (g/construct-shell ShellTestNode {:_node-id parent}
                 (fn materialize-parent [self _evaluation-context]
                   (g/expand-ec
                     (fn expand-parent [evaluation-context]
                       (let [steps (eduction
                                     (map (fn load-prerequisite [prerequisite]
                                            (g/set-property self :value (g/node-value prerequisite :result evaluation-context))))
                                     [prerequisite])]
                         (if deferred steps (into [] steps))))))))
             (g/add-node
               (g/construct-shell ShellTestNode {:_node-id prerequisite}
                 (fn materialize-prerequisite [self _evaluation-context]
                   (g/set-property self :value 11))))])
          (let [evaluation-context (g/make-evaluation-context)
                error (try
                        (g/materialize-node! parent evaluation-context)
                        (catch ExceptionInfo exception exception))]
            (is (= {:ex-type :undeclared-materialization
                    :node-id parent
                    :prerequisite-node-id prerequisite}
                   (ex-data error)))
            (is (in/shell-node? (g/node-by-id @(:basis-ref evaluation-context) parent)))
            (is (in/shell-node? (g/node-by-id @(:basis-ref evaluation-context) prerequisite)))
            (is (= 11 (g/node-value prerequisite :result evaluation-context)))))))))

(g/defnode MaterializationCollector
  (input values g/Int :array)
  (output total g/Int :cached (g/fnk [values] (reduce + 0 values))))

;; Sharing one materialization must not make an older evaluation-context's
;; cached total valid after another evaluation-context has also loaded a second
;; contributor.
(deftest shared-materialization-subset-does-not-overwrite-cache-test
  (with-clean-system
    (let [[a b collector] (g/take-node-ids 3)]
      (g/transact
        [(g/add-node (g/construct MaterializationCollector :_node-id collector))
         (mapv (fn [node-id value]
                 (g/add-node
                   (g/construct-shell ShellTestNode {:_node-id node-id}
                     (fn materialize-contributor [self _evaluation-context]
                       [(g/set-property self :value value)
                        (g/connect self :result collector :values)]))))
               [a b]
               [7 11])])
      (let [older-evaluation-context (g/make-evaluation-context)
            newer-evaluation-context (g/make-evaluation-context)]
        (g/materialize-node! a older-evaluation-context)
        (is (= 7 (g/node-value collector :total older-evaluation-context)))
        (g/materialize-node! a newer-evaluation-context)
        (g/materialize-node! b newer-evaluation-context)
        (is (= 18 (g/node-value collector :total newer-evaluation-context)))
        (g/update-system-from-evaluation-context! newer-evaluation-context)
        (is (= 18 (g/node-value collector :total)))
        (g/update-system-from-evaluation-context! older-evaluation-context)
        (is (= 18 (g/node-value collector :total)))
        (g/clear-system-cache!)
        (is (= 18 (g/node-value collector :total)))))))

;; A shared materialization committed after a user edit must not hide that
;; edit's invalidations when an older evaluation-context subsequently
;; contributes cached values.
(deftest shared-materialization-preserves-cache-after-intervening-edit-test
  (with-clean-system
    (let [consumer (g/make-node! ShellTestNode :value 3)
          [shell] (g/take-node-ids 1)]
      (g/transact
        (g/add-node
          (g/construct-shell ShellTestNode {:_node-id shell}
            (fn materialize-contributor [self _evaluation-context]
              [(g/set-property self :value 7)
               (g/connect self :result consumer :dependency)]))))
      (let [older-evaluation-context (g/make-evaluation-context)]
        (g/materialize-node! shell older-evaluation-context)
        (is (= 10 (g/node-value consumer :result older-evaluation-context)))
        (g/transact
          (g/set-property consumer :value 13))
        (let [newer-evaluation-context (g/make-evaluation-context)]
          (g/materialize-node! shell newer-evaluation-context)
          (is (= 20 (g/node-value consumer :result newer-evaluation-context)))
          (g/update-system-from-evaluation-context! newer-evaluation-context))
        (is (= 20 (g/node-value consumer :result)))
        (g/update-system-from-evaluation-context! older-evaluation-context)
        (is (= 20 (g/node-value consumer :result)))
        (g/clear-system-cache!)
        (is (= 20 (g/node-value consumer :result)))))))

;; Replay preserves contributions realized against different bases.
;; Materialization invalidates cache snapshots and local totals, including
;; through derived contexts, without affecting independent contexts.
(deftest independent-materializations-preserve-array-connections-test
  (doseq [cache-size [-1 0 10]
          custom-context [false true]]
    (with-clean-system {:cache-size cache-size}
      (let [[a b collector] (g/take-node-ids 3)
            calls (atom [])]
        (g/transact
          [(g/add-node (g/construct MaterializationCollector :_node-id collector))
           (mapv (fn [node-id value]
                   (g/add-node
                     (g/construct-shell ShellTestNode {:_node-id node-id}
                       (fn materialize-fn [self _evaluation-context]
                         (swap! calls conj self)
                         [(g/set-property self :value value)
                          (g/connect self :result collector :values)]))))
                 [a b] [7 11])])
        (is (= 0 (g/node-value collector :total)))
        (let [first-context (if custom-context
                              (g/make-evaluation-context {})
                              (g/make-evaluation-context))
              second-context (g/make-evaluation-context)
              derived-context (g/pruned-evaluation-context first-context (constantly true))]
          (g/materialize-node! a derived-context)
          (is (= 7 (g/tx-cached-node-value! collector :total first-context)))
          (is (= 0 (g/node-value collector :total second-context)))
          (is (= 0 (g/node-value collector :total)))
          (g/materialize-node! b second-context)
          (is (= 11 (g/node-value collector :total second-context)))
          (g/materialize-node! b first-context)
          (is (= 18 (g/tx-cached-node-value! collector :total first-context)))
          (g/materialize-node! a second-context)
          (is (= 18 (g/node-value collector :total second-context)))
          (g/update-system-from-evaluation-context! first-context)
          (g/update-system-from-evaluation-context! second-context)
          (is (= 18 (g/node-value collector :total)))
          (is (= #{a b} (set @calls)))
          (is (= 2 (count @calls))))))))

;; Replaying independent overrides must allocate distinct slots on their shared original.
(deftest independent-materializations-preserve-overrides-test
  (with-clean-system
    (let [original (g/make-node! ShellTestNode :value 5)
          [a b] (g/take-node-ids 2)]
      (g/transact
        (mapv (fn [node-id]
                (g/add-node
                  (g/construct-shell ShellWithChild {:_node-id node-id}
                    (fn [self _evaluation-context]
                      (g/override original {}
                        (fn [_evaluation-context original->override]
                          (g/set-property self :child (original->override original))))))))
              [a b]))
      (let [first-context (g/make-evaluation-context)
            second-context (g/make-evaluation-context)
            first-child (g/node-value a :child first-context)
            second-child (g/node-value b :child second-context)]
        (is (= second-child (g/node-value b :child first-context)))
        (g/update-system-from-evaluation-context! first-context)
        (g/update-system-from-evaluation-context! second-context)
        (is (= #{first-child second-child} (set (g/overrides original))))))))

(defn- prerequisite-test-loader [prerequisite-ids entered]
  (fn [self evaluation-context]
    (when entered
      (.countDown ^CountDownLatch entered)
      (when-not (.await ^CountDownLatch entered 10 TimeUnit/SECONDS)
        (throw (ex-info "Timed out starting concurrent prerequisite loads." {}))))
    (try
      (g/materialize-prerequisites! self prerequisite-ids evaluation-context)
      (g/set-property self :value 1)
      (catch ExceptionInfo error
        (if-not (contains? #{:prerequisite-cycle :prerequisite-failed} (:ex-type (ex-data error)))
          (throw error)
          (g/mark-defective self ShellTestNode
                            (g/->error self nil :fatal nil (ex-message error) (ex-data error))))))))

;; Both recursion and cross-thread waits must turn cycles into replayable errors.
(deftest prerequisite-cycles-settle-promises-test
  (doseq [node-count [1 2 3]
          concurrent [false true]]
    (with-clean-system
      (let [node-ids (vec (g/take-node-ids node-count))
            entered (when concurrent (CountDownLatch. node-count))
            prerequisites (zipmap node-ids (conj (subvec node-ids 1) (first node-ids)))]
        (g/transact
          (mapv (fn [node-id]
                  (g/add-node
                    (g/construct-shell ShellTestNode {:_node-id node-id}
                      (prerequisite-test-loader #{(prerequisites node-id)} entered))))
                node-ids))
        (let [contexts (mapv (fn [_] (g/make-evaluation-context)) node-ids)
              results (when concurrent
                        (mapv (fn [node-id context]
                                (future (g/node-value node-id :result context)))
                              node-ids contexts))]
          (try
            (doseq [[index node-id] (map-indexed vector node-ids)]
              (is (g/error-value? (if concurrent
                                    (deref (results index) 10000 ::timeout)
                                    (g/node-value node-id :result (contexts index))))))
            (doseq [[_node-id entry] @(:shared-materialization-registry-atom (first contexts))]
              (is (realized? (:promise entry)))
              (is (= #{} (:prerequisite-shell-node-ids entry))))
            (doseq [context contexts]
              (g/update-system-from-evaluation-context! context))
            (doseq [node-id node-ids]
              (is (g/error-value? (g/node-value node-id :result))))
            (finally
              (doseq [result results]
                (future-cancel result)))))))))

;; A diamond is not a cycle, and a parent's own delta must exclude prerequisite deltas.
(deftest prerequisite-diamond-keeps-changes-separate-test
  (with-clean-system
    (let [[root a b leaf] (g/take-node-ids 4)
          prerequisites {root #{a b} a #{leaf} b #{leaf} leaf #{}}]
      (g/transact
        (mapv (fn [[node-id prerequisite-ids]]
                (g/add-node
                  (g/construct-shell ShellTestNode {:_node-id node-id}
                    (prerequisite-test-loader prerequisite-ids nil))))
              prerequisites))
      (let [evaluation-context (g/make-evaluation-context)]
        (is (= 1 (g/node-value root :result evaluation-context)))
        (doseq [[node-id prerequisite-ids] prerequisites
                :let [info (get @(:materialization-results-atom evaluation-context) node-id)]]
          (is (= prerequisite-ids (:prerequisite-shell-node-ids info)))
          (is (= prerequisite-ids (set (coll/keys (:dependencies info)))))
          (is (coll/not-any? #(instance? internal.transaction.MaterializationTXC %) (:changes info))))
        (g/update-system-from-evaluation-context! evaluation-context)
        (is (= 1 (g/node-value root :result)))))))

;; Exceptions in either phase must settle the shared promise and permit an explicit retry.
(deftest materialization-failure-settles-shared-promise-test
  (doseq [failure-phase [:prepare :realize]]
    (with-clean-system
      (let [shell (first (g/take-node-ids 1))
            entered (promise)
            release (promise)
            fail (atom true)
            failure (ex-info "Loader failed." {})
            fail-fn (fn []
                      (when @fail
                        (deliver entered true)
                        (when (= ::timeout (deref release 10000 ::timeout))
                          (throw (ex-info "Timed out releasing failing loader." {})))
                        (throw failure)))]
        (g/transact
          (g/add-node
            (g/construct-shell ShellTestNode {:_node-id shell}
              (fn [self _evaluation-context]
                (when (= :prepare failure-phase)
                  (fail-fn))
                [(g/set-property self :value 7)
                 (when (= :realize failure-phase)
                   (g/callback fail-fn))]))))
        (let [evaluation-context (g/make-evaluation-context)
              result (future
                       (try (g/node-value shell :result evaluation-context)
                            (catch Exception exception exception)))]
          (try
            (is (= true (deref entered 10000 ::timeout)))
            (let [result-promise (get-in @(:shared-materialization-registry-atom evaluation-context) [shell :promise])
                  waiter (future @result-promise)]
              (deliver release true)
              (is (identical? failure (deref result 10000 ::timeout)))
              (is (identical? failure (:error (deref waiter 10000 ::timeout)))))
            (is (in/shell-node? (g/node-by-id @(:basis-ref evaluation-context) shell)))
            (reset! fail false)
            (is (= 7 (g/node-value shell :result evaluation-context)))
            (finally
              (deliver release true)
              (future-cancel result))))))))

;; Finishing an obsolete shell must neither overwrite nor evict the replacement's result.
(deftest replaced-shell-retains-new-materialization-test
  (doseq [old-load-fails [false true]]
    (with-clean-system
      (let [node-id (first (g/take-node-ids 1))
            entered (promise)
            release (promise)]
        (g/transact
          (g/add-node
            (g/construct-shell ShellTestNode {:_node-id node-id}
              (fn [self _evaluation-context]
                (deliver entered true)
                (when (= ::timeout (deref release 10000 ::timeout))
                  (throw (ex-info "Timed out releasing obsolete loader." {})))
                (when old-load-fails
                  (throw (ex-info "Obsolete loader failed." {})))
                (g/set-property self :value 1)))))
        (let [old-context (g/make-evaluation-context)
              old-result (future
                           (try (g/node-value node-id :result old-context)
                                (catch Exception exception exception)))]
          (try
            (is (= true (deref entered 10000 ::timeout)))
            (g/transact
              (g/add-node
                (g/construct-shell ShellTestNode {:_node-id node-id}
                  (fn [self _evaluation-context]
                    (g/set-property self :value 2)))))
            (is (= 2 (g/node-value node-id :result)))
            (let [new-promise (get-in @(:shared-materialization-registry-atom old-context) [node-id :promise])]
              (deliver release true)
              (let [result (deref old-result 10000 ::timeout)]
                (if old-load-fails
                  (is (instance? ExceptionInfo result))
                  (is (= 1 result))))
              (g/update-system-from-evaluation-context! old-context)
              (is (= 2 (g/node-value node-id :result)))
              (is (identical? new-promise (get-in @(:shared-materialization-registry-atom old-context) [node-id :promise]))))
            (finally
              (deliver release true)
              (future-cancel old-result))))))))

;; Loaders may initialize new nodes, but cannot remove or replace pre-existing graph state.
(deftest materialization-rejects-destructive-steps-test
  (doseq [action [:delete :disconnect :replace :graph-value :transfer-overrides]]
    (with-clean-system
      (let [[shell source target] (g/take-node-ids 3)]
        (g/transact
          [(g/add-node (g/construct ShellTestNode :_node-id source :value 5))
           (g/add-node (g/construct ShellTestNode :_node-id target))
           (g/connect source :result target :dependency)
           (g/add-node
             (g/construct-shell ShellTestNode {:_node-id shell}
               (fn [_self _evaluation-context]
                 (case action
                   :delete (g/delete-node source)
                   :disconnect (g/disconnect source :result target :dependency)
                   :replace (g/add-node (g/construct ShellTestNode :_node-id source :value 9))
                   :graph-value (g/set-graph-value :example 9)
                   :transfer-overrides (g/transfer-overrides {source target})))))])
        (is (thrown-with-msg? ExceptionInfo #"Materialization cannot" (g/node-value shell :result)))
        (is (= 5 (g/node-value target :result)))
        (is (nil? (g/graph-value :example)))))))

;; Clearing an existing override must obey the same isolation rule as assigning a property.
(deftest materialization-rejects-clearing-external-property-test
  (with-clean-system
    (let [[shell original] (g/take-node-ids 2)
          _ (g/transact (g/add-node (g/construct ShellTestNode :_node-id original :value 5)))
          overridden (first (g/tx-nodes-added (g/transact (g/override original {}))))]
      (g/transact
        [(g/set-property overridden :value 9)
         (g/add-node
           (g/construct-shell ShellTestNode {:_node-id shell}
             (fn [_self _evaluation-context]
               (g/clear-property overridden :value))))])
      (is (thrown-with-msg? ExceptionInfo #"Materialization cannot clear" (g/node-value shell :result)))
      (is (= 9 (g/node-value overridden :value))))))
