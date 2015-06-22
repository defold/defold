(ns internal.node-test
  (:require [clojure.string :as str]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [dynamo.graph.test-support :refer [with-clean-system tx-nodes]]
            [dynamo.property :as dp]
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
  (are [expected property] (= expected (get (g/construct WithDefaults) property))
    "o rly?"      :default-value
    "ya rly."     :overridden-literal-value
    "uff-da"      :overridden-indirect
    "uff-da"      :overridden-indirect-by-var
    'string-value :overridden-indirect-by-symbol))

(g/defnode SimpleTestNode
  (property foo (t/maybe t/Str) (default "FOO!")))

(g/defnode VisibilityTestNode
  (input bar (t/maybe t/Str))
  (property baz (t/maybe t/Str) (visible (g/fnk [bar] (not (nil? bar))))))

(g/defnode SimpleIntTestNode
  (property foo (t/maybe t/Int) (default 0)))

(g/defnode EnablementTestNode
  (input bar (t/maybe t/Int))
  (property baz (t/maybe t/Str) (enabled (g/fnk [bar] (pos? bar)))))

(g/defnode NodeWithEvents
  (property message-processed t/Bool (default false))

  (on :mousedown
      (g/transact
       (g/set-property self :message-processed true))
    :ok))

(deftest event-delivery
  (with-clean-system
    (let [[evented] (tx-nodes (g/make-node world NodeWithEvents))]
      (is (= :ok (g/process-one-event evented {:type :mousedown})))
      (is (:message-processed (g/refresh evented))))))

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
    (is (= :owie (complainer (g/construct MyNode)))))
  (testing "node can implement interface not known/visible to internal.node"
    (is (= :ok (.allGood (g/construct MyNode))))))

(g/defnk depends-on-self [this] this)
(g/defnk depends-on-input [an-input] an-input)
(g/defnk depends-on-property [a-property] a-property)
(g/defnk depends-on-several [this project g an-input a-property] [this project g an-input a-property])

(g/defnode DependencyTestNode
  (input an-input String)
  (input unused-input String)

  (property a-property t/Str)

  (output depends-on-self t/Any depends-on-self)
  (output depends-on-input t/Any depends-on-input)
  (output depends-on-property t/Any depends-on-property)
  (output depends-on-several t/Any depends-on-several))

(deftest dependency-mapping
  (testing "node reports its own dependencies"
    (let [deps (g/input-dependencies DependencyTestNode)]
      (are [input affected-outputs] (and (contains? deps input) (= affected-outputs (get deps input)))
           :an-input           #{:depends-on-input :depends-on-several}
           :a-property         #{:depends-on-property :depends-on-several :a-property :properties :self}
           :project            #{:depends-on-several})
      (is (not (contains? deps :this)))
      (is (not (contains? deps :g)))
      (is (not (contains? deps :unused-input))))))

(g/defnode EmptyNode)

(g/defnode SinkNode
  (input a-node-id g/NodeID))

(deftest node-intrinsics
  (with-clean-system
    (let [[node] (tx-nodes (g/make-node world EmptyNode))]
      (is (identical? node (g/node-value node :self)))))
  (with-clean-system
    (let [[n1]         (tx-nodes     (g/make-node world SimpleTestNode))
          foo-before   (g/node-value n1 :foo)
          tx-result    (g/transact (g/set-property n1 :foo "quux"))
          foo-after    (g/node-value n1 :foo)
          [n2]         (tx-nodes     (g/make-node world SimpleTestNode :foo "bar"))
          foo-override (g/node-value n2 :foo)]
      (is (= "FOO!" foo-before))
      (is (= "quux" foo-after))
      (is (= "bar"  foo-override))
      (is (every? t/property-type? (map :type (vals (g/node-value n1 :properties)))))))
  (with-clean-system
    (let [[source sink] (tx-nodes (g/make-node world EmptyNode)
                                  (g/make-node world SinkNode))]
      (g/transact
       (g/connect source :node-id sink :a-node-id))
      (is (= (g/node-id source) (g/node-value source :node-id) (g/node-value sink :a-node-id))))))

(defn- expect-modified
  [node-type properties f]
  (with-clean-system
    (let [[node]    (tx-nodes (g/make-node world node-type :foo "one"))
          tx-result (g/transact (f node))]
      (let [modified (into #{} (map second (:outputs-modified tx-result)))]
        (is (= properties modified))))))

(deftest invalidating-properties-output
  (expect-modified SimpleTestNode #{:properties :foo :self} (fn [node] (g/set-property    node :foo "two")))
  (expect-modified SimpleTestNode #{:properties :foo :self} (fn [node] (g/update-property node :foo str/reverse)))
  (expect-modified SimpleTestNode #{}                       (fn [node] (g/set-property    node :foo "one")))
  (expect-modified SimpleTestNode #{}                       (fn [node] (g/update-property node :foo identity))))

(deftest invalidating-visibility-properties
  (with-clean-system
    (let [[snode vnode] (tx-nodes (g/make-node world SimpleTestNode)
                                  (g/make-node world VisibilityTestNode))]
      (g/transact (g/connect snode :foo vnode :bar))
      (let [tx-result     (g/transact (g/set-property snode :foo "hi"))
            vnode-results (filter #(= (first %) (g/node-id vnode)) (:outputs-modified tx-result))
            modified      (into #{} (map second vnode-results))]
        (is (= #{:properties} modified))))))

(deftest visibility-properties
  (with-clean-system
    (let [[snode vnode] (tx-nodes (g/make-node world SimpleTestNode)
                                  (g/make-node world VisibilityTestNode))]
      (g/transact (g/connect snode :foo vnode :bar))
      (is (= true (get-in (g/node-value vnode :properties) [:baz :visible])))
      (g/transact (g/set-property snode :foo nil))
      (is (= false (get-in (g/node-value vnode :properties) [:baz :visible]))))))

(deftest invalidating-enablement-properties
  (with-clean-system
    (let [[snode enode] (tx-nodes (g/make-node world SimpleIntTestNode)
                                  (g/make-node world EnablementTestNode))]
      (g/transact(g/connect snode :foo enode :bar))
      (let [tx-result     (g/transact (g/set-property snode :foo 1))
            enode-results (filter #(= (first %) (g/node-id enode)) (:outputs-modified tx-result))
            modified      (into #{} (map second enode-results))]
        (is (= #{:properties} modified))))))

(deftest enablement-properties
  (with-clean-system
    (let [[snode enode] (tx-nodes (g/make-node world SimpleIntTestNode :foo 1)
                                  (g/make-node world EnablementTestNode))]
      (g/transact (g/connect snode :foo enode :bar))
      (is (= true (get-in (g/node-value enode :properties) [:baz :enabled])))
      (g/transact (g/set-property snode :foo -1))
      (is (= false (get-in (g/node-value enode :properties) [:baz :enabled]))))))

(g/defnode ProductionFunctionInputsNode
  (input in       t/Keyword)
  (input in-multi t/Keyword :array)
  (property prop t/Keyword)
  (output defnk-this       t/Any       (g/fnk production-fnk-this [this] this))
  (output defnk-prop       t/Keyword   (g/fnk production-fnk-prop [prop] prop))
  (output defnk-in         t/Keyword   (g/fnk production-fnk-in [in] in))
  (output defnk-in-multi   [t/Keyword] (g/fnk production-fnk-in-multi [in-multi] in-multi)))

(deftest production-function-inputs
  (with-clean-system
    (let [[node0 node1 node2] (tx-nodes
                               (g/make-node world ProductionFunctionInputsNode :prop :node0)
                               (g/make-node world ProductionFunctionInputsNode :prop :node1)
                               (g/make-node world ProductionFunctionInputsNode :prop :node2))
          _                   (g/transact
                               (concat
                                (g/connect node0 :defnk-prop node1 :in)
                                (g/connect node0 :defnk-prop node2 :in)
                                (g/connect node1 :defnk-prop node2 :in)
                                (g/connect node0 :defnk-prop node1 :in-multi)
                                (g/connect node0 :defnk-prop node2 :in-multi)
                                (g/connect node1 :defnk-prop node2 :in-multi)))
          graph               (is/basis system)]
      (testing "'special' defnk inputs"
        (is (identical? node0     (g/node-value node0 :defnk-this))))
      (testing "defnk inputs from node properties"
        (is (= :node0 (g/node-value node0 :defnk-prop))))
      (testing "defnk inputs from node inputs"
        (is (nil?              (g/node-value node0 :defnk-in)))
        (is (= :node0          (g/node-value node1 :defnk-in)))
        (is (#{:node0 :node1}  (g/node-value node2 :defnk-in))) ;; TODO - this should just be :node1
        (is (= []              (g/node-value node0 :defnk-in-multi)))
        (is (= [:node0]        (g/node-value node1 :defnk-in-multi)))
        (is (= #{:node0 :node1} (into #{} (g/node-value node2 :defnk-in-multi))))))))

(deftest node-properties-as-node-outputs
  (testing "every property automatically creates an output that produces the property's value"
    (with-clean-system
      (let [[node0 node1] (tx-nodes
                            (g/make-node world ProductionFunctionInputsNode :prop :node0)
                            (g/make-node world ProductionFunctionInputsNode :prop :node1))
            _ (g/transact  (g/connect node0 :prop node1 :in))]
        (is (= :node0 (g/node-value node1 :defnk-in))))))
  (testing "the output has the same type as the property"
    (is (= t/Keyword
          (-> ProductionFunctionInputsNode g/transform-types :prop)
          (-> ProductionFunctionInputsNode g/properties :prop :value-type)))))


(g/defnode AKeywordNode
  (property prop t/Keyword))

(g/defnode AStringNode
  (property prop String))

(g/defnode BOutputNode
  (input keyword-input t/Keyword)
  (output keyword-output t/Keyword (g/fnk [keyword-input] keyword-input))
  (input array-keyword-input t/Keyword :array)
  (output array-keyword-output [t/Keyword] (g/fnk [array-keyword-input] array-keyword-input)))

(deftest node-validate-input-production-functions
  (testing "inputs to production functions are validated to be the same type as expected to the fnk"
    (with-redefs [in/warn (constantly "noop")]
     (with-clean-system
       (let [[source target] (tx-nodes
                              (g/make-node world AKeywordNode :prop "a-string")
                              (g/make-node world BOutputNode))
             _ (g/transact  (g/connect source :prop target :keyword-input))]
         (is (thrown? Exception (g/node-value target :keyword-output))))))))


(g/defnode DependencyNode
  (input in t/Any)
  (input in-multi t/Any :array)
  (output out-from-self     t/Any (g/fnk [out-from-self] out-from-self))
  (output out-from-in       t/Any (g/fnk [in]            in)))

(deftest dependency-loops
  (testing "output dependent on itself"
    (with-clean-system
      (let [[node] (tx-nodes (g/make-node world DependencyNode))]
        (is (thrown? AssertionError (g/node-value node :out-from-self))))))
  (testing "output dependent on itself connected to downstream input"
    (with-clean-system
      (let [[node0 node1] (tx-nodes (g/make-node world DependencyNode) (g/make-node world DependencyNode))]
        (g/transact
         (g/connect node0 :out-from-self node1 :in))
        (is (thrown? AssertionError (g/node-value node1 :out-from-in))))))
  (testing "cycle of period 1"
    (with-clean-system
      (let [[node] (tx-nodes (g/make-node world DependencyNode))]
        (g/transact (g/connect node :out-from-in node :in))
        (is (thrown? AssertionError (g/node-value node :out-from-in))))))
  (testing "cycle of period 2 (single transaction)"
    (with-clean-system
      (let [[node0 node1] (tx-nodes (g/make-node world DependencyNode) (g/make-node world DependencyNode))]
        (g/transact [(g/connect node0 :out-from-in node1 :in)
                     (g/connect node1 :out-from-in node0 :in)])
        (is (thrown? AssertionError (g/node-value node1 :out-from-in)))))))

(g/defnode BasicNode
  (input basic-input t/Int)
  (property string-property t/Str)
  (property property-to-override t/Str)
  (property multi-valued-property [t/Keyword] (default [:basic]))
  (output basic-output t/Keyword :cached (g/fnk [] "hello")))

(dp/defproperty predefined-property-type t/Str
  (default "a-default"))

(g/defnode MultipleInheritance
  (property property-from-multiple t/Str (default "multiple")))

(g/defnode InheritsBasicNode
  (inherits BasicNode)
  (inherits MultipleInheritance)
  (input another-input t/Int :array)
  (property property-to-override t/Str (default "override"))
  (property property-from-type predefined-property-type)
  (property multi-valued-property [t/Str] (default ["extra" "things"]))
  (output another-output t/Keyword (g/fnk [this & _] :keyword))
  (output another-cached-output t/Keyword :cached (g/fnk [this & _] :keyword)))

(deftest inheritance-merges-node-types
  (testing "properties"
    (with-clean-system
      (is (:string-property      (-> (g/construct BasicNode)         g/node-type g/properties)))
      (is (:string-property      (-> (g/construct InheritsBasicNode) g/node-type g/properties)))
      (is (:property-to-override (-> (g/construct InheritsBasicNode) g/node-type g/properties)))
      (is (= nil                 (-> (g/construct BasicNode)         g/node-type g/properties :property-to-override   t/property-default-value)))
      (is (= "override"          (-> (g/construct InheritsBasicNode) g/node-type g/properties :property-to-override   t/property-default-value)))
      (is (= "a-default"         (-> (g/construct InheritsBasicNode) g/node-type g/properties :property-from-type     t/property-default-value)))
      (is (= "multiple"          (-> (g/construct InheritsBasicNode) g/node-type g/properties :property-from-multiple t/property-default-value)))))

  (testing "transforms"
    (is (every? (-> (g/construct BasicNode) g/node-type g/output-labels)
                #{:string-property :property-to-override :multi-valued-property :basic-output}))
    (is (every? (-> (g/construct InheritsBasicNode) g/node-type g/output-labels)
                #{:string-property :property-to-override :multi-valued-property :basic-output :property-from-type :another-cached-output})))

  (testing "transform-types"
    (with-clean-system
      (is (= [t/Keyword] (-> BasicNode g/transform-types :multi-valued-property)))
      (is (= [t/Str]     (-> InheritsBasicNode g/transform-types :multi-valued-property)))))

  (testing "inputs"
    (is (every? (-> (g/construct BasicNode) g/node-type g/inputs) #{:basic-input}))
    (is (every? (-> (g/construct InheritsBasicNode) g/node-type g/inputs)   #{:basic-input :another-input})))

  (testing "cached"
    (is (:basic-output           (-> (g/construct BasicNode)         g/node-type g/cached-outputs)))
    (is (:basic-output           (-> (g/construct InheritsBasicNode) g/node-type g/cached-outputs)))
    (is (:another-cached-output  (-> (g/construct InheritsBasicNode) g/node-type g/cached-outputs)))
    (is (not (:another-output    (-> (g/construct InheritsBasicNode) g/node-type g/cached-outputs))))))

(g/defnode PropertyValidationNode
  (property even-number t/Int
    (default 0)
    (validate must-be-even :message "only even numbers are allowed" even?)))

(deftest validation-errors-delivered-in-properties-output
  (with-clean-system
    (let [[node]     (tx-nodes (g/make-node world PropertyValidationNode :even-number 1))
          properties (g/node-value node :properties)]
      (is (= ["only even numbers are allowed"] (some-> properties :even-number :validation-problems))))))

(g/defnk pass-through [i] i)

(g/defnode Dummy
  (property foo t/Str (default "FOO!"))
  (input i t/Any)
  (output o t/Any pass-through))

(deftest error-on-bad-source-label
  (testing "AssertionError on bad source label"
    (with-clean-system
      (let [[node1 node2] (tx-nodes (g/make-node world Dummy)
                                    (g/make-node world Dummy))]
        (is (thrown? AssertionError (g/connect! node1 :no-such-label node2 :i)))))))

(deftest error-on-bad-target-label
  (testing "AssertionError on bad target label"
    (with-clean-system
      (let [[node1 node2] (tx-nodes (g/make-node world Dummy)
                                    (g/make-node world Dummy))]
        (is (thrown? AssertionError (g/connect! node1 :o node2 :no-such-label)))))))

(deftest error-on-bad-property
  (testing "AssertionError on setting bad property"
    (with-clean-system
      (let [[node] (tx-nodes (g/make-node world Dummy))]
        (is (thrown? AssertionError (g/set-property! node :no-such-property 4711)))))))
