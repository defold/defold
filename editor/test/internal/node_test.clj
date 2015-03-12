(ns internal.node-test
  (:require [clojure.string :as str]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [dynamo.node :as n :refer [construct]]
            [dynamo.project :as p]
            [dynamo.property :as dp :refer [defproperty]]
            [dynamo.system :as ds]
            [dynamo.system.test-support :refer [with-clean-system tx-nodes]]
            [dynamo.types :as t]
            [internal.node :as in]
            [internal.system :as is]
            [internal.transaction :as it]
            [plumbing.core :refer [defnk fnk]]
            [schema.core :as s]))

(def ^:dynamic *calls*)

(defn tally [node fn-symbol]
  (swap! *calls* update-in [(:_id node) fn-symbol] (fnil inc 0)))

(defn get-tally [node fn-symbol]
  (get-in @*calls* [(:_id node) fn-symbol] 0))

(defproperty StringWithDefault s/Str
  (default "o rly?"))

(defn string-value [] "uff-da")

(g/defnode WithDefaults
  (property default-value                 StringWithDefault)
  (property overridden-literal-value      StringWithDefault (default "ya rly."))
;; SAMDISCUSS  (property overridden-function-value     StringWithDefault (default (constantly "vell den.")))
  (property overridden-indirect           StringWithDefault (default string-value))
  (property overridden-indirect-by-var    StringWithDefault (default #'string-value))
  (property overridden-indirect-by-symbol StringWithDefault (default 'string-value)))

(deftest node-property-defaults
  (are [expected property] (= expected (get (construct WithDefaults) property))
    "o rly?"      :default-value
    "ya rly."     :overridden-literal-value
;; SAMDISCUSS    "vell den."   :overridden-function-value
    "uff-da"      :overridden-indirect
    "uff-da"      :overridden-indirect-by-var
    'string-value :overridden-indirect-by-symbol))

(g/defnode SimpleTestNode
  (property foo s/Str (default "FOO!")))

(g/defnode NodeWithEvents
  (on :mousedown
    (g/set-property self :message-processed true)
    :ok))

(deftest event-delivery
  (with-clean-system
    (let [evented (g/transactional (g/add (construct NodeWithEvents)))]
      (is (= :ok (t/process-one-event evented {:type :mousedown})))
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
    (is (= :owie (complainer (construct MyNode)))))
  (testing "node can implement interface not known/visible to internal.node"
    (is (= :ok (.allGood (construct MyNode))))))

(defnk depends-on-self [this] this)
(defnk depends-on-input [an-input] an-input)
(defnk depends-on-property [a-property] a-property)
(defnk depends-on-several [this project g an-input a-property] [this project g an-input a-property])
(defn  depends-on-default-params [this g] [this g])

(g/defnode DependencyTestNode
  (input an-input String)
  (input unused-input String)

  (property a-property s/Str)

  (output depends-on-self s/Any depends-on-self)
  (output depends-on-input s/Any depends-on-input)
  (output depends-on-property s/Any depends-on-property)
  (output depends-on-several s/Any depends-on-several)
  (output depends-on-default-params s/Any depends-on-default-params))

(deftest dependency-mapping
  (testing "node reports its own dependencies"
    (let [test-node (construct DependencyTestNode)
          deps      (t/output-dependencies test-node)]
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
    (let [[node] (tx-nodes (construct EmptyNode))]
      (is (identical? node (g/node-value (:graph @world-ref) cache node :self)))))
  (with-clean-system
    (let [n1       (g/transactional (g/add (construct SimpleTestNode)))
          before   (:graph @world-ref)
          _        (g/transactional (g/set-property n1 :foo "quux"))
          after    (:graph @world-ref)
          n2       (g/transactional (g/add (construct SimpleTestNode :foo "bar")))
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
    (let [node (g/transactional (g/add (construct SimpleTestNode :foo "one")))]
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
  (output inline-fn      s/Keyword (fn [this g] :fn))
  (output defn-as-symbol s/Keyword production-fn)
  (output def-as-symbol  s/Keyword production-val))

(deftest production-function-types
  (with-clean-system
    (let [[node] (tx-nodes (construct ProductionFunctionTypesNode))]
      (is (= :fn   (g/node-value (:graph @world-ref) cache node :inline-fn)))
      (is (= :defn (g/node-value (:graph @world-ref) cache node :defn-as-symbol)))
      (is (= :def  (g/node-value (:graph @world-ref) cache node :def-as-symbol))))))

(defn production-fn-this [this g] this)
(defn production-fn-g [this g] g)
(defnk production-fnk-this [this] this)
(defnk production-fnk-g [g] g)
(defnk production-fnk-prop [prop] prop)
(defnk production-fnk-in [in] in)
(defnk production-fnk-in-multi [in-multi] in-multi)

(g/defnode ProductionFunctionInputsNode
  (input in       s/Keyword)
  (input in-multi [s/Keyword])
  (property prop s/Keyword)
  (output out              s/Keyword (fn [this g] :out-val))
  (output inline-fn-this   s/Any     (fn [this g] this))
  (output inline-fn-g      s/Any     (fn [this g] g))
  (output defn-this        s/Any     production-fn-this)
  (output defn-g           s/Any     production-fn-g)
  (output defnk-this       s/Any     production-fnk-this)
  (output defnk-g          s/Any     production-fnk-g)
  (output defnk-prop       s/Any     production-fnk-prop)
  (output defnk-in         s/Any     production-fnk-in)
  (output defnk-in-multi   s/Any     production-fnk-in-multi))

(deftest production-function-inputs
  (with-clean-system
    (let [[node0 node1 node2] (tx-nodes
                               (construct ProductionFunctionInputsNode :prop :node0)
                               (construct ProductionFunctionInputsNode :prop :node1)
                               (construct ProductionFunctionInputsNode :prop :node2))
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
                            (construct ProductionFunctionInputsNode :prop :node0)
                            (construct ProductionFunctionInputsNode :prop :node1))
            _ (g/transactional (g/connect node0 :prop node1 :in))]
        (is (= :node0 (g/node-value (:graph @world-ref) cache node1 :defnk-in))))))
  (testing "the output has the same type as the property"
    (is (= s/Keyword
          (-> ProductionFunctionInputsNode t/transform-types' :prop)
          (-> ProductionFunctionInputsNode t/properties' :prop :value-type)))))

(defnk out-from-self [out-from-self] out-from-self)
(defnk out-from-in [in] in)
(defnk out-from-in-multi [in-multi] in-multi)

(g/defnode DependencyNode
  (input in s/Any)
  (input in-multi [s/Any])
  (output out-from-self     s/Any out-from-self)
  (output out-from-in       s/Any out-from-in)
  (output out-const         s/Any (fn [this g] :const-val)))

(deftest dependency-loops
  (testing "output dependent on itself"
    (with-clean-system
      (let [[node] (tx-nodes (construct DependencyNode))]
        (is (thrown? Throwable (g/node-value (:graph @world-ref) cache node :out-from-self))))))
  (testing "output dependent on itself connected to downstream input"
    (with-clean-system
      (let [[node0 node1] (tx-nodes (construct DependencyNode) (construct DependencyNode))]
        (g/transactional
         (g/connect node0 :out-from-self node1 :in))
        (is (thrown? Throwable (g/node-value (:graph @world-ref) cache node1 :out-from-in))))))
  (testing "cycle of period 1"
    (with-clean-system
      (let [[node] (tx-nodes (construct DependencyNode))]
        (is (thrown? Throwable (g/transactional
                                (g/connect node :out-from-in node :in)))))))
  (testing "cycle of period 2 (single transaction)"
    (with-clean-system
      (let [[node0 node1] (tx-nodes (construct DependencyNode) (construct DependencyNode))]
        (is (thrown? Throwable (g/transactional
                                (g/connect node0 :out-from-in node1 :in)
                                (g/connect node1 :out-from-in node0 :in)))))))
  (testing "cycle of period 2 (multiple transactions)"
    (with-clean-system
      (let [[node0 node1] (tx-nodes (construct DependencyNode) (construct DependencyNode))]
        (g/transactional (g/connect node0 :out-from-in node1 :in))
        (is (thrown? Throwable (g/transactional
                                (g/connect node1 :out-from-in node0 :in))))))))

(defn throw-exception-defn [this g]
  (throw (ex-info "Exception from production function" {})))

(defnk throw-exception-defnk []
  (throw (ex-info "Exception from production function" {})))

(defn production-fn-with-abort [this g]
  (n/abort "Aborting..." {:some-key :some-value})
  :unreachable-code)

(defnk throw-exception-defnk-with-invocation-count [this]
  (tally this :invocation-count)
  (throw (ex-info "Exception from production function" {})))

(g/defnode SubstituteValueNode
  (output out-defn                     s/Any throw-exception-defn)
  (output out-defnk                    s/Any throw-exception-defnk)
  (output out-defn-with-substitute-fn  s/Any :substitute-value (constantly :substitute) throw-exception-defn)
  (output out-defnk-with-substitute-fn s/Any :substitute-value (constantly :substitute) throw-exception-defnk)
  (output out-defn-with-substitute     s/Any :substitute-value :substitute              throw-exception-defn)
  (output out-defnk-with-substitute    s/Any :substitute-value :substitute              throw-exception-defnk)
  (output substitute-value-passthrough s/Any :substitute-value identity                 throw-exception-defn)
  (output out-abort                    s/Any production-fn-with-abort)
  (output out-abort-with-substitute    s/Any :substitute-value (constantly :substitute) production-fn-with-abort)
  (output out-abort-with-substitute-f  s/Any :substitute-value (fn [_] (throw (ex-info "bailed" {}))) production-fn-with-abort)
  (output out-defnk-with-invocation-count s/Any :cached throw-exception-defnk-with-invocation-count))

(deftest node-output-substitute-value
  (with-clean-system
    (let [n (g/transactional (g/add (construct SubstituteValueNode)))]
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
  (property value s/Int))

(def ^:dynamic *answer-call-count* (atom 0))

(defnk the-answer [in]
  (swap! *answer-call-count* inc)
  (assert (= 42 in))
  in)

(g/defnode AnswerNode
  (input in s/Int)
  (output out                        s/Int                                      the-answer)
  (output out-cached                 s/Int :cached                              the-answer)
  (output out-with-substitute        s/Int         :substitute-value :forty-two the-answer)
  (output out-cached-with-substitute s/Int :cached :substitute-value :forty-two the-answer))

(deftest substitute-values-and-cache-invalidation
  (with-clean-system
    (let [[holder-node answer-node] (tx-nodes (construct ValueHolderNode :value 23) (construct AnswerNode))]

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

(defnk always-fail []
  (assert false "I always fail :-("))

(g/defnode FailureNode
  (output out s/Any always-fail))

(deftest production-fn-input-failure
  (with-clean-system
    (let [[failure-node answer-node] (tx-nodes (construct FailureNode) (construct AnswerNode))]

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
        (g/connect (g/add (construct ValueHolderNode :value 42)) :value answer-node :in))

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
  (input basic-input s/Int)
  (property string-property s/Str)
  (property property-to-override s/Str)
  (property multi-valued-property [s/Keyword] (default [:basic]))
  (output basic-output s/Keyword :cached (fn [this g] :keyword)))

(defproperty predefined-property-type s/Str
  (default "a-default"))

(g/defnode MultipleInheritance
  (property property-from-multiple s/Str (default "multiple")))

(g/defnode InheritsBasicNode
  (inherits BasicNode)
  (inherits MultipleInheritance)
  (input another-input [s/Int])
  (property property-to-override s/Str (default "override"))
  (property property-from-type predefined-property-type)
  (property multi-valued-property [s/Str] (default ["extra" "things"]))
  (output another-output s/Keyword (fn [this g] :keyword))
  (output another-cached-output s/Keyword :cached (fn [this g] :keyword)))

(deftest inheritance-merges-node-types
  (testing "properties"
    (is (:string-property (t/properties (construct BasicNode))))
    (is (:string-property (t/properties (construct InheritsBasicNode))))
    (is (:property-to-override (t/properties (construct InheritsBasicNode))))
    (is (= nil         (-> (construct BasicNode)         t/properties :property-to-override   t/property-default-value)))
    (is (= "override"  (-> (construct InheritsBasicNode) t/properties :property-to-override   t/property-default-value)))
    (is (= "a-default" (-> (construct InheritsBasicNode) t/properties :property-from-type     t/property-default-value)))
    (is (= "multiple"  (-> (construct InheritsBasicNode) t/properties :property-from-multiple t/property-default-value))))
  (testing "transforms"
    (is (every? (-> (construct BasicNode) t/outputs)
                #{:string-property :property-to-override :multi-valued-property :basic-output}))
    (is (every? (-> (construct InheritsBasicNode) t/outputs)
                #{:string-property :property-to-override :multi-valued-property :basic-output :property-from-type :another-cached-output})))
  (testing "transform-types"
    (is (= [s/Keyword] (-> BasicNode t/transform-types' :multi-valued-property)))
    (is (= [s/Str]     (-> InheritsBasicNode t/transform-types' :multi-valued-property))))
  (testing "inputs"
    (is (every? (-> (construct BasicNode) t/inputs)
                #{:basic-input}))
    (is (every? (-> (construct InheritsBasicNode) t/inputs)
                #{:basic-input :another-input})))
  (testing "cached"
    (is (:basic-output (t/cached-outputs (construct BasicNode))))
    (is (:basic-output (t/cached-outputs (construct InheritsBasicNode))))
    (is (:another-cached-output (t/cached-outputs (construct InheritsBasicNode))))
    (is (not (:another-output (t/cached-outputs (construct InheritsBasicNode)))))))

(g/defnode PropertyValidationNode
  (property even-number s/Int
    (default 0)
    (validate must-be-even :message "only even numbers are allowed" even?)))

(deftest validation-errors-delivered-in-properties-output
  (with-clean-system
    (let [[node]     (tx-nodes (n/construct PropertyValidationNode :even-number 1))
          properties (g/node-value (:graph @world-ref) cache node :properties)]
      (is (= ["only even numbers are allowed"] (some-> properties :even-number :validation-problems))))))
