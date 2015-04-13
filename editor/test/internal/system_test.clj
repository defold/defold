(ns internal.system-test
  (:require [clojure.string :as str]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [dynamo.graph.test-support :as ts]
            [internal.graph :as ig]
            [internal.system :as is]
            [internal.graph.types :as gt]))

(g/defnode Root)

(defn fresh-system [] (ts/clean-system {}))

(deftest graph-registration
  (testing "a fresh system has a graph"
    (let [sys (fresh-system)]
      (is (= 1 (count (is/graphs sys))))))

  (testing "a graph can be added to the system"
    (let [sys (fresh-system)
          g   (g/make-graph)
          sys (is/attach-graph sys g)
          gid (:last-graph sys)]
      (is (= 2 (count (is/graphs sys))))
      (is (= gid (:_gid @(get (is/graphs sys) gid))))))

  (testing "a graph can be removed from the system"
    (let [sys (fresh-system)
          g   (g/make-graph)
          sys (is/attach-graph sys (g/make-graph))
          gid (:last-graph sys)
          g   (is/graph sys gid)
          sys (is/detach-graph sys g)]
      (is (= 1 (count (is/graphs sys))))))

  (testing "a graph can be found by its id"
    (ts/with-clean-system
      (let [gid (g/attach-graph (g/make-graph))
            g'  (is/graph @g/*the-system* gid)]
        (is (= (:_gid g') gid))))))

(deftest tx-id
  (testing "graph time advances with transactions"
    (ts/with-clean-system
      (let [gid       (g/attach-graph (g/make-graph))
            before    (is/graph-time @g/*the-system* gid)
            tx-report (g/transact (g/make-node gid Root))
            after     (is/graph-time @g/*the-system* gid)]
        (is (= :ok (:status tx-report)))
        (is (< before after)))))

  #_(testing "graph time does *not* advance for empty transactions"
    (let [sys       (fresh-system)
          world-ref (is/world-ref sys)
          before    (map-vals #(:tx-id @%) (is/graphs sys))
          tx-report (g/transact (atom sys) [])
          after     (map-vals #(:tx-id @%) (is/graphs sys))]
      (is (= :empty (:status tx-report)))
      (is (= before after)))))

(defn history-states
  [href]
  (concat (is/undo-stack href) (is/redo-stack href)))

(deftest history-capture
  (testing "undo history is stored only for undoable graphs"
    (ts/with-clean-system
      (let [pgraph-id      (g/attach-graph-with-history (g/make-graph))
            before         (is/graph-time @g/*the-system* pgraph-id)
            history-before (history-states (is/graph-history @g/*the-system* pgraph-id))
            tx-report      (g/transact (g/make-node pgraph-id Root))
            after          (is/graph-time @g/*the-system* pgraph-id)
            history-after  (history-states (is/graph-history @g/*the-system* pgraph-id))]
        (is (= :ok (:status tx-report)))
        (is (< before after))
        (is (< (count history-before) (count history-after))))))

  (testing "transaction labels appear in the history"
    (ts/with-clean-system
      (let [pgraph-id    (g/attach-graph-with-history (g/make-graph))
            undos-before (is/undo-stack (is/graph-history @g/*the-system* pgraph-id))
            tx-report    (g/transact [(g/make-node pgraph-id Root)
                                      (g/operation-label "Build root")])
            root         (first (g/tx-nodes-added tx-report))
            tx-report    (g/transact [(g/set-property root :touched 1)
                                      (g/operation-label "Increment touch count")])
            undos-after  (is/undo-stack (is/graph-history @g/*the-system* pgraph-id))
            redos-after  (is/redo-stack (is/graph-history @g/*the-system* pgraph-id))]
        (is (= ["Build root" "Increment touch count"] (mapv :label undos-after)))
        (is (= []                                     (mapv :label redos-after)))
        (is/undo-history (is/graph-history @g/*the-system* pgraph-id))

        (let [undos-after-undo  (is/undo-stack (is/graph-history @g/*the-system* pgraph-id))
              redos-after-undo  (is/redo-stack (is/graph-history @g/*the-system* pgraph-id))]
          (is (= ["Build root"]            (mapv :label undos-after-undo)))
          (is (= ["Increment touch count"] (mapv :label redos-after-undo)))))))

  (testing "has-undo? and has-redo?"
    (ts/with-clean-system
      (let [pgraph-id (g/attach-graph-with-history (g/make-graph))]

        (is (not (g/has-undo? pgraph-id)))
        (is (not (g/has-redo? pgraph-id)))

        (let [[root] (ts/tx-nodes (g/make-node pgraph-id Root))]

          (is      (g/has-undo? pgraph-id))
          (is (not (g/has-redo? pgraph-id)))

          (g/transact (g/set-property root :touched 1))

          (is      (g/has-undo? pgraph-id))
          (is (not (g/has-redo? pgraph-id)))

          (g/undo pgraph-id)

          (is (g/has-undo? pgraph-id))
          (is (g/has-redo? pgraph-id))))))

  (testing "history can be cleared"
    (ts/with-clean-system
      (let [pgraph-id (g/attach-graph-with-history (g/make-graph))]

        (is (not (g/has-undo? pgraph-id)))
        (is (not (g/has-redo? pgraph-id)))

        (let [[root] (ts/tx-nodes (g/make-node pgraph-id Root))]

          (is      (g/has-undo? pgraph-id))
          (is (not (g/has-redo? pgraph-id)))

          (g/transact (g/set-property root :touched 1))

          (is      (g/has-undo? pgraph-id))
          (is (not (g/has-redo? pgraph-id)))

          (g/undo pgraph-id)

          (is (g/has-undo? pgraph-id))
          (is (g/has-redo? pgraph-id))

          (g/reset-undo! pgraph-id)

          (is (not (g/has-undo? pgraph-id)))
          (is (not (g/has-redo? pgraph-id))))))))

(defn undo-redo-state?
  [graph undos redos]
  (and (= (g/undo-stack graph) undos)
       (= (g/redo-stack graph) redos)))

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
      (let [pgraph-id (g/attach-graph-with-history (g/make-graph))]

        (is (undo-redo-state? pgraph-id [] []))

        (let [[root] (ts/tx-nodes (g/make-node pgraph-id Root))]

          (is (undo-redo-state? pgraph-id [{:label nil}] []))

          (touch root 1)
          (touch root 2)
          (touch root 3)

          (is (undo-redo-state? pgraph-id [{:label nil} {:label 1} {:label 2} {:label 3}] []))

          (g/undo pgraph-id)

          (is (undo-redo-state? pgraph-id [{:label nil} {:label 1} {:label 2}] [{:label 3}]))))))

  (testing "Transactions with different sequence-ids each make an undo point"
    (ts/with-clean-system
      (let [pgraph-id (g/attach-graph-with-history (g/make-graph))]

        (is (undo-redo-state? pgraph-id [] []))

        (let [[root] (ts/tx-nodes (g/make-node pgraph-id Root))]

          (is (undo-redo-state? pgraph-id [{:label nil}] []))

          (touch root 1 :a)
          (touch root 2 :b)
          (touch root 3 :c)

          (is (undo-redo-state? pgraph-id [{:label nil} {:label 1} {:label 2} {:label 3}] []))

          (g/undo pgraph-id)

          (is (undo-redo-state? pgraph-id [{:label nil} {:label 1} {:label 2}] [{:label 3}]))))))

  (testing "Transactions with the same sequence-id are merged together"
    (ts/with-clean-system
      (let [pgraph-id (g/attach-graph-with-history (g/make-graph))]

        (is (undo-redo-state? pgraph-id [] []))

        (let [[root] (ts/tx-nodes (g/make-node pgraph-id Root))]

          (is (undo-redo-state? pgraph-id [{:label nil}] []))

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

          (is (undo-redo-state? pgraph-id [{:label nil} {:label 2} {:label 3}] []))

          (g/undo pgraph-id)

          (is (undo-redo-state? pgraph-id [{:label nil} {:label 2}] [{:label 3}]))

          (g/undo pgraph-id)

          (is (undo-redo-state? pgraph-id [{:label nil}] [{:label 3} {:label 2}]))))))


  (testing "Cross-graph transactions create an undo point in all affected graphs"
    (ts/with-clean-system
      (let [pgraph-id (g/attach-graph-with-history (g/make-graph))
            agraph-id (g/attach-graph-with-history (g/make-graph))]

        (let [[node-p] (ts/tx-nodes (g/make-node pgraph-id Root :where "graph P"))
              [node-a] (ts/tx-nodes (g/make-node agraph-id Root :where "graph A"))]

          (is (undo-redo-state? pgraph-id [{:label nil}] []))
          (is (undo-redo-state? agraph-id [{:label nil}] []))

          (touch node-p 1 :a)

          (g/transact [(g/set-property node-p :touched 2)
                        (g/set-property node-a :touched 2)
                        (g/operation-label 2)
                        (g/operation-sequence :a)])

          (touch node-p 3 :c)

          (is (undo-redo-state? pgraph-id [{:label nil} {:label 2} {:label 3}] []))
          (is (undo-redo-state? agraph-id [{:label nil} {:label 2}] []))

          (g/undo pgraph-id)

          (is (undo-redo-state? pgraph-id [{:label nil} {:label 2}] [{:label 3}]))
          (is (undo-redo-state? agraph-id [{:label nil} {:label 2}] []))

          (g/undo pgraph-id)

          (is (undo-redo-state? pgraph-id [{:label nil}] [{:label 3} {:label 2}]))
          (is (undo-redo-state? agraph-id [{:label nil}   {:label 2}] [])))))))

(g/defnode Source
  (property source-label String))

(g/defnode Pipe
  (input target-label String)
  (output soft String (g/fnk [target-label] (str/lower-case target-label))))

(g/defnode Sink
  (input target-label String)
  (output loud String (g/fnk [target-label] (str/upper-case target-label))))

(defn id [n] (g/node-id n))

(deftest tracing-across-graphs
  (ts/with-clean-system
    (let [pgraph-id (g/attach-graph-with-history (g/make-graph))
          agraph-id (g/attach-graph (g/make-graph))]

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

        (is (= (g/dependencies (g/now) [[(id source-a1) :source-label]])
               #{[(id sink-a2)   :loud]
                 [(id source-a1) :source-label]}))

        (is (= (g/dependencies (g/now) [[(id source-p1) :source-label]])
               #{[(id sink-p1)   :loud]
                 [(id pipe-p1)   :soft]
                 [(id sink-a1)   :loud]
                 [(id source-p1) :source-label]}))))))

(defn bump-counter
  [transaction graph self & _]
  (swap! (:counter self) inc)
  [])

(g/defnode CountOnDelete
  (trigger deletion :deleted #'bump-counter))

(deftest graph-deletion
  (testing "Deleting a view (non-history) graph"
    (ts/with-clean-system
      (let [pgraph-id (g/attach-graph-with-history (g/make-graph))
            agraph-id (g/attach-graph (g/make-graph :volatility 10))]

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

          (is (undo-redo-state? pgraph-id [{:label nil} {:label nil}] []))

          (g/delete-graph agraph-id)

          (is (= 2 (count (is/graphs @g/*the-system*))))

          (is (undo-redo-state? pgraph-id [{:label nil} {:label nil}] []))

          (is (= (g/dependencies (g/now) [[(id source-p1) :source-label]])
                 #{[(id sink-p1)   :loud]
                   [(id pipe-p1)   :soft]
                   [(id source-p1) :source-label]}))))))

  (testing "Nodes in a deleted graph are deleted"
    (ts/with-clean-system
      (let [ctr       (atom 0)
            pgraph-id (g/attach-graph-with-history (g/make-graph))]
        (let [nodes (ts/tx-nodes
                     (for [n (range 100)]
                       (g/make-node pgraph-id CountOnDelete :counter ctr)))]
          (g/delete-graph pgraph-id)

          (is (= 100 @ctr))))))

  (testing "Deleting a graph with history"
    (ts/with-clean-system
      (let [pgraph-id (g/attach-graph-with-history (g/make-graph))
            agraph-id (g/attach-graph (g/make-graph :volatility 10))]

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

          (is (undo-redo-state? pgraph-id [{:label nil} {:label nil}] []))

          (g/delete-graph pgraph-id)

          (is (= 2 (count (is/graphs @g/*the-system*))))

          (is (nil? (is/graph-ref @g/*the-system* pgraph-id)))

          ;; This documents current behavior: we leave dangling arcs so undo in the project graph "reconnects" to them
          (is (not (empty? (ig/arcs-from-source (is/graph @g/*the-system* agraph-id) (id pipe-p1) :soft))))

          (is (= (g/dependencies (g/now) [[(id source-a1) :source-label]])
                 #{[(id sink-a2)   :loud]
                   [(id source-a1) :source-label]})))))))

(deftest graph-values
  (testing "Values can be attached to graphs"
    (ts/with-clean-system
      (let [node-id (gt/make-node-id 0 1)]
        (g/transact [(g/set-graph-value 0 :string-value "A String")
                     (g/set-graph-value 0 :a-node-id node-id)])
        (is (= "A String" (g/graph-value 0 :string-value)))
        (is (= node-id    (g/graph-value 0 :a-node-id)))))))
