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

(ns internal.transaction-step-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [internal.graph.types :as gt]
            [support.test-support :as test-support]
            [util.fn :as fn])
  (:import [clojure.lang ExceptionInfo]))

(set! *warn-on-reflection* true)

;; -----------------------------------------------------------------------------
;; delete-node
;; -----------------------------------------------------------------------------

(g/defnode DeleteNodeTestOwnedNode)

(g/defnode DeleteNodeTestOwnerNode
  (input regular-cascade-delete-input g/Any :cascade-delete)
  (output regular-cascade-delete-output g/Any :cached
          (g/fnk [regular-cascade-delete-input] regular-cascade-delete-input))

  (input array-cascade-delete-input g/Any :array :cascade-delete)
  (output array-cascade-delete-output g/Any :cached
          (g/fnk [array-cascade-delete-input] array-cascade-delete-input)))

;; -----------------------------------------------------------------------------
;; set-property
;; -----------------------------------------------------------------------------

(def ^:dynamic property-test-node-setter-check-fn fn/constantly-nil)

(g/defnode PropertyTestNode
  (property basic-property g/Keyword)
  (output basic-output g/Keyword :cached
          (g/fnk [basic-property] basic-property))

  (property effecting-property g/Keyword
            (set (fn [evaluation-context self old-value new-value]
                   (property-test-node-setter-check-fn evaluation-context self old-value new-value)
                   (let [log-entry {:old-value old-value
                                    :new-value new-value
                                    :new-output (g/node-value self :effecting-output evaluation-context)}]
                     (g/update-property self :effects-log-property conj log-entry)))))
  (output effecting-output g/Keyword :cached
          (g/fnk [effecting-property] effecting-property))

  (property effects-log-property g/Any (default []))
  (output effects-log-output g/Any :cached
          (g/fnk [effects-log-property] effects-log-property)))

(deftest set-property-effects-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph! :history true)
          original-node-id (first (g/take-node-ids graph-id 1))

          override-node-id
          (second
            (g/tx-nodes-added
              (g/transact
                (concat
                  (g/add-node (g/construct PropertyTestNode :_node-id original-node-id))
                  (g/override original-node-id)))))

          ensure-original-unmodified!
          (fn ensure-original-unmodified! []
            (testing "Original node is unmodified."
              (is (= nil
                     (g/node-value original-node-id :basic-property)
                     (g/node-value original-node-id :basic-output)))))

          ensure-override-unmodified!
          (fn ensure-override-unmodified! []
            (testing "Override node is unmodified."
              (is (= nil
                     (g/node-value override-node-id :basic-property)
                     (g/node-value override-node-id :basic-output)))))

          ensure-original-modified!
          (fn ensure-original-modified! []
            (testing "Original node is modified."
              (is (= :original-basic-property-value
                     (g/node-value original-node-id :basic-property)
                     (g/node-value original-node-id :basic-output)))))

          ensure-original-modified-override-unmodified!
          (fn ensure-original-modified-override-unmodified! []
            (testing "Override node is unmodified."
              (is (= :original-basic-property-value
                     (g/node-value override-node-id :basic-property)
                     (g/node-value override-node-id :basic-output)))))

          ensure-original-modified-override-modified!
          (fn ensure-override-modified! []
            (testing "Override node is modified."
              (is (= :overridden-basic-property-value
                     (g/node-value override-node-id :basic-property)
                     (g/node-value override-node-id :basic-output)))))]

      (testing "Setup successful."
        (is (= original-node-id (g/override-original override-node-id))))

      (testing "Before transact on original node."
        (ensure-original-unmodified!)
        (ensure-override-unmodified!))

      (testing "Transact on original node."
        (g/transact
          (g/set-property original-node-id :basic-property :original-basic-property-value))
        (ensure-original-modified!)
        (ensure-original-modified-override-unmodified!))

      (testing "Undo change to original node."
        (g/undo! graph-id)
        (ensure-original-unmodified!)
        (ensure-override-unmodified!))

      (testing "Redo change to original node."
        (g/redo! graph-id)
        (ensure-original-modified!)
        (ensure-original-modified-override-unmodified!))

      (testing "Transact on override node."
        (g/transact
          (g/set-property override-node-id :basic-property :overridden-basic-property-value))
        (ensure-original-modified!)
        (ensure-original-modified-override-modified!))

      (testing "Undo change to override node."
        (g/undo! graph-id)
        (ensure-original-modified!)
        (ensure-original-modified-override-unmodified!))

      (testing "Redo change to override node."
        (g/redo! graph-id)
        (ensure-original-modified!)
        (ensure-original-modified-override-modified!)))))

