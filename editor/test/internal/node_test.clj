(ns internal.node-test
  (:require [clojure.string :as str]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [dynamo.util :as util]
            [internal.graph.types :as gt]
            [internal.node :as in]
            [internal.system :as is]
            [support.test-support :refer [with-clean-system tx-nodes]]))

(def ^:dynamic *calls*)

(defn tally [node fn-symbol]
  (swap! *calls* update-in [(:_node-id node) fn-symbol] (fnil inc 0)))

(defn get-tally [node fn-symbol]
  (get-in @*calls* [(:_node-id node) fn-symbol] 0))

(g/defproperty StringWithDefault g/Str
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
  (property foo g/Str (default "FOO!")))

(g/defnode VisibilityTestNode
  (input bar g/Str)
  (property baz g/Str (dynamic visible (g/fnk [bar] (not (nil? bar))))))

(g/defnode SimpleIntTestNode
  (property foo g/Int (default 0)))

(g/defnode EnablementTestNode
  (input bar g/Int)
  (property baz g/Str (dynamic enabled (g/fnk [bar] (pos? bar)))))

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

  (property a-property g/Str)

  (output depends-on-self g/Any depends-on-self)
  (output depends-on-input g/Any depends-on-input)
  (output depends-on-property g/Any depends-on-property)
  (output depends-on-several g/Any depends-on-several))

