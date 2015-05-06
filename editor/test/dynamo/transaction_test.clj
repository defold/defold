(ns dynamo.transaction-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [dynamo.graph.test-support :refer :all]
            [dynamo.types :as t]
            [dynamo.util :refer :all]
            [internal.transaction :as it]
            [plumbing.core :refer [defnk fnk]]))

(defn dummy-output [& _] :ok)

(defnk upcase-a [a] (.toUpperCase a))

(g/defnode Resource
  (input a String)
  (output b t/Keyword dummy-output)
  (output c String upcase-a))

(g/defnode Downstream
  (input consumer t/Keyword))

(deftest low-level-transactions
  (testing "one node"
    (with-clean-system
      (let [tx-result (g/transact (g/make-node world Resource :a "known value"))]
        (is (= :ok (:status tx-result))))))
  (testing "two connected nodes"
    (with-clean-system
      (let [[resource1 resource2] (tx-nodes (g/make-node world Resource)
                                            (g/make-node world Downstream))
            id1                   (:_id resource1)
            id2                   (:_id resource2)
            after                 (:basis (g/transact (it/connect resource1 :b resource2 :consumer)))]
        (is (= [id1 :b]        (first (g/sources after id2 :consumer))))
        (is (= [id2 :consumer] (first (g/targets after id1 :b)))))))
  (testing "disconnect two singly-connected nodes"
    (with-clean-system
      (let [[resource1 resource2] (tx-nodes (g/make-node world Resource)
                                            (g/make-node world Downstream))
            id1                   (:_id resource1)
            id2                   (:_id resource2)
            tx-result             (g/transact (it/connect    resource1 :b resource2 :consumer))
            tx-result             (g/transact (it/disconnect resource1 :b resource2 :consumer))
            after                 (:basis tx-result)]
        (is (= :ok (:status tx-result)))
        (is (= [] (g/sources after id2 :consumer)))
        (is (= [] (g/targets after id1 :b))))))
  (testing "simple update"
    (with-clean-system
      (let [[resource] (tx-nodes (g/make-node world Resource :c 0))
            tx-result  (g/transact (it/update-property resource :c (fnil + 0) [42]))]
        (is (= :ok (:status tx-result)))
        (is (= 42 (:c (g/node-by-id (:basis tx-result) (:_id resource))))))))
  (testing "node deletion"
    (with-clean-system
      (let [[resource1 resource2] (tx-nodes (g/make-node world Resource)
                                            (g/make-node world Downstream))]
        (g/transact (it/connect resource1 :b resource2 :consumer))
        (let [tx-result  (g/transact (it/delete-node resource2))
              after      (:basis tx-result)]
          (is (nil?      (g/node-by-id   after (:_id resource2))))
          (is (empty?    (g/targets      after (:_id resource1) :b)))
          (is (contains? (:nodes-deleted tx-result) (:_id resource2)))
          (is (empty?    (:nodes-added   tx-result)))))))
  (testing "node transformation"
    (with-clean-system
      (let [[resource1] (tx-nodes (g/make-node world Resource :marker 99))
            id1         (:_id resource1)
            resource2   (g/construct Downstream)
            tx-result   (g/transact (it/become resource1 resource2))
            after       (:basis tx-result)]
        (is (= :ok (:status tx-result)))
        (is (= Downstream (g/node-type (g/node-by-id after id1))))
        (is (= 99  (:marker (g/node-by-id after id1))))))))

(defn track-trigger-activity
  ([transaction graph self label kind]
   (swap! (:tracking self) update-in [kind] (fnil inc 0))
   [])
  ([transaction graph self label kind afflicted]
   (swap! (:tracking self) update-in [kind] (fnil conj []) afflicted)
   []))

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

  (trigger copy-self :property-touched
           (fn [txn graph self label kind afflicted]
             (when true
               (it/set-property self :sink (:source self))))))

(g/defnode NameCollision
  (input    excalibur t/Str)
  (property excalibur t/Str)
  (trigger tracker :added :deleted :property-touched :input-connections track-trigger-activity))

