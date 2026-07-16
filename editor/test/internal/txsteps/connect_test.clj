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

(ns internal.txsteps.connect-test
  (:require [clojure.test :refer [deftest is testing]]
            [dynamo.graph :as g]
            [internal.graph :as ig]
            [internal.graph.types :as gt]
            [internal.txsteps.helpers :as helpers]
            [support.test-support :as test-support]
            [util.coll :as coll]))

(set! *warn-on-reflection* true)

(g/defnode NonOutputInvalidatingSourceNode
  (output output g/Keyword
          (g/fnk [] :value)))

(g/defnode NonOutputInvalidatingTargetNode
  (input input g/Keyword))

(deftest connection-to-less-volatile-graph-is-rejected-test
  (test-support/with-clean-system
    (let [source-graph-id (g/make-graph! :volatility 1)
          target-graph-id (g/make-graph!)
          [source-node-id target-node-id]
          (g/tx-nodes-added
            (g/transact
              {:undoable false}
              (concat
                (g/make-node source-graph-id helpers/ConnectionSourceNode :property :source-value)
                (g/make-node target-graph-id helpers/ConnectionTargetNode))))]
      (is (thrown? AssertionError
                   (g/transact
                     (g/connect source-node-id :property-output target-node-id :regular-input)))))))

(deftest non-output-invalidating-connection-is-undoable-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph!)

          [source-node-id target-node-id]
          (g/tx-nodes-added
            (g/transact
              (concat
                (g/make-node graph-id NonOutputInvalidatingSourceNode)
                (g/make-node graph-id NonOutputInvalidatingTargetNode))))]

      (g/reset-undo! :undo/global)

      (testing "Transact."
        (g/transact
          (g/connect source-node-id :output target-node-id :input))
        (is (= [[source-node-id :output]] (g/sources-of target-node-id :input)))
        (is (= 1 (g/undo-stack-count :undo/global))))

      (testing "Undo."
        (g/undo! :undo/global)
        (is (= [] (g/sources-of target-node-id :input))))

      (testing "Redo."
        (g/redo! :undo/global)
        (is (= [[source-node-id :output]] (g/sources-of target-node-id :input)))))))

(deftest introduce-connection-on-regular-input-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph!)

          [source-node-id target-node-id]
          (g/tx-nodes-added
            (g/transact
              (concat
                (g/make-node graph-id helpers/ConnectionSourceNode :property :source-value)
                (g/make-node graph-id helpers/ConnectionTargetNode))))

          ensure-before!
          (fn ensure-before! []
            (g/with-auto-evaluation-context evaluation-context
              (let [basis (:basis evaluation-context)]
                (testing "Connections."
                  (is (= [] (g/targets basis source-node-id :property-output)))
                  (is (= [] (g/sources basis target-node-id :regular-input))))

                (testing "Internal arc tables."
                  (is (coll/empty? (helpers/source-arc-table-tuples basis graph-id source-node-id :property-output)))
                  (is (coll/empty? (helpers/target-arc-table-tuples basis graph-id target-node-id :regular-input))))

                (testing "Output values."
                  (is (= nil (g/node-value target-node-id :regular-output evaluation-context)))))))

          ensure-after!
          (fn ensure-after! []
            (g/with-auto-evaluation-context evaluation-context
              (let [basis (:basis evaluation-context)]
                (testing "Connections."
                  (is (= [[target-node-id :regular-input]] (g/targets basis source-node-id :property-output)))
                  (is (= [[source-node-id :property-output]] (g/sources basis target-node-id :regular-input))))

                (testing "Internal arc tables."
                  (is (= [[source-node-id :property-output target-node-id :regular-input]]
                         (helpers/source-arc-table-tuples basis graph-id source-node-id :property-output)
                         (helpers/target-arc-table-tuples basis graph-id target-node-id :regular-input))))

                (testing "Output values."
                  (is (= :source-value (g/node-value target-node-id :regular-output evaluation-context)))))))]

      (testing "Before transact."
        (ensure-before!))

      (testing "Transact."
        (g/transact
          (g/connect source-node-id :property-output target-node-id :regular-input))
        (ensure-after!))

      (testing "Undo."
        (g/undo! :undo/global)
        (ensure-before!))

      (testing "Redo."
        (g/redo! :undo/global)
        (ensure-after!)))))

(deftest replace-connection-on-regular-input-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph!)

          [initial-source-node-id replacement-source-node-id target-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/make-nodes graph-id [initial-source-node-id [helpers/ConnectionSourceNode :property :initial-value]
                                      _replacement-source-node-id [helpers/ConnectionSourceNode :property :replacement-value]
                                      target-node-id helpers/ConnectionTargetNode]
                (g/connect initial-source-node-id :property-output target-node-id :regular-input))))

          ensure-before!
          (fn ensure-before! []
            (g/with-auto-evaluation-context evaluation-context
              (let [basis (:basis evaluation-context)]
                (testing "Connections."
                  (is (= [[target-node-id :regular-input]] (g/targets basis initial-source-node-id :property-output)))
                  (is (= [] (g/targets basis replacement-source-node-id :property-output)))
                  (is (= [[initial-source-node-id :property-output]] (g/sources basis target-node-id :regular-input))))

                (testing "Internal arc tables."
                  (is (= [[initial-source-node-id :property-output target-node-id :regular-input]]
                         (helpers/source-arc-table-tuples basis graph-id initial-source-node-id :property-output)
                         (helpers/target-arc-table-tuples basis graph-id target-node-id :regular-input)))
                  (is (coll/empty? (helpers/source-arc-table-tuples basis graph-id replacement-source-node-id :property-output))))

                (testing "Output values."
                  (is (= :initial-value (g/node-value target-node-id :regular-output evaluation-context)))))))

          ensure-after!
          (fn ensure-after! []
            (g/with-auto-evaluation-context evaluation-context
              (let [basis (:basis evaluation-context)]
                (testing "Connections."
                  (is (= [] (g/targets basis initial-source-node-id :property-output)))
                  (is (= [[target-node-id :regular-input]] (g/targets basis replacement-source-node-id :property-output)))
                  (is (= [[replacement-source-node-id :property-output]] (g/sources basis target-node-id :regular-input))))

                (testing "Internal arc tables."
                  (is (coll/empty? (helpers/source-arc-table-tuples basis graph-id initial-source-node-id :property-output)))
                  (is (= [[replacement-source-node-id :property-output target-node-id :regular-input]]
                         (helpers/source-arc-table-tuples basis graph-id replacement-source-node-id :property-output)
                         (helpers/target-arc-table-tuples basis graph-id target-node-id :regular-input))))

                (testing "Output values."
                  (is (= :replacement-value (g/node-value target-node-id :regular-output evaluation-context)))))))]

      (testing "Before transact."
        (ensure-before!))

      (testing "Transact."
        (g/transact
          (g/connect replacement-source-node-id :property-output target-node-id :regular-input))
        (ensure-after!))

      (testing "Undo."
        (g/undo! :undo/global)
        (ensure-before!))

      (testing "Redo."
        (g/redo! :undo/global)
        (ensure-after!)))))

(deftest replace-connection-on-regular-input-with-missing-source-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph!)

          [initial-source-node-id]
          (g/tx-nodes-added
            (g/transact
              {:undo-key ::add-initial-source}
              (g/make-node graph-id helpers/ConnectionSourceNode :property :initial-value)))

          [replacement-source-node-id target-node-id]
          (g/tx-nodes-added
            (g/transact
              {:undoable false}
              (g/make-nodes graph-id [_replacement-source-node-id [helpers/ConnectionSourceNode :property :replacement-value]
                                      target-node-id helpers/ConnectionTargetNode]
                (g/connect initial-source-node-id :property-output target-node-id :regular-input))))]

      (testing "After undoing the initial source."
        (g/undo! ::add-initial-source)
        (let [basis (g/now)
              initial-arc-tuple [initial-source-node-id :property-output target-node-id :regular-input]]
          (is (nil? (g/node-by-id basis initial-source-node-id)))
          (is (= [initial-arc-tuple]
                 (helpers/source-arc-table-tuples basis graph-id initial-source-node-id :property-output)
                 (helpers/target-arc-table-tuples basis graph-id target-node-id :regular-input)))
          (is (nil? (g/node-value target-node-id :regular-output)))))

      (testing "After replacing the connection."
        (g/transact
          {:undoable false}
          (g/connect replacement-source-node-id :property-output target-node-id :regular-input))
        (let [basis (g/now)
              replacement-arc-tuple [replacement-source-node-id :property-output target-node-id :regular-input]]
          (is (nil? (g/node-by-id basis initial-source-node-id)))
          (is (coll/empty? (helpers/source-arc-table-tuples basis graph-id initial-source-node-id :property-output)))
          (is (= [replacement-arc-tuple]
                 (helpers/source-arc-table-tuples basis graph-id replacement-source-node-id :property-output)
                 (helpers/target-arc-table-tuples basis graph-id target-node-id :regular-input)))
          (is (= :replacement-value (g/node-value target-node-id :regular-output)))))

      (testing "After redoing the initial source."
        (g/redo! ::add-initial-source)
        (let [basis (g/now)
              replacement-arc-tuple [replacement-source-node-id :property-output target-node-id :regular-input]]
          (is (g/node-by-id basis initial-source-node-id))
          (is (coll/empty? (helpers/source-arc-table-tuples basis graph-id initial-source-node-id :property-output)))
          (is (= [replacement-arc-tuple]
                 (helpers/source-arc-table-tuples basis graph-id replacement-source-node-id :property-output)
                 (helpers/target-arc-table-tuples basis graph-id target-node-id :regular-input)))
          (is (= :replacement-value (g/node-value target-node-id :regular-output))))))))

