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
            [support.test-support :refer [with-clean-system]])
  (:import [clojure.lang ExceptionInfo]))

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

(deftest deferred-materialization-test
  (with-clean-system
    (let [[node-id child-id] (g/take-node-ids 2)
          calls (atom 0)]
      (g/transact
        (g/add-node
          (g/construct-shell ShellTestNode
            (fn [self _evaluation-context]
              (swap! calls inc)
              [(g/set-property self :value 7)
               (g/add-node (g/construct ShellTestNode :_node-id child-id :value 5))
               (g/connect child-id :result self :dependency)])
            {:_node-id node-id
             :identity "shell"})))
      (let [evaluation-context (g/make-evaluation-context)
            shell (g/node-by-id (g/ec-basis evaluation-context) node-id)]
        (is (= "shell" (g/node-value node-id :identity evaluation-context)))
        (is (zero? @calls))
        (is (thrown? ExceptionInfo (gt/get-property shell (g/ec-basis evaluation-context) :value)))
        (is (thrown? ExceptionInfo (gt/assigned-properties shell)))
        (is (= 12 (g/node-value node-id :result evaluation-context)))
        (is (= 12 (g/node-value node-id :result evaluation-context)))
        (is (= 1 @calls))
        (is (in/unmaterialized-shell-node? (g/node-by-id (g/now) node-id)))
        (is (nil? (g/node-by-id (g/now) child-id)))
        (is (not (in/unmaterialized-shell-node? (g/node-by-id (g/ec-basis evaluation-context) node-id))))
        (g/update-system-from-evaluation-context! evaluation-context)
        (is (not (in/unmaterialized-shell-node? (g/node-by-id (g/now) node-id))))
        (is (= 12 (g/node-value node-id :result)))
        (g/update-system-from-evaluation-context! evaluation-context)
        (is (= 1 @calls))))))

(deftest materialization-in-property-setter-test
  (with-clean-system
    (let [[source-id consumer-id] (g/take-node-ids 2)

          materialize-fn
          (fn materialize-fn [self _evaluation-context]
            (g/set-property self :value 42))]

      (g/transact
        (concat
          (g/add-node (g/construct-shell ShellTestNode materialize-fn {:_node-id source-id}))
          (g/add-node (g/construct ShellConsumer :_node-id consumer-id))))
      (g/transact
        (g/set-property consumer-id :source source-id))
      (is (= 42 (g/node-value consumer-id :value)))
      (is (= 42 (g/node-value source-id :result)))
      (is (not (in/unmaterialized-shell-node? (g/node-by-id (g/now) source-id)))))))

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
          (g/add-node (g/construct-shell ShellTestNode materialize-fn {:_node-id parent}))
          (g/add-node (g/construct-shell ShellTestNode materialize-fn {:_node-id child}))
          (g/add-node (g/construct-shell ShellTestNode materialize-fn {:_node-id unrelated}))
          (g/connect child :result parent :dependency)))
      (is (= 20 (g/node-value parent :result)))
      (is (= [parent child] @loaded))
      (is (in/unmaterialized-shell-node? (g/node-by-id (g/now) unrelated))))))

