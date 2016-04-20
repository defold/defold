(ns internal.defnode-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [internal.graph.types :as gt]
            [internal.node :as in]
            [support.test-support :refer [tx-nodes with-clean-system]])
  (:import clojure.lang.Compiler$CompilerException))

(defn substitute-value-fn [& _] "substitute value")

(g/defnode BasicNode)

(deftest basic-node-definition
  (is (satisfies? g/NodeType BasicNode))
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
  (is (= [IRootNode]           (g/supertypes ChildNode)))
  (is (= ChildNode             (g/node-type (g/construct ChildNode))))
  (is (= [ChildNode]           (g/supertypes GChild)))
  (is (= GChild                (g/node-type (g/construct GChild))))
  (is (= [ChildNode MixinNode] (g/supertypes GGChild)))
  (is (= GGChild               (g/node-type (g/construct GGChild))))
  (is (thrown? AssertionError (eval '(dynamo.graph/defnode BadInheritance (inherits :not-a-symbol)))))
  (is (thrown? AssertionError (eval '(dynamo.graph/defnode BadInheritance (inherits DoesntExist))))))

(g/defnode OneInputNode
  (input an-input g/Str))

(g/defnode InheritedInputNode
  (inherits OneInputNode))

(g/defnode InjectableInputNode
  (input for-injection g/Int :inject))

(g/defnode SubstitutingInputNode
  (input substitute-fn  String :substitute substitute-value-fn)
  (input var-to-fn      String :substitute #'substitute-value-fn)
  (input inline-literal String :substitute "inline literal"))

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
      (is (= g/Str (-> OneInputNode g/declared-inputs (get :an-input))))))
  (testing "inherited input"
    (let [node (g/construct InheritedInputNode)]
      (is (:an-input (g/declared-inputs InheritedInputNode)))
      (is (= g/Str (g/input-type InheritedInputNode :an-input)))
      (is (= g/Str (-> InheritedInputNode g/declared-inputs (get :an-input))))))
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
    (let [deps (g/input-dependencies GetterFnPropertyNode)]
      (are [x y] (= y (get deps x))
        :_node-id             #{:_node-id}
        :_declared-properties #{:_properties}
        :_output-jammers      #{:_output-jammers}
        :reports-higher       #{:_declared-properties :_properties :reports-higher}))

    (let [deps (g/input-dependencies ComplexGetterFnPropertyNode)]
      (are [x y] (= y (get deps x))
        :_node-id             #{:_node-id}
        :_declared-properties #{:_properties}
        :_output-jammers      #{:_output-jammers}
        :weirdo               #{:weirdo :_declared-properties :_properties}
        :a                    #{:weirdo :_declared-properties :_properties}
        :b                    #{:weirdo :_declared-properties :_properties}
        :c                    #{:weirdo :_declared-properties :_properties})))

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

  (testing "property types have schemas"
    (is (thrown? AssertionError
                 (eval '(dynamo.graph/defnode BadProperty
                          (property a-symbol :not-a-type)))))))

(g/defnode ExternNode
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

  (output string-output         g/Str                                                 string-production-fnk)
  (output integer-output        g/Int                                                 integer-production-fnk)
  (output cached-output         g/Str           :cached                               string-production-fnk)
  (output inline-string         g/Str                                                 (g/fnk [string-input] "inline-string")))

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
      (doseq [expected-output [:string-output :integer-output :cached-output :inline-string]]
        (is (get (g/output-labels MultipleOutputNode) expected-output)))
      (doseq [[label expected-schema] {:string-output g/Str
                                       :integer-output g/Int
                                       :cached-output g/Str
                                       :inline-string g/Str}]
        (is (= expected-schema (-> MultipleOutputNode g/transform-types (get label)))))
      (is (:cached-output (g/cached-outputs MultipleOutputNode)))))

  (testing "output inheritance"
    (let [node (g/construct InheritedOutputNode)]
      (doseq [expected-output [:string-output :integer-output :cached-output :inline-string :abstract-output]]
        (is (get (g/output-labels InheritedOutputNode) expected-output)))
      (doseq [[label expected-schema] {:string-output g/Str
                                       :integer-output g/Int
                                       :cached-output g/Str
                                       :inline-string g/Str
                                       :abstract-output g/Str}]
        (is (= expected-schema (-> InheritedOutputNode g/transform-types (get label)))))
      (is (:cached-output (g/cached-outputs InheritedOutputNode)))))

  (testing "output dependencies include transforms and their inputs"
    (is (= {:_node-id             #{:_node-id}
            :string-input         #{:inline-string}
            :integer-input        #{:string-output :cached-output}
            :_declared-properties #{:_properties}
            :_output-jammers      #{:_output-jammers}}
           (g/input-dependencies MultipleOutputNode)))
    (is (= {:_node-id             #{:_node-id}
            :string-input         #{:inline-string}
            :integer-input        #{:string-output :abstract-output :cached-output}
            :_declared-properties #{:_properties}
            :_output-jammers      #{:_output-jammers}}
           (g/input-dependencies InheritedOutputNode))))

  (testing "output dependencies are the transitive closure of their inputs"
    (is (= {:_node-id             #{:_node-id}
            :a-property           #{:direct-calculation :indirect-calculation :_declared-properties :_properties :a-property}
            :direct-calculation   #{:indirect-calculation}
            :_declared-properties #{:_properties}
            :_output-jammers      #{:_output-jammers}}
           (g/input-dependencies TwoLayerDependencyNode))))

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
            (dynamic two-dynamic (g/fnk [fourth-input fifth-input] "dynamics can return strings"))))

(deftest node-properties-depend-on-dynamic-inputs
  (let [dependencies              (g/input-dependencies PropertyDynamicsNode)
        affects-properties-output #(contains? (get dependencies %) :_properties)
        all-inputs                []]
    (are [x] (affects-properties-output x)
      :an-input :third-input :fourth-input :fifth-input)))

(deftest compile-error-using-property-with-missing-argument-for-dynamic
  (is (thrown? AssertionError
               (eval '(dynamo.graph/defnode BadDynamicArgument
                        (property foo
                                  (dynamic a-dynamic (g/fnk [input-node-doesnt-have] false))))))))

;;; example taken from editor/collection.clj
(def Vec3    [(g/one g/Num "x")
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

  (property overlay String)
  (property subtitle String)
  (property description String))

(g/defnode PartialDisplayOrder
  (inherits SpecificDisplayOrder)

  (property overlay String)
  (property subtitle String)
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
    (are [expected type] (= expected (g/property-display-order type))
      [:position :rotation]                                                    SceneNode
      [:scale :position :rotation]                                             ScalableSceneNode
      [:id :path :scale :position :rotation]                                   CollectionInstanceNode
      [["Material" :specular :ambient] :position :rotation]                    SpecificDisplayOrder
      [:overlay ["Material" :specular :ambient] :subtitle :description :position :rotation] DisplayGroupOrdering
      [:overlay ["Material" :specular :ambient] :subtitle :position :rotation] PartialDisplayOrder
      [["Transform" :scale :position :rotation] :color-red :color-green :color-blue :color-alpha] GroupingBySymbol)))

(run-tests)