(deftest replace-connection-on-regular-input-with-missing-source-graph-test
  (test-support/with-clean-system
    (let [source-graph-id (g/make-graph!)
          target-graph-id (g/make-graph! :volatility 10)

          [source-node-id]
          (g/tx-nodes-added
            (g/transact
              {:undoable false}
              (g/make-node source-graph-id helpers/ConnectionSourceNode :property :source-value)))

          [replacement-source-node-id target-node-id]
          (g/tx-nodes-added
            (g/transact
              {:undoable false}
              (g/make-nodes target-graph-id [_replacement-source-node-id [helpers/ConnectionSourceNode :property :replacement-value]
                                             target-node-id helpers/ConnectionTargetNode]
                (g/connect source-node-id :property-output target-node-id :regular-input))))]

      (g/transact
        {:undo-key ::delete-target}
        (g/delete-node target-node-id))
      (g/delete-graph! source-graph-id)

      (testing "After restoring the target without its source graph."
        (g/undo! ::delete-target)
        (let [basis (g/now)]
          (is (nil? (g/graph source-graph-id)))
          (is (g/node-by-id basis target-node-id))
          (is (= []
                 (g/sources basis target-node-id :regular-input)))
          (is (= [[source-node-id :property-output target-node-id :regular-input]]
                 (helpers/target-arc-table-tuples basis target-graph-id target-node-id :regular-input)))))

      (testing "After replacing the target-only connection."
        (g/transact
          {:undoable false}
          (g/connect replacement-source-node-id :property-output target-node-id :regular-input))
        (let [basis (g/now)
              replacement-arc-tuple [replacement-source-node-id :property-output target-node-id :regular-input]]
          (is (nil? (g/graph source-graph-id)))
          (is (= [[replacement-source-node-id :property-output]]
                 (g/sources basis target-node-id :regular-input)))
          (is (= [replacement-arc-tuple]
                 (helpers/source-arc-table-tuples basis target-graph-id replacement-source-node-id :property-output)
                 (helpers/target-arc-table-tuples basis target-graph-id target-node-id :regular-input)))
          (is (= :replacement-value (g/node-value target-node-id :regular-output))))))))

(deftest introduce-connection-on-array-input-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph!)

          [source-node-id target-node-id]
          (g/tx-nodes-added
            (g/transact
              (concat
                (g/make-node graph-id helpers/ConnectionSourceNode :property :source-value)
                (g/make-node graph-id helpers/ConnectionTargetNode))))

          ensure-before!
          (fn ensure-before! []
            (g/with-auto-evaluation-context evaluation-context
              (let [basis (:basis evaluation-context)]
                (testing "Connections."
                  (is (= [] (g/targets basis source-node-id :property-output)))
                  (is (= [] (g/sources basis target-node-id :array-input))))

                (testing "Internal arc tables."
                  (is (coll/empty? (helpers/source-arc-table-tuples basis graph-id source-node-id :property-output)))
                  (is (coll/empty? (helpers/target-arc-table-tuples basis graph-id target-node-id :array-input))))

                (testing "Output values."
                  (is (= [] (g/node-value target-node-id :array-output evaluation-context)))))))

          ensure-after!
          (fn ensure-after! []
            (g/with-auto-evaluation-context evaluation-context
              (let [basis (:basis evaluation-context)]
                (testing "Connections."
                  (is (= [[target-node-id :array-input]] (g/targets basis source-node-id :property-output)))
                  (is (= [[source-node-id :property-output]] (g/sources basis target-node-id :array-input))))

                (testing "Internal arc tables."
                  (is (= [[source-node-id :property-output target-node-id :array-input]]
                         (helpers/source-arc-table-tuples basis graph-id source-node-id :property-output)
                         (helpers/target-arc-table-tuples basis graph-id target-node-id :array-input))))

                (testing "Output values."
                  (is (= [:source-value] (g/node-value target-node-id :array-output evaluation-context)))))))]

      (testing "Before transact."
        (ensure-before!))

      (testing "Transact."
        (g/transact
          (g/connect source-node-id :property-output target-node-id :array-input))
        (ensure-after!))

      (testing "Undo."
        (g/undo! :undo/global)
        (ensure-before!))

      (testing "Redo."
        (g/redo! :undo/global)
        (ensure-after!)))))

(deftest append-connection-on-array-input-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph!)

          [first-source-node-id second-source-node-id target-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/make-nodes graph-id [first-source-node-id [helpers/ConnectionSourceNode :property :first-value]
                                      _second-source-node-id [helpers/ConnectionSourceNode :property :second-value]
                                      target-node-id helpers/ConnectionTargetNode]
                (g/connect first-source-node-id :property-output target-node-id :array-input))))

          ensure-before!
          (fn ensure-before! []
            (let [basis (g/now)]
              (testing "Connections."
                (is (g/connected? basis first-source-node-id :property-output target-node-id :array-input))
                (is (not (g/connected? basis second-source-node-id :property-output target-node-id :array-input)))
                (is (= [[target-node-id :array-input]] (g/targets basis first-source-node-id :property-output)))
                (is (= [] (g/targets basis second-source-node-id :property-output)))
                (is (= [[first-source-node-id :property-output]] (g/sources basis target-node-id :array-input))))

              (testing "Internal arc tables."
                (is (= [[first-source-node-id :property-output target-node-id :array-input]]
                       (helpers/source-arc-table-tuples basis graph-id first-source-node-id :property-output)
                       (helpers/target-arc-table-tuples basis graph-id target-node-id :array-input)))
                (is (coll/empty? (helpers/source-arc-table-tuples basis graph-id second-source-node-id :property-output))))

              (testing "Output values."
                (is (= [:first-value] (g/node-value target-node-id :array-output))))))

          ensure-after!
          (fn ensure-after! []
            (let [basis (g/now)]
              (testing "Connections."
                (is (g/connected? basis first-source-node-id :property-output target-node-id :array-input))
                (is (g/connected? basis second-source-node-id :property-output target-node-id :array-input))
                (is (= [[target-node-id :array-input]] (g/targets basis first-source-node-id :property-output)))
                (is (= [[target-node-id :array-input]] (g/targets basis second-source-node-id :property-output)))
                (is (= [[first-source-node-id :property-output] [second-source-node-id :property-output]] (g/sources basis target-node-id :array-input))))

              (testing "Internal arc tables."
                (is (= [[first-source-node-id :property-output target-node-id :array-input]]
                       (helpers/source-arc-table-tuples basis graph-id first-source-node-id :property-output)))
                (is (= [[second-source-node-id :property-output target-node-id :array-input]]
                       (helpers/source-arc-table-tuples basis graph-id second-source-node-id :property-output)))
                (is (= [[first-source-node-id :property-output target-node-id :array-input]
                        [second-source-node-id :property-output target-node-id :array-input]]
                       (helpers/target-arc-table-tuples basis graph-id target-node-id :array-input))))

              (testing "Output values."
                (is (= [:first-value :second-value] (g/node-value target-node-id :array-output))))))]

      (testing "Before transact."
        (ensure-before!))

      (testing "Transact."
        (g/transact
          (g/connect second-source-node-id :property-output target-node-id :array-input))
        (ensure-after!))

      (testing "Undo."
        (g/undo! :undo/global)
        (ensure-before!))

      (testing "Redo."
        (g/redo! :undo/global)
        (ensure-after!)))))

