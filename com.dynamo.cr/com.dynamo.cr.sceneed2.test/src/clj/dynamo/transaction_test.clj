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
            [internal.disposal :as disposal]
            [internal.either :as e]
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

(defn track-trigger-activity
  ([transaction graph self label kind]
    (swap! (:tracking self) update-in [kind] (fnil inc 0)))
  ([transaction graph self label kind afflicted]
    (swap! (:tracking self) update-in [kind] (fnil conj []) afflicted)))

(n/defnode StringSource
  (property label s/Str (default "a-string")))

(n/defnode Relay
  (input label s/Str)
  (output label s/Str (fnk [label] label)))

(n/defnode TriggerExecutionCounter
  (input downstream s/Any)
  (property any-property s/Bool)

  (trigger tracker :added :deleted :property-touched :input-connections track-trigger-activity))

(n/defnode PropertySync
  (property source s/Int (default 0))
  (property sink   s/Int (default -1))

  (trigger tracker :added :deleted :property-touched :input-connections track-trigger-activity)

  (trigger copy-self :property-touched (fn [txn graph self label kind afflicted]
                                         (when true (afflicted :source)
                                           (ds/set-property self :sink (:source self))))))

(n/defnode NameCollision
  (input    excalibur s/Str)
  (property excalibur s/Str)
  (trigger tracker :added :deleted :property-touched :input-connections track-trigger-activity))