(deftest dependency-mapping
  (testing "node reports its own dependencies"
    (let [deps (g/input-dependencies DependencyTestNode)]
      (are [input affected-outputs] (and (contains? deps input) (= affected-outputs (get deps input)))
           :an-input           #{:depends-on-input :depends-on-several}
           :a-property         #{:depends-on-property :depends-on-several :a-property :_properties :_declared-properties}
           :project            #{:depends-on-several})
      (is (not (contains? deps :this)))
      (is (not (contains? deps :g)))
      (is (not (contains? deps :unused-input))))))

(g/defnode EmptyNode)

(g/defnode OverrideOutputNode
  (property a-property String (default "a-property"))
  (output overridden String (g/fnk [a-property] a-property)))

(g/defnode SinkNode
  (input a-node-id g/NodeID))

(deftest node-intrinsics
  (testing "the _properties output delivers properties (except the 'internal' properties)"
    (with-clean-system
      (let [[n1]         (tx-nodes     (g/make-node world SimpleTestNode))
            foo-before   (g/node-value n1 :foo)
            tx-result    (g/transact   (g/set-property n1 :foo "quux"))
            foo-after    (g/node-value n1 :foo)
            [n2]         (tx-nodes     (g/make-node world SimpleTestNode :foo "bar"))
            foo-override (g/node-value n2 :foo)]
        (is (= "FOO!" foo-before))
        (is (= "quux" foo-after))
        (is (= "bar"  foo-override))
        (let [properties (g/node-value n1 :_properties)]
          (is (not (empty? (:properties properties))))
          (is (every? gt/property-type? (map :type (vals (:properties properties)))))
          (is (empty? (filter (fn [k] (some k #{:_output-jammers :_node-id})) (keys (:properties properties)))))
          (is (not (empty? (:display-order properties))))))))

  (testing "the _node-id output delivers the node's id."
    (with-clean-system
      (let [[source sink] (tx-nodes (g/make-node world EmptyNode)
                                    (g/make-node world SinkNode))]
        (g/transact
         (g/connect source :_node-id sink :a-node-id))
        (is (= source (g/node-value source :_node-id) (g/node-value sink :a-node-id))))))

  (testing "the _output-jammers property overrides ordinary output values"
    (with-clean-system
      (let [[source] (tx-nodes (g/make-node world OverrideOutputNode))]
        (is (= "a-property" (g/node-value source :overridden)))

        (g/transact (g/set-property source :_output-jammers {:overridden (constantly "Raspberry")}))

        (is (= "Raspberry" (g/node-value source :overridden)))

        (g/transact (g/set-property source :_output-jammers {}))

        (is (= "a-property" (g/node-value source :overridden)))

        (g/transact (g/set-property source :_output-jammers {:something-else (constantly "Plaid")}))

        (is (= "a-property" (g/node-value source :overridden))))))

  (testing "jamming with an error value does not cause exceptions in g/node-value"
    (with-clean-system
      (let [[source] (tx-nodes (g/make-node world OverrideOutputNode))]
        (is (= "a-property" (g/node-value source :overridden)))

        (g/transact (g/set-property source :_output-jammers {:overridden #(g/error-severe "jammed")}))

        (is (g/error? (g/node-value source :overridden)))
        (is (= "jammed" (:user-data (g/node-value source :overridden))))))))


(defn- expect-modified
  [node-type properties f]
  (with-clean-system
    (let [[node-id] (tx-nodes (g/make-node world node-type :foo "one"))
          tx-result (g/transact (f node-id))]
      (let [modified (into #{} (map second (:outputs-modified tx-result)))]
        (is (= properties modified))))))

(deftest invalidating-properties-output
  (expect-modified SimpleTestNode #{:_declared-properties :_properties :foo} (fn [node-id] (g/set-property    node-id :foo "two")))
  (expect-modified SimpleTestNode #{:_declared-properties :_properties :foo} (fn [node-id] (g/update-property node-id :foo str/reverse)))
  (expect-modified SimpleTestNode #{}                                        (fn [node-id] (g/set-property    node-id :foo "one")))
  (expect-modified SimpleTestNode #{}                                        (fn [node-id] (g/update-property node-id :foo identity))))

(deftest invalidating-visibility-properties
  (with-clean-system
    (let [[snode vnode] (tx-nodes (g/make-node world SimpleTestNode)
                                  (g/make-node world VisibilityTestNode))]
      (g/transact (g/connect snode :foo vnode :bar))
      (let [tx-result     (g/transact (g/set-property snode :foo "hi"))
            vnode-results (filter #(= (first %) vnode) (:outputs-modified tx-result))
            modified      (into #{} (map second vnode-results))]
        (is (= #{:_declared-properties :_properties} modified))))))

(deftest visibility-properties
  (with-clean-system
    (let [[snode vnode] (tx-nodes (g/make-node world SimpleTestNode)
                                  (g/make-node world VisibilityTestNode))]
      (g/transact (g/connect snode :foo vnode :bar))
      (is (= true (get-in (g/node-value vnode :_properties) [:properties :baz :visible])))
      (g/transact (g/set-property snode :foo nil))
      (is (= false (get-in (g/node-value vnode :_properties) [:properties :baz :visible]))))))

(deftest invalidating-enablement-properties
  (with-clean-system
    (let [[snode enode] (tx-nodes (g/make-node world SimpleIntTestNode)
                                  (g/make-node world EnablementTestNode))]
      (g/transact (g/connect snode :foo enode :bar))
      (let [tx-result     (g/transact (g/set-property snode :foo 1))
            enode-results (filter #(= (first %) enode) (:outputs-modified tx-result))
            modified      (into #{} (map second enode-results))]
        (is (= #{:_declared-properties :_properties} modified))))))

(deftest enablement-properties
  (with-clean-system
    (let [[snode enode] (tx-nodes (g/make-node world SimpleIntTestNode :foo 1)
                                  (g/make-node world EnablementTestNode))]
      (g/transact (g/connect snode :foo enode :bar))
      (is (= true (get-in (g/node-value enode :_properties) [:properties :baz :enabled])))
      (g/transact (g/set-property snode :foo -1))
      (is (= false (get-in (g/node-value enode :_properties) [:properties :baz :enabled]))))))

(g/defnode PropertyDynamicsTestNode
  (property one-dynamic  g/Str
            (default "FOO!")
            (dynamic emphatic? (g/fnk [one-dynamic] (.endsWith one-dynamic "!"))))

  (property three-dynamics g/Str
            (dynamic emphatic?  (g/fnk [three-dynamics] (.endsWith three-dynamics "!")))
            (dynamic querulous? (g/fnk [three-dynamics] (.endsWith three-dynamics "?")))
            (dynamic mistake?  (g/fnk [three-dynamics] (.startsWith three-dynamics "I've made a huge mistake")))))

(deftest node-property-dynamics-evaluation
  (with-clean-system
    (let [[node] (tx-nodes (g/make-node world PropertyDynamicsTestNode :three-dynamics "You?"))]
      (let [props (get (g/node-value node :_properties) :properties)]
        (is (= true  (get-in props [:one-dynamic :emphatic?])))
        (is (= false (get-in props [:three-dynamics :emphatic?])))
        (is (= true  (get-in props [:three-dynamics :querulous?])))
        (is (= false (get-in props [:three-dynamics :mistake?]))))

      (g/transact
       (g/set-property node :one-dynamic "bar?"))

      (let [props (get (g/node-value node :_properties) :properties)]
        (is (= false (get-in props [:one-dynamic :emphatic?]))))

      (g/transact
       (g/set-property node :three-dynamics "I've made a huge mistake!"))

      (let [props (get (g/node-value node :_properties) :properties)]
        (is (= true  (get-in props [:three-dynamics :emphatic?])))
        (is (= false (get-in props [:three-dynamics :querulous?])))
        (is (= true  (get-in props [:three-dynamics :mistake?])))))))


(g/defnode ProductionFunctionInputsNode
  (input in       g/Keyword)
  (input in-multi g/Keyword :array)
  (property prop g/Keyword)
  (output defnk-this       g/Any       (g/fnk production-fnk-this [this] this))
  (output defnk-prop       g/Keyword   (g/fnk production-fnk-prop [prop] prop))
  (output defnk-in         g/Keyword   (g/fnk production-fnk-in [in] in))
  (output defnk-in-multi   [g/Keyword] (g/fnk production-fnk-in-multi [in-multi] in-multi)))

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
        (is (identical? (g/node-by-id node0) (g/node-value node0 :defnk-this))))
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
    (is (= g/Keyword
          (-> ProductionFunctionInputsNode g/transform-types :prop)
          (-> ProductionFunctionInputsNode g/declared-properties :prop :value-type)))))

(g/defnode AKeywordNode
  (property prop g/Keyword))

(g/defnode AStringNode
  (property prop String))

(g/defnode BOutputNode
  (input keyword-input g/Keyword)
  (output keyword-output g/Keyword (g/fnk [keyword-input] keyword-input))
  (input array-keyword-input g/Keyword :array)
  (output array-keyword-output [g/Keyword] (g/fnk [array-keyword-input] array-keyword-input)))

(deftest node-validate-input-production-functions
  (testing "inputs to production functions are validated to be the same type as expected to the fnk"
    (binding [in/*suppress-schema-warnings* true]
     (with-clean-system
       (let [[source target] (tx-nodes
                              (g/make-node world AKeywordNode :prop "a-string")
                              (g/make-node world BOutputNode))
             _ (g/transact  (g/connect source :prop target :keyword-input))]
         (is (thrown? Exception (g/node-value target :keyword-output))))))))


(g/defnode DependencyNode
  (input in g/Any)
  (input in-multi g/Any :array)
  (output out-from-self     g/Any (g/fnk [out-from-self] out-from-self))
  (output out-from-in       g/Any (g/fnk [in]            in)))

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
  (input basic-input g/Int)
  (property string-property g/Str)
  (property property-to-override g/Str)
  (property multi-valued-property [g/Keyword] (default [:basic]))
  (output basic-output g/Keyword :cached (g/fnk [] "hello")))

(g/defproperty predefined-property-type g/Str
  (default "a-default"))

(g/defnode MultipleInheritance
  (property property-from-multiple g/Str (default "multiple")))

(g/defnode InheritsBasicNode
  (inherits BasicNode)
  (inherits MultipleInheritance)
  (input another-input g/Int :array)
  (property property-to-override g/Str (default "override"))
  (property property-from-type predefined-property-type)
  (property multi-valued-property [g/Str] (default ["extra" "things"]))
  (output another-output g/Keyword (g/fnk [this & _] :keyword))
  (output another-cached-output g/Keyword :cached (g/fnk [this & _] :keyword)))

(deftest inheritance-merges-node-types
  (testing "properties"
    (with-clean-system
      (is (:string-property      (-> (g/construct BasicNode)         g/node-type g/declared-properties)))
      (is (:string-property      (-> (g/construct InheritsBasicNode) g/node-type g/declared-properties)))
      (is (:property-to-override (-> (g/construct InheritsBasicNode) g/node-type g/declared-properties)))
      (is (= nil                 (-> (g/construct BasicNode)         g/node-type g/declared-properties :property-to-override   gt/property-default-value)))
      (is (= "override"          (-> (g/construct InheritsBasicNode) g/node-type g/declared-properties :property-to-override   gt/property-default-value)))
      (is (= "a-default"         (-> (g/construct InheritsBasicNode) g/node-type g/declared-properties :property-from-type     gt/property-default-value)))
      (is (= "multiple"          (-> (g/construct InheritsBasicNode) g/node-type g/declared-properties :property-from-multiple gt/property-default-value)))))

  (testing "transforms"
    (is (every? (-> (g/construct BasicNode) g/node-type g/output-labels)
                #{:string-property :property-to-override :multi-valued-property :basic-output}))
    (is (every? (-> (g/construct InheritsBasicNode) g/node-type g/output-labels)
                #{:string-property :property-to-override :multi-valued-property :basic-output :property-from-type :another-cached-output})))

  (testing "transform-types"
    (with-clean-system
      (is (= [g/Keyword] (-> BasicNode g/transform-types :multi-valued-property)))
      (is (= [g/Str]     (-> InheritsBasicNode g/transform-types :multi-valued-property)))))

  (testing "inputs"
    (is (every? (-> (g/construct BasicNode) g/node-type g/declared-inputs) #{:basic-input}))
    (is (every? (-> (g/construct InheritsBasicNode) g/node-type g/declared-inputs) #{:basic-input :another-input})))

  (testing "cached"
    (is (:basic-output           (-> (g/construct BasicNode)         g/node-type g/cached-outputs)))
    (is (:basic-output           (-> (g/construct InheritsBasicNode) g/node-type g/cached-outputs)))
    (is (:another-cached-output  (-> (g/construct InheritsBasicNode) g/node-type g/cached-outputs)))
    (is (not (:another-output    (-> (g/construct InheritsBasicNode) g/node-type g/cached-outputs))))))

(g/defnode PropertyValidationNode
  (property even-number g/Int
    (default 0)
    (validate (g/fnk [even-number]
                     (when (not (even? even-number))
                       (g/error-warning "only even numbers are allowed"))))))

(deftest validation-errors-delivered-in-properties-output
  (with-clean-system
    (let [[node]     (tx-nodes (g/make-node world PropertyValidationNode :even-number 1))
          properties (g/node-value node :_properties)]
      (is (g/error? (some-> properties :properties :even-number :value))))))

(g/defnk pass-through [i] i)

(g/defnode Dummy
  (property foo g/Str (default "FOO!"))
  (input i g/Any)
  (output o g/Any pass-through))

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
  (testing "Exception on setting bad property"
    (with-clean-system
      (let [[node] (tx-nodes (g/make-node world Dummy))]
        (is (thrown? Exception (g/set-property! node :no-such-property 4711)))))))

(g/defnode AlwaysNode
  (output always-99 g/Int (g/always 99))
  (property foo g/Str (dynamic visible (g/always true))))

(deftest always-fnk-test
  (testing "Always works as a shortcut for fnk constant values"
    (with-clean-system
      (let [[node] (tx-nodes (g/make-node world AlwaysNode))]
        (= 99 (g/node-value node :always-99))
        (is (= true (get-in (g/node-value node :_properties) [:properties :foo :visible])))))))

(deftest test-node-type*
  (testing "node type from node-id"
    (with-clean-system
      (let [[nid] (tx-nodes (g/make-node world AlwaysNode))]
        (is (= AlwaysNode (g/node-type* nid)))))))