(deftest append-duplicated-connection-on-array-input-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph!)

          [first-source-node-id
           duplicated-source-node-id
           second-source-node-id
           target-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/make-nodes graph-id [first-source-node-id [helpers/ConnectionSourceNode :property :first-value]
                                      duplicated-source-node-id [helpers/ConnectionSourceNode :property :duplicated-value]
                                      second-source-node-id [helpers/ConnectionSourceNode :property :second-value]
                                      target-node-id helpers/ConnectionTargetNode]
                (g/connect first-source-node-id :property-output target-node-id :array-input)
                (g/connect duplicated-source-node-id :property-output target-node-id :array-input)
                (g/connect second-source-node-id :property-output target-node-id :array-input))))

          ensure-before!
          (fn ensure-before! []
            (let [basis (g/now)]
              (testing "Connections."
                (is (= [[target-node-id :array-input]] (g/targets basis first-source-node-id :property-output)))
                (is (= [[target-node-id :array-input]] (g/targets basis duplicated-source-node-id :property-output)))
                (is (= [[target-node-id :array-input]] (g/targets basis second-source-node-id :property-output)))
                (is (= [[first-source-node-id :property-output]
                        [duplicated-source-node-id :property-output]
                        [second-source-node-id :property-output]]
                       (g/sources basis target-node-id :array-input))))

              (testing "Internal arc tables."
                (is (= [[first-source-node-id :property-output target-node-id :array-input]]
                       (helpers/source-arc-table-tuples basis graph-id first-source-node-id :property-output)))
                (is (= [[duplicated-source-node-id :property-output target-node-id :array-input]]
                       (helpers/source-arc-table-tuples basis graph-id duplicated-source-node-id :property-output)))
                (is (= [[second-source-node-id :property-output target-node-id :array-input]]
                       (helpers/source-arc-table-tuples basis graph-id second-source-node-id :property-output)))
                (is (= [[first-source-node-id :property-output target-node-id :array-input]
                        [duplicated-source-node-id :property-output target-node-id :array-input]
                        [second-source-node-id :property-output target-node-id :array-input]]
                       (helpers/target-arc-table-tuples basis graph-id target-node-id :array-input))))

              (testing "Output values."
                (is (= [:first-value :duplicated-value :second-value] (g/node-value target-node-id :array-output))))))

          ensure-after!
          (fn ensure-after! []
            (let [basis (g/now)]
              (testing "Connections."
                (is (= [[target-node-id :array-input]] (g/targets basis first-source-node-id :property-output)))
                (is (= [[target-node-id :array-input]
                        [target-node-id :array-input]]
                       (g/targets basis duplicated-source-node-id :property-output)))
                (is (= [[target-node-id :array-input]] (g/targets basis second-source-node-id :property-output)))
                (is (= [[first-source-node-id :property-output]
                        [duplicated-source-node-id :property-output]
                        [second-source-node-id :property-output]
                        [duplicated-source-node-id :property-output]]
                       (g/sources basis target-node-id :array-input))))

              (testing "Internal arc tables."
                (is (= [[first-source-node-id :property-output target-node-id :array-input]]
                       (helpers/source-arc-table-tuples basis graph-id first-source-node-id :property-output)))
                (is (= [[duplicated-source-node-id :property-output target-node-id :array-input]
                        [duplicated-source-node-id :property-output target-node-id :array-input]]
                       (helpers/source-arc-table-tuples basis graph-id duplicated-source-node-id :property-output)))
                (is (= [[second-source-node-id :property-output target-node-id :array-input]]
                       (helpers/source-arc-table-tuples basis graph-id second-source-node-id :property-output)))
                (is (= [[first-source-node-id :property-output target-node-id :array-input]
                        [duplicated-source-node-id :property-output target-node-id :array-input]
                        [second-source-node-id :property-output target-node-id :array-input]
                        [duplicated-source-node-id :property-output target-node-id :array-input]]
                       (helpers/target-arc-table-tuples basis graph-id target-node-id :array-input))))

              (testing "Output values."
                (is (= [:first-value :duplicated-value :second-value :duplicated-value] (g/node-value target-node-id :array-output))))))]

      (testing "Before transact."
        (ensure-before!))

      (testing "Transact."
        (g/transact
          (g/connect duplicated-source-node-id :property-output target-node-id :array-input))
        (ensure-after!))

      (testing "Undo."
        (g/undo! :undo/global)
        (ensure-before!))

      (testing "Redo."
        (g/redo! :undo/global)
        (ensure-after!)))))

(deftest introduce-shadowing-connection-on-regular-input-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph!)

          [initial-source-node-id shadowing-source-node-id original-target-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/make-nodes graph-id [initial-source-node-id [helpers/ConnectionSourceNode :property :initial-source-value]
                                      _shadowing-source-node-id [helpers/ConnectionSourceNode :property :shadowing-source-value]
                                      original-target-node-id helpers/ConnectionTargetNode]
                (g/connect initial-source-node-id :property-output original-target-node-id :regular-input))))

          [first-order-override-target-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/override original-target-node-id)))

          [second-order-override-target-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/override first-order-override-target-node-id)))

          ensure-before!
          (fn ensure-before! []
            (g/with-auto-evaluation-context evaluation-context
              (let [basis (:basis evaluation-context)]
                (testing "Explicit connections."
                  (is (= [[original-target-node-id :regular-input]] (ig/explicit-targets basis initial-source-node-id :property-output)))
                  (is (= [] (ig/explicit-targets basis shadowing-source-node-id :property-output)))
                  (is (= [[initial-source-node-id :property-output]] (ig/explicit-sources basis original-target-node-id :regular-input)))
                  (is (= [] (ig/explicit-sources basis first-order-override-target-node-id :regular-input)))
                  (is (= [] (ig/explicit-sources basis second-order-override-target-node-id :regular-input))))

                (testing "Implicit connections."
                  (is (= [[original-target-node-id :regular-input] [first-order-override-target-node-id :regular-input] [second-order-override-target-node-id :regular-input]] (g/targets basis initial-source-node-id :property-output)))
                  (is (= [] (g/targets basis shadowing-source-node-id :property-output)))
                  (is (= [[initial-source-node-id :property-output]] (g/sources basis original-target-node-id :regular-input)))
                  (is (= [[initial-source-node-id :property-output]] (g/sources basis first-order-override-target-node-id :regular-input)))
                  (is (= [[initial-source-node-id :property-output]] (g/sources basis second-order-override-target-node-id :regular-input))))

                (testing "Internal arc tables."
                  (is (= [[initial-source-node-id :property-output original-target-node-id :regular-input]]
                         (helpers/source-arc-table-tuples basis graph-id initial-source-node-id :property-output)
                         (helpers/target-arc-table-tuples basis graph-id original-target-node-id :regular-input)))
                  (is (coll/empty? (helpers/source-arc-table-tuples basis graph-id shadowing-source-node-id :property-output)))
                  (is (coll/empty? (helpers/target-arc-table-tuples basis graph-id first-order-override-target-node-id :regular-input))))

                (testing "Output values."
                  (is (= :initial-source-value (g/node-value original-target-node-id :regular-output evaluation-context)))
                  (is (= :initial-source-value (g/node-value first-order-override-target-node-id :regular-output evaluation-context)))
                  (is (= :initial-source-value (g/node-value second-order-override-target-node-id :regular-output evaluation-context)))))))

          ensure-after!
          (fn ensure-after! []
            (g/with-auto-evaluation-context evaluation-context
              (let [basis (:basis evaluation-context)]
                (testing "Explicit connections."
                  (is (= [[original-target-node-id :regular-input]] (ig/explicit-targets basis initial-source-node-id :property-output)))
                  (is (= [[first-order-override-target-node-id :regular-input]] (ig/explicit-targets basis shadowing-source-node-id :property-output)))
                  (is (= [[initial-source-node-id :property-output]] (ig/explicit-sources basis original-target-node-id :regular-input)))
                  (is (= [[shadowing-source-node-id :property-output]] (ig/explicit-sources basis first-order-override-target-node-id :regular-input)))
                  (is (= [] (ig/explicit-sources basis second-order-override-target-node-id :regular-input))))

                (testing "Implicit connections."
                  (is (= [[original-target-node-id :regular-input]] (g/targets basis initial-source-node-id :property-output)))
                  (is (= [[first-order-override-target-node-id :regular-input] [second-order-override-target-node-id :regular-input]] (g/targets basis shadowing-source-node-id :property-output)))
                  (is (= [[initial-source-node-id :property-output]] (g/sources basis original-target-node-id :regular-input)))
                  (is (= [[shadowing-source-node-id :property-output]] (g/sources basis first-order-override-target-node-id :regular-input)))
                  (is (= [[shadowing-source-node-id :property-output]] (g/sources basis second-order-override-target-node-id :regular-input))))

                (testing "Internal arc tables."
                  (is (= [[initial-source-node-id :property-output original-target-node-id :regular-input]]
                         (helpers/source-arc-table-tuples basis graph-id initial-source-node-id :property-output)
                         (helpers/target-arc-table-tuples basis graph-id original-target-node-id :regular-input)))
                  (is (= [[shadowing-source-node-id :property-output first-order-override-target-node-id :regular-input]]
                         (helpers/source-arc-table-tuples basis graph-id shadowing-source-node-id :property-output)
                         (helpers/target-arc-table-tuples basis graph-id first-order-override-target-node-id :regular-input))))

                (testing "Output values."
                  (is (= :initial-source-value (g/node-value original-target-node-id :regular-output evaluation-context)))
                  (is (= :shadowing-source-value (g/node-value first-order-override-target-node-id :regular-output evaluation-context)))
                  (is (= :shadowing-source-value (g/node-value second-order-override-target-node-id :regular-output evaluation-context)))))))]

      (testing "Before transact."
        (ensure-before!))

      (testing "Transact."
        (g/transact
          (g/connect shadowing-source-node-id :property-output first-order-override-target-node-id :regular-input))
        (ensure-after!))

      (testing "Undo."
        (g/undo! :undo/global)
        (ensure-before!))

      (testing "Redo."
        (g/redo! :undo/global)
        (ensure-after!)))))