(deftest trigger-activation
  (testing "runs when node is added"
    (with-clean-world
      (let [tracker      (atom {})
            counter      (ds/transactional (ds/add (n/construct TriggerExecutionCounter :tracking tracker)))
            after-adding @tracker]
        (is (= {:added 1} after-adding)))))

  (testing "runs when node is altered in a way that affects an output"
    (with-clean-world
      (let [tracker          (atom {})
            counter          (ds/transactional (ds/add (n/construct TriggerExecutionCounter :tracking tracker)))
            before-updating  @tracker
            _                (ds/transactional (ds/set-property counter :any-property true))
            after-updating   @tracker]
        (is (= {:added 1} before-updating))
        (is (= {:added 1 :property-touched [#{:any-property}]} after-updating)))))

  (testing "property-touched trigger run once for each pass containing a set-property action"
    (with-clean-world
      (let [tracker        (atom {})
            [node]         (tx-nodes (n/construct PropertySync :tracking tracker))
            node           (ds/transactional (ds/set-property node :source 42))
            after-updating @tracker]
        (is (= {:added 1 :property-touched [#{:source} #{:sink}]} after-updating))
        (is (= 42 (:sink node) (:source node))))))

  (testing "property-touched NOT run when an input of the same name is connected or invalidated"
    (with-clean-world
      (let [tracker            (atom {})
            [node1 node2]      (tx-nodes
                                 (n/construct StringSource)
                                 (n/construct NameCollision :tracking tracker))
            _                  (ds/transactional (ds/connect node1 :label node2 :excalibur))
            after-connect      @tracker
            _                  (ds/transactional (ds/set-property node1 :label "there can be only one"))
            after-set-upstream @tracker
            _                  (ds/transactional (ds/set-property node2 :excalibur "basis for a system of government"))
            after-set-property @tracker
            _                  (ds/transactional (ds/disconnect node1 :label node2 :excalibur))
            after-disconnect   @tracker]
        (is (= {:added 1 :input-connections [#{:excalibur}]} after-connect))
        (is (= {:added 1 :input-connections [#{:excalibur}]} after-set-upstream))
        (is (= {:added 1 :input-connections [#{:excalibur}] :property-touched [#{:excalibur}]} after-set-property))
        (is (= {:added 1 :input-connections [#{:excalibur} #{:excalibur}] :property-touched [#{:excalibur}]} after-disconnect)))))

  (testing "runs when node is altered in a way that doesn't affect an output"
    (with-clean-world
      (let [tracker          (atom {})
            counter          (ds/transactional (ds/add (n/construct TriggerExecutionCounter :tracking tracker)))
            before-updating  @tracker
            _                (ds/transactional (ds/set-property counter :dynamic-property true))
            after-updating   @tracker]
        (is (= {:added 1} before-updating))
        (is (= {:added 1 :property-touched [#{:dynamic-property}]} after-updating)))))

  (testing "runs when node is deleted"
    (with-clean-world
      (let [tracker         (atom {})
            counter         (ds/transactional (ds/add (n/construct TriggerExecutionCounter :tracking tracker)))
            before-removing @tracker
            _               (ds/transactional (ds/delete counter))
            after-removing  @tracker]
        (is (= {:added 1} before-removing))
        (is (= {:added 1 :deleted 1} after-removing)))))

  (testing "does *not* run when an upstream output changes"
    (with-clean-world
      (let [tracker         (atom {})
            counter         (ds/transactional (ds/add (n/construct TriggerExecutionCounter :tracking tracker)))
            [s r1 r2 r3]    (tx-nodes (n/construct StringSource) (n/construct Relay) (n/construct Relay) (n/construct Relay))
            _               (ds/transactional
                              (ds/connect s :label r1 :label)
                              (ds/connect r1 :label r2 :label)
                              (ds/connect r2 :label r3 :label)
                              (ds/connect r3 :label counter :downstream))
            before-updating @tracker
            _               (ds/transactional (ds/set-property s :label "a different label"))
            after-updating  @tracker]
        (is (= {:added 1 :input-connections [#{:downstream}]} before-updating))
        (is (= before-updating after-updating)))))

  (testing "runs on the new node type when a node becomes a new node"
    (with-clean-world
      (let [tracker          (atom {})
            counter          (ds/transactional (ds/add (n/construct TriggerExecutionCounter :tracking tracker)))
            before-transmog  @tracker
            stringer         (ds/transactional (ds/become counter (n/construct StringSource)))
            after-transmog   @tracker]
        (is (identical? (:tracking counter) (:tracking stringer)))
        (is (= {:added 1} before-transmog))
        (is (= {:added 1} after-transmog)))))

  (testing "activation is correct even when scopes and injection happen"
    (with-clean-world
      (let [tracker            (atom {})
            scope              (ds/transactional (ds/add (n/construct p/Project)))
            counter            (ds/transactional (ds/in scope (ds/add (n/construct TriggerExecutionCounter :tracking tracker))))
            before-removing    @tracker
            _                  (ds/transactional (ds/delete counter))
            after-removing     @tracker]

        (is (= {:added 1} before-removing))
        (is (= {:added 1 :deleted 1} after-removing))))))

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

(deftest short-lived-nodes
  (with-clean-world
    (let [node1 (n/construct CachedOutputInvalidation)]
      (ds/transactional
        (ds/add node1)
        (ds/delete node1))
      (is :ok))))

(n/defnode CachedValueNode
  (output cached-output s/Str :cached (fnk [] "an-output-value")))

(defn cache-peek
  [world-ref cache-key]
  (some-> world-ref deref :cache (get cache-key)))

(defn cache-locate-key
  [world-ref node-id output]
  (some-> world-ref deref :cache-keys (get-in [node-id output])))

(deftest deleted-nodes-values-removed-from-cache
  (with-clean-world
    (let [node    (ds/transactional (ds/add (n/construct CachedValueNode)))
          node-id (:_id node)]
      (is (= "an-output-value" (n/get-node-value node :cached-output)))
      (let [cache-key    (cache-locate-key world-ref node-id :cached-output)
            cached-value (cache-peek world-ref cache-key)]
        (is (= "an-output-value" (e/result cached-value)))
        (ds/transactional (ds/delete node))
        (is (nil? (cache-peek world-ref cache-key)))
        (is (nil? (cache-locate-key world-ref node-id :cached-output)))))))

(defrecord DisposableValue [disposed?]
  t/IDisposable
  (dispose [this]
    (deliver disposed? true)))

(n/defnode DisposableCachedValueNode
  (property a-property s/Str)

  (output cached-output t/IDisposable :cached (fnk [a-property] (->DisposableValue (promise)))))

(deftest cached-values-are-disposed-when-invalidated
  (with-clean-world
    (let [node (ds/transactional (ds/add (n/construct DisposableCachedValueNode)))
          value1 (n/get-node-value node :cached-output)]
      (ds/transactional (ds/set-property node :a-property "this should trigger disposal"))
      (disposal/dispose-pending world-ref)
      (is (= true (deref (:disposed? value1) 100 :timeout))))))

(n/defnode OriginalNode
  (output original-output s/Str :cached (fnk [] "original-output-value")))

(n/defnode ReplacementNode
  (output original-output s/Str (fnk [] "original-value-replaced"))
  (output additional-output s/Str :cached (fnk [] "new-output-added")))

(deftest become-interacts-with-caching
  (testing "vanished keys are removed"
    (with-clean-world
      (let [node (ds/transactional (ds/add (n/construct OriginalNode)))]
        (is (not (nil? (cache-locate-key world-ref (:_id node) :original-output))))

        (let [node (ds/transactional (ds/become node (n/construct ReplacementNode)))]
          (is (nil? (cache-locate-key world-ref (:_id node) :original-output)))))))

  (testing "new keys are added"
    (with-clean-world
      (let [node (ds/transactional (ds/add (n/construct OriginalNode)))]
        (is (nil? (cache-locate-key world-ref (:_id node) :additional-output)))

        (let [node (ds/transactional (ds/become node (n/construct ReplacementNode)))]
          (def cache-keys* (-> world-ref deref :cache-keys))
          (is (not (nil? (cache-locate-key world-ref (:_id node) :additional-output))))))))

  (testing "new uncacheable values are disposed"
    (with-clean-world
      (let [node         (ds/transactional (ds/add (n/construct OriginalNode)))
            cache-key    (cache-locate-key world-ref (:_id node) :original-output)
            cached-value (n/get-node-value node :original-output)]
        (is (= cached-value (e/result (cache-peek world-ref cache-key))))
        (is (not (nil? (cache-peek world-ref cache-key))))

        (let [node (ds/transactional (ds/become node (n/construct ReplacementNode)))]
          (is (nil? (cache-peek world-ref cache-key)))
          (is (nil? (cache-locate-key world-ref (:_id node) :original-output)))))))

  (testing "new cacheable values are indeed cached"
    (with-clean-world
      (let [node         (ds/transactional (ds/add (n/construct OriginalNode)))
            node         (ds/transactional (ds/become node (n/construct ReplacementNode)))
            cache-key    (cache-locate-key world-ref (:_id node) :additional-output)
            cached-value (n/get-node-value node :additional-output)]
        (is (not (nil? cache-key)))
        (is (= cached-value (e/result (cache-peek world-ref cache-key))))
        (is (not (nil? (cache-peek world-ref cache-key))))))))

(n/defnode NumberSource
  (property x   s/Num (default 0))
  (output   sum s/Num (fnk [x] x)))

(n/defnode InputAndPropertyAdder
  (input    x          s/Num)
  (property y          s/Num (default 0))
  (output   sum        s/Num         (fnk [x y] (+ x y)))
  (output   cached-sum s/Num :cached (fnk [x y] (+ x y))))

(n/defnode InputAdder
  (input xs [s/Num])
  (output sum        s/Num         (fnk [xs] (reduce + 0 xs)))
  (output cached-sum s/Num :cached (fnk [xs] (reduce + 0 xs))))

(defn build-adder-tree
  "Builds a binary tree of connected adder nodes; returns a 2-tuple of root node and leaf nodes."
  [tree-levels]
  (if (pos? tree-levels)
    (let [[n1 l1] (build-adder-tree (dec tree-levels))
          [n2 l2] (build-adder-tree (dec tree-levels))
          n (ds/add (n/construct InputAdder))]
      (ds/connect n1 :sum n :xs)
      (ds/connect n2 :sum n :xs)
      [n (vec (concat l1 l2))])
    (let [n (ds/add (n/construct NumberSource :x 0))]
      [n [n]])))

(deftest output-computation-inconsistencies
  (testing "computing output with stale property value"
    (with-clean-world
      (let [number-source (ds/transactional (ds/add (n/construct NumberSource :x 2)))
            adder-before  (ds/transactional (ds/add (n/construct InputAndPropertyAdder :y 3)))
            _             (ds/transactional (ds/connect number-source :x adder-before :x))
            adder-after   (ds/transactional (ds/update-property adder-before :y inc))]
        (is (= 6 (n/get-node-value adder-after  :sum)))
        (is (= 5 (n/get-node-value adder-before :sum)))))
    (with-clean-world
      (let [number-source (ds/transactional (ds/add (n/construct NumberSource :x 2)))
            adder-before  (ds/transactional (ds/add (n/construct InputAndPropertyAdder :y 3)))
            _             (ds/transactional (ds/connect number-source :x adder-before :x))
            _             (ds/transactional (ds/set-property number-source :x 22))
            adder-after   (ds/transactional (ds/update-property adder-before :y inc))]
        (is (= 26 (n/get-node-value adder-after  :sum)))
        (is (= 25 (n/get-node-value adder-before :sum))))))
  (testing "caching stale output value"
    (with-clean-world
      (let [number-source (ds/transactional (ds/add (n/construct NumberSource :x 2)))
            adder-before  (ds/transactional (ds/add (n/construct InputAndPropertyAdder :y 3)))
            _             (ds/transactional (ds/connect number-source :x adder-before :x))
            adder-after   (ds/transactional (ds/update-property adder-before :y inc))]
        (is (= 5 (n/get-node-value adder-before :cached-sum)))
        (is (= 5 (n/get-node-value adder-before :sum)))
        (is (= 5 (n/get-node-value adder-after  :cached-sum)))
        (is (= 6 (n/get-node-value adder-after  :sum))))))
  (testing "computation with inconsistent world"
    (with-clean-world
      ; Fails non-deterministically; increase tree-levels or iterations to increase odds of failure
      (let [tree-levels            2
            iterations             1
            [adder number-sources] (ds/transactional (build-adder-tree tree-levels))]
        (is (= 0 (n/get-node-value adder :sum)))
        (dotimes [i iterations]
          (let [f1 (future (n/get-node-value adder :sum))
                f2 (future (ds/transactional (doseq [n number-sources] (ds/update-property n :x inc))))]
            (is (zero? (mod @f1 (count number-sources))))
            @f2))
        (is (= (* iterations (count number-sources)) (n/get-node-value adder :sum))))))
  (testing "caching result of computation with inconsistent world"
    (with-clean-world
      ; Fails non-deterministically; increase tree-levels or iterations to increase odds of failure
      (let [tree-levels            2
            iterations             3
            [adder number-sources] (ds/transactional (build-adder-tree tree-levels))]
        (is (= 0 (n/get-node-value adder :cached-sum)))
        (loop [i iterations]
          (when (pos? i)
            (let [f1 (future (n/get-node-value adder :cached-sum))
                  f2 (future (ds/transactional (doseq [n number-sources] (ds/update-property n :x inc))))]
              @f2
              (when (zero? (mod @f1 (count number-sources)))
                (recur (dec i))))))
        (is (= (n/get-node-value adder :sum) (n/get-node-value adder :cached-sum)))))))
