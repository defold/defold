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

(ns internal.txsteps.expand-test
  (:require [clojure.test :refer [deftest is testing]]
            [dynamo.graph :as g]
            [internal.node :as in]
            [internal.txsteps.helpers :as helpers]
            [support.test-support :as test-support]))

(set! *warn-on-reflection* true)

;; Expanded transaction steps participate in undo and redo.
(deftest expand-test
  (test-support/with-clean-system
    (let [node-id (g/make-node! helpers/PropertyTestNode)]

      (testing "Transact."
        (g/transact
          (g/expand nil
            (fn expanded-tx-steps [node-id property-label value]
              (g/set-property node-id property-label value))
            node-id
            :basic-property
            :basic-property-value))
        (is (= :basic-property-value
               (g/node-value node-id :basic-property)
               (g/node-value node-id :basic-output))))

      (testing "Undo."
        (g/undo! :undo/global)
        (is (= nil
               (g/node-value node-id :basic-property)
               (g/node-value node-id :basic-output))))

      (testing "Redo."
        (g/redo! :undo/global)
        (is (= :basic-property-value
               (g/node-value node-id :basic-property)
               (g/node-value node-id :basic-output)))))))

;; Nested expansions apply all returned steps and participate in undo and redo.
(deftest expand-within-expanded-tx-steps-test
  (test-support/with-clean-system
    (let [node-id (g/make-node! helpers/PropertyTestNode)]

      (testing "Transact."
        (g/transact
          (g/expand nil
            (fn expanded-tx-steps [node-id]
              [(g/set-property node-id :basic-property :basic-property-value)
               (g/expand nil
                 (fn nested-expanded-tx-steps [node-id]
                   (g/set-property node-id :effecting-property :effecting-property-value))
                 node-id)])
            node-id))
        (is (= :basic-property-value
               (g/node-value node-id :basic-property)
               (g/node-value node-id :basic-output)))
        (is (= :effecting-property-value
               (g/node-value node-id :effecting-property)
               (g/node-value node-id :effecting-output))))

      (testing "Undo."
        (g/undo! :undo/global)
        (is (= nil
               (g/node-value node-id :basic-property)
               (g/node-value node-id :basic-output)))
        (is (= nil
               (g/node-value node-id :effecting-property)
               (g/node-value node-id :effecting-output))))

      (testing "Redo."
        (g/redo! :undo/global)
        (is (= :basic-property-value
               (g/node-value node-id :basic-property)
               (g/node-value node-id :basic-output)))
        (is (= :effecting-property-value
               (g/node-value node-id :effecting-property)
               (g/node-value node-id :effecting-output)))))))

(g/defnode ExpandTestNode
  (property identity g/Str :unjammable)
  (property value g/Int (default 0))
  (input dependency g/Int :cascade-delete)
  (output result g/Int :cached (g/fnk [value dependency] (+ value (or dependency 0)))))

;; Scoped recovery preserves earlier steps and rolls back failed graph changes,
;; non-undoable edits, undo records and changes to :tx-data-context-atom.
(deftest expand-recovers-from-original-state-test
  (doseq [fail [false true]
          inject-evaluation-context [false true]]
    (test-support/with-clean-system
      (let [node-id (g/make-node! ExpandTestNode :identity "original" :value 1)]
        (g/reset-undo! :undo/global)
        (g/transact
          [(g/set-property node-id :identity "before")
           (g/callback-ec
             (fn initialize-tx-data-context [{:keys [tx-data-context-atom]}]
               (swap! tx-data-context-atom assoc ::recovery :before)))
           (g/expand
             {:inject-evaluation-context inject-evaluation-context
              :catch-fn
              (fn catch-fn [exception]
                (is fail)
                (is (= "Failed transaction steps." (ex-message exception)))
                [(g/callback-ec
                   (fn check-recovery-state
                     [{:keys [tx-data-context-atom] :as evaluation-context}]
                     (is (= :before (::recovery @tx-data-context-atom)))
                     (is (= "before" (g/node-value node-id :identity evaluation-context)))
                     (is (= 1 (g/node-value node-id :result evaluation-context)))))
                 (g/set-property node-id :value 2)])}
             (fn expanded-steps [& _args]
               [(g/non-undoable (g/set-property node-id :identity "attempted"))
                (g/set-property node-id :value 9)
                (g/callback-ec
                  (fn update-tx-data-context [{:keys [tx-data-context-atom]}]
                    (swap! tx-data-context-atom assoc ::recovery :attempted)))
                (g/expand nil
                  (fn finish-attempt []
                    (when fail
                      (throw (ex-info "Failed transaction steps." {})))))]))
           (g/callback-ec
             (fn check-final-tx-data-context [{:keys [tx-data-context-atom]}]
               (is (= (if fail :before :attempted)
                      (::recovery @tx-data-context-atom)))))
           (g/set-property node-id :identity "after")])
        (is (= "after" (g/node-value node-id :identity)))
        (is (= (if fail 2 9) (g/node-value node-id :result)))
        (g/undo! :undo/global)
        (is (= "original" (g/node-value node-id :identity)))
        (is (= 1 (g/node-value node-id :result)))
        (g/redo! :undo/global)
        (is (= "after" (g/node-value node-id :identity)))
        (is (= (if fail 2 9) (g/node-value node-id :result)))))))