(deftest failed-materialization-can-be-retried-test
  (with-clean-system
    (let [node-id (first (g/take-node-ids 1))
          fail (atom true)]
      (g/transact
        (g/add-node
          (g/construct-shell ShellTestNode
            (fn materialize-fn [self evaluation-context]
              (g/merge-evaluation-user-data! evaluation-context {self {:source-value :staged}})
              [(g/set-property self :value 9)
               (g/callback #(when @fail (throw (ex-info "load failed" {}))))])
            {:_node-id node-id})))
      (let [evaluation-context (g/make-evaluation-context)]
        (is (thrown-with-msg? ExceptionInfo #"load failed" (g/node-value node-id :result evaluation-context)))
        (is (in/unmaterialized-shell-node? (g/node-by-id (g/ec-basis evaluation-context) node-id)))
        (g/update-system-from-evaluation-context! evaluation-context)
        (is (nil? (g/user-data node-id :source-value)))
        (reset! fail false)
        (is (= 9 (g/node-value node-id :result evaluation-context)))
        (g/update-system-from-evaluation-context! evaluation-context)
        (is (= :staged (g/user-data node-id :source-value)))))))

(deftest materialization-replays-non-undoable-changes-test
  (with-clean-system
    (let [node-id (first (g/take-node-ids 1))]
      (g/transact
        (g/add-node
          (g/construct-shell ShellTestNode
            (fn [self _evaluation-context]
              (g/non-undoable (g/set-property self :value 13)))
            {:_node-id node-id})))
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
          (g/construct-shell ShellTestNode materialize-fn {:_node-id node-id})))
      (let [evaluation-context (g/make-evaluation-context)]
        (is (= 1 (g/node-value node-id :result evaluation-context)))
        (g/transact (g/delete-node node-id))
        (g/update-system-from-evaluation-context! evaluation-context)
        (is (nil? (g/node-by-id (g/now) node-id)))))))

(deftest evaluation-context-can-commit-more-than-once-test
  (with-clean-system
    (let [[a b] (g/take-node-ids 2)
          materialize-fn (fn [self _evaluation-context] (g/set-property self :value 17))]
      (g/transact
        (concat
          (g/add-node (g/construct-shell ShellTestNode materialize-fn {:_node-id a}))
          (g/add-node (g/construct-shell ShellTestNode materialize-fn {:_node-id b}))))
      (let [evaluation-context (g/make-evaluation-context)]
        (doseq [node-id [a b]]
          (is (= 17 (g/node-value node-id :result evaluation-context)))
          (g/update-system-from-evaluation-context! evaluation-context)
          (is (not (in/unmaterialized-shell-node? (g/node-by-id (g/now) node-id)))))))))

(g/defnode SharedMaterializationState
  (property values g/Any (default {})))

(deftest concurrent-materialization-preserves-shared-state-test
  (with-clean-system
    (let [[a b shared] (g/take-node-ids 3)

          materialize-fn
          (fn materialize-fn [self _evaluation-context]
            [(g/set-property self :value 1)
             (g/update-property shared :values assoc self :loaded)])]

      (g/transact
        (concat
          (g/add-node (g/construct SharedMaterializationState :_node-id shared))
          (g/add-node (g/construct-shell ShellTestNode materialize-fn {:_node-id a}))
          (g/add-node (g/construct-shell ShellTestNode materialize-fn {:_node-id b}))))
      (let [first-context (g/make-evaluation-context)
            second-context (g/make-evaluation-context)]
        (g/node-value a :result first-context)
        (g/node-value b :result second-context)
        (g/update-system-from-evaluation-context! first-context)
        (g/update-system-from-evaluation-context! second-context)
        (is (= {a :loaded} (g/node-value shared :values)))
        (is (in/unmaterialized-shell-node? (g/node-by-id (g/now) b)))
        (is (= 1 (g/node-value b :result)))
        (is (= {a :loaded
                b :loaded} (g/node-value shared :values)))))))

(deftest tracing-materializes-with-real-values-test
  (with-clean-system
    (let [[parent child] (g/take-node-ids 2)]
      (g/transact
        (concat
          (g/add-node
            (g/construct-shell ShellTestNode
              (fn materialize-fn [self evaluation-context]
                (g/set-property self :value (g/node-value child :result evaluation-context)))
              {:_node-id parent}))
          (g/add-node
            (g/construct-shell ShellTestNode
              (fn materialize-fn [self _evaluation-context]
                (g/set-property self :value 23))
              {:_node-id child}))))
      (let [evaluation-context (g/make-evaluation-context)]
        (g/node-value parent :result (assoc evaluation-context :dry-run true))
        (is (= 23 (g/node-value parent :result evaluation-context)))
        (g/update-system-from-evaluation-context! evaluation-context)
        (is (= 23 (g/node-value parent :result)))
        (is (= 23 (g/node-value child :result)))))))

(deftest editing-unjammable-property-preserves-shell-test
  (with-clean-system
    (let [node-id (first (g/take-node-ids 1))
          calls (atom 0)]
      (g/transact
        (g/add-node
          (g/construct-shell ShellTestNode
            (fn materialize-fn [self _evaluation-context]
              (swap! calls inc)
              (g/set-property self :value 7))
            {:_node-id node-id
             :identity "before"})))
      (g/transact (g/set-property node-id :identity "after"))
      (is (= "after" (g/node-value node-id :identity)))
      (is (zero? @calls))
      (is (in/unmaterialized-shell-node? (g/node-by-id (g/now) node-id)))
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
              (fn materialize-fn [self _evaluation-context]
                (g/set-property self :value 31))
              {:_node-id source}))
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
            (fn materialize-fn [self _evaluation-context]
              [(g/add-node (g/construct ShellTestNode :_node-id child :value 19))
               (g/connect child :result self :dependency)])
            {:_node-id source})))
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