(deftest replace-shadowing-connection-on-regular-input-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph!)

          [initial-source-node-id initial-shadowing-source-node-id replacement-shadowing-source-node-id original-target-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/make-nodes graph-id [initial-source-node-id [helpers/ConnectionSourceNode :property :initial-source-value]
                                      _initial-shadowing-source-node-id [helpers/ConnectionSourceNode :property :initial-shadowing-source-value]
                                      _replacement-shadowing-source-node-id [helpers/ConnectionSourceNode :property :replacement-shadowing-source-value]
                                      original-target-node-id helpers/ConnectionTargetNode]
                (g/connect initial-source-node-id :property-output original-target-node-id :regular-input))))

          [first-order-override-target-node-id second-order-override-target-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/override original-target-node-id nil
                (fn [_evaluation-context id-mapping]
                  (let [first-order-override-target-node-id (get id-mapping original-target-node-id)]
                    (concat
                      (g/connect initial-shadowing-source-node-id :property-output first-order-override-target-node-id :regular-input)
                      (g/override first-order-override-target-node-id)))))))

          ensure-before!
          (fn ensure-before! []
            (g/with-auto-evaluation-context evaluation-context
              (let [basis (:basis evaluation-context)]
                (testing "Explicit connections."
                  (is (= [[original-target-node-id :regular-input]] (ig/explicit-targets basis initial-source-node-id :property-output)))
                  (is (= [[first-order-override-target-node-id :regular-input]] (ig/explicit-targets basis initial-shadowing-source-node-id :property-output)))
                  (is (= [] (ig/explicit-targets basis replacement-shadowing-source-node-id :property-output)))
                  (is (= [[initial-source-node-id :property-output]] (ig/explicit-sources basis original-target-node-id :regular-input)))
                  (is (= [[initial-shadowing-source-node-id :property-output]] (ig/explicit-sources basis first-order-override-target-node-id :regular-input)))
                  (is (= [] (ig/explicit-sources basis second-order-override-target-node-id :regular-input))))

                (testing "Implicit connections."
                  (is (= [[original-target-node-id :regular-input]] (g/targets basis initial-source-node-id :property-output)))
                  (is (= [[first-order-override-target-node-id :regular-input] [second-order-override-target-node-id :regular-input]] (g/targets basis initial-shadowing-source-node-id :property-output)))
                  (is (= [] (g/targets basis replacement-shadowing-source-node-id :property-output)))
                  (is (= [[initial-source-node-id :property-output]] (g/sources basis original-target-node-id :regular-input)))
                  (is (= [[initial-shadowing-source-node-id :property-output]] (g/sources basis first-order-override-target-node-id :regular-input)))
                  (is (= [[initial-shadowing-source-node-id :property-output]] (g/sources basis second-order-override-target-node-id :regular-input))))

                (testing "Internal arc tables."
                  (is (= [[initial-source-node-id :property-output original-target-node-id :regular-input]]
                         (helpers/source-arc-table-tuples basis graph-id initial-source-node-id :property-output)
                         (helpers/target-arc-table-tuples basis graph-id original-target-node-id :regular-input)))
                  (is (= [[initial-shadowing-source-node-id :property-output first-order-override-target-node-id :regular-input]]
                         (helpers/source-arc-table-tuples basis graph-id initial-shadowing-source-node-id :property-output)
                         (helpers/target-arc-table-tuples basis graph-id first-order-override-target-node-id :regular-input)))
                  (is (coll/empty? (helpers/source-arc-table-tuples basis graph-id replacement-shadowing-source-node-id :property-output))))

                (testing "Output values."
                  (is (= :initial-source-value (g/node-value original-target-node-id :regular-output evaluation-context)))
                  (is (= :initial-shadowing-source-value (g/node-value first-order-override-target-node-id :regular-output evaluation-context)))
                  (is (= :initial-shadowing-source-value (g/node-value second-order-override-target-node-id :regular-output evaluation-context)))))))

          ensure-after!
          (fn ensure-after! []
            (g/with-auto-evaluation-context evaluation-context
              (let [basis (:basis evaluation-context)]
                (testing "Explicit connections."
                  (is (= [[original-target-node-id :regular-input]] (ig/explicit-targets basis initial-source-node-id :property-output)))
                  (is (= [] (ig/explicit-targets basis initial-shadowing-source-node-id :property-output)))
                  (is (= [[first-order-override-target-node-id :regular-input]] (ig/explicit-targets basis replacement-shadowing-source-node-id :property-output)))
                  (is (= [[initial-source-node-id :property-output]] (ig/explicit-sources basis original-target-node-id :regular-input)))
                  (is (= [[replacement-shadowing-source-node-id :property-output]] (ig/explicit-sources basis first-order-override-target-node-id :regular-input)))
                  (is (= [] (ig/explicit-sources basis second-order-override-target-node-id :regular-input))))

                (testing "Implicit connections."
                  (is (= [[original-target-node-id :regular-input]] (g/targets basis initial-source-node-id :property-output)))
                  (is (= [] (g/targets basis initial-shadowing-source-node-id :property-output)))
                  (is (= [[first-order-override-target-node-id :regular-input] [second-order-override-target-node-id :regular-input]] (g/targets basis replacement-shadowing-source-node-id :property-output)))
                  (is (= [[initial-source-node-id :property-output]] (g/sources basis original-target-node-id :regular-input)))
                  (is (= [[replacement-shadowing-source-node-id :property-output]] (g/sources basis first-order-override-target-node-id :regular-input)))
                  (is (= [[replacement-shadowing-source-node-id :property-output]] (g/sources basis second-order-override-target-node-id :regular-input))))

                (testing "Internal arc tables."
                  (is (= [[initial-source-node-id :property-output original-target-node-id :regular-input]]
                         (helpers/source-arc-table-tuples basis graph-id initial-source-node-id :property-output)
                         (helpers/target-arc-table-tuples basis graph-id original-target-node-id :regular-input)))
                  (is (= [[replacement-shadowing-source-node-id :property-output first-order-override-target-node-id :regular-input]]
                         (helpers/source-arc-table-tuples basis graph-id replacement-shadowing-source-node-id :property-output)
                         (helpers/target-arc-table-tuples basis graph-id first-order-override-target-node-id :regular-input)))
                  (is (coll/empty? (helpers/source-arc-table-tuples basis graph-id initial-shadowing-source-node-id :property-output))))

                (testing "Output values."
                  (is (= :initial-source-value (g/node-value original-target-node-id :regular-output evaluation-context)))
                  (is (= :replacement-shadowing-source-value (g/node-value first-order-override-target-node-id :regular-output evaluation-context)))
                  (is (= :replacement-shadowing-source-value (g/node-value second-order-override-target-node-id :regular-output evaluation-context)))))))]

      (testing "Before transact."
        (ensure-before!))

      (testing "Transact."
        (g/transact
          (g/connect replacement-shadowing-source-node-id :property-output first-order-override-target-node-id :regular-input))
        (ensure-after!))

      (testing "Undo."
        (g/undo! :undo/global)
        (ensure-before!))

      (testing "Redo."
        (g/redo! :undo/global)
        (ensure-after!)))))

