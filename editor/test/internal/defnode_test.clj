(ns internal.defnode-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [internal.graph.types :as gt]
            [internal.node :as in]
            [support.test-support :refer [tx-nodes with-clean-system]]
            [internal.util :as util]
            [schema.core :as s])
  (:import clojure.lang.Compiler))

(g/deftype Int s/Int)

(defn- vtr [x] (in/->ValueTypeRef x))
(def int-ref (vtr :internal.defnode-test/Int))

(deftest parsing-type-forms
  (testing "aliases for the same type ref"
    (are [x] (= {:value-type int-ref :flags #{}}
                (in/parse-type-form "test" x))
      'Int
      `Int
      :internal.defnode-test/Int
      'internal.defnode-test/Int))

  (testing "multivalued cases"
    (are [x] (= {:value-type int-ref :flags #{:collection}}
                (in/parse-type-form "test" x))
      '[Int]
      `[Int]
      [:internal.defnode-test/Int]
      '[internal.defnode-test/Int]))

  (testing "unhappy path"
    (are [x] (thrown? AssertionError (in/parse-type-form "sadpath test" x))
      'NoSuchSymbol
      `NoSuchSymbol)))


(defn substitute-value-fn [& _] "substitute value")

(g/defnode BasicNode)

(deftest basic-node-definition
  (is (instance? clojure.lang.IDeref BasicNode))
  (is (satisfies? g/NodeType @BasicNode))
  (is (= BasicNode (g/node-type (g/construct BasicNode)))))

(g/defnode IRootNode)
(g/defnode ChildNode
  (inherits IRootNode))
(g/defnode GChild
  (inherits ChildNode))
(g/defnode MixinNode)
(g/defnode GGChild
  (inherits ChildNode)
  (inherits MixinNode))

(deftest inheritance
  (is (= #{IRootNode}                      (g/supertypes ChildNode)))
  (is (= ChildNode                         (g/node-type (g/construct ChildNode))))
  (is (= #{ChildNode IRootNode}            (g/supertypes GChild)))
  (is (= GChild                            (g/node-type (g/construct GChild))))
  (is (= #{ChildNode MixinNode IRootNode}  (g/supertypes GGChild)))
  (is (= GGChild                           (g/node-type (g/construct GGChild))))
  (is (thrown? AssertionError              (eval '(dynamo.graph/defnode BadInheritance (inherits :not-a-symbol)))))
  (is (thrown? AssertionError              (eval '(dynamo.graph/defnode BadInheritance (inherits DoesntExist))))))

(g/defnode OneInputNode
  (input an-input g/Str))

(g/defnode InheritedInputNode
  (inherits OneInputNode))

(g/defnode InjectableInputNode
  (input for-injection g/Int :inject))

(g/defnode SubstitutingInputNode
  (input substitute-fn  g/Str :substitute substitute-value-fn)
  (input var-to-fn      g/Str :substitute #'substitute-value-fn)
  (input inline-literal g/Str :substitute "inline literal"))

(g/defnode CardinalityInputNode
  (input single-scalar-value        g/Str)
  (input single-collection-value    [g/Str])
  (input multiple-scalar-values     g/Str :array)
  (input multiple-collection-values [g/Str] :array))

(g/defnode CascadingInputNode
  (input component g/Str :cascade-delete))

(deftest nodes-can-have-inputs
  (testing "inputs must be defined with symbols"
    (is (thrown? AssertionError
                 (eval '(dynamo.graph/defnode BadInput (input :not-a-symbol g/Str))))))
  (testing "inputs must be defined with a schema"
    (is (thrown? AssertionError
                 (eval '(dynamo.graph/defnode BadInput (input a-input (fn [] "not a schema")))))))
  (testing "labeled input"
    (let [node (g/construct OneInputNode)]
      (is (:an-input (g/declared-inputs OneInputNode)))
      (is (= g/Str (g/input-type OneInputNode :an-input)))
      (is (= g/Str (-> OneInputNode g/declared-inputs :an-input :value-type)))))
  (testing "inherited input"
    (let [node (g/construct InheritedInputNode)]
      (is (:an-input (g/declared-inputs InheritedInputNode)))
      (is (= g/Str (g/input-type InheritedInputNode :an-input)))
      (is (= g/Str (-> InheritedInputNode g/declared-inputs :an-input :value-type)))))
  (testing "inputs can be flagged for injection"
    (let [node (g/construct InjectableInputNode)]
      (is (:for-injection (g/injectable-inputs InjectableInputNode)))))
  (testing "inputs can have substitute values to use when there is no source"
    (is (= substitute-value-fn   (g/substitute-for SubstitutingInputNode :substitute-fn)))
    (is (= #'substitute-value-fn (g/substitute-for SubstitutingInputNode :var-to-fn)))
    (is (= "inline literal"      (g/substitute-for SubstitutingInputNode :inline-literal))))

  (testing "the default cardinality is :one"
    (is (= :one (g/input-cardinality CardinalityInputNode :single-scalar-value)))
    (is (= :one (g/input-cardinality CardinalityInputNode :single-collection-value))))

  (testing "inputs can declare their cardinality"
    (is (= :many (g/input-cardinality CardinalityInputNode :multiple-scalar-values)))
    (is (= :many (g/input-cardinality CardinalityInputNode :multiple-collection-values))))

  (testing "inputs can declare a cascade-delete flag"
    (is (= (empty?       (g/cascade-deletes CardinalityInputNode))))
    (is (= #{:component} (g/cascade-deletes CascadingInputNode)))))


(defn- private-function [x] [x :ok])

(g/defnode SinglePropertyNode
  (property a-property g/Str))

(g/defnode TwoPropertyNode
 (property a-property g/Str (default "default value"))
 (property another-property g/Int))

(g/defnode InheritedPropertyNode
  (inherits TwoPropertyNode)
  (property another-property g/Int (default -1)))

(g/defnode VisibiltyFunctionPropertyNode
  (input foo g/Any)
  (property a-property g/Str (dynamic visible (g/fnk [foo] true))))

(g/defnode SetterFnPropertyNode
  (property underneath g/Int)
  (property self-incrementing g/Int
            (value (g/fnk [underneath] underneath))
            (set (fn [basis self old-value new-value]
                   (g/set-property self :underneath (or (and new-value (inc new-value)) 0))))))

(g/defnode GetterFnPropertyNode
  (property reports-higher g/Int
            (value (g/fnk [this]
                          (inc (or (get this :int-val) 0))))))

(g/defnode ComplexGetterFnPropertyNode
  (input a g/Any)
  (input b g/Any)
  (input c g/Any)

  (property weirdo g/Any
            (value (g/fnk [this a b c] [this a b c]))))

(deftest nodes-can-include-properties
  (testing "a single property"
    (let [node (g/construct SinglePropertyNode)]
      (is (:a-property (g/property-labels SinglePropertyNode)))
      (is (:a-property (-> node g/node-type g/property-labels)))
      (is (some #{:a-property} (keys node)))))

  (testing "_node-id is an internal property"
    (is (= [:_node-id :_output-jammers] (keys (g/internal-properties SinglePropertyNode))))
    (is (= [:a-property] (keys (g/public-properties SinglePropertyNode)))))

  (testing "two properties"
    (let [node (g/construct TwoPropertyNode)]
      (is (contains? (g/property-labels TwoPropertyNode) :a-property))
      (is (contains? (g/property-labels TwoPropertyNode) :another-property))
      (is (some #{:a-property}       (keys node)))
      (is (some #{:another-property} (keys node)))))

  (testing "properties can have defaults"
    (let [node (g/construct TwoPropertyNode)]
      (is (= "default value" (:a-property node)))))

  (testing "properties are inherited"
    (let [node (g/construct InheritedPropertyNode)]
      (is (contains? (g/property-labels InheritedPropertyNode) :a-property))
      (is (contains? (g/property-labels InheritedPropertyNode) :another-property))
      (is (some #{:a-property}       (keys node)))
      (is (some #{:another-property} (keys node)))))

  (testing "property value types are inherited"
    (are [t p vt] (= vt (in/property-type t p))
      SinglePropertyNode    :a-property       g/Str
      TwoPropertyNode       :a-property       g/Str
      TwoPropertyNode       :another-property g/Int
      InheritedPropertyNode :a-property       g/Str
      InheritedPropertyNode :another-property g/Int))

  (testing "property defaults can be inherited or overridden"
    (let [node (g/construct InheritedPropertyNode)]
      (is (= "default value" (:a-property node)))
      (is (= -1              (:another-property node)))))

  (testing "output dependencies include properties"
    (let [deps (g/input-dependencies InheritedPropertyNode)]
      (are [x y] (= y (get deps x))
        :_node-id             #{:_node-id}
        :_declared-properties #{:_properties}
        :another-property     #{:_declared-properties :_properties :another-property}
        :a-property           #{:_declared-properties :_properties :a-property}
        :_output-jammers      #{:_output-jammers})))

  (testing "property value functions' dependencies are reported"
    (are [x y] (= y (get (g/input-dependencies GetterFnPropertyNode) x))
      :_node-id             #{:_node-id}
      :_declared-properties #{:_properties}
      :_output-jammers      #{:_output-jammers}
      :reports-higher       #{:_declared-properties :_properties})

    (are [x y] (= y (get (g/input-dependencies ComplexGetterFnPropertyNode) x))
      :_node-id             #{:_node-id}
      :_declared-properties #{:_properties}
      :_output-jammers      #{:_output-jammers}
      :weirdo               #{:_declared-properties :_properties}
      :a                    #{:weirdo :_declared-properties :_properties}
      :b                    #{:weirdo :_declared-properties :_properties}
      :c                    #{:weirdo :_declared-properties :_properties}))

  (testing "setter functions are only invoked when the node gets added to the graph"
    (with-clean-system
      (let [standalone (g/construct SetterFnPropertyNode :self-incrementing 0)]
        (is (= 0 (:self-incrementing standalone)))

        (let [ingraph-id (first
                          (g/tx-nodes-added
                           (g/transact
                            (g/make-node world SetterFnPropertyNode :self-incrementing 0))))]
          (is (= 1 (g/node-value ingraph-id :self-incrementing)))

          (g/transact
           (g/set-property ingraph-id :self-incrementing 10))

          (is (= 11 (g/node-value ingraph-id :self-incrementing)))))))

  (testing "getter functions are invoked when supplying values"
    (with-clean-system
      (let [[getter-node] (g/tx-nodes-added
                           (g/transact
                            (g/make-node world GetterFnPropertyNode :reports-higher 0)))]
        (is (= 1 (g/node-value getter-node :reports-higher))))))

  (testing "do not allow a property to shadow an input of the same name"
    (is (thrown? AssertionError
                 (eval '(dynamo.graph/defnode ReflexiveFeedbackPropertySingularToSingular
                          (property port dynamo.graph/Keyword (default :x))
                          (input port dynamo.graph/Keyword :inject))))))

  (testing "visibility dependencies include properties"
    (let [node (g/construct VisibiltyFunctionPropertyNode)]
      (is (= {:a-property #{:_declared-properties :_properties :a-property}}
             (select-keys (-> node g/node-type g/input-dependencies) [:a-property])))))

  (testing "properties are named by symbols"
    (is (thrown? AssertionError
                 (eval '(dynamo.graph/defnode BadProperty
                          (property :not-a-symbol dynamo.graph/Keyword))))))

  (testing "properties must have values"
    (is (thrown? AssertionError
                 (eval '(dynamo.graph/defnode BadProperty
                          (property no-schema (fn [] "no value type provided"))))))))

(g/defnode ExternNode
  (property internal-resource g/Str (default "/bar"))
  (extern external-resource g/Str (default "/foo")))

(deftest node-externs
  (testing "Nodes can have externs"
    (let [node (g/construct ExternNode)]
      (is (= "/foo" (:external-resource node)))
      (is (contains? (g/property-labels ExternNode) :external-resource))
      (is (contains? (g/externs ExternNode) :external-resource))
      (is (some #{:external-resource} (keys node))))))

(g/defnk string-production-fnk [this integer-input] "produced string")
(g/defnk integer-production-fnk [this] 42)

(g/defnode MultipleOutputNode
  (input integer-input g/Int)
  (input string-input g/Str)

  (output string-output  g/Str         string-production-fnk)
  (output integer-output g/Int         integer-production-fnk)
  (output cached-output  g/Str :cached string-production-fnk)
  (output inline-string  g/Str         (g/fnk [string-input] "inline-string")))

(g/defnode AbstractOutputNode
  (output abstract-output g/Str :abstract))

(g/defnode InheritedOutputNode
  (inherits MultipleOutputNode)
  (inherits AbstractOutputNode)

  (output abstract-output g/Str string-production-fnk))

(g/defnode TwoLayerDependencyNode
  (property a-property g/Str)

  (output direct-calculation g/Str (g/fnk [a-property] a-property))
  (output indirect-calculation g/Str (g/fnk [direct-calculation] direct-calculation)))

(deftest nodes-can-have-outputs
  (testing "outputs must be defined with symbols"
    (is (thrown? AssertionError
                 (eval '(dynamo.graph/defnode BadOutput (output :not-a-symbol dynamo.graph/Str :abstract))))))

  (testing "outputs must be defined with a schema"
    (is (thrown? AssertionError
                 (eval '(dynamo.graph/defnode BadOutput (output a-output (fn [] "not a schema") :abstract))))))

  (testing "outputs must have flags after the schema and before the production function"
    (is (thrown? AssertionError
                 (eval '(dynamo.graph/defnode BadOutput (output a-output dynamo.graph/Str (dynamo.graph/fnk []) :cached))))))

  (testing "outputs must either have a production function defined or be marked as abstract"
    (is (thrown? AssertionError
                 (eval '(dynamo.graph/defnode BadOutput (output a-output dynamo.graph/Str))))))

  (testing "basic output definition"
    (let [node (g/construct MultipleOutputNode)]
      (are [o] (not (nil? (get (g/output-labels MultipleOutputNode) o)))
        :string-output
        :integer-output
        :cached-output
        :inline-string)
      (are [label expected-schema] (= expected-schema (get (g/transform-types MultipleOutputNode) label))
        :string-output  g/Str
        :integer-output g/Int
        :cached-output  g/Str
        :inline-string  g/Str)
      (is (:cached-output (g/cached-outputs MultipleOutputNode)))))

  (testing "output inheritance"
    (are [t o] (contains? (g/output-labels t) o)
      MultipleOutputNode  :string-output
      MultipleOutputNode  :integer-output
      MultipleOutputNode  :cached-output
      MultipleOutputNode  :inline-string
      AbstractOutputNode  :abstract-output
      InheritedOutputNode :string-output
      InheritedOutputNode :integer-output
      InheritedOutputNode :cached-output
      InheritedOutputNode :inline-string
      InheritedOutputNode :abstract-output)

    (are [t o vt] (= vt (get (g/transform-types t) o))
      MultipleOutputNode  :string-output   g/Str
      MultipleOutputNode  :integer-output  g/Int
      MultipleOutputNode  :cached-output   g/Str
      MultipleOutputNode  :inline-string   g/Str
      AbstractOutputNode  :abstract-output g/Str
      InheritedOutputNode :string-output   g/Str
      InheritedOutputNode :integer-output  g/Int
      InheritedOutputNode :cached-output   g/Str
      InheritedOutputNode :inline-string   g/Str
      InheritedOutputNode :abstract-output g/Str)

    (are [t co] (contains? (g/cached-outputs t) co)
      MultipleOutputNode  :cached-output
      InheritedOutputNode :cached-output))

  (testing "output dependencies include transforms and their inputs"
    (are [input outputs] (= outputs (get (g/input-dependencies MultipleOutputNode) input))
      :_node-id             #{:_node-id}
      :string-input         #{:inline-string}
      :integer-input        #{:string-output :cached-output}
      :_declared-properties #{:_properties}
      :_output-jammers      #{:_output-jammers})

    (are [input outputs] (= outputs (get (g/input-dependencies InheritedOutputNode) input))
      :_node-id             #{:_node-id}
      :string-input         #{:inline-string}
      :integer-input        #{:string-output :abstract-output :cached-output}
      :_declared-properties #{:_properties}
      :_output-jammers      #{:_output-jammers}))

  (testing "output dependencies are the transitive closure of their inputs"
    (are [input outputs] (= outputs (get (g/input-dependencies TwoLayerDependencyNode) input))
      :_node-id             #{:_node-id}
      :a-property           #{:direct-calculation :indirect-calculation :_declared-properties :_properties :a-property}
      :direct-calculation   #{:indirect-calculation}
      :_declared-properties #{:_properties}
      :_output-jammers      #{:_output-jammers}))

  (testing "outputs defined without the type cause a compile error"
    (is (not (nil? (eval '(dynamo.graph/defnode FooNode
                            (output my-output dynamo.graph/Any :abstract))))))
    (is (thrown? AssertionError
                 (eval '(dynamo.graph/defnode FooNode
                          (output my-output :abstract)))))
    (is (thrown? AssertionError
                 (eval '(dynamo.graph/defnode FooNode
                          (output my-output (dynamo.graph/fnk [] "constant string"))))))))

(g/defnode PropertyDynamicsNode
  (input an-input       g/Num)
  (input third-input    g/Num)
  (input fourth-input   g/Num)
  (input fifth-input    g/Num)

  (property internal-property g/Num
            (dynamic internal-dynamic (g/fnk [third-input] false))
            (validate (g/fnk [an-input] (when (nil? an-input) (g/error-info "Select a resource")))))
  (property internal-multiple g/Num
            (dynamic one-dynamic (g/fnk [fourth-input] false))
            (dynamic two-dynamic (g/fnk [fourth-input fifth-input] "dynamics can return strings")))

  (output   passthrough g/Num (g/fnk [fifth-input] fifth-input)))

(g/defnode InheritedPropertyDynamicsNode
  (inherits PropertyDynamicsNode)
  (property another-property g/Num)
  (output   output-from-inherited-property g/Num :cached (g/fnk [an-input third-input] an-input))
  (output   output-from-inherited-input    g/Num :cached (g/fnk [third-input] third-input))
  (output   output-from-inherited-output   g/Num :cached (g/fnk [passthrough] passthrough))
  (output   output-from-combined-labels    g/Num         (g/fnk [another-property an-input passthrough] passthrough)))

(deftest node-properties-depend-on-dynamic-inputs
  (let [dependencies              (g/input-dependencies PropertyDynamicsNode)
        affects-properties-output #(contains? (get dependencies %) :_properties)]


    (are [t x] (contains? (-> t g/input-dependencies x) :_properties)
      PropertyDynamicsNode          :an-input
      PropertyDynamicsNode          :third-input
      PropertyDynamicsNode          :fourth-input
      PropertyDynamicsNode          :fifth-input

      InheritedPropertyDynamicsNode :an-input
      InheritedPropertyDynamicsNode :third-input
      InheritedPropertyDynamicsNode :fourth-input
      InheritedPropertyDynamicsNode :fifth-input
      InheritedPropertyDynamicsNode :another-property)))

;;; example taken from editor/collection.clj
(g/deftype Vec3 [(g/one g/Num "x")
                 (g/one g/Num "y")
                 (g/one g/Num "z")])

(g/defnode SceneNode
  (property position Vec3)
  (property rotation Vec3))

(g/defnode ScalableSceneNode
  (inherits SceneNode)

  (property scale Vec3))

(g/defnode CollectionInstanceNode
  (inherits ScalableSceneNode)

  (property id g/Str)
  (property path g/Str))

(g/defnode SpecificDisplayOrder
  (inherits SceneNode)

  (property ambient Vec3)
  (property specular Vec3)

  (display-order [["Material" :specular :ambient]]))

(g/defnode DisplayGroupOrdering
  (inherits SpecificDisplayOrder)

  (display-order [:overlay ["Material"] :subtitle])

  (property overlay g/Str)
  (property subtitle g/Str)
  (property description g/Str))

(g/defnode PartialDisplayOrder
  (inherits SpecificDisplayOrder)

  (property overlay g/Str)
  (property subtitle g/Str)
  (display-order [:overlay ["Material"]]))

(g/defnode EmitterKeys
  (property color-alpha g/Int)
  (property color-blue g/Int)
  (property color-green g/Int)
  (property color-red g/Int)

  (display-order [:color-red :color-green :color-blue]))

(g/defnode GroupingBySymbol
  (inherits ScalableSceneNode)
  (inherits EmitterKeys)

  (display-order [["Transform" ScalableSceneNode] EmitterKeys]))

(deftest properties-have-a-display-order
  (testing "The default display order is declaration order"
    (are [type expected] (= expected (g/property-display-order type))
      SceneNode              [:position :rotation]
      ScalableSceneNode      [:scale :position :rotation]
      CollectionInstanceNode [:id :path :scale :position :rotation]
      SpecificDisplayOrder   [["Material" :specular :ambient] :position :rotation]
      DisplayGroupOrdering   [:overlay ["Material" :specular :ambient] :subtitle :description :position :rotation]
      PartialDisplayOrder    [:overlay ["Material" :specular :ambient] :subtitle :position :rotation]
      GroupingBySymbol       [["Transform" :scale :position :rotation] :color-red :color-green :color-blue :color-alpha])))

(defprotocol AProtocol
  (path      [this])
  (children  [this])
  (workspace [this]))

(g/deftype AProtocolType (s/protocol AProtocol))

(g/defnode ProtocolPropertyNode
  (property protocol-property AProtocolType)
  (property protocol-property-multi [AProtocolType]))

(g/defnode InheritsProtocolPropertyNode
  (inherits ProtocolPropertyNode))

(g/defnode ProtocolOutputNode
  (output protocol-output AProtocolType (g/fnk [] nil))
  (output protocol-output-multi [AProtocolType] (g/fnk [] nil)))

(g/defnode InheritsProtocolOutputNode
  (inherits ProtocolOutputNode))

(g/defnode ProtocolInputNode
  (input protocol-input AProtocolType)
  (input protocol-input-multi [AProtocolType] :array))

(g/defnode InheritsProtocolInputNode
  (inherits ProtocolInputNode))

(g/defnk fnk-without-arguments [])
(g/defnk fnk-with-arguments [a b c d])

(g/deftype KeywordNumMap {g/Keyword g/Num})

(g/defnode OutputFunctionArgumentsNode
  (property in1 g/Any)
  (property in2 g/Any)
  (property in3 g/Any)
  (property a   g/Any)
  (property b   g/Any)
  (property c   g/Any)
  (property d   g/Any)
  (property commands [g/Str])
  (property roles    g/Any)
  (property blah     KeywordNumMap)
  (output inline-arguments                   g/Any (g/fnk [in1 in2 in3]))
  (output arguments-from-nilary-fnk          g/Any fnk-without-arguments)
  (output arguments-from-fnk                 g/Any fnk-with-arguments)
  (output arguments-from-fnk-as-var          g/Any #'fnk-with-arguments)
  (output inline-arguments-with-input-schema g/Any (g/fnk [commands :- [g/Str] roles :- g/Any blah :- {g/Keyword g/Num}])))

(deftest output-function-arguments
  (testing "arguments with inline function definition"
    (are [o x] (= x (in/output-arguments OutputFunctionArgumentsNode o))
      :inline-arguments                   #{:in1 :in2 :in3}
      :arguments-from-nilary-fnk          #{}
      :arguments-from-fnk                 #{:a :b :c :d}
      :arguments-from-fnk-as-var          #{:a :b :c :d}
      :inline-arguments-with-input-schema #{:commands :roles :blah})))

(deftest inheritance-across-namespaces
  (let [original-ns (symbol (str *ns*))
        ns1 (gensym "ns")
        ns2 (gensym "ns")]
    (try
      (eval `(do
               (ns ~ns1)
               ;; define a node type in one namespace
               (dynamo.graph/defnode ~'ANode (~'property ~'anode-property dynamo.graph/Num))

               ;; inherit from it in another
               (ns ~ns2 (:require [~ns1 :as ~'n]))
               (dynamo.graph/defnode ~'BNode (~'inherits ~'n/ANode))))
      (finally
        (in-ns original-ns)))))

(g/defnode ValidationUsesInputs
  (input single-valued g/Any)
  (input multi-valued  [g/Any] :array)
  (input single-valued-with-sub g/Any :substitute false)
  (input multi-valued-with-sub  [g/Any] :array :substitute 0)

  (property valid-on-single-valued-input g/Any
            (validate (g/fnk [single-valued] true)))

  (property valid-on-multi-valued-input g/Any
            (validate (g/fnk [multi-valued] true)))

  (property valid-on-single-valued-with-sub g/Any
            (validate (g/fnk [single-valued-with-sub] true)))

  (input  overloading-single-valued g/Any)
  (output overloading-single-valued g/Any :cached (g/fnk [overloading-single-valued] overloading-single-valued))
  (property valid-on-overloading-single-valued g/Any
            (validate (g/fnk [single-valued overloading-single-valued] overloading-single-valued))))
