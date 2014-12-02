(ns dynamo.transaction-test
  (:require [clojure.core.async :as a]
            [clojure.test.check :as tc]
            [clojure.test.check.generators :as gen]
            [clojure.test.check.properties :as prop]
            [clojure.test.check.clojure-test :refer [defspec]]
            [clojure.test :refer :all]
            [schema.core :as s]
            [plumbing.core :refer [defnk]]
            [dynamo.node :as n :refer [Scope]]
            [dynamo.system :as ds]
            [dynamo.system.test-support :refer :all]
            [dynamo.types :as t]
            [internal.graph.dgraph :as dg]
            [internal.graph.lgraph :as lg]
            [internal.transaction :as it]))

(defn dummy-output [& _] :ok)

(n/defnode Resource
  (input a String)
  (output b s/Keyword dummy-output))

(n/defnode Downstream
  (input consumer String))

(def gen-node (gen/fmap (fn [n] (make-resource :_id n :original-id n)) (gen/such-that #(< % 0) gen/neg-int)))

(defspec tempids-resolve-correctly
  (prop/for-all [nn gen-node]
    (with-clean-world
      (let [before (:_id nn)]
        (= before (:original-id (first (tx-nodes nn))))))))

(deftest low-level-transactions
  (testing "one node with tempid"
    (with-clean-world
      (let [tx-result (it/transact world-ref (it/new-node (make-resource :_id -5 :a "known value")))]
        (is (= :ok (:status tx-result)))
        (is (= "known value" (:a (dg/node (:graph tx-result) (it/resolve-tempid tx-result -5))))))))
  (testing "two connected nodes"
    (with-clean-world
      (let [[resource1 resource2] (tx-nodes (make-resource :_id -1) (make-downstream :_id -2))
            id1                   (:_id resource1)
            id2                   (:_id resource2)
            after                 (:graph (it/transact world-ref (it/connect resource1 :b resource2 :consumer)))]
        (is (= [id1 :b]        (first (lg/sources after id2 :consumer))))
        (is (= [id2 :consumer] (first (lg/targets after id1 :b)))))))
  (testing "disconnect two singly-connected nodes"
    (with-clean-world
      (let [[resource1 resource2] (tx-nodes (make-resource :_id -1) (make-downstream :_id -2))
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
      (let [[resource] (tx-nodes (make-resource :c 0))
            tx-result  (it/transact world-ref (it/update-property resource :c (fnil + 0) [42]))]
        (is (= :ok (:status tx-result)))
        (is (= 42 (:c (dg/node (:graph tx-result) (:_id resource))))))))
  (testing "node deletion"
    (with-clean-world
      (let [[resource1 resource2] (tx-nodes (make-resource :_id -1) (make-downstream :_id -2))]
        (it/transact world-ref (it/connect resource1 :b resource2 :consumer))
        (let [tx-result (it/transact world-ref (it/delete-node resource2))
              after     (:graph tx-result)]
          (is (nil?   (dg/node    after (:_id resource2))))
          (is (empty? (lg/targets after (:_id resource1) :b)))
          (is (= [(:_id resource2)] (map :_id (:nodes-removed tx-result)))))))))

(def trigger-called (atom 0))

(defn count-calls [& _]
  (swap! trigger-called inc))

(n/defnode CountingScope
  (inherits Scope)
  (property triggers t/Triggers (default [#'count-calls])))

(deftest trigger-runs-once
  (testing "attach one node output to input on another node"
    (with-clean-world
      (reset! trigger-called 0)
      (let [consumer (ds/transactional
                      (ds/in
                       (ds/add (make-counting-scope))
                       (ds/add (make-downstream))
                       (ds/add (make-resource))))]
        (is (= 1 @trigger-called))))))

(n/defnode NamedThing
  (property name s/Str))

(defnk friendly-name [first-name] first-name)
(defnk full-name [first-name surname] (str first-name " " surname))
(defnk age [date-of-birth] date-of-birth)

(n/defnode Person
  (property date-of-birth java.util.Date)

  (input first-name String)
  (input surname String)

  (output friendly-name String friendly-name)
  (output full-name String full-name)
  (output age java.util.Date age))

(defnk passthrough [generic-input] generic-input)

(n/defnode Receiver
  (input generic-input s/Any)
  (property touched s/Bool (default false))
  (output passthrough s/Any passthrough))

(defnk aggregator [aggregator] aggregator)

(n/defnode FocalNode
  (input aggregator [s/Any])
  (output aggregated [s/Any] aggregator))

(defn- build-network
  []
  (let [nodes {:person            (make-person)
               :first-name-cell   (make-named-thing)
               :last-name-cell    (make-named-thing)
               :greeter           (make-receiver)
               :formal-greeter    (make-receiver)
               :calculator        (make-receiver)
               :multi-node-target (make-focal-node)}
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

(defn map-keys [f m]
  (zipmap
    (map f (keys m))
    (vals m)))

(defmacro affected-by [& forms]
  `(do
     (ds/transactional ~@forms)
     (:outputs-modified (:last-tx (deref ~'world-ref)))))

(deftest precise-invalidation
  (with-clean-world
    (let [{:keys [calculator person first-name-cell greeter formal-greeter multi-node-target]} (build-network)]
      (are [update expected] (= (map-keys :_id expected) (affected-by (apply ds/set-property update)))
        [calculator :touched true]                {calculator        #{:touched}}
        [person :date-of-birth (java.util.Date.)] {person            #{:age :date-of-birth}
                                                   calculator        #{:passthrough}}
        [first-name-cell :name "Sam"]             {first-name-cell   #{:name}
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
                     (ds/add (make-event-receiver :latch (promise))))]
      (ds/transactional
        (ds/send-after receiver {:type :custom-event}))
      (is (= true (deref (:latch receiver) 500 :timeout))))))

(defnk say-hello [first-name] (str "Hello, " first-name))

(n/defnode AutoUpdateOutput
  (input first-name String)

  (output ordinary String [this g] "a-string")
  (output updating String :on-update say-hello))

(deftest on-update-properties-noted-by-transaction
  (with-clean-world
    (let [update-node (make-auto-update-output)
          event-receiver (make-event-receiver)
          tx-result (it/transact world-ref [(it/new-node event-receiver) (it/new-node update-node)])]
      (let [real-updater (dg/node (:graph tx-result) (it/resolve-tempid tx-result (:_id update-node)))]
        (is (= 1 (count (:expired-outputs tx-result))))
        (is (= [real-updater :updating] (first (:expired-outputs tx-result))))))))

(n/defnode DisposableNode
  t/IDisposable
  (dispose [this] true))

(deftest nodes-are-disposed-after-deletion
  (with-clean-world
    (let [tx-result  (it/transact world-ref (it/new-node (make-disposable-node :_id -1)))
          disposable (dg/node (:graph tx-result) (it/resolve-tempid tx-result -1))
          tx-result  (it/transact world-ref (it/delete-node disposable))]
      (is (= disposable (first (:values-to-dispose tx-result)))))))