(deftest introduce-shadowing-connection-on-array-input-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph!)

          [initial-source-one-node-id initial-source-two-node-id shadowing-source-node-id original-target-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/make-nodes graph-id [initial-source-one-node-id [helpers/ConnectionSourceNode :property :initial-source-one-value]
                                      initial-source-two-node-id [helpers/ConnectionSourceNode :property :initial-source-two-value]
                                      _shadowing-source-node-id [helpers/ConnectionSourceNode :property :shadowing-source-value]
                                      original-target-node-id helpers/ConnectionTargetNode]
                (g/connect initial-source-one-node-id :property-output original-target-node-id :array-input)
                (g/connect initial-source-two-node-id :property-output original-target-node-id :array-input))))

          [first-order-override-target-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/override original-target-node-id)))

          [second-order-override-target-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/override first-order-override-target-node-id)))

          ensure-before!
          (fn ensure-before! []
            (g/with-auto-evaluation-context evaluation-context
              (let [basis (:basis evaluation-context)]
                (testing "Explicit connections."
                  (is (= [[original-target-node-id :array-input]] (ig/explicit-targets basis initial-source-one-node-id :property-output)))
                  (is (= [[original-target-node-id :array-input]] (ig/explicit-targets basis initial-source-two-node-id :property-output)))
                  (is (= [] (ig/explicit-targets basis shadowing-source-node-id :property-output)))
                  (is (= [[initial-source-one-node-id :property-output] [initial-source-two-node-id :property-output]] (ig/explicit-sources basis original-target-node-id :array-input)))
                  (is (= [] (ig/explicit-sources basis first-order-override-target-node-id :array-input)))
                  (is (= [] (ig/explicit-sources basis second-order-override-target-node-id :array-input))))

                (testing "Implicit connections."
                  (is (= [[original-target-node-id :array-input] [first-order-override-target-node-id :array-input] [second-order-override-target-node-id :array-input]] (g/targets basis initial-source-one-node-id :property-output)))
                  (is (= [[original-target-node-id :array-input] [first-order-override-target-node-id :array-input] [second-order-override-target-node-id :array-input]] (g/targets basis initial-source-two-node-id :property-output)))
                  (is (= [] (g/targets basis shadowing-source-node-id :property-output)))
                  (is (= [[initial-source-one-node-id :property-output] [initial-source-two-node-id :property-output]] (g/sources basis original-target-node-id :array-input)))
                  (is (= [[initial-source-one-node-id :property-output] [initial-source-two-node-id :property-output]] (g/sources basis first-order-override-target-node-id :array-input)))
                  (is (= [[initial-source-one-node-id :property-output] [initial-source-two-node-id :property-output]] (g/sources basis second-order-override-target-node-id :array-input))))

                (testing "Internal arc tables."
                  (is (= [[initial-source-one-node-id :property-output original-target-node-id :array-input]]
                         (helpers/source-arc-table-tuples basis graph-id initial-source-one-node-id :property-output)))
                  (is (= [[initial-source-two-node-id :property-output original-target-node-id :array-input]]
                         (helpers/source-arc-table-tuples basis graph-id initial-source-two-node-id :property-output)))
                  (is (= [[initial-source-one-node-id :property-output original-target-node-id :array-input]
                          [initial-source-two-node-id :property-output original-target-node-id :array-input]]
                         (helpers/target-arc-table-tuples basis graph-id original-target-node-id :array-input)))
                  (is (coll/empty? (helpers/source-arc-table-tuples basis graph-id shadowing-source-node-id :property-output)))
                  (is (coll/empty? (helpers/target-arc-table-tuples basis graph-id first-order-override-target-node-id :array-input))))

                (testing "Output values."
                  (is (= [:initial-source-one-value :initial-source-two-value] (g/node-value original-target-node-id :array-output evaluation-context)))
                  (is (= [:initial-source-one-value :initial-source-two-value] (g/node-value first-order-override-target-node-id :array-output evaluation-context)))
                  (is (= [:initial-source-one-value :initial-source-two-value] (g/node-value second-order-override-target-node-id :array-output evaluation-context)))))))

          ensure-after!
          (fn ensure-after! []
            (g/with-auto-evaluation-context evaluation-context
              (let [basis (:basis evaluation-context)]
                (testing "Explicit connections."
                  (is (= [[original-target-node-id :array-input]] (ig/explicit-targets basis initial-source-one-node-id :property-output)))
                  (is (= [[original-target-node-id :array-input]] (ig/explicit-targets basis initial-source-two-node-id :property-output)))
                  (is (= [[first-order-override-target-node-id :array-input]] (ig/explicit-targets basis shadowing-source-node-id :property-output)))
                  (is (= [[initial-source-one-node-id :property-output] [initial-source-two-node-id :property-output]] (ig/explicit-sources basis original-target-node-id :array-input)))
                  (is (= [[shadowing-source-node-id :property-output]] (ig/explicit-sources basis first-order-override-target-node-id :array-input)))
                  (is (= [] (ig/explicit-sources basis second-order-override-target-node-id :array-input))))

                (testing "Implicit connections."
                  (is (= [[original-target-node-id :array-input]] (g/targets basis initial-source-one-node-id :property-output)))
                  (is (= [[original-target-node-id :array-input]] (g/targets basis initial-source-two-node-id :property-output)))
                  (is (= [[first-order-override-target-node-id :array-input] [second-order-override-target-node-id :array-input]] (g/targets basis shadowing-source-node-id :property-output)))
                  (is (= [[initial-source-one-node-id :property-output] [initial-source-two-node-id :property-output]] (g/sources basis original-target-node-id :array-input)))
                  (is (= [[shadowing-source-node-id :property-output]] (g/sources basis first-order-override-target-node-id :array-input)))
                  (is (= [[shadowing-source-node-id :property-output]] (g/sources basis second-order-override-target-node-id :array-input))))

                (testing "Internal arc tables."
                  (is (= [[initial-source-one-node-id :property-output original-target-node-id :array-input]]
                         (helpers/source-arc-table-tuples basis graph-id initial-source-one-node-id :property-output)))
                  (is (= [[initial-source-two-node-id :property-output original-target-node-id :array-input]]
                         (helpers/source-arc-table-tuples basis graph-id initial-source-two-node-id :property-output)))
                  (is (= [[initial-source-one-node-id :property-output original-target-node-id :array-input]
                          [initial-source-two-node-id :property-output original-target-node-id :array-input]]
                         (helpers/target-arc-table-tuples basis graph-id original-target-node-id :array-input)))
                  (is (= [[shadowing-source-node-id :property-output first-order-override-target-node-id :array-input]]
                         (helpers/source-arc-table-tuples basis graph-id shadowing-source-node-id :property-output)
                         (helpers/target-arc-table-tuples basis graph-id first-order-override-target-node-id :array-input))))

                (testing "Output values."
                  (is (= [:initial-source-one-value :initial-source-two-value] (g/node-value original-target-node-id :array-output evaluation-context)))
                  (is (= [:shadowing-source-value] (g/node-value first-order-override-target-node-id :array-output evaluation-context)))
                  (is (= [:shadowing-source-value] (g/node-value second-order-override-target-node-id :array-output evaluation-context)))))))]

      (testing "Before transact."
        (ensure-before!))

      (testing "Transact."
        (g/transact
          (g/connect shadowing-source-node-id :property-output first-order-override-target-node-id :array-input))
        (ensure-after!))

      (testing "Undo."
        (g/undo! :undo/global)
        (ensure-before!))

      (testing "Redo."
        (g/redo! :undo/global)
        (ensure-after!)))))