(deftest set-property-setter-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph! :history true)
          original-node-id (g/make-node! graph-id PropertyTestNode)

          override-props
          {:effects-log-property []} ; Don't inherit from original.

          override-node-id
          (second
            (g/tx-nodes-added
              (g/transact
                (concat
                  (g/add-node (g/construct PropertyTestNode :_node-id original-node-id))
                  (g/override original-node-id {:init-props-fn (constantly override-props)})))))

          ensure-original-unmodified!
          (fn ensure-original-unmodified! []
            (testing "Original node is unmodified."
              (is (= nil
                     (g/node-value original-node-id :basic-property)
                     (g/node-value original-node-id :basic-output)))
              (is (= nil
                     (g/node-value original-node-id :effecting-property)
                     (g/node-value original-node-id :effecting-output)))
              (is (= []
                     (g/node-value original-node-id :effects-log-property)
                     (g/node-value original-node-id :effects-log-output)))))

          ensure-override-unmodified!
          (fn ensure-override-unmodified! []
            (testing "Override node is unmodified."
              (is (= nil
                     (g/node-value override-node-id :basic-property)
                     (g/node-value override-node-id :basic-output)))
              (is (= nil
                     (g/node-value override-node-id :effecting-property)
                     (g/node-value override-node-id :effecting-output)))
              (is (= []
                     (g/node-value override-node-id :effects-log-property)
                     (g/node-value override-node-id :effects-log-output)))))

          ensure-original-modified!
          (fn ensure-original-modified! []
            (testing "Original node is modified."
              (is (= :original-basic-property-value
                     (g/node-value original-node-id :basic-property)
                     (g/node-value original-node-id :basic-output)))
              (is (= :original-effecting-property-value
                     (g/node-value original-node-id :effecting-property)
                     (g/node-value original-node-id :effecting-output)))
              (is (= [{:old-value nil
                       :new-value :original-effecting-property-value
                       :new-output :original-effecting-property-value}]
                     (g/node-value original-node-id :effects-log-property)
                     (g/node-value original-node-id :effects-log-output)))))

          ensure-original-modified-override-unmodified!
          (fn ensure-original-modified-override-unmodified! []
            (testing "Override node is unmodified."
              (is (= :original-basic-property-value
                     (g/node-value override-node-id :basic-property)
                     (g/node-value override-node-id :basic-output)))
              (is (= :original-effecting-property-value
                     (g/node-value override-node-id :effecting-property)
                     (g/node-value override-node-id :effecting-output)))
              (is (= []
                     (g/node-value override-node-id :effects-log-property)
                     (g/node-value override-node-id :effects-log-output)))))

          ensure-original-modified-override-modified!
          (fn ensure-override-modified! []
            (testing "Override node is modified."
              (is (= :original-basic-property-value
                     (g/node-value override-node-id :basic-property)
                     (g/node-value override-node-id :basic-output)))
              (is (= :overridden-effecting-property-value
                     (g/node-value override-node-id :effecting-property)
                     (g/node-value override-node-id :effecting-output)))
              (is (= [{:old-value :original-effecting-property-value
                       :new-value :overridden-effecting-property-value
                       :new-output :overridden-effecting-property-value}]
                     (g/node-value override-node-id :effects-log-property)
                     (g/node-value override-node-id :effects-log-output)))))]

      (testing "Setup successful."
        (is (= original-node-id (g/override-original override-node-id))))

      (testing "Before transact on original node."
        (ensure-original-unmodified!)
        (ensure-override-unmodified!))

      (testing "Setter is called during transact on original node."
        (binding [property-test-node-setter-check-fn
                  (fn check-fn [evaluation-context self old-value new-value]
                    (is (= original-node-id self))
                    (is (nil? old-value))
                    (is (= :original-effecting-property-value new-value))
                    (is (= :original-basic-property-value
                           (g/node-value self :basic-property evaluation-context)
                           (g/node-value self :basic-output evaluation-context))
                        "Called with in-transaction evaluation-context.")
                    (is (= :original-effecting-property-value
                           (g/node-value self :effecting-property evaluation-context)
                           (g/node-value self :effecting-output evaluation-context))
                        "Backing property was assigned before invoking setter."))]
          (g/transact
            (concat
              (g/set-property original-node-id :basic-property :original-basic-property-value)
              (g/set-property original-node-id :effecting-property :original-effecting-property-value)))))

      (testing "After transact on original node."
        (ensure-original-modified!)
        (ensure-original-modified-override-unmodified!))

      (testing "Undo change to original node."
        (g/undo! graph-id)
        (ensure-original-unmodified!)
        (ensure-override-unmodified!))

      (testing "Redo change to original node."
        (g/redo! graph-id)
        (ensure-original-modified!)
        (ensure-original-modified-override-unmodified!))

      (testing "Setter is called during transact on override node."
        (binding [property-test-node-setter-check-fn
                  (fn check-fn [evaluation-context self old-value new-value]
                    (is (= override-node-id self))
                    (is (= :original-effecting-property-value old-value))
                    (is (= :overridden-effecting-property-value new-value))
                    (is (= :overridden-effecting-property-value
                           (g/node-value self :effecting-property evaluation-context)
                           (g/node-value self :effecting-output evaluation-context))
                        "Backing property was assigned before invoking setter."))]
          (g/transact
            (g/set-property override-node-id :effecting-property :overridden-effecting-property-value))))

      (testing "Transact on override node."
        (ensure-original-modified!)
        (ensure-original-modified-override-modified!))

      (testing "Undo change to override node."
        (g/undo! graph-id)
        (ensure-original-modified!)
        (ensure-original-modified-override-unmodified!))

      (testing "Redo change to override node."
        (g/redo! graph-id)
        (ensure-original-modified!)
        (ensure-original-modified-override-modified!)))))

