(ns internal.system-test
  (:require [clojure.test :refer :all]
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