(deftest append-shadowing-connection-on-array-input-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph!)

          [initial-source-one-node-id initial-source-two-node-id shadowing-source-one-node-id shadowing-source-two-node-id original-target-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/make-nodes graph-id [initial-source-one-node-id [helpers/ConnectionSourceNode :property :initial-source-one-value]
                                      initial-source-two-node-id [helpers/ConnectionSourceNode :property :initial-source-two-value]
                                      _shadowing-source-one-node-id [helpers/ConnectionSourceNode :property :shadowing-source-one-value]
                                      _shadowing-source-two-node-id [helpers/ConnectionSourceNode :property :shadowing-source-two-value]
                                      original-target-node-id helpers/ConnectionTargetNode]
                (g/connect initial-source-one-node-id :property-output original-target-node-id :array-input)
                (g/connect initial-source-two-node-id :property-output original-target-node-id :array-input))))

          [first-order-override-target-node-id second-order-override-target-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/override original-target-node-id nil
                (fn [_evaluation-context id-mapping]
                  (let [first-order-override-target-node-id (get id-mapping original-target-node-id)]
                    (concat
                      (g/connect shadowing-source-one-node-id :property-output first-order-override-target-node-id :array-input)
                      (g/override first-order-override-target-node-id)))))))

          ensure-before!
          (fn ensure-before! []
            (g/with-auto-evaluation-context evaluation-context
              (let [basis (:basis evaluation-context)]
                (testing "Explicit connections."
                  (is (= [[original-target-node-id :array-input]] (ig/explicit-targets basis initial-source-one-node-id :property-output)))
                  (is (= [[original-target-node-id :array-input]] (ig/explicit-targets basis initial-source-two-node-id :property-output)))
                  (is (= [[first-order-override-target-node-id :array-input]] (ig/explicit-targets basis shadowing-source-one-node-id :property-output)))
                  (is (= [] (ig/explicit-targets basis shadowing-source-two-node-id :property-output)))
                  (is (= [[initial-source-one-node-id :property-output] [initial-source-two-node-id :property-output]] (ig/explicit-sources basis original-target-node-id :array-input)))
                  (is (= [[shadowing-source-one-node-id :property-output]] (ig/explicit-sources basis first-order-override-target-node-id :array-input)))
                  (is (= [] (ig/explicit-sources basis second-order-override-target-node-id :array-input))))

                (testing "Implicit connections."
                  (is (= [[original-target-node-id :array-input]] (g/targets basis initial-source-one-node-id :property-output)))
                  (is (= [[original-target-node-id :array-input]] (g/targets basis initial-source-two-node-id :property-output)))
                  (is (= [[first-order-override-target-node-id :array-input] [second-order-override-target-node-id :array-input]] (g/targets basis shadowing-source-one-node-id :property-output)))
                  (is (= [] (g/targets basis shadowing-source-two-node-id :property-output)))
                  (is (= [[initial-source-one-node-id :property-output] [initial-source-two-node-id :property-output]] (g/sources basis original-target-node-id :array-input)))
                  (is (= [[shadowing-source-one-node-id :property-output]] (g/sources basis first-order-override-target-node-id :array-input)))
                  (is (= [[shadowing-source-one-node-id :property-output]] (g/sources basis second-order-override-target-node-id :array-input))))

                (testing "Internal arc tables."
                  (is (= [[initial-source-one-node-id :property-output original-target-node-id :array-input]]
                         (helpers/source-arc-table-tuples basis graph-id initial-source-one-node-id :property-output)))
                  (is (= [[initial-source-two-node-id :property-output original-target-node-id :array-input]]
                         (helpers/source-arc-table-tuples basis graph-id initial-source-two-node-id :property-output)))
                  (is (= [[initial-source-one-node-id :property-output original-target-node-id :array-input]
                          [initial-source-two-node-id :property-output original-target-node-id :array-input]]
                         (helpers/target-arc-table-tuples basis graph-id original-target-node-id :array-input)))
                  (is (= [[shadowing-source-one-node-id :property-output first-order-override-target-node-id :array-input]]
                         (helpers/source-arc-table-tuples basis graph-id shadowing-source-one-node-id :property-output)
                         (helpers/target-arc-table-tuples basis graph-id first-order-override-target-node-id :array-input)))
                  (is (coll/empty? (helpers/source-arc-table-tuples basis graph-id shadowing-source-two-node-id :property-output))))

                (testing "Output values."
                  (is (= [:initial-source-one-value :initial-source-two-value] (g/node-value original-target-node-id :array-output evaluation-context)))
                  (is (= [:shadowing-source-one-value] (g/node-value first-order-override-target-node-id :array-output evaluation-context)))
                  (is (= [:shadowing-source-one-value] (g/node-value second-order-override-target-node-id :array-output evaluation-context)))))))

          ensure-after!
          (fn ensure-after! []
            (g/with-auto-evaluation-context evaluation-context
              (let [basis (:basis evaluation-context)]
                (testing "Explicit connections."
                  (is (= [[original-target-node-id :array-input]] (ig/explicit-targets basis initial-source-one-node-id :property-output)))
                  (is (= [[original-target-node-id :array-input]] (ig/explicit-targets basis initial-source-two-node-id :property-output)))
                  (is (= [[first-order-override-target-node-id :array-input]] (ig/explicit-targets basis shadowing-source-one-node-id :property-output)))
                  (is (= [[first-order-override-target-node-id :array-input]] (ig/explicit-targets basis shadowing-source-two-node-id :property-output)))
                  (is (= [[initial-source-one-node-id :property-output] [initial-source-two-node-id :property-output]] (ig/explicit-sources basis original-target-node-id :array-input)))
                  (is (= [[shadowing-source-one-node-id :property-output] [shadowing-source-two-node-id :property-output]] (ig/explicit-sources basis first-order-override-target-node-id :array-input)))
                  (is (= [] (ig/explicit-sources basis second-order-override-target-node-id :array-input))))

                (testing "Implicit connections."
                  (is (= [[original-target-node-id :array-input]] (g/targets basis initial-source-one-node-id :property-output)))
                  (is (= [[original-target-node-id :array-input]] (g/targets basis initial-source-two-node-id :property-output)))
                  (is (= [[first-order-override-target-node-id :array-input] [second-order-override-target-node-id :array-input]] (g/targets basis shadowing-source-one-node-id :property-output)))
                  (is (= [[first-order-override-target-node-id :array-input] [second-order-override-target-node-id :array-input]] (g/targets basis shadowing-source-two-node-id :property-output)))
                  (is (= [[initial-source-one-node-id :property-output] [initial-source-two-node-id :property-output]] (g/sources basis original-target-node-id :array-input)))
                  (is (= [[shadowing-source-one-node-id :property-output] [shadowing-source-two-node-id :property-output]] (g/sources basis first-order-override-target-node-id :array-input)))
                  (is (= [[shadowing-source-one-node-id :property-output] [shadowing-source-two-node-id :property-output]] (g/sources basis second-order-override-target-node-id :array-input))))

                (testing "Internal arc tables."
                  (is (= [[initial-source-one-node-id :property-output original-target-node-id :array-input]]
                         (helpers/source-arc-table-tuples basis graph-id initial-source-one-node-id :property-output)))
                  (is (= [[initial-source-two-node-id :property-output original-target-node-id :array-input]]
                         (helpers/source-arc-table-tuples basis graph-id initial-source-two-node-id :property-output)))
                  (is (= [[initial-source-one-node-id :property-output original-target-node-id :array-input]
                          [initial-source-two-node-id :property-output original-target-node-id :array-input]]
                         (helpers/target-arc-table-tuples basis graph-id original-target-node-id :array-input)))
                  (is (= [[shadowing-source-one-node-id :property-output first-order-override-target-node-id :array-input]]
                         (helpers/source-arc-table-tuples basis graph-id shadowing-source-one-node-id :property-output)))
                  (is (= [[shadowing-source-two-node-id :property-output first-order-override-target-node-id :array-input]]
                         (helpers/source-arc-table-tuples basis graph-id shadowing-source-two-node-id :property-output)))
                  (is (= [[shadowing-source-one-node-id :property-output first-order-override-target-node-id :array-input]
                          [shadowing-source-two-node-id :property-output first-order-override-target-node-id :array-input]]
                         (helpers/target-arc-table-tuples basis graph-id first-order-override-target-node-id :array-input))))

                (testing "Output values."
                  (is (= [:initial-source-one-value :initial-source-two-value] (g/node-value original-target-node-id :array-output evaluation-context)))
                  (is (= [:shadowing-source-one-value :shadowing-source-two-value] (g/node-value first-order-override-target-node-id :array-output evaluation-context)))
                  (is (= [:shadowing-source-one-value :shadowing-source-two-value] (g/node-value second-order-override-target-node-id :array-output evaluation-context)))))))]

      (testing "Before transact."
        (ensure-before!))

      (testing "Transact."
        (g/transact
          (g/connect shadowing-source-two-node-id :property-output first-order-override-target-node-id :array-input))
        (ensure-after!))

      (testing "Undo."
        (g/undo! :undo/global)
        (ensure-before!))

      (testing "Redo."
        (g/redo! :undo/global)
        (ensure-after!)))))

