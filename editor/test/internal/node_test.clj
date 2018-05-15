(ns internal.node-test
  (:require [clojure.string :as str]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [internal.node :as in]
            [internal.system :as is]
            [internal.util :as util]
            [schema.core :as s]
            [support.test-support :refer [tx-nodes with-clean-system]]
            [internal.graph.types :as gt])
  (:import clojure.lang.ExceptionInfo))

(def ^:dynamic *calls*)

(defn tally [node fn-symbol]
  (swap! *calls* update-in [(:_node-id node) fn-symbol] (fnil inc 0)))

(defn get-tally [node fn-symbol]
  (get-in @*calls* [(:_node-id node) fn-symbol] 0))

(g/defnk string-value [] "uff-da")

(g/defnode WithDefaults
  (property default-value                 g/Str (default "o rly?"))
  (property overridden-indirect           g/Str (default string-value)))

(deftest node-property-defaults
  (are [expected property] (= expected (get (g/construct WithDefaults) property))
       "o rly?"      :default-value
       "uff-da"      :overridden-indirect))

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

(g/defnk depends-on-self [this] this)
(g/defnk depends-on-input [an-input] an-input)
(g/defnk depends-on-property [a-property] a-property)
(g/defnk depends-on-several [this an-input a-property] [this an-input a-property])

(g/defnode DependencyTestNode
  (input an-input g/Str)
  (input unused-input g/Str)

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
                  :a-property         #{:depends-on-property :depends-on-several :a-property :_properties :_declared-properties})
             (is (not (contains? deps :this)))
             (is (not (contains? deps :unused-input))))))

(g/defnode EmptyNode)

(g/defnode OverrideOutputNode
  (property a-property g/Str (default "a-property"))
  (output overridden g/Str (g/fnk [a-property] a-property)))

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

                 (is (every? in/value-type? (map (comp deref :type) (vals (:properties properties)))))
                 (is (empty? (filter (fn [k] (some k #{:_output-jammers :_node-id})) (keys (:properties properties)))))
                 (is (not (empty? (:display-order properties))))))))

  (testing "the _node-id output delivers the node's id."
    (with-clean-system
      (let [[source sink] (tx-nodes (g/make-node world EmptyNode)
                                    (g/make-node world SinkNode))]
        (g/transact
         (g/connect source :_node-id sink :a-node-id))
        (is (= source (g/node-value source :_node-id)))
        (is (= source (g/node-value sink   :a-node-id))))))

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

        (g/transact (g/set-property source :_output-jammers {:overridden #(g/error-fatal "jammed")}))

        (is (g/error? (g/node-value source :overridden)))
        (is (= "jammed" (:message (g/node-value source :overridden))))))))

(deftest construct-with-maps
  (testing "supplying a map to make-nodes"
           (let [params {:foo "foo"}]
             (with-clean-system
               (let [[n] (tx-nodes (g/make-node world SimpleTestNode params))]
                 (is (= "foo" (g/node-value n :foo))))
               (let [[n] (tx-nodes (g/make-nodes world [n [SimpleTestNode params]]))]
                 (is (= "foo" (g/node-value n :foo))))))))

