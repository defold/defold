(ns internal.system-test
  (:require [clojure.string :as str]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [support.test-support :as ts]
            [internal.util :as u]
            [internal.graph.types :as gt]
            [internal.system :as is]))

(g/defnode Root
  (property where g/Str)
  (property touched g/Num))

(defn graphs        []    (is/graphs        @g/*the-system*))
(defn graph         [gid] (is/graph         @g/*the-system* gid))
(defn graph-ref     [gid] (is/graph-ref     @g/*the-system* gid))
(defn graph-time    [gid] (is/graph-time    @g/*the-system* gid))
(defn graph-history [gid] (is/graph-history @g/*the-system* gid))

(defn history-states
  [gid]
  (let [href (graph-history gid)]
    (concat (is/undo-stack href) (is/redo-stack href))))

(defn undo-redo-state?
  [graph undos redos]
  (let [href (graph-history graph)]
    (and (= (map :label (is/undo-stack href)) undos)
         (= (map :label (is/redo-stack href)) redos))))

(deftest graph-registration
  (testing "a fresh system has a graph"
    (ts/with-clean-system
      (is (= 1 (count (graphs))))))

  (testing "a graph can be added to the system"
    (ts/with-clean-system
      (let [gid        (g/make-graph!)
            all-graphs (u/map-vals deref (graphs))
            all-gids   (into #{} (map :_gid (vals all-graphs)))]
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
        (is (= (:_gid g') gid))))))

(deftest tx-id
  (testing "graph time advances with transactions"
    (ts/with-clean-system
      (let [gid       (g/make-graph!)
            before    (graph-time gid)
            tx-report (g/transact (g/make-node gid Root))
            after     (graph-time gid)]
        (is (= :ok (:status tx-report)))
        (is (< before after))))))

(deftest history-capture
  (testing "undo history is stored only for undoable graphs"
    (ts/with-clean-system
      (let [pgraph-id      (g/make-graph! :history true)
            before         (graph-time pgraph-id)
            history-before (history-states pgraph-id)
            tx-report      (g/transact (g/make-node pgraph-id Root))
            after          (graph-time pgraph-id)
            history-after  (history-states pgraph-id)]
        (is (= :ok (:status tx-report)))
        (is (< before after))
        (is (< (count history-before) (count history-after))))))

  (testing "transaction labels appear in the history"
    (ts/with-clean-system
      (let [pgraph-id    (g/make-graph! :history true)
            undos-before (is/undo-stack (graph-history pgraph-id))
            tx-report    (g/transact [(g/make-node pgraph-id Root)
                                      (g/operation-label "Build root")])
            root         (first (g/tx-nodes-added tx-report))
            tx-report    (g/transact [(g/set-property root :touched 1)
                                      (g/operation-label "Increment touch count")])
            undos-after  (is/undo-stack (graph-history pgraph-id))
            redos-after  (is/redo-stack (graph-history pgraph-id))
            snapshot     @g/*the-system*]
        (is (= ["Build root" "Increment touch count"] (mapv :label undos-after)))
        (is (= []                                     (mapv :label redos-after)))
        (is/undo-history! snapshot pgraph-id)

        (let [undos-after-undo  (is/undo-stack (graph-history pgraph-id))
              redos-after-undo  (is/redo-stack (graph-history pgraph-id))]
          (is (= ["Build root"]            (mapv :label undos-after-undo)))
          (is (= ["Increment touch count"] (mapv :label redos-after-undo)))))))

  (testing "has-undo? and has-redo?"
    (ts/with-clean-system
      (let [pgraph-id (g/make-graph! :history true)]

        (is (not (g/has-undo? pgraph-id)))
        (is (not (g/has-redo? pgraph-id)))

        (let [root (g/make-node! pgraph-id Root)]

          (is      (g/has-undo? pgraph-id))
          (is (not (g/has-redo? pgraph-id)))

          (g/transact (g/set-property root :touched 1))

          (is      (g/has-undo? pgraph-id))
          (is (not (g/has-redo? pgraph-id)))

          (g/undo! pgraph-id)

          (is (g/has-undo? pgraph-id))
          (is (g/has-redo? pgraph-id))))))

  (testing "history can be cleared"
    (ts/with-clean-system
      (let [pgraph-id (g/make-graph! :history true)]

        (is (not (g/has-undo? pgraph-id)))
        (is (not (g/has-redo? pgraph-id)))

        (let [[root] (ts/tx-nodes (g/make-node pgraph-id Root))]

          (is      (g/has-undo? pgraph-id))
          (is (not (g/has-redo? pgraph-id)))

          (g/transact (g/set-property root :touched 1))

          (is      (g/has-undo? pgraph-id))
          (is (not (g/has-redo? pgraph-id)))

          (g/undo! pgraph-id)

          (is (g/has-undo? pgraph-id))
          (is (g/has-redo? pgraph-id))

          (g/reset-undo! pgraph-id)

          (is (not (g/has-undo? pgraph-id)))
          (is (not (g/has-redo? pgraph-id))))))))

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
      (let [pgraph-id (g/make-graph! :history true)]

        (is (undo-redo-state? pgraph-id [] []))

        (let [[root] (ts/tx-nodes (g/make-node pgraph-id Root))]

          (is (undo-redo-state? pgraph-id [nil] []))

          (touch root 1)
          (touch root 2)
          (touch root 3)

          (is (undo-redo-state? pgraph-id [nil 1 2 3] []))

          (g/undo! pgraph-id)

          (is (undo-redo-state? pgraph-id [nil 1 2] [3]))))))

  (testing "Transactions with different sequence-ids each make an undo point"
    (ts/with-clean-system
      (let [pgraph-id (g/make-graph! :history true)]

        (is (undo-redo-state? pgraph-id [] []))

        (let [[root] (ts/tx-nodes (g/make-node pgraph-id Root))]

          (is (undo-redo-state? pgraph-id [nil] []))

          (touch root 1 :a)
          (touch root 2 :b)
          (touch root 3 :c)

          (is (undo-redo-state? pgraph-id [nil 1 2 3] []))

          (g/undo! pgraph-id)

          (is (undo-redo-state? pgraph-id [nil 1 2] [3]))))))

  (testing "Transactions with the same sequence-id are merged together"
    (ts/with-clean-system
      (let [pgraph-id (g/make-graph! :history true)]

        (is (undo-redo-state? pgraph-id [] []))

        (let [[root] (ts/tx-nodes (g/make-node pgraph-id Root))]

          (is (undo-redo-state? pgraph-id [nil] []))

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

          (is (undo-redo-state? pgraph-id [nil 2 3] []))

          (g/undo! pgraph-id)

          (is (undo-redo-state? pgraph-id [nil 2] [3]))

          (g/undo! pgraph-id)

          (is (undo-redo-state? pgraph-id [nil] [3 2]))))))

  (testing "Canceling the current sequence leaves nothing new in the history"
    (ts/with-clean-system
      (let [pgraph-id (g/make-graph! :history true)]

        (is (undo-redo-state? pgraph-id [] []))

        (let [[root] (ts/tx-nodes (g/make-node pgraph-id Root))]

          (is (undo-redo-state? pgraph-id [nil] []))

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

          (g/cancel! pgraph-id :b)

          (is (undo-redo-state? pgraph-id [nil 1] []))

          (touch root 3 :c)

          (is (undo-redo-state? pgraph-id [nil 1 3] []))

          (g/undo! pgraph-id)

          (is (undo-redo-state? pgraph-id [nil 1] [3]))

          (g/undo! pgraph-id)

          (is (undo-redo-state? pgraph-id [nil] [3 1]))))))

(testing "Canceling a sequence that is not the current sequence does nothing"
    (ts/with-clean-system
      (let [pgraph-id (g/make-graph! :history true)]

        (is (undo-redo-state? pgraph-id [] []))

        (let [[root] (ts/tx-nodes (g/make-node pgraph-id Root))]

          (is (undo-redo-state? pgraph-id [nil] []))

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

          (g/cancel! pgraph-id :a)

          (is (undo-redo-state? pgraph-id [nil 1 2.9] []))))))

  (testing "Cross-graph transactions create an undo point in all affected graphs"
    (ts/with-clean-system
      (let [pgraph-id (g/make-graph! :history true)
            agraph-id (g/make-graph! :history true)]

        (let [[node-p] (ts/tx-nodes (g/make-node pgraph-id Root :where "graph P"))
              [node-a] (ts/tx-nodes (g/make-node agraph-id Root :where "graph A"))]

          (is (undo-redo-state? pgraph-id [nil] []))
          (is (undo-redo-state? agraph-id [nil] []))

          (touch node-p 1 :a)

          (g/transact [(g/set-property node-p :touched 2)
                        (g/set-property node-a :touched 2)
                        (g/operation-label 2)
                        (g/operation-sequence :a)])

          (touch node-p 3 :c)

          (is (undo-redo-state? pgraph-id [nil 2 3] []))
          (is (undo-redo-state? agraph-id [nil 2] []))

          (g/undo! pgraph-id)

          (is (undo-redo-state? pgraph-id [nil 2] [3]))
          (is (undo-redo-state? agraph-id [nil 2] []))

          (g/undo! pgraph-id)

          (is (undo-redo-state? pgraph-id [nil] [3 2]))
          (is (undo-redo-state? agraph-id [nil 2] [])))))))

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
    (let [pgraph-id (g/make-graph! :history true)
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

        (is (= (ts/graph-dependencies [[source-a1 :source-label]])
               #{[sink-a2   :loud]
                 [source-a1 :source-label]
                 [source-a1 :_declared-properties]
                 [source-a1 :_properties]}))

        (is (= (ts/graph-dependencies [[source-p1 :source-label]])
               #{[sink-p1   :loud]
                 [pipe-p1   :soft]
                 [sink-a1   :loud]
                 [source-p1 :source-label]
                 [source-p1 :_declared-properties]
                 [source-p1 :_properties]}))))))

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
      (let [project-graph      (g/make-graph! :history true)
            view-graph         (g/make-graph! :history false)
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

        (g/undo! project-graph)
        (is (= "AFTER CHANGE" (g/node-value sink :loud)))

        (g/delete-node! source)
        (is (= nil (g/node-value sink :loud)))))))

(defn- successors [node-id label]
  (-> @g/*the-system* :graphs (get (g/node-id->graph-id node-id)) deref :successors (get-in [node-id label])))

(defn- sarcs [node-id label]
  (-> @g/*the-system* :graphs (get (g/node-id->graph-id node-id)) deref :sarcs (get-in [node-id label])))

(defn- tarcs [node-id label]
  (-> @g/*the-system* :graphs (get (g/node-id->graph-id node-id)) deref :tarcs (get-in [node-id label])))


(deftest undo-restores-hydrated-successors
  (testing "undo connection P->V preserves connection and successors"
    (ts/with-clean-system
      (let [project-graph (g/make-graph! :history true)
            view-graph (g/make-graph! :history false)
            [p-source p-source2 v-sink] (ts/tx-nodes
                                          (g/make-node project-graph Source :source-label "initial value")
                                          (g/make-node project-graph Source)
                                          (g/make-node view-graph Sink))]

        (is (= 1 (count (ts/undo-stack project-graph))))
        
        ;; This creates a dummy history step (that only touches p-source2) after the setup transaction.
        ;; If we don't do this, the make-node's transaction will cause p-source + source-label to end up in
        ;; modified nodes, and that will update the succeessor connection p-source->v-sink masking the bug
        ;; we're trying to expose.
        (g/transact
          (g/set-property p-source2 :source-label "dummy"))

        (is (= 2 (count (ts/undo-stack project-graph))))        

        (is (= {p-source #{:_declared-properties :source-label :_properties}} (successors p-source :source-label)))
        (is (= nil (sarcs p-source :source-label)))
        (is (= nil (tarcs v-sink :target-label)))

        (g/transact
          [(g/set-property p-source2 :source-label "whateverzzzzz") ; we include this action to ensure a history entry is created
           (g/connect p-source :source-label v-sink :target-label)]) ; this alone will not create a history entry since the target graph does not have history

        (is (= 3 (count (ts/undo-stack project-graph))))

        (is (= {p-source #{:_declared-properties :source-label :_properties} v-sink #{:loud}} (successors p-source :source-label)))
        (is (= [[p-source :source-label v-sink :target-label]] (g/arcs->tuples (sarcs p-source :source-label))))
        (is (= [[p-source :source-label v-sink :target-label]] (g/arcs->tuples (tarcs v-sink :target-label))))

        (is (= "INITIAL VALUE" (g/node-value v-sink :loud)))

        (g/undo! project-graph)

        (is (= 2 (count (ts/undo-stack project-graph))))

        ;; check hydrated after undo, v-sink :loud used to be missing from successors
        (is (= {p-source #{:_declared-properties :source-label :_properties} v-sink #{:loud}} (successors p-source :source-label)))
        (is (= [[p-source :source-label v-sink :target-label]] (g/arcs->tuples (sarcs p-source :source-label))))
        (is (= [[p-source :source-label v-sink :target-label]] (g/arcs->tuples (tarcs v-sink :target-label))))

        (is (= "INITIAL VALUE" (g/node-value v-sink :loud)))

        (g/transact
          (g/set-property p-source :source-label "after undo"))

        ;; check cache invalidation works (via successors) - this used to fail
        (is (= (g/node-value v-sink :loud) "AFTER UNDO"))))))

(g/defnode CountOnDelete)

(deftest graph-deletion
  (testing "Deleting a view (non-history) graph"
    (ts/with-clean-system
      (let [pgraph-id (g/make-graph! :history true)
            agraph-id (g/make-graph! :volatility 10)]

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

          (is (undo-redo-state? pgraph-id [nil nil] []))

          (g/delete-graph! agraph-id)

          (is (= 2 (count (graphs))))

          (is (undo-redo-state? pgraph-id [nil nil] []))

          (is (= (ts/graph-dependencies [[source-p1 :source-label]])
                 #{[sink-p1   :loud]
                   [pipe-p1   :soft]
                   [source-p1 :source-label]
                   [source-p1 :_declared-properties]
                   [source-p1 :_properties]}))))))

  (testing "Nodes in a deleted graph are deleted"
    (ts/with-clean-system
      (let [pgraph-id (g/make-graph! :history true)]
        (let [nodes (ts/tx-nodes
                     (for [n (range 100)]
                       (g/make-node pgraph-id CountOnDelete)))]

          (g/delete-graph! pgraph-id)

          (is (every? nil? (map g/node-by-id nodes)))))))

  (testing "Deleting a graph with history"
    (ts/with-clean-system
      (let [project-graph-id (g/make-graph! :history true)
            view-graph-id (g/make-graph! :volatility 10)]

        (let [[source-p1 pipe-p1 sink-p1] (ts/tx-nodes (g/make-node project-graph-id Source :source-label "first")
                                                       (g/make-node project-graph-id Pipe)
                                                       (g/make-node project-graph-id Sink))

              [source-a1 sink-a1 sink-a2] (ts/tx-nodes (g/make-node view-graph-id Source :source-label "second")
                                                       (g/make-node view-graph-id Sink)
                                                       (g/make-node view-graph-id Sink))]

          (g/transact
           [(g/connect source-p1 :source-label sink-p1 :target-label)
            (g/connect source-p1 :source-label pipe-p1 :target-label)
            (g/connect pipe-p1   :soft         sink-a1 :target-label)
            (g/connect source-a1 :source-label sink-a2 :target-label)])

          (is (undo-redo-state? project-graph-id [nil nil] []))

          (g/delete-graph! project-graph-id)

          (is (= 2 (count (graphs))))

          (is (nil? (graph-ref project-graph-id)))

          (is (= (ts/graph-dependencies [[source-a1 :source-label]])
                 #{[sink-a2   :loud]
                   [source-a1 :source-label]
                   [source-a1 :_declared-properties]
                   [source-a1 :_properties]})))))))

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
    (let [project-graph-id (g/make-graph! :history true)
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