(deftest override-node-creation-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph!)

          [indirectly-owned-node-id
           directly-owned-node-id
           owner-node-id
           first-order-override-owner-node-id
           second-order-override-owner-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/make-nodes graph-id
                [indirectly-owned-node-id helpers/OverrideTestNode
                 directly-owned-node-id helpers/OverrideTestNode
                 owner-node-id helpers/OverrideTestNode]
                (g/connect indirectly-owned-node-id :property-output directly-owned-node-id :regular-cascade-delete-input)
                (g/override owner-node-id nil
                  (fn [_evaluation-context id-lookup]
                    (let [first-order-override-owner-node-id (get id-lookup owner-node-id)]
                      (g/override first-order-override-owner-node-id)))))))

          ensure-before!
          (fn ensure-before! []
            (let [basis (g/now)
                  graph (get-in basis [:graphs graph-id])]
              (is (empty? (g/overrides basis directly-owned-node-id)))
              (is (empty? (g/overrides basis indirectly-owned-node-id)))
              (is (= [[indirectly-owned-node-id :property-output directly-owned-node-id :regular-cascade-delete-input]]
                     (helpers/source-arc-table-tuples basis graph-id indirectly-owned-node-id :property-output)
                     (helpers/target-arc-table-tuples basis graph-id directly-owned-node-id :regular-cascade-delete-input)))
              (is (coll/empty? (helpers/source-arc-table-tuples basis graph-id directly-owned-node-id :regular-cascade-delete-output)))
              (is (coll/empty? (helpers/target-arc-table-tuples basis graph-id owner-node-id :regular-cascade-delete-input)))
              (is (= #{indirectly-owned-node-id
                       directly-owned-node-id
                       owner-node-id
                       first-order-override-owner-node-id
                       second-order-override-owner-node-id}
                     (set (g/node-ids graph))))))

          ensure-after!
          (fn ensure-after! []
            (let [basis (g/now)
                  graph (get-in basis [:graphs graph-id])
                  [first-order-override-directly-owned-node-id :as overrides-of-directly-owned-node-id] (g/overrides basis directly-owned-node-id)
                  [first-order-override-indirectly-owned-node-id :as overrides-of-indirectly-owned-node-id] (g/overrides basis indirectly-owned-node-id)
                  [second-order-override-directly-owned-node-id :as overrides-of-first-order-override-directly-owned-node-id] (g/overrides basis first-order-override-directly-owned-node-id)
                  [second-order-override-indirectly-owned-node-id :as overrides-of-first-order-override-indirectly-owned-node-id] (g/overrides basis first-order-override-indirectly-owned-node-id)]
              (is (= 1 (count overrides-of-directly-owned-node-id)))
              (is (= 1 (count overrides-of-indirectly-owned-node-id)))
              (is (= 1 (count overrides-of-first-order-override-directly-owned-node-id)))
              (is (= 1 (count overrides-of-first-order-override-indirectly-owned-node-id)))
              (is (g/node-id? first-order-override-directly-owned-node-id))
              (is (g/node-id? first-order-override-indirectly-owned-node-id))
              (is (g/node-id? second-order-override-directly-owned-node-id))
              (is (g/node-id? second-order-override-indirectly-owned-node-id))
              (is (= #{indirectly-owned-node-id
                       directly-owned-node-id
                       owner-node-id
                       first-order-override-owner-node-id
                       second-order-override-owner-node-id
                       first-order-override-directly-owned-node-id
                       second-order-override-directly-owned-node-id
                       first-order-override-indirectly-owned-node-id
                       second-order-override-indirectly-owned-node-id}
                     (set (g/node-ids graph))))
              (is (= [[indirectly-owned-node-id :property-output directly-owned-node-id :regular-cascade-delete-input]]
                     (helpers/source-arc-table-tuples basis graph-id indirectly-owned-node-id :property-output)
                     (helpers/target-arc-table-tuples basis graph-id directly-owned-node-id :regular-cascade-delete-input)))
              (is (= [[directly-owned-node-id :regular-cascade-delete-output owner-node-id :regular-cascade-delete-input]]
                     (helpers/source-arc-table-tuples basis graph-id directly-owned-node-id :regular-cascade-delete-output)
                     (helpers/target-arc-table-tuples basis graph-id owner-node-id :regular-cascade-delete-input)))))]

      (testing "Before transact."
        (ensure-before!))

      (testing "Transact."
        (let [[first-order-override-directly-owned-node-id
               first-order-override-indirectly-owned-node-id
               second-order-override-directly-owned-node-id
               second-order-override-indirectly-owned-node-id
               :as created-node-ids]
              (g/tx-nodes-added
                (g/transact
                  (g/connect directly-owned-node-id :regular-cascade-delete-output owner-node-id :regular-cascade-delete-input)))]
          (is (= 4 (count created-node-ids)))
          (is (= directly-owned-node-id (g/override-original first-order-override-directly-owned-node-id)))
          (is (= indirectly-owned-node-id (g/override-original first-order-override-indirectly-owned-node-id)))
          (is (= first-order-override-directly-owned-node-id (g/override-original second-order-override-directly-owned-node-id)))
          (is (= first-order-override-indirectly-owned-node-id (g/override-original second-order-override-indirectly-owned-node-id)))
          (is (= [first-order-override-directly-owned-node-id] (g/overrides directly-owned-node-id)))
          (is (= [first-order-override-indirectly-owned-node-id] (g/overrides indirectly-owned-node-id)))
          (is (= [second-order-override-directly-owned-node-id] (g/overrides first-order-override-directly-owned-node-id)))
          (is (= [second-order-override-indirectly-owned-node-id] (g/overrides first-order-override-indirectly-owned-node-id))))
        (ensure-after!))

      (testing "Undo."
        (g/undo! :undo/global)
        (ensure-before!))

      (testing "Redo."
        (g/redo! :undo/global)
        (ensure-after!)))))

(deftest override-node-creation-with-limited-traversal-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph!)

          traverse-fn
          (g/make-override-traverse-fn
            (fn limited-override-traverse-fn [basis arc]
              (is (gt/basis? basis))
              (= :regular-cascade-delete-output (gt/source-label arc))))

          [indirectly-owned-node-id
           directly-owned-node-id
           owner-node-id
           first-order-override-owner-node-id
           second-order-override-owner-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/make-nodes graph-id
                [indirectly-owned-node-id helpers/OverrideTestNode
                 directly-owned-node-id helpers/OverrideTestNode
                 owner-node-id helpers/OverrideTestNode]
                (g/connect indirectly-owned-node-id :property-output directly-owned-node-id :regular-cascade-delete-input)
                (g/override owner-node-id {:traverse-fn traverse-fn}
                  (fn [_evaluation-context id-lookup]
                    (let [first-order-override-owner-node-id (get id-lookup owner-node-id)]
                      (g/override first-order-override-owner-node-id {:traverse-fn traverse-fn})))))))

          ensure-before!
          (fn ensure-before! []
            (let [basis (g/now)
                  graph (get-in basis [:graphs graph-id])]
              (is (empty? (g/overrides basis directly-owned-node-id)))
              (is (empty? (g/overrides basis indirectly-owned-node-id)))
              (is (= #{indirectly-owned-node-id
                       directly-owned-node-id
                       owner-node-id
                       first-order-override-owner-node-id
                       second-order-override-owner-node-id}
                     (set (g/node-ids graph))))))

          ensure-after!
          (fn ensure-after! []
            (let [basis (g/now)
                  graph (get-in basis [:graphs graph-id])
                  [first-order-override-directly-owned-node-id :as overrides-of-directly-owned-node-id] (g/overrides basis directly-owned-node-id)
                  [second-order-override-directly-owned-node-id :as overrides-of-first-order-override-directly-owned-node-id] (g/overrides basis first-order-override-directly-owned-node-id)]
              (is (= 1 (count overrides-of-directly-owned-node-id)))
              (is (= 0 (count (g/overrides basis indirectly-owned-node-id))))
              (is (= 1 (count overrides-of-first-order-override-directly-owned-node-id)))
              (is (= directly-owned-node-id (g/override-original basis first-order-override-directly-owned-node-id)))
              (is (= first-order-override-directly-owned-node-id (g/override-original basis second-order-override-directly-owned-node-id)))
              (is (= #{indirectly-owned-node-id
                       directly-owned-node-id
                       owner-node-id
                       first-order-override-owner-node-id
                       second-order-override-owner-node-id
                       first-order-override-directly-owned-node-id
                       second-order-override-directly-owned-node-id}
                     (set (g/node-ids graph))))))]

      (testing "Before transact."
        (ensure-before!))

      (testing "Transact."
        (let [[first-order-override-directly-owned-node-id
               second-order-override-directly-owned-node-id
               :as created-node-ids]
              (g/tx-nodes-added
                (g/transact
                  (g/connect directly-owned-node-id :regular-cascade-delete-output owner-node-id :regular-cascade-delete-input)))]
          (is (= 2 (count created-node-ids)))
          (is (= directly-owned-node-id (g/override-original first-order-override-directly-owned-node-id)))
          (is (= first-order-override-directly-owned-node-id (g/override-original second-order-override-directly-owned-node-id)))
          (is (= [first-order-override-directly-owned-node-id] (g/overrides directly-owned-node-id)))
          (is (empty? (g/overrides indirectly-owned-node-id)))
          (is (= [second-order-override-directly-owned-node-id] (g/overrides first-order-override-directly-owned-node-id))))
        (ensure-after!))

      (testing "Undo."
        (g/undo! :undo/global)
        (ensure-before!))

      (testing "Redo."
        (g/redo! :undo/global)
        (ensure-after!)))))