(deftest trigger-activation
  (testing "runs when node is added"
    (with-clean-system
      (let [tracker      (atom {})
            [counter]    (tx-nodes (g/make-node world TriggerExecutionCounter :tracking tracker))
            after-adding @tracker]
        (is (= {:added 1} after-adding)))))

  (testing "runs when node is altered in a way that affects an output"
    (with-clean-system
      (let [tracker         (atom {})
            [counter]       (tx-nodes (g/make-node world TriggerExecutionCounter :tracking tracker))
            before-updating @tracker
            _               (g/transact (g/set-property counter :any-property true))
            after-updating  @tracker]
        (is (= {:added 1} before-updating))
        (is (= {:added 1 :property-touched [#{:any-property}]} after-updating)))))

  (testing "property-touched trigger run once for each pass containing a set-property action"
    (with-clean-system
      (let [tracker        (atom {})
            [node]         (tx-nodes (g/make-node world PropertySync :tracking tracker))
            _              (g/transact (g/set-property node :source 42))
            after-updating @tracker
            node           (g/refresh node)]
        (is (= {:added 1 :property-touched [#{:source} #{:sink}]} after-updating))
        (is (= 42 (:sink node) (:source node))))))

  (testing "property-touched NOT run when an input of the same name is connected or invalidated"
    (with-clean-system
      (let [tracker            (atom {})
            [node1 node2]      (tx-nodes
                                (g/make-node world StringSource)
                                (g/make-node world NameCollision :tracking tracker))
            _                  (g/transact (g/connect node1 :label node2 :excalibur))
            after-connect      @tracker
            _                  (g/transact (g/set-property node1 :label "there can be only one"))
            after-set-upstream @tracker
            _                  (g/transact (g/set-property node2 :excalibur "basis for a system of government"))
            after-set-property @tracker
            _                  (g/transact (g/disconnect node1 :label node2 :excalibur))
            after-disconnect   @tracker]
        (is (= {:added 1 :input-connections [#{:excalibur}]} after-connect))
        (is (= {:added 1 :input-connections [#{:excalibur}]} after-set-upstream))
        (is (= {:added 1 :input-connections [#{:excalibur}] :property-touched [#{:excalibur}]} after-set-property))
        (is (= {:added 1 :input-connections [#{:excalibur} #{:excalibur}] :property-touched [#{:excalibur}]} after-disconnect)))))

  (testing "runs when node is altered in a way that doesn't affect an output"
    (with-clean-system
      (let [tracker         (atom {})
            [counter]       (tx-nodes (g/make-node world TriggerExecutionCounter :tracking tracker))
            before-updating @tracker
            _               (g/transact (g/set-property counter :dynamic-property true))
            after-updating  @tracker]
        (is (= {:added 1} before-updating))
        (is (= {:added 1 :property-touched [#{:dynamic-property}]} after-updating)))))

  (testing "runs when node is deleted"
    (with-clean-system
      (let [tracker         (atom {})
            [counter]       (tx-nodes (g/make-node world TriggerExecutionCounter :tracking tracker))
            before-removing @tracker
            _               (g/transact (g/delete-node counter))
            after-removing  @tracker]
        (is (= {:added 1} before-removing))
        (is (= {:added 1 :deleted 1} after-removing)))))

  (testing "does *not* run when an upstream output changes"
    (with-clean-system
      (let [tracker         (atom {})
            [counter]       (tx-nodes (g/make-node world TriggerExecutionCounter :tracking tracker))
            [s r1 r2 r3]    (tx-nodes (g/make-node world StringSource) (g/make-node world Relay) (g/make-node world Relay) (g/make-node world Relay))
            _               (g/transact
                             (concat
                              (g/connect s :label r1 :label)
                              (g/connect r1 :label r2 :label)
                              (g/connect r2 :label r3 :label)
                              (g/connect r3 :label counter :downstream)))
            before-updating @tracker
            _               (g/transact (g/set-property s :label "a different label"))
            after-updating  @tracker]
        (is (= {:added 1 :input-connections [#{:downstream}]} before-updating))
        (is (= before-updating after-updating)))))

  (testing "runs on the new node type when a node becomes a new node"
    (with-clean-system
      (let [tracker         (atom {})
            [counter]       (tx-nodes (g/make-node world TriggerExecutionCounter :tracking tracker))
            before-transmog @tracker
            _               (g/transact (g/become counter (g/construct StringSource)))
            stringer        (g/refresh counter)
            after-transmog  @tracker]
        (is (identical? (:tracking counter) (:tracking stringer)))
        (is (= {:added 1} before-transmog))
        (is (= {:added 1} after-transmog))))))

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
  (input aggregator t/Any :array)
  (output aggregated [t/Any] (fnk [aggregator] aggregator)))

(defn- build-network
  [world]
  (let [nodes {:person            (g/make-node world Person)
               :first-name-cell   (g/make-node world NamedThing)
               :last-name-cell    (g/make-node world NamedThing)
               :greeter           (g/make-node world Receiver)
               :formal-greeter    (g/make-node world Receiver)
               :calculator        (g/make-node world Receiver)
               :multi-node-target (g/make-node world FocalNode)}
        nodes (zipmap (keys nodes) (apply tx-nodes (vals nodes)))]
    (g/transact
     (for [[f f-l t t-l]
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
  `(let [tx-result# (g/transact ~@forms)]
     (:outputs-modified tx-result#)))

(defn pairwise [f m]
  (for [[k vs] m
        v vs]
    [(f k) v]))

(deftest precise-invalidation
  (with-clean-system
    (let [{:keys [calculator person first-name-cell greeter formal-greeter multi-node-target]} (build-network world)]
      (are [update expected] (= (into #{} (pairwise :_id expected)) (affected-by (apply g/set-property update)))
        [calculator :touched true]                {calculator        #{:properties :touched :self}}
        [person :date-of-birth (java.util.Date.)] {person            #{:properties :age :date-of-birth :self}
                                                   calculator        #{:passthrough}}
        [first-name-cell :name "Sam"]             {first-name-cell   #{:properties :name :self}
                                                   person            #{:full-name :friendly-name}
                                                   greeter           #{:passthrough}
                                                   formal-greeter    #{:passthrough}
                                                   multi-node-target #{:aggregated}}))))

(g/defnode EventReceiver
  (on :custom-event
    (deliver (:latch self) true)))

(deftest event-loops-started-by-transaction
  (with-clean-system
    (let [[receiver] (tx-nodes (g/make-node world EventReceiver :latch (promise)))]
      (g/dispatch-message (g/now) receiver :custom-event)
      (is (= true (deref (:latch receiver) 500 :timeout))))))

(g/defnode DisposableNode
  t/IDisposable
  (dispose [this] true))

(deftest nodes-are-disposed-after-deletion
  (with-clean-system
    (let [[disposable] (tx-nodes (g/make-node world DisposableNode))
          tx-result    (g/transact (g/delete-node disposable))]
      (yield)
      (is (= disposable (first (take-waiting-to-dispose system)))))))

(g/defnode CachedOutputInvalidation
  (property a-property String (default "a-string"))

  (output ordinary String :cached (fnk [a-property] a-property))
  (output self-dependent String :cached (fnk [ordinary] ordinary)))

(deftest invalidated-properties-noted-by-transaction
  (with-clean-system
    (let [tx-result        (g/transact (g/make-node world CachedOutputInvalidation))
          real-node        (first (g/tx-nodes-added tx-result))
          real-id          (g/node-id real-node)
          outputs-modified (:outputs-modified tx-result)]
      (is (some #{real-id} (map first outputs-modified)))
      (is (= #{:properties :self :self-dependent :a-property :ordinary} (into #{} (map second outputs-modified))))
      (let [tx-data          [(it/update-property real-node :a-property (constantly "new-value") [])]
            tx-result        (g/transact tx-data)
            outputs-modified (:outputs-modified tx-result)]
        (is (some #{real-id} (map first outputs-modified)))
        (is (= #{:properties :a-property :ordinary :self-dependent :self} (into #{} (map second outputs-modified))))))))

(g/defnode CachedValueNode
  (output cached-output t/Str :cached (fnk [] "an-output-value")))

(defn cache-peek
  [system node-id output]
  (some-> system :cache deref (get [node-id output])))

;; TODO - move this to an integration test group
(deftest values-of-a-deleted-node-are-removed-from-cache
  (with-clean-system
    (let [[node]  (tx-nodes (g/make-node world CachedValueNode))
          node-id (:_id node)]
      (is (= "an-output-value" (g/node-value node :cached-output)))
      (let [cached-value (cache-peek system node-id :cached-output)]
        (is (= "an-output-value" cached-value))
        (g/transact (g/delete-node node))
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
    (let [[node]   (tx-nodes (g/make-node world DisposableCachedValueNode))
          value1 (g/node-value node :cached-output)
          tx-result (g/transact [(it/update-property node :a-property (constantly "this should trigger disposal") [])])]
      (is (= [value1] (take-waiting-to-dispose system))))))

(g/defnode OriginalNode
  (output original-output t/Str :cached (fnk [] "original-output-value")))

(g/defnode ReplacementNode
  (output original-output t/Str (fnk [] "original-value-replaced"))
  (output additional-output t/Str :cached (fnk [] "new-output-added")))

(deftest become-interacts-with-caching
  (testing "newly uncacheable values are disposed"
    (with-clean-system
      (let [[node]         (tx-nodes (g/make-node world OriginalNode))
            node-id        (:_id node)
            expected-value (g/node-value node :original-output)]
        (is (not (nil? expected-value)))
        (is (= expected-value (cache-peek system node-id :original-output)))
        (let [tx-result (g/transact (g/become node (g/construct ReplacementNode)))]
          (yield)
          (is (nil? (cache-peek system node-id :original-output)))))))

  (testing "newly cacheable values are indeed cached"
    (with-clean-system
      (let [[node]       (tx-nodes (g/make-node world OriginalNode))
            node-id      (:_id node)
            tx-result    (g/transact (g/become node (g/construct ReplacementNode)))
            node         (g/refresh node)
            cached-value (g/node-value node :additional-output)]
        (yield)
        (is (= cached-value (cache-peek system (:_id node) :additional-output)))))))

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
  (input xs t/Num :array)
  (output sum        t/Num         (fnk [xs] (reduce + 0 xs)))
  (output cached-sum t/Num :cached (fnk [xs] (reduce + 0 xs))))

(defn build-adder-tree
  "Builds a binary tree of connected adder nodes; returns a 2-tuple of root node and leaf nodes."
  [world output-name tree-levels]
  (if (pos? tree-levels)
    (let [[n1 l1] (build-adder-tree world output-name (dec tree-levels))
          [n2 l2] (build-adder-tree world output-name (dec tree-levels))
          [n] (tx-nodes (g/make-node world InputAdder))]
      (g/transact
       [(g/connect n1 output-name n :xs)
        (g/connect n2 output-name n :xs)])
      [n (vec (concat l1 l2))])
    (let [[n] (tx-nodes (g/make-node world NumberSource :x 0))]
      [n [n]])))

(deftest output-computation-inconsistencies
  (testing "computing output with stale property value"
    (with-clean-system
      (let [[number-source] (tx-nodes (g/make-node world NumberSource :x 2))
            [adder-before]  (tx-nodes (g/make-node world InputAndPropertyAdder :y 3))
            _               (g/transact (g/connect number-source :x adder-before :x))
            _               (g/transact (g/update-property adder-before :y inc))
            adder-after     (g/refresh adder-before)]
        (is (= 6 (g/node-value adder-after  :sum)))
        (is (= 6 (g/node-value adder-before :sum)))))

    (with-clean-system
      (let [[number-source] (tx-nodes (g/make-node world NumberSource :x 2))
            [adder-before]  (tx-nodes (g/make-node world InputAndPropertyAdder :y 3))
            _               (g/transact (g/connect number-source :x adder-before :x))
            _               (g/transact (g/set-property number-source :x 22))
            _               (g/transact (g/update-property adder-before :y inc))
            adder-after     (g/refresh adder-before)]
        (is (= 26 (g/node-value adder-after  :sum)))
        (is (= 26 (g/node-value adder-before :sum))))))

  (testing "caching stale output value"
    (with-clean-system
      (let [[number-source] (tx-nodes (g/make-node world NumberSource :x 2))
            [adder-before]  (tx-nodes (g/make-node world InputAndPropertyAdder :y 3))
            _               (g/transact (g/connect number-source :x adder-before :x))
            _               (g/transact (g/update-property adder-before :y inc))
            adder-after     (g/refresh adder-before)]
        (is (= 6 (g/node-value adder-before :cached-sum)))
        (is (= 6 (g/node-value adder-before :sum)))
        (is (= 6 (g/node-value adder-after  :cached-sum)))
        (is (= 6 (g/node-value adder-after  :sum))))))

  (testing "computation with inconsistent world"
    (with-clean-system
      (let [tree-levels            5
            iterations             100
            [adder number-sources] (build-adder-tree world :sum tree-levels)]
        (is (= 0 (g/node-value adder :sum)))
        (dotimes [i iterations]
          (let [f1 (future (g/node-value adder :sum))
                f2 (future (g/transact (for [n number-sources] (g/update-property n :x inc))))]
            (is (zero? (mod @f1 (count number-sources))))
            @f2))
        (is (= (* iterations (count number-sources)) (g/node-value adder :sum))))))

  ;; This is nondeterministic because there's no guarantee that the last cache invalidation
  ;; gets executed after the last transaction, but the test assumes that it always would.
  ;;
  ;; IOW, the test itself has a race condition that causes spurious failures.
  #_(testing "caching result of computation with inconsistent world"
    (with-clean-system
      (let [tree-levels            5
            iterations             250
            [adder number-sources] (build-adder-tree world :sum tree-levels)]
        (is (= 0 (g/node-value adder :cached-sum)))
        (loop [i iterations]
          (when (pos? i)
            (let [f1 (future (g/node-value adder :cached-sum))
                  f2 (future (g/transact (for [n number-sources] (g/update-property n :x inc))))]
              @f2
              @f1
              (recur (dec i)))))
        (let [sum        (g/node-value adder :sum)
              cached-sum (g/node-value adder :cached-sum)]
          (is (= sum cached-sum))))))

  (testing "recursively computed values are cached"
    (with-clean-system
      (let [tree-levels            5
            [adder number-sources] (build-adder-tree world :cached-sum tree-levels)]
        (g/transact (mapcat #(g/update-property % :x inc) number-sources))
        (is (nil? (cache-peek system (:_id adder) :cached-sum)))
        (is (nil? (cache-peek system (:_id (first number-sources)) :cached-sum)))
        (g/node-value adder :cached-sum)
        (is (= (count number-sources) (some-> (cache-peek system (:_id adder) :cached-sum))))
        (is (= 1                      (some-> (cache-peek system (:_id (first number-sources)) :cached-sum))))))))


;; This case is taken from editor.core/Scope.
;; Code is copied here to avoid inverted dependencies
(defn dispose-nodes
  [transaction graph self label kind]
  (when (g/is-deleted? transaction self)
    (for [node-to-delete (g/node-value self :nodes)]
      (g/delete-node node-to-delete))))

(g/defnode Container
  (input nodes t/Any :array)

  (trigger garbage-collection :deleted #'dispose-nodes))

(deftest double-deletion-is-safe
  (testing "delete scope first"
    (with-clean-system
      (let [[outer inner] (tx-nodes (g/make-node world Container) (g/make-node world Resource))]
        (g/transact (g/connect inner :self outer :nodes))
        (is (= :ok (:status (g/transact
                             (concat
                              (g/delete-node outer)
                              (g/delete-node inner)))))))))

  (testing "delete inner node first"
    (with-clean-system
      (let [[outer inner] (tx-nodes (g/make-node world Container) (g/make-node world Resource))]
        (g/transact (g/connect inner :self outer :nodes))

        (is (= :ok (:status (g/transact
                             (concat
                              (g/delete-node inner)
                              (g/delete-node outer))))))))))
