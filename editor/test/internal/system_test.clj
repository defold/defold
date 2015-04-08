(ns internal.system-test
  (:require [clojure.string :as str]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [dynamo.node :as n]
            [dynamo.system :as ds]
            [dynamo.system.test-support :as ts]
            [internal.graph :as ig]
            [internal.system :as is]
            [internal.transaction :as it]))

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
    (let [sys (fresh-system)
          g   (g/make-graph)
          sys (is/attach-graph sys g)
          g'  (is/graph sys (g/graph-id g))]
      (is (= (g/graph-id g') (g/graph-id g))))))

(deftest tx-id
  (testing "graph time advances with transactions"
    (let [sys       (fresh-system)
          sys       (is/attach-graph sys (g/make-graph))
          gid       (:last-graph sys)
          before    (is/graph-time sys gid)
          tx-report (ds/transact (atom sys) (g/make-node gid Root))
          after     (is/graph-time sys gid)]
      (is (= :ok (:status tx-report)))
      (is (< before after))))

  #_(testing "graph time does *not* advance for empty transactions"
    (let [sys       (fresh-system)
          world-ref (is/world-ref sys)
          before    (map-vals #(:tx-id @%) (is/graphs sys))
          tx-report (ds/transact (atom sys) [])
          after     (map-vals #(:tx-id @%) (is/graphs sys))]
      (is (= :empty (:status tx-report)))
      (is (= before after)))))

(defn history-states
  [href]
  (concat (is/undo-stack href) (is/redo-stack href)))

(deftest history-capture
  (testing "undo history is stored only for undoable graphs"
    (let [sys            (fresh-system)
          sys            (is/attach-graph-with-history sys (g/make-graph))
          pgraph-id      (:last-graph sys)
          before         (is/graph-time sys pgraph-id)
          history-before (history-states (is/graph-history sys pgraph-id))
          tx-report      (ds/transact (atom sys) (g/make-node pgraph-id Root))
          after          (is/graph-time sys pgraph-id)
          history-after  (history-states (is/graph-history sys pgraph-id))]
      (is (= :ok (:status tx-report)))
      (is (< before after))
      (is (< (count history-before) (count history-after)))))

  (testing "transaction labels appear in the history"
    (let [sys          (fresh-system)
          sys          (is/attach-graph-with-history sys (g/make-graph))
          pgraph-id    (:last-graph sys)
          sys-holder   (ref sys)
          undos-before (is/undo-stack (is/graph-history sys pgraph-id))
          tx-report    (ds/transact sys-holder
                                    [(g/make-node pgraph-id Root)
                                     (g/operation-label "Build root")])
          root         (first (ds/tx-nodes-added tx-report))
          tx-report    (ds/transact sys-holder
                                    [(g/set-property root :touched 1)
                                     (g/operation-label "Increment touch count")])
          undos-after  (is/undo-stack (is/graph-history sys pgraph-id))
          redos-after  (is/redo-stack (is/graph-history sys pgraph-id))]
      (is (= ["Build root" "Increment touch count"] (mapv :label undos-after)))
      (is (= []                                     (mapv :label redos-after)))
      (is/undo-history (is/graph-history sys pgraph-id))

      (let [undos-after-undo  (is/undo-stack (is/graph-history sys pgraph-id))
            redos-after-undo  (is/redo-stack (is/graph-history sys pgraph-id))]
        (is (= ["Build root"]            (mapv :label undos-after-undo)))
        (is (= ["Increment touch count"] (mapv :label redos-after-undo))))))

  (testing "has-undo? and has-redo?"
    (ts/with-clean-system
      (let [pgraph-id (ds/attach-graph-with-history (g/make-graph))]

        (is (not (ds/has-undo? pgraph-id)))
        (is (not (ds/has-redo? pgraph-id)))

        (let [[root] (ts/tx-nodes (g/make-node pgraph-id Root))]

          (is      (ds/has-undo? pgraph-id))
          (is (not (ds/has-redo? pgraph-id)))

          (ds/transact (g/set-property root :touched 1))

          (is      (ds/has-undo? pgraph-id))
          (is (not (ds/has-redo? pgraph-id)))

          (ds/undo pgraph-id)

          (is (ds/has-undo? pgraph-id))
          (is (ds/has-redo? pgraph-id))))))

  (testing "history can be cleared"
    (ts/with-clean-system
      (let [pgraph-id (ds/attach-graph-with-history (g/make-graph))]

        (is (not (ds/has-undo? pgraph-id)))
        (is (not (ds/has-redo? pgraph-id)))

        (let [[root] (ts/tx-nodes (g/make-node pgraph-id Root))]

          (is      (ds/has-undo? pgraph-id))
          (is (not (ds/has-redo? pgraph-id)))

          (ds/transact (g/set-property root :touched 1))

          (is      (ds/has-undo? pgraph-id))
          (is (not (ds/has-redo? pgraph-id)))

          (ds/undo pgraph-id)

          (is (ds/has-undo? pgraph-id))
          (is (ds/has-redo? pgraph-id))

          (ds/reset-undo! pgraph-id)

          (is (not (ds/has-undo? pgraph-id)))
          (is (not (ds/has-redo? pgraph-id))))))))

(defn undo-redo-state?
  [graph undos redos]
  (and (= (ds/undo-stack graph) undos)
       (= (ds/redo-stack graph) redos)))

(defn touch
  [node label & [seq-id]]
  (ds/transact (keep identity
                     [(g/operation-label label)
                      (when seq-id
                        (g/operation-sequence seq-id))
                      (g/set-property node :touched label)])))

(deftest undo-coalescing
  (testing "Transactions with no sequence-id each make an undo point"
    (ts/with-clean-system
      (let [pgraph-id (ds/attach-graph-with-history (g/make-graph))]

        (is (undo-redo-state? pgraph-id [] []))

        (let [[root] (ts/tx-nodes (g/make-node pgraph-id Root))]

          (is (undo-redo-state? pgraph-id [{:label nil}] []))

          (touch root 1)
          (touch root 2)
          (touch root 3)

          (is (undo-redo-state? pgraph-id [{:label nil} {:label 1} {:label 2} {:label 3}] []))

          (ds/undo pgraph-id)

          (is (undo-redo-state? pgraph-id [{:label nil} {:label 1} {:label 2}] [{:label 3}]))))))

  (testing "Transactions with different sequence-ids each make an undo point"
    (ts/with-clean-system
      (let [pgraph-id (ds/attach-graph-with-history (g/make-graph))]

        (is (undo-redo-state? pgraph-id [] []))

        (let [[root] (ts/tx-nodes (g/make-node pgraph-id Root))]

          (is (undo-redo-state? pgraph-id [{:label nil}] []))

          (touch root 1 :a)
          (touch root 2 :b)
          (touch root 3 :c)

          (is (undo-redo-state? pgraph-id [{:label nil} {:label 1} {:label 2} {:label 3}] []))

          (ds/undo pgraph-id)

          (is (undo-redo-state? pgraph-id [{:label nil} {:label 1} {:label 2}] [{:label 3}]))))))

  (testing "Transactions with the same sequence-id are merged together"
    (ts/with-clean-system
      (let [pgraph-id (ds/attach-graph-with-history (g/make-graph))]

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

          (ds/undo pgraph-id)

          (is (undo-redo-state? pgraph-id [{:label nil} {:label 2}] [{:label 3}]))

          (ds/undo pgraph-id)

          (is (undo-redo-state? pgraph-id [{:label nil}] [{:label 3} {:label 2}]))))))


  (testing "Cross-graph transactions create an undo point in all affected graphs"
    (ts/with-clean-system
      (let [pgraph-id (ds/attach-graph-with-history (g/make-graph))
            agraph-id (ds/attach-graph-with-history (g/make-graph))]

        (let [[node-p] (ts/tx-nodes (g/make-node pgraph-id Root :where "graph P"))
              [node-a] (ts/tx-nodes (g/make-node agraph-id Root :where "graph A"))]

          (is (undo-redo-state? pgraph-id [{:label nil}] []))
          (is (undo-redo-state? agraph-id [{:label nil}] []))

          (touch node-p 1 :a)

          (ds/transact [(g/set-property node-p :touched 2)
                        (g/set-property node-a :touched 2)
                        (g/operation-label 2)
                        (g/operation-sequence :a)])

          (touch node-p 3 :c)

          (is (undo-redo-state? pgraph-id [{:label nil} {:label 2} {:label 3}] []))
          (is (undo-redo-state? agraph-id [{:label nil} {:label 2}] []))

          (ds/undo pgraph-id)

          (is (undo-redo-state? pgraph-id [{:label nil} {:label 2}] [{:label 3}]))
          (is (undo-redo-state? agraph-id [{:label nil} {:label 2}] []))

          (ds/undo pgraph-id)

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
    (let [pgraph-id (ds/attach-graph-with-history (g/make-graph))
          agraph-id (ds/attach-graph (g/make-graph))]

      (let [[source-p1 pipe-p1 sink-p1] (ts/tx-nodes (g/make-node pgraph-id Source :source-label "first")
                                                     (g/make-node pgraph-id Pipe)
                                                     (g/make-node pgraph-id Sink))

            [source-a1 sink-a1 sink-a2] (ts/tx-nodes (g/make-node agraph-id Source :source-label "second")
                                                     (g/make-node agraph-id Sink)
                                                     (g/make-node agraph-id Sink))]

        (ds/transact
         [(g/connect source-p1 :source-label sink-p1 :target-label)
          (g/connect source-p1 :source-label pipe-p1 :target-label)
          (g/connect pipe-p1   :soft         sink-a1 :target-label)
          (g/connect source-a1 :source-label sink-a2 :target-label)])

        (is (= (g/dependencies (ds/now) [[(id source-a1) :source-label]])
               #{[(id sink-a2)   :loud]
                 [(id source-a1) :source-label]}))

        (is (= (g/dependencies (ds/now) [[(id source-p1) :source-label]])
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
      (let [pgraph-id (ds/attach-graph-with-history (g/make-graph))
            agraph-id (ds/attach-graph (g/make-graph :volatility 10))]

        (let [[source-p1 pipe-p1 sink-p1] (ts/tx-nodes (g/make-node pgraph-id Source :source-label "first")
                                                       (g/make-node pgraph-id Pipe)
                                                       (g/make-node pgraph-id Sink))

              [source-a1 sink-a1 sink-a2] (ts/tx-nodes (g/make-node agraph-id Source :source-label "second")
                                                       (g/make-node agraph-id Sink)
                                                       (g/make-node agraph-id Sink))]

          (ds/transact
           [(g/connect source-p1 :source-label sink-p1 :target-label)
            (g/connect source-p1 :source-label pipe-p1 :target-label)
            (g/connect pipe-p1   :soft         sink-a1 :target-label)
            (g/connect source-a1 :source-label sink-a2 :target-label)])

          (is (undo-redo-state? pgraph-id [{:label nil} {:label nil}] []))

          (ds/delete-graph agraph-id)

          (is (= 2 (count (is/graphs @ds/*the-system*))))

          (is (undo-redo-state? pgraph-id [{:label nil} {:label nil}] []))

          (is (= (g/dependencies (ds/now) [[(id source-p1) :source-label]])
                 #{[(id sink-p1)   :loud]
                   [(id pipe-p1)   :soft]
                   [(id source-p1) :source-label]}))))))

  (testing "Nodes in a deleted graph are deleted"
    (ts/with-clean-system
      (let [ctr       (atom 0)
            pgraph-id (ds/attach-graph-with-history (g/make-graph))]
        (let [nodes (ts/tx-nodes
                     (for [n (range 100)]
                       (g/make-node pgraph-id CountOnDelete :counter ctr)))]
          (ds/delete-graph pgraph-id)

          (is (= 100 @ctr))))))

  (testing "Deleting a graph with history"
    (ts/with-clean-system
      (let [pgraph-id (ds/attach-graph-with-history (g/make-graph))
            agraph-id (ds/attach-graph (g/make-graph :volatility 10))]

        (let [[source-p1 pipe-p1 sink-p1] (ts/tx-nodes (g/make-node pgraph-id Source :source-label "first")
                                                       (g/make-node pgraph-id Pipe)
                                                       (g/make-node pgraph-id Sink))

              [source-a1 sink-a1 sink-a2] (ts/tx-nodes (g/make-node agraph-id Source :source-label "second")
                                                       (g/make-node agraph-id Sink)
                                                       (g/make-node agraph-id Sink))]

          (ds/transact
           [(g/connect source-p1 :source-label sink-p1 :target-label)
            (g/connect source-p1 :source-label pipe-p1 :target-label)
            (g/connect pipe-p1   :soft         sink-a1 :target-label)
            (g/connect source-a1 :source-label sink-a2 :target-label)])

          (is (undo-redo-state? pgraph-id [{:label nil} {:label nil}] []))

          (ds/delete-graph pgraph-id)

          (is (= 2 (count (is/graphs @ds/*the-system*))))

          (is (nil? (is/graph-ref @ds/*the-system* pgraph-id)))

          ;; This documents current behavior: we leave dangling arcs so undo in the project graph "reconnects" to them
          (is (not (empty? (ig/arcs-from-source (is/graph @ds/*the-system* agraph-id) (id pipe-p1) :soft))))

          (is (= (g/dependencies (ds/now) [[(id source-a1) :source-label]])
                 #{[(id sink-a2)   :loud]
                   [(id source-a1) :source-label]})))))))
