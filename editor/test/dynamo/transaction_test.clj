(ns dynamo.transaction-test
  (:require [clojure.test.check.clojure-test :refer [defspec]]
            [clojure.test.check.generators :as gen]
            [clojure.test.check.properties :as prop]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [dynamo.node :as n]
            [dynamo.project :as p]
            [dynamo.system :as ds]
            [dynamo.system.test-support :refer :all]
            [dynamo.types :as t]
            [dynamo.util :refer :all]
            [internal.either :as e]
            [internal.graph.dgraph :as dg]
            [internal.graph.lgraph :as lg]
            [internal.transaction :as it]
            [plumbing.core :refer [defnk fnk]]))

(defn dummy-output [& _] :ok)

(defnk upcase-a [a] (.toUpperCase a))

(g/defnode Resource
  (input a String)
  (output b t/Keyword dummy-output)
  (output c String upcase-a))

(g/defnode Downstream
  (input consumer String))

(def gen-node (gen/fmap (fn [n] (n/construct Resource :_id n :original-id n)) (gen/such-that #(< % 0) gen/neg-int)))

(defspec tempids-resolve-correctly
  (prop/for-all [nn gen-node]
    (with-clean-system
      (let [before (:_id nn)]
        (= before (:original-id (first (tx-nodes nn))))))))

(deftest low-level-transactions
  (testing "one node with tempid"
    (with-clean-system
      (let [tx-result (it/transact world-ref (it/new-node (n/construct Resource :_id -5 :a "known value")))]
        (is (= :ok (:status tx-result)))
        (is (= "known value" (:a (dg/node (:graph tx-result) (it/resolve-tempid tx-result -5))))))))
  (testing "two connected nodes"
    (with-clean-system
      (let [[resource1 resource2] (tx-nodes (n/construct Resource :_id -1) (n/construct Downstream :_id -2))
            id1                   (:_id resource1)
            id2                   (:_id resource2)
            after                 (:graph (it/transact world-ref (it/connect resource1 :b resource2 :consumer)))]
        (is (= [id1 :b]        (first (lg/sources after id2 :consumer))))
        (is (= [id2 :consumer] (first (lg/targets after id1 :b)))))))
  (testing "disconnect two singly-connected nodes"
    (with-clean-system
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
    (with-clean-system
      (let [[resource] (tx-nodes (n/construct Resource :c 0))
            tx-result  (it/transact world-ref (it/update-property resource :c (fnil + 0) [42]))]
        (is (= :ok (:status tx-result)))
        (is (= 42 (:c (dg/node (:graph tx-result) (:_id resource))))))))
  (testing "node deletion"
    (with-clean-system
      (let [[resource1 resource2] (tx-nodes (n/construct Resource :_id -1) (n/construct Downstream :_id -2))]
        (it/transact world-ref (it/connect resource1 :b resource2 :consumer))
        (let [tx-result (it/transact world-ref (it/delete-node resource2))
              after     (:graph tx-result)]
          (is (nil?   (dg/node    after (:_id resource2))))
          (is (empty? (lg/targets after (:_id resource1) :b)))
          (is (contains? (:nodes-deleted tx-result) (:_id resource2)))
          (is (empty?    (:nodes-added   tx-result)))))))
  (testing "node transformation"
    (with-clean-system
      (let [[resource1] (tx-nodes (n/construct Resource :marker 99))
            id1         (:_id resource1)
            resource2   (n/construct Downstream :_id -1)
            tx-result   (it/transact world-ref (it/become resource1 resource2))
            after       (:graph tx-result)]
        (is (= :ok (:status tx-result)))
        (is (= id1 (it/resolve-tempid tx-result -1)))
        (is (= Downstream (g/node-type (dg/node after id1))))
        (is (= 99  (:marker (dg/node after id1))))))))

(defn track-trigger-activity
  ([transaction graph self label kind]
    (swap! (:tracking self) update-in [kind] (fnil inc 0)))
  ([transaction graph self label kind afflicted]
    (swap! (:tracking self) update-in [kind] (fnil conj []) afflicted)))

(g/defnode StringSource
  (property label t/Str (default "a-string")))

(g/defnode Relay
  (input label t/Str)
  (output label t/Str (fnk [label] label)))

(g/defnode TriggerExecutionCounter
  (input downstream t/Any)
  (property any-property t/Bool)

  (trigger tracker :added :deleted :property-touched :input-connections track-trigger-activity))

(g/defnode PropertySync
  (property source t/Int (default 0))
  (property sink   t/Int (default -1))

  (trigger tracker :added :deleted :property-touched :input-connections track-trigger-activity)

  (trigger copy-self :property-touched (fn [txn graph self label kind afflicted]
                                         (when true (afflicted :source)
                                           (g/set-property self :sink (:source self))))))

(g/defnode NameCollision
  (input    excalibur t/Str)
  (property excalibur t/Str)
  (trigger tracker :added :deleted :property-touched :input-connections track-trigger-activity))

(deftest trigger-activation
  (testing "runs when node is added"
    (with-clean-system
      (let [tracker      (atom {})
            counter      (g/transactional (g/add (n/construct TriggerExecutionCounter :tracking tracker)))
            after-adding @tracker]
        (is (= {:added 1} after-adding)))))

  (testing "runs when node is altered in a way that affects an output"
    (with-clean-system
      (let [tracker          (atom {})
            counter          (g/transactional (g/add (n/construct TriggerExecutionCounter :tracking tracker)))
            before-updating  @tracker
            _                (g/transactional (g/set-property counter :any-property true))
            after-updating   @tracker]
        (is (= {:added 1} before-updating))
        (is (= {:added 1 :property-touched [#{:any-property}]} after-updating)))))

  (testing "property-touched trigger run once for each pass containing a set-property action"
    (with-clean-system
      (let [tracker        (atom {})
            [node]         (tx-nodes (n/construct PropertySync :tracking tracker))
            node           (g/transactional (g/set-property node :source 42))
            after-updating @tracker]
        (is (= {:added 1 :property-touched [#{:source} #{:sink}]} after-updating))
        (is (= 42 (:sink node) (:source node))))))

  (testing "property-touched NOT run when an input of the same name is connected or invalidated"
    (with-clean-system
      (let [tracker            (atom {})
            [node1 node2]      (tx-nodes
                                 (n/construct StringSource)
                                 (n/construct NameCollision :tracking tracker))
            _                  (g/transactional (g/connect node1 :label node2 :excalibur))
            after-connect      @tracker
            _                  (g/transactional (g/set-property node1 :label "there can be only one"))
            after-set-upstream @tracker
            _                  (g/transactional (g/set-property node2 :excalibur "basis for a system of government"))
            after-set-property @tracker
            _                  (g/transactional (g/disconnect node1 :label node2 :excalibur))
            after-disconnect   @tracker]
        (is (= {:added 1 :input-connections [#{:excalibur}]} after-connect))
        (is (= {:added 1 :input-connections [#{:excalibur}]} after-set-upstream))
        (is (= {:added 1 :input-connections [#{:excalibur}] :property-touched [#{:excalibur}]} after-set-property))
        (is (= {:added 1 :input-connections [#{:excalibur} #{:excalibur}] :property-touched [#{:excalibur}]} after-disconnect)))))

  (testing "runs when node is altered in a way that doesn't affect an output"
    (with-clean-system
      (let [tracker          (atom {})
            counter          (g/transactional (g/add (n/construct TriggerExecutionCounter :tracking tracker)))
            before-updating  @tracker
            _                (g/transactional (g/set-property counter :dynamic-property true))
            after-updating   @tracker]
        (is (= {:added 1} before-updating))
        (is (= {:added 1 :property-touched [#{:dynamic-property}]} after-updating)))))

  (testing "runs when node is deleted"
    (with-clean-system
      (let [tracker         (atom {})
            counter         (g/transactional (g/add (n/construct TriggerExecutionCounter :tracking tracker)))
            before-removing @tracker
            _               (g/transactional (g/delete counter))
            after-removing  @tracker]
        (is (= {:added 1} before-removing))
        (is (= {:added 1 :deleted 1} after-removing)))))

  (testing "does *not* run when an upstream output changes"
    (with-clean-system
      (let [tracker         (atom {})
            counter         (g/transactional (g/add (n/construct TriggerExecutionCounter :tracking tracker)))
            [s r1 r2 r3]    (tx-nodes (n/construct StringSource) (n/construct Relay) (n/construct Relay) (n/construct Relay))
            _               (g/transactional
                              (g/connect s :label r1 :label)
                              (g/connect r1 :label r2 :label)
                              (g/connect r2 :label r3 :label)
                              (g/connect r3 :label counter :downstream))
            before-updating @tracker
            _               (g/transactional (g/set-property s :label "a different label"))
            after-updating  @tracker]
        (is (= {:added 1 :input-connections [#{:downstream}]} before-updating))
        (is (= before-updating after-updating)))))

  (testing "runs on the new node type when a node becomes a new node"
    (with-clean-system
      (let [tracker          (atom {})
            counter          (g/transactional (g/add (n/construct TriggerExecutionCounter :tracking tracker)))
            before-transmog  @tracker
            stringer         (g/transactional (ds/become counter (n/construct StringSource)))
            after-transmog   @tracker]
        (is (identical? (:tracking counter) (:tracking stringer)))
        (is (= {:added 1} before-transmog))
        (is (= {:added 1} after-transmog)))))

  (testing "activation is correct even when scopes and injection happen"
    (with-clean-system
      (let [tracker            (atom {})
            scope              (g/transactional (g/add (n/construct p/Project)))
            counter            (g/transactional (ds/in scope (g/add (n/construct TriggerExecutionCounter :tracking tracker))))
            before-removing    @tracker
            _                  (g/transactional (g/delete counter))
            after-removing     @tracker]

        (is (= {:added 1} before-removing))
        (is (= {:added 1 :deleted 1} after-removing))))))

(g/defnode NamedThing
  (property name t/Str))

(g/defnode Person
  (property date-of-birth java.util.Date)

  (input first-name String)
  (input surname String)

  (output friendly-name String (fnk [first-name] first-name))
  (output full-name String (fnk [first-name surname] (str first-name " " surname)))
  (output age java.util.Date (fnk [date-of-birth] date-of-birth)))

(g/defnode Receiver
  (input generic-input t/Any)
  (property touched t/Bool (default false))
  (output passthrough t/Any (fnk [generic-input] generic-input)))

(g/defnode FocalNode
  (input aggregator [t/Any])
  (output aggregated [t/Any] (fnk [aggregator] aggregator)))

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
    (g/transactional
      (doseq [[f f-l t t-l]
              [[:first-name-cell :name          :person            :first-name]
               [:last-name-cell  :name          :person            :surname]
               [:person          :friendly-name :greeter           :generic-input]
               [:person          :full-name     :formal-greeter    :generic-input]
               [:person          :age           :calculator        :generic-input]
               [:person          :full-name     :multi-node-target :aggregator]
               [:formal-greeter  :passthrough   :multi-node-target :aggregator]]]
        (g/connect (f nodes) f-l (t nodes) t-l)))
    nodes))

(defmacro affected-by [& forms]
  `(do
     (g/transactional ~@forms)
     (:outputs-modified (:last-tx (deref ~'world-ref)))))

(defn pairwise [f m]
  (for [[k vs] m
        v vs]
    [(f k) v]))

(deftest precise-invalidation
  (with-clean-system
    (let [{:keys [calculator person first-name-cell greeter formal-greeter multi-node-target]} (build-network)]
      (are [update expected] (= (into #{} (pairwise :_id expected)) (affected-by (apply g/set-property update)))
        [calculator :touched true]                {calculator        #{:properties :touched}}
        [person :date-of-birth (java.util.Date.)] {person            #{:properties :age :date-of-birth}
                                                   calculator        #{:passthrough}}
        [first-name-cell :name "Sam"]             {first-name-cell   #{:properties :name}
                                                   person            #{:full-name :friendly-name}
                                                   greeter           #{:passthrough}
                                                   formal-greeter    #{:passthrough}
                                                   multi-node-target #{:aggregated}}))))
(g/defnode EventReceiver
  (on :custom-event
    (deliver (:latch self) true)))

(deftest event-loops-started-by-transaction
  (with-clean-system
    (let [receiver (g/transactional
                     (g/add (n/construct EventReceiver :latch (promise))))]
      (g/transactional
        (ds/send-after receiver {:type :custom-event}))
      (is (= true (deref (:latch receiver) 500 :timeout))))))

(g/defnode DisposableNode
  t/IDisposable
  (dispose [this] true))

(deftest nodes-are-disposed-after-deletion
  (with-clean-system
    (let [tx-result  (it/transact world-ref (it/new-node (n/construct DisposableNode :_id -1)))
          disposable (dg/node (:graph tx-result) (it/resolve-tempid tx-result -1))
          tx-result  (it/transact world-ref (it/delete-node disposable))]
      (yield)
      (is (= disposable (first (take-waiting-to-dispose system)))))))

(g/defnode CachedOutputInvalidation
  (property a-property String (default "a-string"))

  (output ordinary String :cached (fnk [a-property] a-property))
  (output self-dependent String :cached (fnk [ordinary] ordinary)))

(deftest invalidated-properties-noted-by-transaction
  (with-clean-system
    (let [node             (n/construct CachedOutputInvalidation)
          tx-result        (it/transact world-ref [(it/new-node node)])
          real-node        (dg/node (:graph tx-result) (it/resolve-tempid tx-result (:_id node)))
          real-id          (:_id real-node)
          outputs-modified (:outputs-modified tx-result)]
      (is (some #{real-id} (map first outputs-modified)))
      (is (= #{:properties :self :self-dependent :a-property :ordinary} (into #{} (map second outputs-modified))))
      (let [tx-data          [(it/update-property real-node :a-property (constantly "new-value") [])]
            tx-result        (it/transact world-ref tx-data)
            outputs-modified (:outputs-modified tx-result)]
        (is (some #{real-id} (map first outputs-modified)))
        (is (= #{:properties :a-property :ordinary :self-dependent} (into #{} (map second outputs-modified))))))))

(deftest short-lived-nodes
  (with-clean-system
    (let [node1 (n/construct CachedOutputInvalidation)]
      (g/transactional
        (g/add node1)
        (g/delete node1))
      (is :ok))))

(g/defnode CachedValueNode
  (output cached-output t/Str :cached (fnk [] "an-output-value")))

(defn cache-peek
  [system node-id output]
  (some-> system :cache deref (get [node-id output])))

;; TODO - move this to an integration test group
(deftest values-of-a-deleted-node-are-removed-from-cache
  (with-clean-system
    (let [node    (g/transactional (g/add (n/construct CachedValueNode)))
          node-id (:_id node)]
      (is (= "an-output-value" (g/node-value (:graph @world-ref) cache node :cached-output)))
      (let [cached-value (cache-peek system node-id :cached-output)]
        (is (= "an-output-value" (e/result cached-value)))
        (g/transactional (g/delete node))
        (is (nil? (cache-peek system node-id :cached-output)))))))

(defrecord DisposableValue [disposed?]
  t/IDisposable
  (dispose [this]
    (deliver disposed? true)))

(g/defnode DisposableCachedValueNode
  (property a-property t/Str)

  (output cached-output t/IDisposable :cached (fnk [a-property] (->DisposableValue (promise)))))

(deftest cached-values-are-disposed-when-invalidated
  (with-clean-system
    (let [node   (g/transactional (g/add (n/construct DisposableCachedValueNode)))
          value1 (g/node-value (:graph @world-ref) cache node :cached-output)
          tx-result (it/transact world-ref [(it/update-property node :a-property (constantly "this should trigger disposal") [])])]
      (is (= [value1] (take-waiting-to-dispose system))))))

(g/defnode OriginalNode
  (output original-output t/Str :cached (fnk [] "original-output-value")))

(g/defnode ReplacementNode
  (output original-output t/Str (fnk [] "original-value-replaced"))
  (output additional-output t/Str :cached (fnk [] "new-output-added")))

(deftest become-interacts-with-caching
  (testing "newly uncacheable values are disposed"
    (with-clean-system
      (let [node           (g/transactional (g/add (n/construct OriginalNode)))
            node-id        (:_id node)
            expected-value (g/node-value (:graph @world-ref) cache node :original-output)]
        (is (not (nil? expected-value)))
        (is (= expected-value (e/result (cache-peek system node-id :original-output))))
        (let [node (g/transactional (ds/become node (n/construct ReplacementNode)))]
          (yield)
          (is (nil? (cache-peek system node-id :original-output)))))))

  (testing "newly cacheable values are indeed cached"
    (with-clean-system
      (let [node         (g/transactional (g/add (n/construct OriginalNode)))
            node         (g/transactional (ds/become node (n/construct ReplacementNode)))
            cached-value (g/node-value (:graph @world-ref) cache node :additional-output)]
        (yield)
        (is (= cached-value (e/result (cache-peek system (:_id node) :additional-output))))))))

(g/defnode NumberSource
  (property x          t/Num         (default 0))
  (output   sum        t/Num         (fnk [x] x))
  (output   cached-sum t/Num :cached (fnk [x] x)))

(g/defnode InputAndPropertyAdder
  (input    x          t/Num)
  (property y          t/Num (default 0))
  (output   sum        t/Num         (fnk [x y] (+ x y)))
  (output   cached-sum t/Num :cached (fnk [x y] (+ x y))))

(g/defnode InputAdder
  (input xs [t/Num])
  (output sum        t/Num         (fnk [xs] (reduce + 0 xs)))
  (output cached-sum t/Num :cached (fnk [xs] (reduce + 0 xs))))

(defn build-adder-tree
  "Builds a binary tree of connected adder nodes; returns a 2-tuple of root node and leaf nodes."
  [output-name tree-levels]
  (if (pos? tree-levels)
    (let [[n1 l1] (build-adder-tree output-name (dec tree-levels))
          [n2 l2] (build-adder-tree output-name (dec tree-levels))
          n (g/add (n/construct InputAdder))]
      (g/connect n1 output-name n :xs)
      (g/connect n2 output-name n :xs)
      [n (vec (concat l1 l2))])
    (let [n (g/add (n/construct NumberSource :x 0))]
      [n [n]])))

(deftest output-computation-inconsistencies
  (testing "computing output with stale property value"
    (with-clean-system
      (let [number-source (g/transactional (g/add (n/construct NumberSource :x 2)))
            adder-before  (g/transactional (g/add (n/construct InputAndPropertyAdder :y 3)))
            _             (g/transactional (g/connect number-source :x adder-before :x))
            adder-after   (g/transactional (g/update-property adder-before :y inc))]
        (is (= 6 (g/node-value (:graph @world-ref) cache adder-after  :sum)))
        (is (= 6 (g/node-value (:graph @world-ref) cache adder-before :sum)))))

    (with-clean-system
      (let [number-source (g/transactional (g/add (n/construct NumberSource :x 2)))
            adder-before  (g/transactional (g/add (n/construct InputAndPropertyAdder :y 3)))
            _             (g/transactional (g/connect number-source :x adder-before :x))
            _             (g/transactional (g/set-property number-source :x 22))
            adder-after   (g/transactional (g/update-property adder-before :y inc))]
        (is (= 26 (g/node-value (:graph @world-ref) cache adder-after  :sum)))
        (is (= 26 (g/node-value (:graph @world-ref) cache adder-before :sum))))))

  (testing "caching stale output value"
    (with-clean-system
      (let [number-source (g/transactional (g/add (n/construct NumberSource :x 2)))
            adder-before  (g/transactional (g/add (n/construct InputAndPropertyAdder :y 3)))
            _             (g/transactional (g/connect number-source :x adder-before :x))
            adder-after   (g/transactional (g/update-property adder-before :y inc))]
        (is (= 6 (g/node-value (:graph @world-ref) cache adder-before :cached-sum)))
        (is (= 6 (g/node-value (:graph @world-ref) cache adder-before :sum)))
        (is (= 6 (g/node-value (:graph @world-ref) cache adder-after  :cached-sum)))
        (is (= 6 (g/node-value (:graph @world-ref) cache adder-after  :sum))))))

  (testing "computation with inconsistent world"
    (with-clean-system
      (let [tree-levels            5
            iterations             100
            [adder number-sources] (g/transactional (build-adder-tree :sum tree-levels))]
        (is (= 0 (g/node-value (:graph @world-ref) cache adder :sum)))
        (dotimes [i iterations]
          (let [f1 (future (g/node-value (:graph @world-ref) cache adder :sum))
                f2 (future (g/transactional (doseq [n number-sources] (g/update-property n :x inc))))]
            (is (zero? (mod @f1 (count number-sources))))
            @f2))
        (is (= (* iterations (count number-sources)) (g/node-value (:graph @world-ref) cache adder :sum))))))

  (testing "caching result of computation with inconsistent world"
    (with-clean-system
      (let [tree-levels            5
            iterations             250
            [adder number-sources] (g/transactional (build-adder-tree :sum tree-levels))]
        (is (= 0 (g/node-value (:graph @world-ref) cache adder :cached-sum)))
        (loop [i iterations]
          (when (pos? i)
            (let [f1 (future (g/node-value (:graph @world-ref) cache adder :cached-sum))
                  f2 (future (g/transactional (doseq [n number-sources] (g/update-property n :x inc))))]
              @f2
              @f1
              (recur (dec i)))))
        (is (= (g/node-value (:graph @world-ref) cache adder :sum)
               (g/node-value (:graph @world-ref) cache adder :cached-sum))))))

  (testing "recursively computed values are cached"
    (with-clean-system
      (let [tree-levels            5
            [adder number-sources] (g/transactional (build-adder-tree :cached-sum tree-levels))]
        (g/transactional (doseq [n number-sources] (g/update-property n :x inc)))
        (is (nil? (cache-peek system (:_id adder) :cached-sum)))
        (is (nil? (cache-peek system (:_id (first number-sources)) :cached-sum)))
        (g/node-value (:graph @world-ref) cache adder :cached-sum)
        (is (= (count number-sources) (some-> (cache-peek system (:_id adder) :cached-sum)  e/result)))
        (is (= 1                      (some-> (cache-peek system (:_id (first number-sources)) :cached-sum) e/result)))))))