(deftest invalid-property-type
  (testing "supplying a map to make-nodes but property type is invalid"
           (with-clean-system
             (binding [in/*suppress-schema-warnings* true]
               (is (thrown? ExceptionInfo (tx-nodes (g/make-node world SimpleTestNode :foo 1))))))))

(defn- expect-modified
  [node-type properties f]
  (with-clean-system
    (let [[node-id] (tx-nodes (g/make-node world node-type :foo "one"))
          tx-result (g/transact (f node-id))]
      (let [modified (into #{} (map second (:outputs-modified tx-result)))]
        (is (= properties modified))))))

(deftest invalidating-properties-output
  (expect-modified SimpleTestNode
                   #{:_declared-properties :_properties :foo}
                   (fn [node-id] (g/set-property    node-id :foo "two")))
  (expect-modified SimpleTestNode
                   #{:_declared-properties :_properties :foo}
                   (fn [node-id] (g/update-property node-id :foo str/reverse)))
  (expect-modified SimpleTestNode #{} (fn [node-id] (g/set-property    node-id :foo "one")))
  (expect-modified SimpleTestNode #{} (fn [node-id] (g/update-property node-id :foo identity))))


(deftest invalidation-affects-dynamics
  (with-clean-system
    (let [[source target]               (tx-nodes
                                         (g/make-nodes world [source SimpleTestNode
                                                              target VisibilityTestNode]
                                                       (g/connect source :foo target :bar)))
          tx-result                     (g/set-property! source :foo "hi")
          properties-modified-on-target (set (keep #(when (= (first %) target) (second %)) (:outputs-modified tx-result)))]
      (is (= #{:_declared-properties :baz :_properties} properties-modified-on-target)))))

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
        (is (= #{:_declared-properties :baz :_properties} modified))))))

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
            (dynamic emphatic? (g/fnk [one-dynamic] (.endsWith ^String one-dynamic "!"))))

  (property three-dynamics g/Str
            (dynamic emphatic?  (g/fnk [three-dynamics] (.endsWith ^String three-dynamics "!")))
            (dynamic querulous? (g/fnk [three-dynamics] (.endsWith ^String three-dynamics "?")))
            (dynamic mistake?  (g/fnk [three-dynamics] (.startsWith ^String three-dynamics "I've made a huge mistake")))))

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
  (property prop  g/Keyword)
  (output defnk-this       g/Any       (g/fnk [this] this))
  (output defnk-prop       g/Keyword   (g/fnk [prop] prop))
  (output defnk-in         g/Keyword   (g/fnk [in] in))
  (output defnk-in-multi   [g/Keyword] (g/fnk [in-multi] in-multi)))

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
          (-> ProductionFunctionInputsNode (g/output-type :prop))
          (-> ProductionFunctionInputsNode g/declared-properties :prop :value-type)))))

(g/defnode AKeywordNode
  (property prop g/Keyword))

(g/defnode AStringNode
  (property prop g/Str))

(g/defnode BOutputNode
  (input keyword-input g/Keyword)
  (output keyword-output g/Keyword (g/fnk [keyword-input] keyword-input))
  (input array-keyword-input g/Keyword :array)
  (output array-keyword-output [g/Keyword] (g/fnk [array-keyword-input] array-keyword-input)))

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

(g/defnode MultipleInheritance
  (property property-from-multiple g/Str (default "multiple")))

(g/defnode InheritsBasicNode
  (inherits BasicNode)
  (inherits MultipleInheritance)
  (input another-input g/Int :array)
  (property property-to-override g/Str (default "override"))
  (property multi-valued-property [g/Str] (default ["extra" "things"]))
  (output another-output g/Keyword (g/fnk [this] :keyword))
  (output another-cached-output g/Keyword :cached (g/fnk [this] :keyword)))

(deftest inheritance-merges-node-types
  (testing "properties"
    (with-clean-system
      (is (:string-property      (-> (g/construct BasicNode)         g/node-type g/declared-properties)))
      (is (:string-property      (-> (g/construct InheritsBasicNode) g/node-type g/declared-properties)))
      (is (:property-to-override (-> (g/construct InheritsBasicNode) g/node-type g/declared-properties)))
      (is (= nil                 (-> (g/construct BasicNode)         :property-to-override  )))
      (is (= "override"          (-> (g/construct InheritsBasicNode) :property-to-override   )))
      (is (= "multiple"          (-> (g/construct InheritsBasicNode) :property-from-multiple )))))

  (testing "transforms"
    (is (every? (-> (g/construct BasicNode) g/node-type g/output-labels)
                #{:string-property :property-to-override :multi-valued-property :basic-output}))
    (is (every? (-> (g/construct InheritsBasicNode) g/node-type g/output-labels)
                #{:string-property :property-to-override :multi-valued-property :basic-output :another-cached-output})))

  (testing "transform-types"
    (with-clean-system
      (are [nt p vt] (= vt (in/output-type nt p))
        BasicNode         :multi-valued-property g/Keyword
        InheritsBasicNode :multi-valued-property g/Str)))

  (testing "inputs"
    (is (every? (-> (g/construct BasicNode) g/node-type g/declared-inputs) #{:basic-input}))
    (is (every? (-> (g/construct InheritsBasicNode) g/node-type g/declared-inputs) #{:basic-input :another-input})))

  (testing "cached"
    (is (:basic-output           (-> (g/construct BasicNode)         g/node-type g/cached-outputs)))
    (is (:basic-output           (-> (g/construct InheritsBasicNode) g/node-type g/cached-outputs)))
    (is (:another-cached-output  (-> (g/construct InheritsBasicNode) g/node-type g/cached-outputs)))
    (is (not (:another-output    (-> (g/construct InheritsBasicNode) g/node-type g/cached-outputs))))))

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
  (testing "AssertionError on setting bad property"
    (with-clean-system
      (let [[node] (tx-nodes (g/make-node world Dummy))]
        (is (thrown? AssertionError (g/set-property! node :no-such-property 4711)))))))

(g/defnode AlwaysNode
  (output always-99 g/Int (g/fnk [] 99))
  (property foo g/Str (dynamic visible true)))

(deftest dynamics-allow-constant-values
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

(g/defnode SetPropertyNode
  (property bar g/Str)
  (property foo g/Str
            (value (g/fnk [foo] foo))
            (set (fn [_evaluation-context self old-value new-value]
                   (concat
                    (g/set-property self :foo new-value)
                    (g/set-property self :bar new-value)))))
  (property multi-prop g/Str
            (value (g/fnk [bar foo] (str bar "-" foo)))
            (dynamic visible (g/fnk [multi-prop] multi-prop))))

(deftest test-set-property-recursive
  (testing "node type from node-id"
           (with-clean-system
             (let [[nid] (tx-nodes (g/make-node world SetPropertyNode))]
               (g/transact (g/set-property nid :foo "foo"))
               (is (= "foo" (g/node-value nid :foo)))
               (is (= "foo" (g/node-value nid :bar)))
               (is (= "foo-foo" (g/node-value nid :multi-prop)))
               (let [p (get-in (g/node-value nid :_properties) [:properties :multi-prop])]
                 (is (= "foo-foo" (:visible p))))))))

(g/defnode DynamicGetterNode
  (property suffixed g/Str
            (value (g/fnk [prefix suffixed]
                          (str prefix "/" suffixed))))
  (input prefix g/Str))

(g/defnode DynamicGetterOutputNode
  (inherits DynamicGetterNode)
  (output suffixed g/Str (g/fnk [suffixed] (subs suffixed 1))))

(deftest test-dynamic-getter
  (testing "input collection for dynamic getters"
    (with-clean-system
      (let [[_ nid] (tx-nodes (g/make-nodes world [from [SimpleTestNode :foo "directory"]
                                                   to [DynamicGetterNode :suffixed "file"]]
                                            (g/connect from :foo to :prefix)))]
        (is (= "directory/file" (g/node-value nid :suffixed)))
        (g/transact (g/set-property nid :suffixed "unnamed"))
        (is (= "directory/unnamed" (g/node-value nid :suffixed)))
        (is (= "directory/unnamed" (get-in (g/node-value nid :_properties) [:properties :suffixed :value]))))))
  (testing "input collection for dynamic getters with overloaded outputs"
    (with-clean-system
      (let [[_ nid] (tx-nodes (g/make-nodes world [from [SimpleTestNode :foo "directory"]
                                                   to [DynamicGetterOutputNode :suffixed "file"]]
                                            (g/connect from :foo to :prefix)))]
        (is (= "irectory/file" (g/node-value nid :suffixed)))
        (g/transact (g/set-property nid :suffixed "unnamed"))
        (is (= "irectory/unnamed" (g/node-value nid :suffixed)))))))

(deftest property-display-order-merging
  (are [expected _ sources] (= expected (apply g/merge-display-order sources))
    [:id :path]                                                     -> [[:id :path]]
    [:id :path :rotation :position]                                 -> [[:id :path] [:rotation :position]]
    [:rotation :position :id :path]                                 -> [[:rotation :position] [:id :path]]
    [:rotation :position :scale :id :path]                          -> [[:rotation :position] [:scale] [:id :path]]
    [["Transform" :rotation :position :scale]]                      -> [[["Transform" :rotation :position]] [["Transform" :scale]]]
    [["Transform" :rotation :position :scale] :path]                -> [[["Transform" :rotation :position]] [["Transform" :scale] :path]]
    [:id ["Transform" :rotation :scale] :path ["Foo" :scale] :cake] -> [[:id ["Transform" :rotation]] [["Transform" :scale] :path] [["Foo" :scale] :cake]]
    [:id :path ["Transform" :rotation :position :scale]]            -> [[:id :path ["Transform"]] [["Transform" :rotation :position :scale]]]
    [["Material" :specular :ambient] :position :rotation]           -> [[["Material" :specular :ambient]] [:specular :ambient] [:position :rotation]]))

(g/defnode DonorNode
  (property a-property g/Int (default 0)))

(g/defnode AdoptorNode
  (property own-property g/Int (default -1))

  (input submitted-properties g/Properties)

  (output _properties g/Properties
          (g/fnk [_node-id _declared-properties submitted-properties]
                 (->> submitted-properties
                     (g/adopt-properties _node-id)
                     (g/aggregate-properties _declared-properties)))))

(deftest property-adoption
  (with-clean-system
    (let [[donor adoptor] (g/tx-nodes-added
                           (g/transact
                            (g/make-nodes world [donor DonorNode adoptor AdoptorNode]
                                          (g/connect donor :_properties adoptor :submitted-properties))))
          final-props     (g/node-value adoptor :_properties)]
      (is (contains? (:properties final-props) :own-property))
      (is (= adoptor (get-in final-props [:properties :own-property :node-id])))

      (is (contains? (:properties final-props) :a-property))
      (is (= adoptor (get-in final-props [:properties :a-property :node-id]))))))

(g/defnode CachedBoolean
  (property counter g/Any)
  (output cached-boolean g/Bool :cached (g/fnk [counter] (swap! counter inc) false)))

(deftest cached-booleans
  (with-clean-system
    (let [[nid] (tx-nodes (g/make-nodes world [n [CachedBoolean :counter (atom 0)]]))]
      (is (false? (g/node-value nid :cached-boolean)))
      (is (= 1 @(g/node-value nid :counter)))
      (is (false? (g/node-value nid :cached-boolean)))
      (is (= 1 @(g/node-value nid :counter))))))

(g/defnode MyNode
  (property a-property g/Str))

(deftest make-nodes-complains-about-missing-properties
  (with-clean-system
    (is (thrown? AssertionError
                 (eval `(dynamo.graph/make-nodes ~world [new-node# [MyNode :no-such-property 1]]))))))

(deftest construct-complains-about-missing-properties
  (with-clean-system
    (is (thrown? AssertionError
                 (g/construct MyNode :_node-id 1 :no-such-property 1)))))

(g/defnode ResourceNode
  (property resource g/Str (default "Hello")))

(g/defnode IntermediateResourceNode
  (inherits ResourceNode))

(g/defnode PlaceholderNode
  (inherits IntermediateResourceNode))

(deftest grandchild-property-inheritance
  (with-clean-system
    (let [[resource-node] (tx-nodes (g/make-node world PlaceholderNode))]
      (is (= "Hello" (g/node-value resource-node :resource))))))

(g/defnode GetterFromSetter
  (property old-foo g/Int)
  (property foo g/Int
            (value (g/fnk [foo] (inc foo)))
            (set (fn [_evaluation-context self old-value new-value]
                   (g/set-property self :old-foo old-value))))
  (output foo g/Int (g/fnk [] 100)))

(deftest test-getter-from-setter
  (with-clean-system
    (let [[nid] (tx-nodes (g/make-node world GetterFromSetter :foo 1))]
      ;; (set ...) will have been called with old = nil, new = 1 since it's the constructor
      (is (= nil (g/node-value nid :old-foo)))
      (g/transact (g/set-property nid :foo 3))
      ;; (set ...) will have been called with old = (inc 1), new = 3 since it's the constructor
      (is (= 2 (g/node-value nid :old-foo))))))

(def ^:private production-count (atom 0))

(g/defnode Producer
  (property val g/Str (default ""))
  (output produce g/Str (g/fnk [val]
                               (swap! production-count inc)
                               val)))

(g/defnode Consumer
  (input in1 g/Str)
  (input in2 g/Str)
  (output result g/Str (g/fnk [in1 in2] (str in1 in2))))

(deftest non-cached-produced-once
  (with-clean-system
    (let [[wat-producer nil-producer consumer] (tx-nodes (g/make-node world Producer :val "wat")
                                                         (g/make-node world Producer :val nil)
                                                         (g/make-node world Consumer))]

      ;; NOTE! If we're seriously unlucky a gc could make these tests
      ;; fail by collecting the result held in a WeakReference

      ;; check "wat" is temp cached
      (g/transact
        (concat
          (g/connect wat-producer :produce consumer :in1)
          (g/connect wat-producer :produce consumer :in2)))

      (reset! production-count 0)
      (is (= (g/node-value consumer :result) "watwat"))
      (is (= @production-count 1))

      ;, check nil is temp cached
      (g/transact
        (concat
          (g/connect nil-producer :produce consumer :in1)
          (g/connect nil-producer :produce consumer :in2)))

      (reset! production-count 0)
      (is (= (g/node-value consumer :result) ""))
      (is (= @production-count 1))


      ;; check :no-local-temp is respected
      (reset! production-count 0)
      (is (= (g/node-value consumer :result (g/make-evaluation-context {:no-local-temp true})) ""))
      (is (= @production-count 2))

      (reset! production-count 0)
      (is (= (g/node-value consumer :result (g/make-evaluation-context {:no-local-temp false})) ""))
      (is (= @production-count 1)))))
