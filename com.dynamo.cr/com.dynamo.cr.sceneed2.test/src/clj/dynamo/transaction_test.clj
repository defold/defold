(ns dynamo.transaction-test
  (:require [clojure.core.async :as a]
            [clojure.test.check :as tc]
            [clojure.test.check.generators :as gen]
            [clojure.test.check.properties :as prop]
            [clojure.test.check.clojure-test :refer [defspec]]
            [clojure.test :refer :all]
            [schema.core :as s]
            [plumbing.core :refer [defnk fnk]]
            [dynamo.node :as n :refer [Scope]]
            [dynamo.project :as p]
            [dynamo.system :as ds]
            [dynamo.system.test-support :refer :all]
            [dynamo.types :as t]
            [dynamo.util :refer :all]
            [internal.graph.dgraph :as dg]
            [internal.graph.lgraph :as lg]
            [internal.transaction :as it]))

(defn dummy-output [& _] :ok)

(defnk upcase-a [a] (.toUpperCase a))

(n/defnode Resource
  (input a String)
  (output b s/Keyword dummy-output)
  (output c String upcase-a))

(n/defnode Downstream
  (input consumer String))

(def gen-node (gen/fmap (fn [n] (n/construct Resource :_id n :original-id n)) (gen/such-that #(< % 0) gen/neg-int)))

(defspec tempids-resolve-correctly
  (prop/for-all [nn gen-node]
    (with-clean-world
      (let [before (:_id nn)]
        (= before (:original-id (first (tx-nodes nn))))))))

(deftest low-level-transactions
  (testing "one node with tempid"
    (with-clean-world
      (let [tx-result (it/transact world-ref (it/new-node (n/construct Resource :_id -5 :a "known value")))]
        (is (= :ok (:status tx-result)))
        (is (= "known value" (:a (dg/node (:graph tx-result) (it/resolve-tempid tx-result -5))))))))
  (testing "two connected nodes"
    (with-clean-world
      (let [[resource1 resource2] (tx-nodes (n/construct Resource :_id -1) (n/construct Downstream :_id -2))
            id1                   (:_id resource1)
            id2                   (:_id resource2)
            after                 (:graph (it/transact world-ref (it/connect resource1 :b resource2 :consumer)))]
        (is (= [id1 :b]        (first (lg/sources after id2 :consumer))))
        (is (= [id2 :consumer] (first (lg/targets after id1 :b)))))))
  (testing "disconnect two singly-connected nodes"
    (with-clean-world
      (let [[resource1 resource2] (tx-nodes (n/construct Resource :_id -1) (n/construct Downstream :_id -2))
            id1                   (:_id resource1)
            id2                   (:_id resource2)
            tx-result             (it/transact world-ref (it/connect    resource1 :b resource2 :consumer))
            tx-result             (it/transact world-ref (it/disconnect resource1 :b resource2 :consumer))
            after                 (:graph tx-result)]
        (is (= :ok (:status tx-result)))
        (is (= [] (lg/sources after id2 :consumer)))
        (is (= [] (lg/targets after id1 :b))))))
  (testing "simple update"
    (with-clean-world
      (let [[resource] (tx-nodes (n/construct Resource :c 0))
            tx-result  (it/transact world-ref (it/update-property resource :c (fnil + 0) [42]))]
        (is (= :ok (:status tx-result)))
        (is (= 42 (:c (dg/node (:graph tx-result) (:_id resource))))))))
  (testing "node deletion"
    (with-clean-world
      (let [[resource1 resource2] (tx-nodes (n/construct Resource :_id -1) (n/construct Downstream :_id -2))]
        (it/transact world-ref (it/connect resource1 :b resource2 :consumer))
        (let [tx-result (it/transact world-ref (it/delete-node resource2))
              after     (:graph tx-result)]
          (is (nil?   (dg/node    after (:_id resource2))))
          (is (empty? (lg/targets after (:_id resource1) :b)))
          (is (contains? (:nodes-deleted tx-result) (:_id resource2)))
          (is (empty?    (:nodes-added   tx-result)))))))
  (testing "node transformation"
    (with-clean-world
      (let [[resource1] (tx-nodes (n/construct Resource :marker 99))
            id1         (:_id resource1)
            resource2   (n/construct Downstream :_id -1)
            tx-result   (it/transact world-ref (it/become resource1 resource2))
            after       (:graph tx-result)]
        (is (= :ok (:status tx-result)))
        (is (= id1 (it/resolve-tempid tx-result -1)))
        (is (= Downstream (t/node-type (dg/node after id1))))
        (is (= 99  (:marker (dg/node after id1))))))))

(defn track-trigger-activity [graph self transaction]
  (swap! (:tracking self)
    (fn [m]
      {:was-added?    (ds/is-added? transaction self)
       :was-modified? (ds/is-modified? transaction self)
       :was-removed?  (ds/is-deleted? transaction self)
       :call-count    (inc (get m :call-count 0))})))

(n/defnode StringSource
  (property label s/Str (default "a-string")))

(n/defnode Relay
  (input label s/Str)
  (output label s/Str (fnk [label] label)))

(n/defnode TriggerExecutionCounter
  (input downstream s/Any)
  (property any-property s/Bool)
  (property triggers n/Triggers (default [#'track-trigger-activity])))

(defn alter-output [graph self transaction]
  (when (and (ds/is-modified? transaction self) (> 5 (:automatic-property self)))
    (ds/update-property self :automatic-property inc)))

(n/defnode MutatesByTrigger
  (property automatic-property s/Int (default 0))
  (property triggers n/Triggers (default [#'alter-output])))

(deftest trigger-activation
  (testing "runs when node is added"
    (with-clean-world
      (let [tracker      (atom {})
            counter      (ds/transactional (ds/add (n/construct TriggerExecutionCounter :tracking tracker)))
            after-adding @tracker]
        (is (= {:call-count 1 :was-added? true :was-modified? true :was-removed? false} after-adding)))))

  (testing "runs when node is altered in a way that affects an output"
    (with-clean-world
      (let [tracker          (atom {})
            counter          (ds/transactional (ds/add (n/construct TriggerExecutionCounter :tracking tracker)))
            before-updating  @tracker
            _                (ds/transactional (ds/set-property counter :any-property true))
            after-updating   @tracker]
        (is (= {:call-count 1 :was-added? true  :was-modified? true :was-removed? false} before-updating))
        (is (= {:call-count 2 :was-added? false :was-modified? true :was-removed? false} after-updating)))))

  (testing "runs when node is altered in a way that doesn't affects an output"
    (with-clean-world
      (let [tracker          (atom {})
            counter          (ds/transactional (ds/add (n/construct TriggerExecutionCounter :tracking tracker)))
            before-updating  @tracker
            _                (ds/transactional (ds/set-property counter :dynamic-property true))
            after-updating   @tracker]
        (is (= {:call-count 1 :was-added? true  :was-modified? true :was-removed? false} before-updating))
        (is (= {:call-count 2 :was-added? false :was-modified? true :was-removed? false} after-updating)))))

  (testing "runs when node is deleted"
    (with-clean-world
      (let [tracker         (atom {})
            counter         (ds/transactional (ds/add (n/construct TriggerExecutionCounter :tracking tracker)))
            before-removing @tracker
            _               (ds/transactional (ds/delete counter))
            after-removing  @tracker]
        (is (= {:call-count 1 :was-added? true  :was-modified? true :was-removed? false} before-removing))
        (is (= {:call-count 2 :was-added? false :was-modified? true :was-removed? true} after-removing)))))

  (testing "runs when an upstream output changes"
    (with-clean-world
      (let [tracker         (atom {})
            counter         (ds/transactional (ds/add (n/construct TriggerExecutionCounter :tracking tracker)))
            before-updating @tracker
            [s r1 r2 r3]    (tx-nodes (n/construct StringSource) (n/construct Relay) (n/construct Relay) (n/construct Relay))
            _               (ds/transactional
                              (ds/connect s :label r1 :label)
                              (ds/connect r1 :label r2 :label)
                              (ds/connect r2 :label r3 :label)
                              (ds/connect r3 :label counter :downstream))
            _               (ds/transactional (ds/set-property s :label "a different label"))
            after-updating  @tracker]
        (is (= {:call-count 1 :was-added? true  :was-modified? true :was-removed? false} before-updating))
        (is (= {:call-count 3 :was-added? false :was-modified? true :was-removed? false} after-updating)))))

  (testing "runs on the new node type when a node becomes a new node"
    (with-clean-world
      (let [tracker          (atom {})
            counter          (ds/transactional (ds/add (n/construct TriggerExecutionCounter :tracking tracker)))
            before-transmog  @tracker
            stringer         (ds/transactional (ds/become counter (n/construct StringSource)))
            after-transmog   @tracker]
        (is (= {:call-count 1 :was-added? true  :was-modified? true :was-removed? false} before-transmog))
        (is (= {:call-count 2 :was-added? false :was-modified? true :was-removed? false} after-transmog)))))

  (testing "One node may activate another node in a trigger"
    (with-clean-world
      (let [tracker            (atom {})
            counter            (ds/transactional (ds/add (n/construct TriggerExecutionCounter :tracking tracker)))
            mutator            (ds/transactional (ds/add (n/construct MutatesByTrigger)))
            before-transaction @tracker]

        (is (= 1 (:call-count before-transaction)))

        (ds/transactional (ds/connect mutator :automatic-property counter :downstream))

        (let [after-transaction  @tracker]
          (is (= 2 (:call-count after-transaction)))
          (is (not (:was-added? after-transaction)))
          (is (:was-modified? after-transaction))

          (ds/transactional
            (ds/set-property counter :dynamic-property true)
            (ds/set-property mutator :any-change true))

          (let [after-trigger @tracker]
            (is (= 3 (:call-count after-trigger)))
            (is (not (:was-added? after-trigger)))
            (is (:was-modified? after-trigger)))))))

  (testing "activation is correct even when scopes and injection happen"
    (with-clean-world
      (let [tracker            (atom {})
            scope              (ds/transactional (ds/add (n/construct p/Project)))
            counter            (ds/transactional (ds/in scope (ds/add (n/construct TriggerExecutionCounter :tracking tracker))))
            before-removing    @tracker
            _                  (ds/transactional (ds/delete counter))
            after-removing     @tracker]

        (is (= {:call-count 1 :was-added? true  :was-modified? true :was-removed? false} before-removing))
        (is (= {:call-count 2 :was-added? false :was-modified? true :was-removed? true} after-removing))))))

(n/defnode NamedThing
  (property name s/Str))

(n/defnode Person
  (property date-of-birth java.util.Date)

  (input first-name String)
  (input surname String)

  (output friendly-name String (fnk [first-name] first-name))
  (output full-name String (fnk [first-name surname] (str first-name " " surname)))
  (output age java.util.Date (fnk [date-of-birth] date-of-birth)))

(n/defnode Receiver
  (input generic-input s/Any)
  (property touched s/Bool (default false))
  (output passthrough s/Any (fnk [generic-input] generic-input)))

(n/defnode FocalNode
  (input aggregator [s/Any])
  (output aggregated [s/Any] (fnk [aggregator] aggregator)))

(defn- build-network
  []
  (let [nodes {:person            (n/construct Person)
               :first-name-cell   (n/construct NamedThing)
               :last-name-cell    (n/construct NamedThing)
               :greeter           (n/construct Receiver)
               :formal-greeter    (n/construct Receiver)
               :calculator        (n/construct Receiver)
               :multi-node-target (n/construct FocalNode)}
        nodes (zipmap (keys nodes) (apply tx-nodes (vals nodes)))]
    (ds/transactional
      (doseq [[f f-l t t-l]
              [[:first-name-cell :name          :person            :first-name]
               [:last-name-cell  :name          :person            :surname]
               [:person          :friendly-name :greeter           :generic-input]
               [:person          :full-name     :formal-greeter    :generic-input]
               [:person          :age           :calculator        :generic-input]
               [:person          :full-name     :multi-node-target :aggregator]
               [:formal-greeter  :passthrough   :multi-node-target :aggregator]]]
        (ds/connect (f nodes) f-l (t nodes) t-l)))
    nodes))

(defmacro affected-by [& forms]
  `(do
     (ds/transactional ~@forms)
     (:outputs-modified (:last-tx (deref ~'world-ref)))))

(deftest precise-invalidation
  (with-clean-world
    (let [{:keys [calculator person first-name-cell greeter formal-greeter multi-node-target]} (build-network)]
      (are [update expected] (= (map-keys :_id expected) (affected-by (apply ds/set-property update)))
        [calculator :touched true]                {calculator        #{:properties :touched}}
        [person :date-of-birth (java.util.Date.)] {person            #{:properties :age :date-of-birth}
                                                   calculator        #{:passthrough}}
        [first-name-cell :name "Sam"]             {first-name-cell   #{:properties :name}
                                                   person            #{:full-name :friendly-name}
                                                   greeter           #{:passthrough}
                                                   formal-greeter    #{:passthrough}
                                                   multi-node-target #{:aggregated}}))))

(n/defnode EventReceiver
  (on :custom-event
    (deliver (:latch self) true)))

(deftest event-loops-started-by-transaction
  (with-clean-world
    (let [receiver (ds/transactional
                     (ds/add (n/construct EventReceiver :latch (promise))))]
      (ds/transactional
        (ds/send-after receiver {:type :custom-event}))
      (is (= true (deref (:latch receiver) 500 :timeout))))))

(n/defnode AutoUpdateOutput
  (input first-name String)

  (output ordinary String (fn [this g] "a-string"))
  (output updating String :on-update (fnk [first-name] (str "Hello, " first-name))))

(deftest on-update-properties-noted-by-transaction
  (with-clean-world
    (let [update-node (n/construct AutoUpdateOutput)
          event-receiver (n/construct EventReceiver)
          tx-result (it/transact world-ref [(it/new-node event-receiver) (it/new-node update-node)])]
      (let [real-updater (dg/node (:graph tx-result) (it/resolve-tempid tx-result (:_id update-node)))]
        (is (= 1 (count (:expired-outputs tx-result))))
        (is (= [real-updater :updating] (first (:expired-outputs tx-result))))))))

(n/defnode DisposableNode
  t/IDisposable
  (dispose [this] true))

(deftest nodes-are-disposed-after-deletion
  (with-clean-world
    (let [tx-result  (it/transact world-ref (it/new-node (n/construct DisposableNode :_id -1)))
          disposable (dg/node (:graph tx-result) (it/resolve-tempid tx-result -1))
          tx-result  (it/transact world-ref (it/delete-node disposable))]
      (is (= disposable (first (:values-to-dispose tx-result)))))))

(n/defnode CachedOutputInvalidation
  (property a-property String (default "a-string"))

  (output ordinary String :cached (fnk [a-property] a-property))
  (output self-dependent String :cached (fnk [ordinary] ordinary)))

(deftest invalidated-properties-noted-by-transaction
  (with-clean-world
    (let [node      (n/construct CachedOutputInvalidation)
          tx-result (it/transact world-ref [(it/new-node node)])]
      (let [real-node (dg/node (:graph tx-result) (it/resolve-tempid tx-result (:_id node)))]
        (is (= 1 (count (:outputs-modified tx-result))))
        (is (= [(:_id real-node) #{:properties :self :self-dependent :a-property :ordinary}] (first (:outputs-modified tx-result))))
        (let [tx-data   [(it/update-property real-node :a-property (constantly "new-value") [])]
              tx-result (it/transact world-ref tx-data)]
          (is (= 1 (count (:outputs-modified tx-result))))
          (is (= [(:_id real-node) #{:properties :a-property :ordinary :self-dependent}] (first (:outputs-modified tx-result)))))))))