(deftest set-property-validation-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph! :history true)
          node-id (g/make-node! graph-id PropertyTestNode)]

      (testing "Before transaction attempt."
        (is (= nil
               (g/node-value node-id :basic-property)
               (g/node-value node-id :basic-output))))

      (testing "Transaction attempt fails."
        (is (thrown?
              ExceptionInfo
              (g/transact
                (g/set-property node-id :basic-property "invalid-non-keyword-value")))))

      (testing "After transaction attempt."
        (is (= nil
               (g/node-value node-id :basic-property)
               (g/node-value node-id :basic-output)))))))

;; -----------------------------------------------------------------------------
;; clear-property
;; -----------------------------------------------------------------------------

(deftest clear-property-effects-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph! :history true)
          original-node-id (first (g/take-node-ids graph-id 1))

          original-props
          {:effects-log-property []
           :basic-property :original-basic-property-value
           :effecting-property :original-effecting-property-value}

          override-props
          {:effects-log-property []
           :basic-property :overridden-basic-property-value
           :effecting-property :overridden-effecting-property-value}

          override-node-id
          (second
            (g/tx-nodes-added
              (g/transact
                (concat
                  (g/add-node (g/construct PropertyTestNode (assoc original-props :_node-id original-node-id)))
                  (g/override original-node-id {:init-props-fn (constantly override-props)})))))]

      (testing "Setup successful."
        (is (= original-node-id (g/override-original override-node-id))))

      (testing "Can't clear property on original."
        (is (thrown?
              ExceptionInfo
              (g/transact
                (g/clear-property original-node-id :basic-property)))))

      (testing "Before transact."
        (is (= :original-basic-property-value
               (g/node-value original-node-id :basic-property)
               (g/node-value original-node-id :basic-output)))
        (is (= :overridden-basic-property-value
               (g/node-value override-node-id :basic-property)
               (g/node-value override-node-id :basic-output))))

      (testing "Transact."
        (g/transact
          (g/clear-property override-node-id :basic-property))
        (is (= :original-basic-property-value
               (g/node-value original-node-id :basic-property)
               (g/node-value original-node-id :basic-output)))
        (is (= :original-basic-property-value
               (g/node-value override-node-id :basic-property)
               (g/node-value override-node-id :basic-output))))

      (testing "Undo."
        (g/undo! graph-id)
        (is (= :original-basic-property-value
               (g/node-value original-node-id :basic-property)
               (g/node-value original-node-id :basic-output)))
        (is (= :overridden-basic-property-value
               (g/node-value override-node-id :basic-property)
               (g/node-value override-node-id :basic-output))))

      (testing "Redo."
        (g/redo! graph-id)
        (is (= :original-basic-property-value
               (g/node-value original-node-id :basic-property)
               (g/node-value original-node-id :basic-output)))
        (is (= :original-basic-property-value
               (g/node-value override-node-id :basic-property)
               (g/node-value override-node-id :basic-output)))))))

