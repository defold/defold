(ns dynamo.transaction-test
  (:require [clojure.core.async :as a]
            [clojure.test.check :as tc]
            [clojure.test.check.generators :as gen]
            [clojure.test.check.properties :as prop]
            [clojure.test.check.clojure-test :refer [defspec]]
            [clojure.test :refer :all]
            [schema.core :as s]
            [plumbing.core :refer [defnk]]
            [dynamo.system :as ds]
            [dynamo.system.test-support :refer :all]
            [dynamo.node :as n :refer [Scope]]
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
        (is (= 42 (:c (dg/node (:graph tx-result) (:_id resource)))))))))

(def trigger-called (atom 0))

(defn count-calls [& _]
  (swap! trigger-called inc))

(n/defnode CountingScope
  (inherits Scope)
  (property triggers {:default [#'count-calls]}))

(deftest trigger-runs-once
  (testing "attach one node output to input on another node"
    (with-clean-world
      (reset! trigger-called 0)
      (let [consumer (ds/transactional
                      (ds/in
                       (ds/add (make-counting-scope))
                       (ds/add (make-downstream))
                       (ds/add (make-resource))

                       ))]
        (is (= 1 @trigger-called))))))

(n/defnode NamedThing
  (property name String))

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
  (property touched {:schema s/Bool :default false})
  (output passthrough s/Any passthrough))

(defn- build-network
  []
  (let [all (zipmap [:person :first-name-cell :last-name-cell :greeter :formal-greeter :calculator]
                    (tx-nodes (make-person) (make-named-thing) (make-named-thing) (make-receiver) (make-receiver) (make-receiver)))]
    (ds/transactional
     (doseq [[f f-l t t-l]
             [[:first-name-cell :name          :person         :first-name]
              [:last-name-cell  :name          :person         :surname]
              [:person          :friendly-name :greeter        :generic-input]
              [:person          :full-name     :formal-greeter :generic-input]
              [:person          :age           :calculator     :generic-input]]]
       (ds/connect (f all) f-l (t all) t-l)))
    all))

(defn- should-be-affected [nodes ident-labels]
  (apply merge-with concat {}
    (for [[node-id label] ident-labels]
      {(get-in nodes [node-id :_id]) [label]})))

(defmacro affected-by [& forms]
  `(do
     (ds/transactional ~@forms)
     (:outputs-modified (:last-tx (deref ~'world-ref)))))

(deftest precise-invalidation
  (with-clean-world
    (let [nodes (build-network)]
      (are [expected tx] (= (should-be-affected nodes expected) tx)
           []                            (affected-by
                                          (ds/update-property (:calculator nodes) :touched (constantly true)))
           [[:person :age]
            [:calculator :passthrough]]  (affected-by
                                          (ds/set-property (:person nodes) :date-of-birth (java.util.Date.)))))))

(n/defnode EventReceiver
  (on :custom-event
    (deliver (:latch self) true)))

(deftest event-loops-started-by-transaction
  (with-clean-world
    (let [receiver (ds/transactional
                    (ds/add (make-event-receiver :latch (promise))))]
      (ds/transactional
       (ds/send-after receiver {:type :custom-event}))
      (is (= true @(:latch receiver))))))

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