(deftest override-node-creation-with-init-props-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph!)

          [indirectly-owned-node-id
           directly-owned-node-id
           owner-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/make-nodes graph-id
                [indirectly-owned-node-id [helpers/OverrideTestNode :property :indirectly-owned-property-value]
                 directly-owned-node-id [helpers/OverrideTestNode :property :directly-owned-property-value]
                 _owner-node-id [helpers/OverrideTestNode :property :owner-property-value]]
                (g/connect indirectly-owned-node-id :regular-cascade-delete-output directly-owned-node-id :regular-cascade-delete-input))))

          init-props-fn
          (fn init-props-fn [original-property-value->overridden-property-value basis original-node-id node-type]
            (is (gt/basis? basis))
            (is (g/node-id? original-node-id))
            (is (= helpers/OverrideTestNode node-type))
            (is (= helpers/OverrideTestNode (g/node-type* basis original-node-id)))
            (let [original-property-value (g/raw-property-value basis original-node-id :property)
                  overridden-property-value (original-property-value->overridden-property-value original-property-value)]
              (is (contains? original-property-value->overridden-property-value original-property-value))
              {:property overridden-property-value}))

          [first-order-override-owner-node-id
           second-order-override-owner-node-id]
          (g/tx-nodes-added
            (g/transact
              (g/override
                owner-node-id
                {:init-props-fn
                 (partial
                   init-props-fn
                   {:owner-property-value :first-order-override-owner-property-value
                    :directly-owned-property-value :first-order-override-directly-owned-property-value
                    :indirectly-owned-property-value :first-order-override-indirectly-owned-property-value})}
                (fn [_evaluation-context id-lookup]
                  (let [first-order-override-owner-node-id (get id-lookup owner-node-id)]
                    (g/override
                      first-order-override-owner-node-id
                      {:init-props-fn
                       (partial
                         init-props-fn
                         {:first-order-override-owner-property-value :second-order-override-owner-property-value
                          :first-order-override-directly-owned-property-value :second-order-override-directly-owned-property-value
                          :first-order-override-indirectly-owned-property-value :second-order-override-indirectly-owned-property-value})}))))))

          assert-property-value!
          (fn assert-property-value! [basis evaluation-context node-id property-value]
            (is (= property-value
                   (g/raw-property-value basis node-id :property)
                   (g/node-value node-id :property-output evaluation-context))))

          ensure-before!
          (fn ensure-before! []
            (g/with-auto-evaluation-context evaluation-context
              (let [basis (:basis evaluation-context)]
                (assert-property-value! basis evaluation-context owner-node-id :owner-property-value)
                (assert-property-value! basis evaluation-context directly-owned-node-id :directly-owned-property-value)
                (assert-property-value! basis evaluation-context indirectly-owned-node-id :indirectly-owned-property-value)
                (assert-property-value! basis evaluation-context first-order-override-owner-node-id :first-order-override-owner-property-value)
                (assert-property-value! basis evaluation-context second-order-override-owner-node-id :second-order-override-owner-property-value)
                (is (empty? (g/overrides basis directly-owned-node-id)))
                (is (empty? (g/overrides basis indirectly-owned-node-id))))))

          ensure-after!
          (fn ensure-after! []
            (g/with-auto-evaluation-context evaluation-context
              (let [basis (:basis evaluation-context)
                    [first-order-override-directly-owned-node-id :as overrides-of-directly-owned-node-id] (g/overrides basis directly-owned-node-id)
                    [first-order-override-indirectly-owned-node-id :as overrides-of-indirectly-owned-node-id] (g/overrides basis indirectly-owned-node-id)
                    [second-order-override-directly-owned-node-id :as overrides-of-first-order-override-directly-owned-node-id] (g/overrides basis first-order-override-directly-owned-node-id)
                    [second-order-override-indirectly-owned-node-id :as overrides-of-first-order-override-indirectly-owned-node-id] (g/overrides basis first-order-override-indirectly-owned-node-id)]
                (is (= 1 (count overrides-of-directly-owned-node-id)))
                (is (= 1 (count overrides-of-indirectly-owned-node-id)))
                (is (= 1 (count overrides-of-first-order-override-directly-owned-node-id)))
                (is (= 1 (count overrides-of-first-order-override-indirectly-owned-node-id)))
                (assert-property-value! basis evaluation-context owner-node-id :owner-property-value)
                (assert-property-value! basis evaluation-context directly-owned-node-id :directly-owned-property-value)
                (assert-property-value! basis evaluation-context indirectly-owned-node-id :indirectly-owned-property-value)
                (assert-property-value! basis evaluation-context first-order-override-owner-node-id :first-order-override-owner-property-value)
                (assert-property-value! basis evaluation-context second-order-override-owner-node-id :second-order-override-owner-property-value)
                (assert-property-value! basis evaluation-context first-order-override-directly-owned-node-id :first-order-override-directly-owned-property-value)
                (assert-property-value! basis evaluation-context second-order-override-directly-owned-node-id :second-order-override-directly-owned-property-value)
                (assert-property-value! basis evaluation-context first-order-override-indirectly-owned-node-id :first-order-override-indirectly-owned-property-value)
                (assert-property-value! basis evaluation-context second-order-override-indirectly-owned-node-id :second-order-override-indirectly-owned-property-value))))]

      (testing "Before transact."
        (ensure-before!))

      (testing "Transact."
        (let [created-node-ids
              (g/tx-nodes-added
                (g/transact
                  (g/connect directly-owned-node-id :regular-cascade-delete-output owner-node-id :regular-cascade-delete-input)))]
          (is (= 4 (count created-node-ids))))
        (ensure-after!))

      (testing "Undo."
        (g/undo! :undo/global)
        (ensure-before!))

      (testing "Redo."
        (g/redo! :undo/global)
        (ensure-after!)))))

(deftest override-node-creation-from-non-undoable-connect-test
  (test-support/with-clean-system
    (let [graph-id (g/make-graph!)

          [target-node-id
           source-node-id
           override-target-node-id]
          (g/tx-nodes-added
            (g/transact
              {:undoable false}
              (g/make-nodes graph-id
                [target-node-id helpers/ConnectionTargetNode
                 source-node-id [helpers/ConnectionSourceNode :property :before]]
                (g/override target-node-id))))

          ensure-connected!
          (fn ensure-connected! []
            (let [[override-source-node-id] (g/overrides source-node-id)]
              (is (= [override-target-node-id] (g/overrides target-node-id)))
              (is (some? override-source-node-id))
              (is (= target-node-id (g/override-original override-target-node-id)))
              (is (= source-node-id (g/override-original override-source-node-id)))
              (is (= [[source-node-id :property-output]] (g/sources-of target-node-id :regular-cascade-delete-input)))
              (is (= [[override-source-node-id :property-output]] (g/sources-of override-target-node-id :regular-cascade-delete-input)))))

          ensure-disconnected!
          (fn ensure-disconnected! []
            (is (= [override-target-node-id] (g/overrides target-node-id)))
            (is (empty? (g/overrides source-node-id)))
            (is (= target-node-id (g/override-original override-target-node-id)))
            (is (= [] (g/sources-of target-node-id :regular-cascade-delete-input)))
            (is (= [] (g/sources-of override-target-node-id :regular-cascade-delete-input))))]

      (testing "Before transact."
        (is (= :before (g/node-value source-node-id :property-output)))
        (ensure-disconnected!))

      (testing "Transact."
        (g/transact
          (concat
            (g/set-property source-node-id :property :after)
            (g/non-undoable
              (g/connect source-node-id :property-output target-node-id :regular-cascade-delete-input))))
        (is (= :after (g/node-value source-node-id :property-output)))
        (ensure-connected!))

      (testing "Undo."
        (g/undo! :undo/global)
        (is (= :before (g/node-value source-node-id :property-output)))
        (ensure-connected!))

      (testing "Redo."
        (g/redo! :undo/global)
        (is (= :after (g/node-value source-node-id :property-output)))
        (ensure-connected!)))))