(deftest clear-property-setter-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph! :history true)
          original-node-id (first (g/take-node-ids graph-id 1))

          original-props
          {:basic-property :original-basic-property-value
           :effecting-property :original-effecting-property-value}

          override-props
          {:effects-log-property [] ; Don't inherit from original.
           :basic-property :overridden-basic-property-value
           :effecting-property :overridden-effecting-property-value}

          override-node-id
          (second
            (g/tx-nodes-added
              (g/transact
                (concat
                  (g/add-node (g/construct PropertyTestNode (assoc original-props :_node-id original-node-id)))
                  (g/override original-node-id {:init-props-fn (constantly override-props)})))))

          ensure-before!
          (fn ensure-before! []
            (testing "Original node."
              (is (= :original-basic-property-value
                     (g/node-value original-node-id :basic-property)
                     (g/node-value original-node-id :basic-output)))
              (is (= :original-effecting-property-value
                     (g/node-value original-node-id :effecting-property)
                     (g/node-value original-node-id :effecting-output)))
              (is (= [{:old-value nil
                       :new-value :original-effecting-property-value
                       :new-output :original-effecting-property-value}]
                     (g/node-value original-node-id :effects-log-property)
                     (g/node-value original-node-id :effects-log-output))))

            (testing "Override node."
              (is (= :overridden-basic-property-value
                     (g/node-value override-node-id :basic-property)
                     (g/node-value override-node-id :basic-output)))
              (is (= :overridden-effecting-property-value
                     (g/node-value override-node-id :effecting-property)
                     (g/node-value override-node-id :effecting-output)))
              (is (= [{:old-value nil
                       :new-value :overridden-effecting-property-value
                       :new-output :overridden-effecting-property-value}]
                     (g/node-value override-node-id :effects-log-property)
                     (g/node-value override-node-id :effects-log-output)))))

          ensure-after!
          (fn ensure-after! []
            (testing "Original node."
              (is (= :new-original-basic-property-value
                     (g/node-value original-node-id :basic-property)
                     (g/node-value original-node-id :basic-output)))
              (is (= :original-effecting-property-value
                     (g/node-value original-node-id :effecting-property)
                     (g/node-value original-node-id :effecting-output)))
              (is (= [{:old-value nil
                       :new-value :original-effecting-property-value
                       :new-output :original-effecting-property-value}]
                     (g/node-value original-node-id :effects-log-property)
                     (g/node-value original-node-id :effects-log-output))))

            (testing "Override node."
              (is (= :overridden-basic-property-value
                     (g/node-value override-node-id :basic-property)
                     (g/node-value override-node-id :basic-output)))
              (is (= :original-effecting-property-value
                     (g/node-value override-node-id :effecting-property)
                     (g/node-value override-node-id :effecting-output)))
              (is (= [{:old-value nil
                       :new-value :overridden-effecting-property-value
                       :new-output :overridden-effecting-property-value}
                      {:old-value :overridden-effecting-property-value
                       :new-value nil
                       :new-output :original-effecting-property-value}]
                     (g/node-value override-node-id :effects-log-property)
                     (g/node-value override-node-id :effects-log-output)))))]

      (testing "Setup successful."
        (is (= original-node-id (g/override-original override-node-id))))

      (testing "Before transact."
        (ensure-before!))

      (testing "Setter is called during transact."
        (binding [property-test-node-setter-check-fn
                  (fn check-fn [evaluation-context self old-value new-value]
                    (is (= override-node-id self))
                    (is (= :overridden-effecting-property-value old-value))
                    (is (nil? new-value))
                    (is (= :new-original-basic-property-value
                           (g/node-value original-node-id :basic-property evaluation-context)
                           (g/node-value original-node-id :basic-output evaluation-context))
                        "Called with in-transaction evaluation-context.")
                    (is (= :original-effecting-property-value
                           (g/node-value override-node-id :effecting-property evaluation-context)
                           (g/node-value override-node-id :effecting-output evaluation-context))
                        "Backing property was cleared before invoking setter."))]
          (g/transact
            (concat
              (g/set-property original-node-id :basic-property :new-original-basic-property-value)
              (g/clear-property override-node-id :effecting-property)))))

      (testing "After transact."
        (ensure-after!))

      (testing "Undo."
        (g/undo! graph-id)
        (ensure-before!))

      (testing "Redo."
        (g/redo! graph-id)
        (ensure-after!)))))