;; Expansion handlers also recover from step-generation failures, discarding
;; materializations and changes to :tx-data-context-atom made through the
;; injected evaluation context.
(deftest expand-recovers-from-generation-failures-test
  (doseq [inject-evaluation-context [false true]]
    (test-support/with-clean-system
      (let [node-id (g/make-node! ExpandTestNode :value 1)
            shell (first (g/take-node-ids 1))
            failure (ex-info "Failed to generate steps." {})]
        (g/transact
          (g/add-node
            (g/construct-shell ExpandTestNode {:_node-id shell}
              (fn materialize-fn [self _evaluation-context]
                (g/set-property self :value 9)))))
        (g/transact
          [(g/callback-ec
             (fn initialize-tx-data-context [{:keys [tx-data-context-atom]}]
               (swap! tx-data-context-atom assoc ::recovery :before)))
           (apply g/expand
                  {:inject-evaluation-context inject-evaluation-context
                   :catch-fn
                   (fn catch-fn [exception]
                     (is (identical? failure exception))
                     [(g/callback-ec
                        (fn check-recovery-state
                          [{:keys [basis-ref tx-data-context-atom]}]
                          (is (= :before (::recovery @tx-data-context-atom)))
                          (is (in/shell-node? (g/node-by-id @basis-ref shell)))))
                      (g/set-property node-id :value 2)])}
                  (fn expanded-steps [evaluation-context target-id]
                    (is (= node-id target-id))
                    (is (= inject-evaluation-context (some? evaluation-context)))
                    (when evaluation-context
                      (swap! (:tx-data-context-atom evaluation-context)
                             assoc ::recovery :attempted)
                      (is (= 9 (g/node-value shell :result evaluation-context))))
                    (throw failure))
                  (if inject-evaluation-context
                    [node-id]
                    [nil node-id]))])
        (is (= 2 (g/node-value node-id :result)))
        (is (in/shell-node? (g/node-by-id (g/now) shell)))))))

;; Fatal errors remain transaction failures instead of being converted into
;; recoverable results by a scoped exception handler.
(deftest expand-propagates-errors-test
  (test-support/with-clean-system
    (let [node-id (g/make-node! ExpandTestNode :value 1)]
      (is (thrown-with-msg?
            AssertionError
            #"Fatal load error"
            (g/transact
              (g/expand
                {:catch-fn
                 (fn catch-fn [_exception]
                   (is false "An Error must not reach catch-fn."))}
                (fn expanded-steps []
                  [(g/set-property node-id :value 9)
                   (g/callback
                     (fn fail []
                       (throw (AssertionError. "Fatal load error"))))])))))
      (is (= 1 (g/node-value node-id :result))))))

;; Expanded transaction steps must execute in order so later steps can use node
;; ids allocated by earlier steps, even when the steps are produced lazily.
(deftest expanded-transaction-steps-retain-execution-order-test
  (test-support/with-clean-system
    (let [parent (g/make-node! ExpandTestNode)
          child (atom nil)]
      (g/transact
        (g/expand-ec
          (fn expanded-steps [_evaluation-context]
            (eduction
              (map (fn [step]
                     (case step
                       :create
                       (g/expand nil
                         (fn create-child []
                           (let [node-id (first (g/take-node-ids 1))]
                             (reset! child node-id)
                             (g/add-node
                               (g/construct ExpandTestNode :_node-id node-id :value 37)))))

                       :connect
                       (g/connect @child :result parent :dependency))))
              [:create :connect]))))
      (is (= 37 (g/node-value parent :result))))))
