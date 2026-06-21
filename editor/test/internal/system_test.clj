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

(ns internal.system-test
  (:require [clojure.string :as str]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.localization :as localization]
            [internal.graph.types :as gt]
            [internal.system :as is]
            [support.test-support :as ts]))

(g/defnode Root
  (property where g/Str)
  (property touched g/Num))

(defn graphs        []    (is/graphs        @g/*the-system*))
(defn graph         [gid] (is/graph         @g/*the-system* gid))

(defn- undo-states
  []
  (let [undo (is/undo @g/*the-system* :undo/global)]
    (concat (is/undo-stack undo) (is/redo-stack undo))))

(defn undo-redo-states
  []
  (let [undo (is/undo @g/*the-system* :undo/global)]
    [(map :label (is/undo-stack undo))
     (map :label (is/redo-stack undo))]))

(deftest graph-registration
  (testing "a fresh system has a graph"
    (ts/with-clean-system
      (is (= 1 (count (graphs))))))

  (testing "a graph can be added to the system"
    (ts/with-clean-system
      (let [gid        (g/make-graph!)
            all-graphs (graphs)
            all-gids   (into #{} (map :_graph-id (vals all-graphs)))]
        (is (= 2 (count all-graphs)))
        (is (all-gids gid)))))

  (testing "a graph can be removed from the system"
    (ts/with-clean-system
      (let [gid (g/make-graph!)
            g   (graph gid)
            _   (g/delete-graph! gid)]
        (is (= 1 (count (graphs)))))))

  (testing "a graph can be found by its id"
    (ts/with-clean-system
      (let [gid (g/make-graph!)
            g'  (graph gid)]
        (is (= (:_graph-id g') gid))))))

(deftest tx-id
  (testing "graph time advances with transactions"
    (ts/with-clean-system
      (let [gid       (g/make-graph!)
            before    (is/graph-time @g/*the-system* gid)
            tx-report (g/transact (g/make-node gid Root))
            after     (is/graph-time @g/*the-system* gid)]
        (is (= :ok (:status tx-report)))
        (is (< before after))))))

(deftest undo-capture
  (testing "undoable actions are stored"
    (ts/with-clean-system
      (let [pgraph-id (g/make-graph!)
            before (is/graph-time @g/*the-system* pgraph-id)
            undo-before (undo-states)
            tx-report (g/transact (g/make-node pgraph-id Root))
            after (is/graph-time @g/*the-system* pgraph-id)
            undo-after (undo-states)]
        (is (= :ok (:status tx-report)))
        (is (< before after))
        (is (< (count undo-before) (count undo-after))))))

  (testing "transaction labels appear in undo"
    (ts/with-clean-system
      (let [pgraph-id (g/make-graph!)
            tx-report (g/transact [(g/make-node pgraph-id Root)
                                   (g/operation-label (localization/message "operation.build-root"))])
            root (first (g/tx-nodes-added tx-report))
            _ (g/transact [(g/set-property root :touched 1)
                           (g/operation-label (localization/message "operation.increment-touch-count"))])
            undos-after (is/undo-stack (is/undo @g/*the-system* :undo/global))
            redos-after (is/redo-stack (is/undo @g/*the-system* :undo/global))
            snapshot @g/*the-system*]
        (is (= [(localization/message "operation.build-root")
                (localization/message "operation.increment-touch-count")]
               (mapv :label undos-after)))
        (is (= [] (mapv :label redos-after)))
        (let [system-after-undo (is/undo-action snapshot :undo/global)
              undos-after-undo (is/undo-stack (is/undo system-after-undo :undo/global))
              redos-after-undo (is/redo-stack (is/undo system-after-undo :undo/global))]
          (is (= [(localization/message "operation.build-root")]
                 (mapv :label undos-after-undo)))
          (is (= [(localization/message "operation.increment-touch-count")]
                 (mapv :label redos-after-undo)))))))

  (testing "transaction labels alone do not create undo steps"
    (ts/with-clean-system
      (g/transact
        [(g/operation-label "label only")
         (g/operation-sequence :label-only)])

      (is (= 0 (g/undo-stack-count :undo/global)))))

  (testing "has-undo? and has-redo?"
    (ts/with-clean-system
      (let [pgraph-id (g/make-graph!)]

        (is (not (g/has-undo? :undo/global)))
        (is (not (g/has-redo? :undo/global)))

        (let [root (g/make-node! pgraph-id Root)]

          (is (g/has-undo? :undo/global))
          (is (not (g/has-redo? :undo/global)))

          (g/transact (g/set-property root :touched 1))

          (is (g/has-undo? :undo/global))
          (is (not (g/has-redo? :undo/global)))

          (g/undo! :undo/global)

          (is (g/has-undo? :undo/global))
          (is (g/has-redo? :undo/global))))))

  (testing "undo can be cleared"
    (ts/with-clean-system
      (let [pgraph-id (g/make-graph!)]

        (is (not (g/has-undo? :undo/global)))
        (is (not (g/has-redo? :undo/global)))

        (let [[root] (ts/tx-nodes (g/make-node pgraph-id Root))]

          (is (g/has-undo? :undo/global))
          (is (not (g/has-redo? :undo/global)))

          (g/transact (g/set-property root :touched 1))

          (is (g/has-undo? :undo/global))
          (is (not (g/has-redo? :undo/global)))

          (g/undo! :undo/global)

          (is (g/has-undo? :undo/global))
          (is (g/has-redo? :undo/global))

          (g/reset-undo! :undo/global)

          (is (not (g/has-undo? :undo/global)))
          (is (not (g/has-redo? :undo/global)))))))

  (testing "undo is not graph-gated"
    (ts/with-clean-system
      (let [graph-id (g/make-graph!)]
        (is (not (g/has-undo? :undo/global)))

        (g/transact (g/make-node graph-id Root))

        (is (g/has-undo? :undo/global))))))

(deftest non-undoable-transactions
  (testing "non-undoable transactions are not appended to undo"
    (ts/with-clean-system
      (let [pgraph-id (g/make-graph!)
            [root] (ts/tx-nodes (g/make-node pgraph-id Root :where "initial" :touched 0))]
        (g/reset-undo! :undo/global)

        (g/transact
          (g/set-property root :where "undoable"))

        (is (= 1 (g/undo-stack-count :undo/global)))

        (g/transact {:undoable false}
          (g/set-property root :touched 42))

        (is (= 1 (g/undo-stack-count :undo/global)))

        (g/undo! :undo/global)

        (is (= "initial" (g/node-value root :where)))
        (is (= 42 (g/node-value root :touched))))))

  (testing "nested non-undoable transaction data is not reverted by undo"
    (ts/with-clean-system
      (let [pgraph-id (g/make-graph!)
            [root] (ts/tx-nodes (g/make-node pgraph-id Root :where "initial" :touched 0))]
        (g/reset-undo! :undo/global)

        (g/transact
          (concat
            (g/set-property root :where "undoable")
            (g/non-undoable
              (g/set-property root :touched 42))))

        (is (= 1 (g/undo-stack-count :undo/global)))

        (g/undo! :undo/global)

        (is (= "initial" (g/node-value root :where)))
        (is (= 42 (g/node-value root :touched))))))

  (testing "nested non-undoable transaction data does not make labels undoable"
    (ts/with-clean-system
      (let [pgraph-id (g/make-graph!)
            [root] (ts/tx-nodes (g/make-node pgraph-id Root :where "initial" :touched 0))]
        (g/reset-undo! :undo/global)

        (g/transact
          [(g/operation-label "non-undoable touch")
           (g/non-undoable
             (g/set-property root :touched 42))])

        (is (= 0 (g/undo-stack-count :undo/global)))
        (is (= 42 (g/node-value root :touched))))))

  (testing "later non-undoable transactions to the same property are reverted"
    (ts/with-clean-system
      (let [pgraph-id (g/make-graph!)
            [root] (ts/tx-nodes (g/make-node pgraph-id Root :where "initial"))]
        (g/reset-undo! :undo/global)

        (g/transact
          (g/set-property root :where "undoable"))

        (g/transact {:undoable false}
          (g/set-property root :where "non-undoable"))

        (g/undo! :undo/global)

        (is (= "initial" (g/node-value root :where))))))

  (testing "later non-undoable clears to the same property are reverted"
    (ts/with-clean-system
      (let [pgraph-id (g/make-graph!)
            [root] (ts/tx-nodes (g/make-node pgraph-id Root :where "initial"))
            [override-root] (ts/tx-nodes (g/override root))]
        (g/reset-undo! :undo/global)

        (g/transact
          (g/set-property override-root :where "undoable"))

        (g/transact {:undoable false}
          (g/clear-property override-root :where))

        (g/undo! :undo/global)

        (is (= "initial" (g/node-value override-root :where)))))))

(defn touch
  [node label & [seq-id]]
  (g/transact (keep identity
                     [(g/operation-label label)
                      (when seq-id
                        (g/operation-sequence seq-id))
                      (g/set-property node :touched label)])))

(deftest undo-coalescing
  (testing "Transactions with no sequence-id each make an undo point"
    (ts/with-clean-system
      (let [pgraph-id (g/make-graph!)]

        (is (= (undo-redo-states) [[] []]))

        (let [[root] (ts/tx-nodes (g/make-node pgraph-id Root))]

          (is (= (undo-redo-states) [[nil] []]))

          (touch root 1)
          (touch root 2)
          (touch root 3)

          (is (= (undo-redo-states) [[nil 1 2 3] []]))

          (g/undo! :undo/global)

          (is (= (undo-redo-states) [[nil 1 2] [3]]))))))

  (testing "Transactions with different sequence-ids each make an undo point"
    (ts/with-clean-system
      (let [pgraph-id (g/make-graph!)]

        (is (= (undo-redo-states) [[] []]))

        (let [[root] (ts/tx-nodes (g/make-node pgraph-id Root))]

          (is (= (undo-redo-states) [[nil] []]))

          (touch root 1 :a)
          (touch root 2 :b)
          (touch root 3 :c)

          (is (= (undo-redo-states) [[nil 1 2 3] []]))

          (g/undo! :undo/global)

          (is (= (undo-redo-states) [[nil 1 2] [3]]))))))

  (testing "Transactions with the same sequence-id are merged together"
    (ts/with-clean-system
      (let [pgraph-id (g/make-graph!)]

        (is (= (undo-redo-states) [[] []]))

        (let [[root] (ts/tx-nodes (g/make-node pgraph-id Root))]

          (is (= (undo-redo-states) [[nil] []]))

          (touch root 1 :a)
          (touch root 1.1 :a)
          (touch root 1.2 :a)
          (touch root 1.3 :a)
          (touch root 1.4 :a)
          (touch root 1.5 :a)
          (touch root 1.6 :a)
          (touch root 1.7 :a)
          (touch root 1.8 :a)
          (touch root 1.9 :a)
          (touch root 2 :a)
          (touch root 3 :c)

          (is (= (undo-redo-states) [[nil 2 3] []]))

          (g/undo! :undo/global)

          (is (= (undo-redo-states) [[nil 2] [3]]))

          (g/undo! :undo/global)

          (is (= (undo-redo-states) [[nil] [3 2]]))))))

  (testing "Canceling the current sequence leaves nothing new in undo"
    (ts/with-clean-system
      (let [pgraph-id (g/make-graph!)]

        (is (= (undo-redo-states) [[] []]))

        (let [[root] (ts/tx-nodes (g/make-node pgraph-id Root))]

          (is (= (undo-redo-states) [[nil] []]))

          (touch root 1 :a)
          (touch root 2 :b)
          (touch root 2.1 :b)
          (touch root 2.2 :b)
          (touch root 2.3 :b)
          (touch root 2.4 :b)
          (touch root 2.5 :b)
          (touch root 2.6 :b)
          (touch root 2.7 :b)
          (touch root 2.8 :b)
          (touch root 2.9 :b)

          (g/cancel! :undo/global :b)

          (is (= (undo-redo-states) [[nil 1] []]))

          (touch root 3 :c)

          (is (= (undo-redo-states) [[nil 1 3] []]))

          (g/undo! :undo/global)

          (is (= (undo-redo-states) [[nil 1] [3]]))

          (g/undo! :undo/global)

          (is (= (undo-redo-states) [[nil] [3 1]]))))))

  (testing "Canceling a sequence that is not the current sequence does nothing"
    (ts/with-clean-system
      (let [pgraph-id (g/make-graph!)]

        (is (= (undo-redo-states) [[] []]))

        (let [[root] (ts/tx-nodes (g/make-node pgraph-id Root))]

          (is (= (undo-redo-states) [[nil] []]))

          (touch root 1 :a)
          (touch root 2 :b)
          (touch root 2.1 :b)
          (touch root 2.2 :b)
          (touch root 2.3 :b)
          (touch root 2.4 :b)
          (touch root 2.5 :b)
          (touch root 2.6 :b)
          (touch root 2.7 :b)
          (touch root 2.8 :b)
          (touch root 2.9 :b)

          (g/cancel! :undo/global :a)

          (is (= (undo-redo-states) [[nil 1 2.9] []]))))))

  (testing "Cross-graph transactions create a single undo point"
    (ts/with-clean-system
      (let [pgraph-id (g/make-graph!)
            agraph-id (g/make-graph!)]

        (let [[node-p] (ts/tx-nodes (g/make-node pgraph-id Root :where "graph P"))
              [node-a] (ts/tx-nodes (g/make-node agraph-id Root :where "graph A"))]

          (is (= (undo-redo-states) [[nil nil] []]))

          (touch node-p 1 :a)

          (g/transact [(g/set-property node-p :touched 2)
                       (g/set-property node-a :touched 2)
                       (g/operation-label 2)
                       (g/operation-sequence :a)])

          (touch node-p 3 :c)

          (is (= (undo-redo-states) [[nil nil 2 3] []]))

          (g/undo! :undo/global)

          (is (= (undo-redo-states) [[nil nil 2] [3]]))

          (g/undo! :undo/global)

          (is (= (undo-redo-states) [[nil nil] [3 2]])))))))

(g/defnode Source
  (property source-label g/Str))

(g/defnode Pipe
  (input target-label g/Str)
  (output soft g/Str (g/fnk [target-label] (str/lower-case target-label))))

(g/defnode Sink
  (input target-label g/Str)
  (output loud g/Str :cached (g/fnk [target-label] (when target-label (str/upper-case target-label)))))

(deftest tracing-across-graphs
  (ts/with-clean-system
    (let [pgraph-id (g/make-graph!)
          agraph-id (g/make-graph!)]

      (let [[source-p1 pipe-p1 sink-p1] (ts/tx-nodes (g/make-node pgraph-id Source :source-label "first")
                                                     (g/make-node pgraph-id Pipe)
                                                     (g/make-node pgraph-id Sink))

            [source-a1 sink-a1 sink-a2] (ts/tx-nodes (g/make-node agraph-id Source :source-label "second")
                                                     (g/make-node agraph-id Sink)
                                                     (g/make-node agraph-id Sink))]

        (g/transact
         [(g/connect source-p1 :source-label sink-p1 :target-label)
          (g/connect source-p1 :source-label pipe-p1 :target-label)
          (g/connect pipe-p1   :soft         sink-a1 :target-label)
          (g/connect source-a1 :source-label sink-a2 :target-label)])

        (is (= (ts/graph-dependencies [(gt/endpoint source-a1 :source-label)])
               #{(gt/endpoint sink-a2   :loud)
                 (gt/endpoint source-a1 :source-label)
                 (gt/endpoint source-a1 :_declared-properties)
                 (gt/endpoint source-a1 :_properties)}))

        (is (= (ts/graph-dependencies [(gt/endpoint source-p1 :source-label)])
               #{(gt/endpoint sink-p1   :loud)
                 (gt/endpoint pipe-p1   :soft)
                 (gt/endpoint sink-a1   :loud)
                 (gt/endpoint source-p1 :source-label)
                 (gt/endpoint source-p1 :_declared-properties)
                 (gt/endpoint source-p1 :_properties)}))))))

(g/defnode ChainedLink
  (input source-label g/Str)
  (output source-label g/Str :cached (g/fnk [source-label] (when source-label (str/upper-case source-label)))))

(defn- show-sarcs-tarcs [msg graph]
  (println msg
           "\n\t:sarcs " (get-in (g/now) [:graphs graph :sarcs])
           "\n\t:tarcs"  (get-in (g/now) [:graphs graph :tarcs])))

(deftest undo-restores-all-source-arcs
  (testing "Delete with cross-graph connections, undo, re-delete"
    (ts/with-clean-system
      (let [project-graph (g/make-graph!)
            view-graph (g/make-graph!)
            [source link sink] (ts/tx-nodes (g/make-node project-graph Source :source-label "from project graph")
                                            (g/make-node project-graph ChainedLink)
                                            (g/make-node view-graph Sink))]
        (g/transact
         (concat
          (g/connect source :source-label link :source-label)
          (g/connect link   :source-label sink :target-label)))

        (is (= "FROM PROJECT GRAPH" (g/node-value sink :loud)))
        (g/transact
         (g/set-property source :source-label "after change"))

        (g/delete-node! source)
        (is (= nil (g/node-value sink :loud)))

        (g/undo! :undo/global)
        (is (= "AFTER CHANGE" (g/node-value sink :loud)))

        (g/delete-node! source)
        (is (= nil (g/node-value sink :loud)))))))

(defn- successors [node-id label]
  (g/successors (g/now) node-id label))

(defn- sarcs [node-id label]
  (get-in @g/*the-system* [:graphs (g/node-id->graph-id node-id) :sarcs node-id label]))

(defn- tarcs [node-id label]
  (get-in @g/*the-system* [:graphs (g/node-id->graph-id node-id) :tarcs node-id label]))

(defn- cached?
  [endpoint]
  (contains? (is/system-cache @g/*the-system*) endpoint))

(defn- invalidate-count
  [endpoint]
  (get (is/invalidate-counters @g/*the-system*) endpoint 0))

(deftest undo-preserves-non-undoable-cross-graph-connection
  (testing "undo property change keeps non-undoable P->V connection and successors"
    (ts/with-clean-system
      (let [project-graph (g/make-graph!)
            view-graph (g/make-graph!)
            [p-source v-sink] (ts/tx-nodes
                                (g/make-node project-graph Source :source-label "initial value")
                                (g/make-node view-graph Sink))]

        (g/reset-undo! :undo/global)
        (g/transact {:undoable false}
          (g/connect p-source :source-label v-sink :target-label))

        (is (= 0 (count (ts/undo-stack project-graph))))
        (is (= "INITIAL VALUE" (g/node-value v-sink :loud)))
        (is (= #{(gt/endpoint p-source :_declared-properties)
                 (gt/endpoint p-source :source-label)
                 (gt/endpoint p-source :_properties)
                 (gt/endpoint v-sink :loud)}
               (set (successors p-source :source-label))))

        (g/transact
          (g/set-property p-source :source-label "edited"))

        (is (= 1 (count (ts/undo-stack project-graph))))
        (is (= "EDITED" (g/node-value v-sink :loud)))

        (g/undo! :undo/global)

        (is (= 0 (count (ts/undo-stack project-graph))))
        (is (= "INITIAL VALUE" (g/node-value v-sink :loud)))
        (is (= [[p-source :source-label v-sink :target-label]]
               (g/arcs->tuples (sarcs p-source :source-label))))
        (is (= [[p-source :source-label v-sink :target-label]]
               (g/arcs->tuples (tarcs v-sink :target-label))))
        (is (= #{(gt/endpoint p-source :_declared-properties)
                 (gt/endpoint p-source :source-label)
                 (gt/endpoint p-source :_properties)
                 (gt/endpoint v-sink :loud)}
               (set (successors p-source :source-label))))

        (g/redo! :undo/global)

        (is (= "EDITED" (g/node-value v-sink :loud)))))))

(deftest undo-redo-invalidates-modified-outputs
  (ts/with-clean-system
    (let [project-graph (g/make-graph!)
          [source sink unrelated-source unrelated-sink] (ts/tx-nodes
                                                          (g/make-node project-graph Source :source-label "initial")
                                                          (g/make-node project-graph Sink)
                                                          (g/make-node project-graph Source :source-label "unrelated")
                                                          (g/make-node project-graph Sink))
          sink-output (gt/endpoint sink :loud)
          unrelated-output (gt/endpoint unrelated-sink :loud)]
      (g/transact
        [(g/connect source :source-label sink :target-label)
         (g/connect unrelated-source :source-label unrelated-sink :target-label)])
      (g/reset-undo! :undo/global)

      (g/transact
        (g/set-property source :source-label "edited"))
      (is (= "EDITED" (g/node-value sink :loud)))
      (is (= "UNRELATED" (g/node-value unrelated-sink :loud)))

      (let [sink-invalidate-count (invalidate-count sink-output)
            unrelated-invalidate-count (invalidate-count unrelated-output)]
        (is (cached? sink-output))
        (is (cached? unrelated-output))

        (g/undo! :undo/global)

        (is (not (cached? sink-output)))
        (is (cached? unrelated-output))
        (is (< sink-invalidate-count (invalidate-count sink-output)))
        (is (= unrelated-invalidate-count (invalidate-count unrelated-output))))

      (is (= "INITIAL" (g/node-value sink :loud)))
      (is (cached? sink-output))

      (let [sink-invalidate-count (invalidate-count sink-output)]
        (g/redo! :undo/global)

        (is (not (cached? sink-output)))
        (is (< sink-invalidate-count (invalidate-count sink-output)))))))

(deftest undo-removes-user-data-for-deleted-nodes
  (ts/with-clean-system
    (let [project-graph (g/make-graph!)
          [source] (ts/tx-nodes (g/make-node project-graph Source :source-label "initial"))]
      (g/user-data! source ::undo-user-data :value)
      (is (= :value (g/user-data source ::undo-user-data)))

      (g/undo! :undo/global)

      (is (nil? (g/user-data source ::undo-user-data))))))

(deftest undo-reverts-cross-graph-connection
  (testing "undo connection P->V removes connection and successors"
    (ts/with-clean-system
      (let [project-graph (g/make-graph!)
            view-graph (g/make-graph!)
            [p-source p-source2 v-sink] (ts/tx-nodes
                                          (g/make-node project-graph Source :source-label "initial value")
                                          (g/make-node project-graph Source)
                                          (g/make-node view-graph Sink))]

        (is (= 1 (count (ts/undo-stack project-graph))))

        ;; This creates a dummy undo step that only touches p-source2 after the setup transaction.
        (g/transact
          (g/set-property p-source2 :source-label "dummy"))

        (is (= 2 (count (ts/undo-stack project-graph))))

        (is (= #{(gt/endpoint p-source :_declared-properties)
                 (gt/endpoint p-source :source-label)
                 (gt/endpoint p-source :_properties)}
               (set (successors p-source :source-label))))
        (is (= nil (sarcs p-source :source-label)))
        (is (= nil (tarcs v-sink :target-label)))

        (g/transact
          [(g/set-property p-source2 :source-label "whateverzzzzz") ; we include this action to ensure an undo entry is created
           (g/connect p-source :source-label v-sink :target-label)])

        (is (= 3 (count (ts/undo-stack project-graph))))

        (is (= #{(gt/endpoint p-source :_declared-properties)
                 (gt/endpoint p-source :source-label)
                 (gt/endpoint p-source :_properties)
                 (gt/endpoint v-sink :loud)}
               (set (successors p-source :source-label))))
        (is (= [[p-source :source-label v-sink :target-label]] (g/arcs->tuples (sarcs p-source :source-label))))
        (is (= [[p-source :source-label v-sink :target-label]] (g/arcs->tuples (tarcs v-sink :target-label))))

        (is (= "INITIAL VALUE" (g/node-value v-sink :loud)))

        (g/undo! :undo/global)

        (is (= 2 (count (ts/undo-stack project-graph))))

        (is (= #{(gt/endpoint p-source :_declared-properties)
                 (gt/endpoint p-source :source-label)
                 (gt/endpoint p-source :_properties)}
               (set (successors p-source :source-label))))
        (is (= [] (g/arcs->tuples (sarcs p-source :source-label))))
        (is (= [] (g/arcs->tuples (tarcs v-sink :target-label))))

        (is (= nil (g/node-value v-sink :loud)))

        (g/transact
          (g/set-property p-source :source-label "after undo"))

        (is (= nil (g/node-value v-sink :loud)))))))

(g/defnode CountOnDelete)

(deftest graph-deletion
  (testing "Deleting a view graph"
    (ts/with-clean-system
      (let [pgraph-id (g/make-graph!)
            agraph-id (g/make-graph! :volatility 10)]

        (let [[source-p1 pipe-p1 sink-p1] (ts/tx-nodes (g/make-node pgraph-id Source :source-label "first")
                                                       (g/make-node pgraph-id Pipe)
                                                       (g/make-node pgraph-id Sink))

              [source-a1 sink-a1 sink-a2] (g/tx-nodes-added
                                            (g/transact
                                              {:undoable false}
                                              (concat
                                                (g/make-node agraph-id Source :source-label "second")
                                                (g/make-node agraph-id Sink)
                                                (g/make-node agraph-id Sink))))]

          (g/transact
           [(g/connect source-p1 :source-label sink-p1 :target-label)
            (g/connect source-p1 :source-label pipe-p1 :target-label)
            (g/connect pipe-p1   :soft         sink-a1 :target-label)
            (g/connect source-a1 :source-label sink-a2 :target-label)])

          (is (= (undo-redo-states) [[nil nil] []]))

          (g/delete-graph! agraph-id)

          (is (= 2 (count (graphs))))

          (is (= (undo-redo-states) [[nil nil] []]))

          (is (= (ts/graph-dependencies [(gt/endpoint source-p1 :source-label)])
                 #{(gt/endpoint sink-p1 :loud)
                   (gt/endpoint pipe-p1 :soft)
                   (gt/endpoint source-p1 :source-label)
                   (gt/endpoint source-p1 :_declared-properties)
                   (gt/endpoint source-p1 :_properties)}))))))

  (testing "Nodes in a deleted graph are deleted"
    (ts/with-clean-system
      (let [pgraph-id (g/make-graph!)]
        (let [nodes (ts/tx-nodes
                     (for [n (range 100)]
                       (g/make-node pgraph-id CountOnDelete)))]

          (g/delete-graph! pgraph-id)

          (is (every? nil? (map g/node-by-id nodes)))))))

  (testing "Deleting a graph"
    (ts/with-clean-system
      (let [project-graph-id (g/make-graph!)
            view-graph-id (g/make-graph! :volatility 10)]

        (let [[source-p1 pipe-p1 sink-p1] (ts/tx-nodes (g/make-node project-graph-id Source :source-label "first")
                                                       (g/make-node project-graph-id Pipe)
                                                       (g/make-node project-graph-id Sink))

              [source-a1 sink-a1 sink-a2] (g/tx-nodes-added
                                            (g/transact
                                              {:undoable false}
                                              (concat
                                                (g/make-node view-graph-id Source :source-label "second")
                                                (g/make-node view-graph-id Sink)
                                                (g/make-node view-graph-id Sink))))]

          (g/transact
           [(g/connect source-p1 :source-label sink-p1 :target-label)
            (g/connect source-p1 :source-label pipe-p1 :target-label)
            (g/connect pipe-p1   :soft         sink-a1 :target-label)
            (g/connect source-a1 :source-label sink-a2 :target-label)])

          (is (= (undo-redo-states) [[nil nil] []]))

          (g/delete-graph! project-graph-id)

          (is (= 2 (count (graphs))))

          (is (nil? (is/graph @g/*the-system* project-graph-id)))

          (is (= (ts/graph-dependencies [(gt/endpoint source-a1 :source-label)])
                 #{(gt/endpoint sink-a2   :loud)
                   (gt/endpoint source-a1 :source-label)
                   (gt/endpoint source-a1 :_declared-properties)
                   (gt/endpoint source-a1 :_properties)})))))))

(deftest graph-values
  (testing "Values can be attached to graphs"
   (ts/with-clean-system
     (let [node-id (gt/make-node-id 0 1)]
       (g/transact [(g/set-graph-value 0 :string-value "A String")
                    (g/set-graph-value 0 :a-node-id node-id)])
       (is (= "A String" (g/graph-value 0 :string-value)))
       (is (= node-id    (g/graph-value 0 :a-node-id))))))

  (testing "Graph values do not interfer with the original members of the graph"
    (ts/with-clean-system
      (let [[src-node] (ts/tx-nodes (g/make-nodes world [src [Source :source-label "test"]]))]
        (g/set-graph-value! world :nodes :new-value)
        (is (= "test" (g/node-value src-node :source-label)))
        (is (= :new-value (g/graph-value world :nodes)))))))

(deftest user-data
  (ts/with-clean-system
    (let [project-graph-id (g/make-graph!)
          view-graph-id (g/make-graph! :volatility 10)
          [project-node view-node] (ts/tx-nodes (g/make-node project-graph-id Source :source-label "first")
                                     (g/make-node view-graph-id Sink))]
      (g/user-data! project-node ::my-user-data :project)
      (g/user-data! view-node ::my-user-data :view)
      (is (= :project (g/user-data project-node ::my-user-data)))
      (is (= :view (g/user-data view-node ::my-user-data)))
      (testing "swapping in a value"
        (is (= :new-view (g/user-data-swap! view-node ::my-user-data (fn [v prefix] (keyword (str prefix (name v)))) "new-")))
        (is (= :new-view (g/user-data view-node ::my-user-data))))
      (testing "value removed after node is deleted"
        (g/delete-node! project-node)
        (is (nil? (g/user-data project-node ::my-user-data)))
        (is (= :new-view (g/user-data view-node ::my-user-data))))
      (testing "value removed after graph is deleted"
        (g/delete-graph! view-graph-id)
        (is (nil? (g/user-data view-node ::my-user-data)))))))
