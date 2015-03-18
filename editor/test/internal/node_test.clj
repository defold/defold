(ns internal.node-test
  (:require [clojure.string :as str]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [dynamo.node :as n]
            [dynamo.property :as dp]
            [dynamo.system :as ds]
            [dynamo.system.test-support :refer [with-clean-system tx-nodes]]
            [dynamo.types :as t]
            [internal.node :as in]
            [internal.system :as is]))

(def ^:dynamic *calls*)

(defn tally [node fn-symbol]
  (swap! *calls* update-in [(:_id node) fn-symbol] (fnil inc 0)))

(defn get-tally [node fn-symbol]
  (get-in @*calls* [(:_id node) fn-symbol] 0))

(dp/defproperty StringWithDefault t/Str
  (default "o rly?"))

(defn string-value [] "uff-da")

(g/defnode WithDefaults
  (property default-value                 StringWithDefault)
  (property overridden-literal-value      StringWithDefault (default "ya rly."))
  (property overridden-indirect           StringWithDefault (default string-value))
  (property overridden-indirect-by-var    StringWithDefault (default #'string-value))
  (property overridden-indirect-by-symbol StringWithDefault (default 'string-value)))

(deftest node-property-defaults
  (are [expected property] (= expected (get (n/construct WithDefaults) property))
    "o rly?"      :default-value
    "ya rly."     :overridden-literal-value
    "uff-da"      :overridden-indirect
    "uff-da"      :overridden-indirect-by-var
    'string-value :overridden-indirect-by-symbol))

(g/defnode SimpleTestNode
  (property foo t/Str (default "FOO!")))

(g/defnode NodeWithEvents
  (on :mousedown
    (g/set-property self :message-processed true)
    :ok))

(deftest event-delivery
  (with-clean-system
    (let [evented (g/transactional (g/add (n/construct NodeWithEvents)))]
      (is (= :ok (g/process-one-event evented {:type :mousedown})))
      (is (:message-processed (ds/node world-ref (:_id evented)))))))

(defprotocol AProtocol
  (complainer [this]))

(definterface IInterface
  (allGood []))

(g/defnode MyNode
  AProtocol
  (complainer [this] :owie)
  IInterface
  (allGood [this] :ok))

(deftest node-respects-namespaces
  (testing "node can implement protocols not known/visible to internal.node"
    (is (= :owie (complainer (n/construct MyNode)))))
  (testing "node can implement interface not known/visible to internal.node"
    (is (= :ok (.allGood (n/construct MyNode))))))

(g/defnk depends-on-self [this] this)
(g/defnk depends-on-input [an-input] an-input)
(g/defnk depends-on-property [a-property] a-property)
(g/defnk depends-on-several [this project g an-input a-property] [this project g an-input a-property])
(defn  depends-on-default-params [this g] [this g])

(g/defnode DependencyTestNode
  (input an-input String)
  (input unused-input String)

  (property a-property t/Str)

  (output depends-on-self t/Any depends-on-self)
  (output depends-on-input t/Any depends-on-input)
  (output depends-on-property t/Any depends-on-property)
  (output depends-on-several t/Any depends-on-several)
  (output depends-on-default-params t/Any depends-on-default-params))

(deftest dependency-mapping
  (testing "node reports its own dependencies"
    (let [test-node (n/construct DependencyTestNode)
          deps      (g/output-dependencies test-node)]
      (are [input affected-outputs] (and (contains? deps input) (= affected-outputs (get deps input)))
           :an-input           #{:depends-on-input :depends-on-several}
           :a-property         #{:depends-on-property :depends-on-several :a-property :properties}
           :project            #{:depends-on-several})
      (is (not (contains? deps :this)))
      (is (not (contains? deps :g)))
      (is (not (contains? deps :unused-input))))))

(g/defnode EmptyNode)

(deftest node-intrinsics
  (with-clean-system
    (let [[node] (tx-nodes (n/construct EmptyNode))]
      (is (identical? node (g/node-value (:graph @world-ref) cache node :self)))))
  (with-clean-system
    (let [n1       (g/transactional (g/add (n/construct SimpleTestNode)))
          before   (:graph @world-ref)
          _        (g/transactional (g/set-property n1 :foo "quux"))
          after    (:graph @world-ref)
          n2       (g/transactional (g/add (n/construct SimpleTestNode :foo "bar")))
          override (:graph @world-ref)]
      (is (not (identical? before after)))
      (are [expected-value when node]
        (= expected-value (-> (g/node-value when cache node :properties) :foo :value))
           "FOO!" before   n1
           "quux" after    n1
           "bar"  override n2)
      (is (every? t/property-type? (map :type (vals (g/node-value before cache n1 :properties))))))))

(defn- expect-modified
  [properties f]
  (with-clean-system
    (let [node (g/transactional (g/add (n/construct SimpleTestNode :foo "one")))]
      (g/transactional (f node))
      (let [modified (into #{} (map second (get-in @(:world-ref node) [:last-tx :outputs-modified])))]
        (is (= properties modified))))))

(deftest invalidating-properties-output
  (expect-modified #{:properties :foo} (fn [node] (g/set-property    node :foo "two")))
  (expect-modified #{:properties :foo} (fn [node] (g/update-property node :foo str/reverse)))
  (expect-modified #{}                 (fn [node] (g/set-property    node :foo "one")))
  (expect-modified #{}                 (fn [node] (g/update-property node :foo identity))))

(defn ^:dynamic production-fn [this g] :defn)
(def ^:dynamic production-val :def)

(g/defnode ProductionFunctionTypesNode
  (output inline-fn      t/Keyword (fn [this g] :fn))
  (output defn-as-symbol t/Keyword production-fn)
  (output def-as-symbol  t/Keyword production-val))

(deftest production-function-types
  (with-clean-system
    (let [[node] (tx-nodes (n/construct ProductionFunctionTypesNode))]
      (is (= :fn   (g/node-value (:graph @world-ref) cache node :inline-fn)))
      (is (= :defn (g/node-value (:graph @world-ref) cache node :defn-as-symbol)))
      (is (= :def  (g/node-value (:graph @world-ref) cache node :def-as-symbol))))))

(defn production-fn-this [this g] this)
(defn production-fn-g [this g] g)
(g/defnk production-fnk-this [this] this)
(g/defnk production-fnk-g [g] g)
(g/defnk production-fnk-prop [prop] prop)
(g/defnk production-fnk-in [in] in)
(g/defnk production-fnk-in-multi [in-multi] in-multi)

(g/defnode ProductionFunctionInputsNode
  (input in       t/Keyword)
  (input in-multi [t/Keyword])
  (property prop t/Keyword)
  (output out              t/Keyword (fn [this g] :out-val))
  (output inline-fn-this   t/Any     (fn [this g] this))
  (output inline-fn-g      t/Any     (fn [this g] g))
  (output defn-this        t/Any     production-fn-this)
  (output defn-g           t/Any     production-fn-g)
  (output defnk-this       t/Any     production-fnk-this)
  (output defnk-g          t/Any     production-fnk-g)
  (output defnk-prop       t/Any     production-fnk-prop)
  (output defnk-in         t/Any     production-fnk-in)
  (output defnk-in-multi   t/Any     production-fnk-in-multi))

(deftest production-function-inputs
  (with-clean-system
    (let [[node0 node1 node2] (tx-nodes
                               (n/construct ProductionFunctionInputsNode :prop :node0)
                               (n/construct ProductionFunctionInputsNode :prop :node1)
                               (n/construct ProductionFunctionInputsNode :prop :node2))
          _ (g/transactional
              (g/connect node0 :defnk-prop node1 :in)
              (g/connect node0 :defnk-prop node2 :in)
              (g/connect node1 :defnk-prop node2 :in)
              (g/connect node0 :defnk-prop node1 :in-multi)
              (g/connect node0 :defnk-prop node2 :in-multi)
              (g/connect node1 :defnk-prop node2 :in-multi))
          graph (is/graph world-ref)]
      (testing "inline fn parameters"
        (is (identical? node0 (g/node-value (:graph @world-ref) cache node0 :inline-fn-this)))
        (is (identical? graph (g/node-value (:graph @world-ref) cache node0 :inline-fn-g))))
      (testing "standard defn parameters"
        (is (identical? node0 (g/node-value (:graph @world-ref) cache node0 :defn-this)))
        (is (identical? graph (g/node-value (:graph @world-ref) cache node0 :defn-g))))
      (testing "'special' defnk inputs"
        (is (identical? node0     (g/node-value (:graph @world-ref) cache node0 :defnk-this)))
        (is (identical? graph     (g/node-value (:graph @world-ref) cache node0 :defnk-g))))
      (testing "defnk inputs from node properties"
        (is (= :node0 (g/node-value (:graph @world-ref) cache node0 :defnk-prop))))
      (testing "defnk inputs from node inputs"
        (is (nil?              (g/node-value (:graph @world-ref) cache node0 :defnk-in)))
        (is (= :node0          (g/node-value (:graph @world-ref) cache node1 :defnk-in)))
        (is (#{:node0 :node1}  (g/node-value (:graph @world-ref) cache node2 :defnk-in)))
        (is (= []              (g/node-value (:graph @world-ref) cache node0 :defnk-in-multi)))
        (is (= [:node0]        (g/node-value (:graph @world-ref) cache node1 :defnk-in-multi)))
        (is (= [:node0 :node1] (g/node-value (:graph @world-ref) cache node2 :defnk-in-multi)))))))

(deftest node-properties-as-node-outputs
  (testing "every property automatically creates an output that produces the property's value"
    (with-clean-system
      (let [[node0 node1] (tx-nodes
                            (n/construct ProductionFunctionInputsNode :prop :node0)
                            (n/construct ProductionFunctionInputsNode :prop :node1))
            _ (g/transactional (g/connect node0 :prop node1 :in))]
        (is (= :node0 (g/node-value (:graph @world-ref) cache node1 :defnk-in))))))
  (testing "the output has the same type as the property"
    (is (= t/Keyword
          (-> ProductionFunctionInputsNode g/transform-types' :prop)
          (-> ProductionFunctionInputsNode g/properties' :prop :value-type)))))

(g/defnk out-from-self [out-from-self] out-from-self)
(g/defnk out-from-in [in] in)
(g/defnk out-from-in-multi [in-multi] in-multi)

(g/defnode DependencyNode
  (input in t/Any)
  (input in-multi [t/Any])
  (output out-from-self     t/Any out-from-self)
  (output out-from-in       t/Any out-from-in)
  (output out-const         t/Any (fn [this g] :const-val)))

(deftest dependency-loops
  (testing "output dependent on itself"
    (with-clean-system
      (let [[node] (tx-nodes (n/construct DependencyNode))]
        (is (thrown? Throwable (g/node-value (:graph @world-ref) cache node :out-from-self))))))
  (testing "output dependent on itself connected to downstream input"
    (with-clean-system
      (let [[node0 node1] (tx-nodes (n/construct DependencyNode) (n/construct DependencyNode))]
        (g/transactional
         (g/connect node0 :out-from-self node1 :in))
        (is (thrown? Throwable (g/node-value (:graph @world-ref) cache node1 :out-from-in))))))
  (testing "cycle of period 1"
    (with-clean-system
      (let [[node] (tx-nodes (n/construct DependencyNode))]
        (is (thrown? Throwable (g/transactional
                                (g/connect node :out-from-in node :in)))))))
  (testing "cycle of period 2 (single transaction)"
    (with-clean-system
      (let [[node0 node1] (tx-nodes (n/construct DependencyNode) (n/construct DependencyNode))]
        (is (thrown? Throwable (g/transactional
                                (g/connect node0 :out-from-in node1 :in)
                                (g/connect node1 :out-from-in node0 :in)))))))
  (testing "cycle of period 2 (multiple transactions)"
    (with-clean-system
      (let [[node0 node1] (tx-nodes (n/construct DependencyNode) (n/construct DependencyNode))]
        (g/transactional (g/connect node0 :out-from-in node1 :in))
        (is (thrown? Throwable (g/transactional
                                (g/connect node1 :out-from-in node0 :in))))))))

(defn throw-exception-defn [this g]
  (throw (ex-info "Exception from production function" {})))

(g/defnk throw-exception-defnk []
  (throw (ex-info "Exception from production function" {})))

(defn production-fn-with-abort [this g]
  (n/abort "Aborting..." {:some-key :some-value})
  :unreachable-code)

(g/defnk throw-exception-defnk-with-invocation-count [this]
  (tally this :invocation-count)
  (throw (ex-info "Exception from production function" {})))

(g/defnode SubstituteValueNode
  (output out-defn                     t/Any throw-exception-defn)
  (output out-defnk                    t/Any throw-exception-defnk)
  (output out-defn-with-substitute-fn  t/Any :substitute-value (constantly :substitute) throw-exception-defn)
  (output out-defnk-with-substitute-fn t/Any :substitute-value (constantly :substitute) throw-exception-defnk)
  (output out-defn-with-substitute     t/Any :substitute-value :substitute              throw-exception-defn)
  (output out-defnk-with-substitute    t/Any :substitute-value :substitute              throw-exception-defnk)
  (output substitute-value-passthrough t/Any :substitute-value identity                 throw-exception-defn)
  (output out-abort                    t/Any production-fn-with-abort)
  (output out-abort-with-substitute    t/Any :substitute-value (constantly :substitute) production-fn-with-abort)
  (output out-abort-with-substitute-f  t/Any :substitute-value (fn [_] (throw (ex-info "bailed" {}))) production-fn-with-abort)
  (output out-defnk-with-invocation-count t/Any :cached throw-exception-defnk-with-invocation-count))

(deftest node-output-substitute-value
  (with-clean-system
    (let [n (g/transactional (g/add (n/construct SubstituteValueNode)))]
      (testing "exceptions from node-value (:graph @world-ref) cache when no substitute value fn"
        (is (thrown? Throwable (g/node-value (:graph @world-ref) cache n :out-defn)))
        (is (thrown? Throwable (g/node-value (:graph @world-ref) cache n :out-defnk))))
      (testing "substitute value replacement"
        (is (= :substitute (g/node-value (:graph @world-ref) cache n :out-defn-with-substitute-fn)))
        (is (= :substitute (g/node-value (:graph @world-ref) cache n :out-defnk-with-substitute-fn)))
        (is (= :substitute (g/node-value (:graph @world-ref) cache n :out-defn-with-substitute)))
        (is (= :substitute (g/node-value (:graph @world-ref) cache n :out-defnk-with-substitute))))
      (testing "parameters to substitute value fn"
        (is (= (:_id n) (:node-id (g/node-value (:graph @world-ref) cache n :substitute-value-passthrough)))))
      (testing "exception from substitute value fn"
        (is (thrown-with-msg? Throwable #"bailed" (g/node-value (:graph @world-ref) cache n :out-abort-with-substitute-f))))
      (testing "abort"
        (is (thrown-with-msg? Throwable #"Aborting\.\.\." (g/node-value (:graph @world-ref) cache n :out-abort)))
        (try
          (g/node-value (:graph @world-ref) cache n :out-abort)
          (is false "Expected node-value (:graph @world-ref) cache to throw an exception")
          (catch Throwable e
            (is (= {:some-key :some-value} (ex-data e)))))
        (is (= :substitute (g/node-value (:graph @world-ref) cache n :out-abort-with-substitute))))
      (testing "interaction with cache"
        (binding [*calls* (atom {})]
          (is (= 0 (get-tally n :invocation-count)))
          (is (thrown? Throwable (g/node-value (:graph @world-ref) cache n :out-defnk-with-invocation-count)))
          (is (= 1 (get-tally n :invocation-count)))
          (is (thrown? Throwable (g/node-value (:graph @world-ref) cache n :out-defnk-with-invocation-count)))
          (is (= 1 (get-tally n :invocation-count))))))))

(g/defnode ValueHolderNode
  (property value t/Int))

(def ^:dynamic *answer-call-count* (atom 0))

(g/defnk the-answer [in]
  (swap! *answer-call-count* inc)
  (assert (= 42 in))
  in)

(g/defnode AnswerNode
  (input in t/Int)
  (output out                        t/Int                                      the-answer)
  (output out-cached                 t/Int :cached                              the-answer)
  (output out-with-substitute        t/Int         :substitute-value :forty-two the-answer)
  (output out-cached-with-substitute t/Int :cached :substitute-value :forty-two the-answer))

(deftest substitute-values-and-cache-invalidation
  (with-clean-system
    (let [[holder-node answer-node] (tx-nodes (n/construct ValueHolderNode :value 23) (n/construct AnswerNode))]

      (g/transactional (g/connect holder-node :value answer-node :in))

      (binding [*answer-call-count* (atom 0)]
        (is (thrown? Throwable (g/node-value (:graph @world-ref) cache answer-node :out)))
        (is (thrown? Throwable (g/node-value (:graph @world-ref) cache answer-node :out)))
        (is (= 2 @*answer-call-count*)))
      (binding [*answer-call-count* (atom 0)]
        (is (thrown? Throwable (g/node-value (:graph @world-ref) cache answer-node :out-cached)))
        (is (thrown? Throwable (g/node-value (:graph @world-ref) cache answer-node :out-cached)))
        (is (= 1 @*answer-call-count*)))
      (binding [*answer-call-count* (atom 0)]
        (is (= :forty-two (g/node-value (:graph @world-ref) cache answer-node :out-with-substitute)))
        (is (= :forty-two (g/node-value (:graph @world-ref) cache answer-node :out-with-substitute)))
        (is (= 2 @*answer-call-count*)))
      (binding [*answer-call-count* (atom 0)]
        (is (= :forty-two (g/node-value (:graph @world-ref) cache answer-node :out-cached-with-substitute)))
        (is (= :forty-two (g/node-value (:graph @world-ref) cache answer-node :out-cached-with-substitute)))
        (is (= 1 @*answer-call-count*)))

      (g/transactional (g/set-property holder-node :value 42))

      (binding [*answer-call-count* (atom 0)]
        (is (= 42 (g/node-value (:graph @world-ref) cache answer-node :out)))
        (is (= 1 @*answer-call-count*)))
      (binding [*answer-call-count* (atom 0)]
        (is (= 42 (g/node-value (:graph @world-ref) cache answer-node :out-cached)))
        (is (= 1 @*answer-call-count*)))
      (binding [*answer-call-count* (atom 0)]
        (is (= 42 (g/node-value (:graph @world-ref) cache answer-node :out-with-substitute)))
        (is (= 1 @*answer-call-count*)))
      (binding [*answer-call-count* (atom 0)]
        (is (= 42 (g/node-value (:graph @world-ref) cache answer-node :out-cached-with-substitute)))
        (is (= 1 @*answer-call-count*))))))

(g/defnk always-fail []
  (assert false "I always fail :-("))

(g/defnode FailureNode
  (output out t/Any always-fail))

(deftest production-fn-input-failure
  (with-clean-system
    (let [[failure-node answer-node] (tx-nodes (n/construct FailureNode) (n/construct AnswerNode))]

      (println 'production-fn-input-failure :answer-node (select-keys answer-node (keys answer-node)))

      (g/transactional (g/connect failure-node :out answer-node :in))

      (binding [*answer-call-count* (atom 0)]
        (is (thrown? Throwable (g/node-value (:graph @world-ref) cache answer-node :out)))
        (is (= 0 @*answer-call-count*)))
      (binding [*answer-call-count* (atom 0)]
        (is (thrown? Throwable (g/node-value (:graph @world-ref) cache answer-node :out-cached)))
        (is (= 0 @*answer-call-count*)))
      (binding [*answer-call-count* (atom 0)]
        (is (= :forty-two (g/node-value (:graph @world-ref) cache answer-node :out-with-substitute)))
        (is (= 0 @*answer-call-count*)))
      (binding [*answer-call-count* (atom 0)]
        (is (= :forty-two (g/node-value (:graph @world-ref) cache answer-node :out-cached-with-substitute)))
        (is (= 0 @*answer-call-count*)))

      (g/transactional
        (g/disconnect failure-node :out answer-node :in)
        (g/connect (g/add (n/construct ValueHolderNode :value 42)) :value answer-node :in))

      (binding [*answer-call-count* (atom 0)]
        (is (= 42 (g/node-value (:graph @world-ref) cache answer-node :out)))
        (is (= 1 @*answer-call-count*)))
      (binding [*answer-call-count* (atom 0)]
        (is (= 42 (g/node-value (:graph @world-ref) cache answer-node :out-cached)))
        (is (= 42 (g/node-value (:graph @world-ref) cache answer-node :out-cached)))
        (is (= 1 @*answer-call-count*)))
      (binding [*answer-call-count* (atom 0)]
        (is (= 42 (g/node-value (:graph @world-ref) cache answer-node :out-with-substitute)))
        (is (= 1 @*answer-call-count*)))
      (binding [*answer-call-count* (atom 0)]
        (is (= 42 (g/node-value (:graph @world-ref) cache answer-node :out-cached-with-substitute)))
        (is (= 42 (g/node-value (:graph @world-ref) cache answer-node :out-cached-with-substitute)))
        (is (= 1 @*answer-call-count*))))))


(g/defnode BasicNode
  (input basic-input t/Int)
  (property string-property t/Str)
  (property property-to-override t/Str)
  (property multi-valued-property [t/Keyword] (default [:basic]))
  (output basic-output t/Keyword :cached (fn [this g] :keyword)))

(dp/defproperty predefined-property-type t/Str
  (default "a-default"))

(g/defnode MultipleInheritance
  (property property-from-multiple t/Str (default "multiple")))

(g/defnode InheritsBasicNode
  (inherits BasicNode)
  (inherits MultipleInheritance)
  (input another-input [t/Int])
  (property property-to-override t/Str (default "override"))
  (property property-from-type predefined-property-type)
  (property multi-valued-property [t/Str] (default ["extra" "things"]))
  (output another-output t/Keyword (fn [this g] :keyword))
  (output another-cached-output t/Keyword :cached (fn [this g] :keyword)))

(deftest inheritance-merges-node-types
  (testing "properties"
    (is (:string-property (g/properties (n/construct BasicNode))))
    (is (:string-property (g/properties (n/construct InheritsBasicNode))))
    (is (:property-to-override (g/properties (n/construct InheritsBasicNode))))
    (is (= nil         (-> (n/construct BasicNode)         g/properties :property-to-override   t/property-default-value)))
    (is (= "override"  (-> (n/construct InheritsBasicNode) g/properties :property-to-override   t/property-default-value)))
    (is (= "a-default" (-> (n/construct InheritsBasicNode) g/properties :property-from-type     t/property-default-value)))
    (is (= "multiple"  (-> (n/construct InheritsBasicNode) g/properties :property-from-multiple t/property-default-value))))
  (testing "transforms"
    (is (every? (-> (n/construct BasicNode) g/outputs)
                #{:string-property :property-to-override :multi-valued-property :basic-output}))
    (is (every? (-> (n/construct InheritsBasicNode) g/outputs)
                #{:string-property :property-to-override :multi-valued-property :basic-output :property-from-type :another-cached-output})))
  (testing "transform-types"
    (is (= [t/Keyword] (-> BasicNode g/transform-types' :multi-valued-property)))
    (is (= [t/Str]     (-> InheritsBasicNode g/transform-types' :multi-valued-property))))
  (testing "inputs"
    (is (every? (-> (n/construct BasicNode) g/inputs)
                #{:basic-input}))
    (is (every? (-> (n/construct InheritsBasicNode) g/inputs)
                #{:basic-input :another-input})))
  (testing "cached"
    (is (:basic-output (g/cached-outputs (n/construct BasicNode))))
    (is (:basic-output (g/cached-outputs (n/construct InheritsBasicNode))))
    (is (:another-cached-output (g/cached-outputs (n/construct InheritsBasicNode))))
    (is (not (:another-output (g/cached-outputs (n/construct InheritsBasicNode)))))))

(g/defnode PropertyValidationNode
  (property even-number t/Int
    (default 0)
    (validate must-be-even :message "only even numbers are allowed" even?)))

(deftest validation-errors-delivered-in-properties-output
  (with-clean-system
    (let [[node]     (tx-nodes (n/construct PropertyValidationNode :even-number 1))
          properties (g/node-value (:graph @world-ref) cache node :properties)]
      (is (= ["only even numbers are allowed"] (some-> properties :even-number :validation-problems))))))
