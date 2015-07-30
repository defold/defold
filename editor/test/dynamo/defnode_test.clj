(ns dynamo.defnode-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [support.test-support :refer [tx-nodes with-clean-system]]
            [internal.node :as in])
  (:import clojure.lang.Compiler$CompilerException))

(defn substitute-value-fn [& _] "substitute value")

(deftest nodetype
  (testing "is created from a data structure"
    (is (satisfies? g/NodeType (in/make-node-type {:inputs {:an-input g/Str}}))))
  (testing "supports direct inheritance"
    (let [super-type (in/make-node-type {:inputs {:an-input g/Str}})
          node-type  (in/make-node-type {:supertypes [super-type]})]
      (is (= [super-type] (g/supertypes node-type)))
      (is (= {:an-input g/Str} (g/declared-inputs node-type)))))
  (testing "supports multiple inheritance"
    (let [super-type (in/make-node-type {:inputs {:an-input g/Str}})
          mixin-type (in/make-node-type {:inputs {:mixin-input g/Str}})
          node-type  (in/make-node-type {:supertypes [super-type mixin-type]})]
      (is (= [super-type mixin-type] (g/supertypes node-type)))
      (is (= {:an-input g/Str :mixin-input g/Str} (g/declared-inputs node-type)))))
  (testing "supports inheritance hierarchy"
    (let [grandparent-type (in/make-node-type {:inputs {:grandparent-input g/Str}})
          parent-type      (in/make-node-type {:supertypes [grandparent-type] :inputs {:parent-input g/Str}})
          node-type        (in/make-node-type {:supertypes [parent-type]})]
      (is (= {:parent-input g/Str :grandparent-input g/Str} (g/declared-inputs node-type))))))

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
  (is (thrown? Compiler$CompilerException (eval '(dynamo.graph/defnode BadInheritance (inherits :not-a-symbol)))))
  (is (thrown? Compiler$CompilerException (eval '(dynamo.graph/defnode BadInheritance (inherits DoesntExist))))))

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

(deftest nodes-can-have-inputs
  (testing "inputs must be defined with symbols"
    (is (thrown? Compiler$CompilerException
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
    (is (= :many (g/input-cardinality CardinalityInputNode :multiple-collection-values)))))


(definterface MarkerInterface)
(definterface SecondaryInterface)

(g/defnode MarkerInterfaceNode
  MarkerInterface)

(g/defnode MarkerAndSecondaryInterfaceNode
  MarkerInterface
  SecondaryInterface)

(g/defnode InheritedInterfaceNode
 (inherits MarkerInterfaceNode))

(definterface OneMethodInterface
  (oneMethod [^Long x]))

(defn- private-function [x] [x :ok])

(g/defnode OneMethodNode
  (input an-input g/Str)

  OneMethodInterface
  (oneMethod [this ^Long x] (private-function x)))

(g/defnode InheritedMethodNode
  (inherits OneMethodNode))

(g/defnode OverriddenMethodNode
  (inherits OneMethodNode)

  OneMethodInterface
  (oneMethod [this x] [x :overridden]))

(deftest nodes-can-implement-interfaces
  (testing "implement a single interface"
    (let [node (g/construct MarkerInterfaceNode)]
      (is (= #{`MarkerInterface} (g/interfaces MarkerInterfaceNode)))
      (is (instance? MarkerInterface node))
      (is (not (instance? SecondaryInterface node)))))
  (testing "implement two interfaces"
    (let [node (g/construct MarkerAndSecondaryInterfaceNode)]
      (is (= #{`MarkerInterface `SecondaryInterface} (g/interfaces MarkerAndSecondaryInterfaceNode)))
      (is (instance? MarkerInterface node))
      (is (instance? SecondaryInterface node))))
  (testing "implement interface with methods"
    (let [node (g/construct OneMethodNode)]
      (is (instance? OneMethodInterface node))
      (is (= [5 :ok] (.oneMethod node 5)))))
  (testing "interface inheritance"
    (let [node (g/construct InheritedInterfaceNode)]
      (is (instance? MarkerInterface node)))
    (let [node (g/construct InheritedMethodNode)]
      (is (instance? OneMethodInterface node))
      (is (= [5 :ok] (.oneMethod node 5))))
    (let [node (g/construct OverriddenMethodNode)]
      (is (instance? OneMethodInterface node))
      (is (= [42 :overridden] (.oneMethod node 42)))))
  (testing "preserves type hints"
    (let [[arglist _] (get (g/method-impls OneMethodNode) 'oneMethod)]
      (is (= ['this 'x] arglist))
      (is (= {:tag 'Long} (meta (second arglist)))))))

(defprotocol LocalProtocol
  (protocol-method [this x y]))

(g/defnode LocalProtocolNode
  LocalProtocol
  (protocol-method [this x y] [:ok x y]))

(g/defnode InheritedLocalProtocol
  (inherits LocalProtocolNode))

(g/defnode InheritedProtocolOverride
  (inherits LocalProtocolNode)
  (protocol-method [this x y] [:override-ok x y]))

(deftest nodes-can-support-protocols
  (testing "support a single protocol"
    (let [node (g/construct LocalProtocolNode)]
      (is (= #{`LocalProtocol} (g/protocols LocalProtocolNode)))
      (is (satisfies? LocalProtocol node))
      (is (= [:ok 5 10] (protocol-method node 5 10))))
    (let [node (g/construct InheritedLocalProtocol)]
      (is (satisfies? LocalProtocol node))
      (is (= [:ok 5 10] (protocol-method node 5 10))))
    (let [node (g/construct InheritedProtocolOverride)]
      (is (satisfies? LocalProtocol node))
      (is (= [:override-ok 5 10] (protocol-method node 5 10))))))

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

(deftest nodes-can-include-properties
  (testing "a single property"
    (let [node (g/construct SinglePropertyNode)]
      (is (:a-property (g/property-labels SinglePropertyNode)))
      (is (:a-property (-> node g/node-type g/property-labels)))
      (is (some #{:a-property} (keys node)))))

  (testing "_id is an internal property"
    (is (= [:_id :_output-jammers] (keys (g/internal-properties SinglePropertyNode))))
    (is (= [:a-property] (keys (g/properties SinglePropertyNode)))))

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
    (let [node (g/construct InheritedPropertyNode)]
      (is (= {:_id              #{:_id}
              :another-property #{:_properties :another-property}
              :a-property       #{:_properties :a-property}
              :_output-jammers  #{:_output-jammers}}
             (-> node g/node-type g/input-dependencies)))))

  (testing "do not allow a property to shadow an input of the same name"
    (is (thrown? AssertionError
                 (eval '(dynamo.graph/defnode ReflexiveFeedbackPropertySingularToSingular
                          (property port dynamo.graph/Keyword (default :x))
                          (input port dynamo.graph/Keyword :inject))))))

  (testing "visibility dependencies include properties"
    (let [node (g/construct VisibiltyFunctionPropertyNode)]
      (is (= {:a-property #{:_properties :a-property}}
             (select-keys (-> node g/node-type g/input-dependencies) [:a-property])))))

  (testing "properties are named by symbols"
    (is (thrown? Compiler$CompilerException
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
(g/defnk integer-production-fnk [this project] 42)

(g/defproperty IntegerProperty g/Int (validate positive? (comp not neg?)))

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
    (is (thrown? Compiler$CompilerException
                 (eval '(dynamo.graph/defnode BadOutput (output :not-a-symbol dynamo.graph/Str :abstract))))))
  (testing "outputs must be defined with a schema"
    (is (thrown? AssertionError
                 (eval '(dynamo.graph/defnode BadOutput (output a-output (fn [] "not a schema") :abstract))))))
  (testing "outputs must have flags after the schema and before the production function"
    (is (thrown? AssertionError
                 (eval '(dynamo.graph/defnode BadOutput (output a-output dynamo.graph/Str (dynamo.graph/fnk []) :cached))))))
  (testing "outputs must either have a production function defined or be marked as abstract"
    (is (thrown? Compiler$CompilerException
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
    (is (= {:_id             #{:_id}
            :project         #{:integer-output}
            :string-input    #{:inline-string}
            :integer-input   #{:string-output :cached-output}
            :_output-jammers #{:_output-jammers}}
           (g/input-dependencies MultipleOutputNode)))
    (is (= {:_id             #{:_id}
            :project         #{:integer-output}
            :string-input    #{:inline-string}
            :integer-input   #{:string-output :abstract-output :cached-output}
            :_output-jammers #{:_output-jammers}}
           (g/input-dependencies InheritedOutputNode))))

  (testing "output dependencies are the transitive closure of their inputs"
    (is (= {:_id                #{:_id}
            :a-property         #{:direct-calculation :indirect-calculation :_properties :a-property}
            :direct-calculation #{:indirect-calculation}
            :_output-jammers    #{:_output-jammers}}
           (g/input-dependencies TwoLayerDependencyNode))))

  (testing "outputs defined without the type cause a compile error"
    (is (not (nil? (eval '(dynamo.graph/defnode FooNode
                            (output my-output dynamo.graph/Any :abstract))))))
    (is (thrown? Compiler$CompilerException
                 (eval '(dynamo.graph/defnode FooNode
                          (output my-output :abstract)))))
    (is (thrown? Compiler$CompilerException
                 (eval '(dynamo.graph/defnode FooNode
                          (output my-output (dynamo.graph/fnk [] "constant string")))))))

  (testing "outputs defined without the production function being a fnk or a val cause an Assertion error"
    (is (thrown? AssertionError
                 (eval '(let [foo-fn (fn [x] "foo")]
                          (dynamo.graph/defnode FooNode
                            (output my-output dynamo.graph/Any foo-fn))))))
    (is (thrown? AssertionError
                 (eval '(dynamo.graph/defnode FooNode
                          (output my-output dynamo.graph/Any (fn [x] "foo"))))))))

(defn- not-neg? [x] (not (neg? x)))

(g/defproperty TypedProperty g/Int)
(g/defproperty DerivedProperty TypedProperty)
(g/defproperty DefaultProperty DerivedProperty
  (default 0))
(g/defproperty ValidatedProperty DefaultProperty
  (validate must-be-positive not-neg?))

(g/defnode NodeWithPropertyVariations
  (property typed-external TypedProperty)
  (property derived-external DerivedProperty)
  (property default-external DefaultProperty)
  (property validated-external ValidatedProperty)

  (property typed-internal g/Int)
  (property derived-internal TypedProperty)
  (property default-internal TypedProperty
    (default 0))
  (property validated-internal DefaultProperty
            (validate always-valid (fn [value] true)))
  (property literally-disabled TypedProperty
            (dynamic enabled (g/always false))))

(g/defnode InheritsPropertyVariations
  (inherits NodeWithPropertyVariations))


;;; TODO - Make some tests for the Derived Properties etc...

(def original-node-definition
  '(dynamo.graph/defnode MutagenicNode
     (property a-property schema.core/Str  (default "a-string"))
     (property b-property schema.core/Bool (default true))))

(def replacement-node-definition
  '(dynamo.graph/defnode MutagenicNode
     (property a-property schema.core/Str  (default "Genosha"))
     (property b-property schema.core/Bool (default false))
     (property c-property schema.core/Int  (default 42))
     dynamo.defnode_test.MarkerInterface))

(deftest redefining-nodes-updates-existing-world-instances
  (with-clean-system
    (binding [*ns* (find-ns 'dynamo.defnode-test)]
      (eval original-node-definition))

    (let [node-type-var          (resolve 'dynamo.defnode-test/MutagenicNode)
          node-type              (var-get node-type-var)
          [original-node-id] (tx-nodes (g/make-node world node-type))
          node-before-mutation  (g/node-by-id original-node-id)]
      (binding [*ns* (find-ns 'dynamo.defnode-test)]
        (eval replacement-node-definition))

      (let [node-after-mutation (g/node-by-id original-node-id)]
        (is (not (instance? MarkerInterface node-before-mutation)))
        (is (= "a-string" (:a-property node-after-mutation)))
        (is (= true       (:b-property node-after-mutation)))
        (is (= 42         (:c-property node-after-mutation)))
        (is (instance? MarkerInterface node-after-mutation))))))

(g/defnode BaseTriggerNode
  (trigger added-trigger        :added             (fn [& _] nil))
  (trigger multiway-trigger     :added :deleted    (fn [& _] nil))
  (trigger on-delete            :deleted           (fn [& _] nil))
  (trigger on-property-touched  :property-touched  (fn [& _] nil))
  (trigger on-input-connections :input-connections (fn [& _] nil)))

(g/defnode InheritedTriggerNode
  (inherits BaseTriggerNode)

  (trigger extra-added      :added           (fn [& _] nil))
  (trigger on-delete        :deleted         (fn [& _] :override)))

(deftest nodes-can-have-triggers
  (testing "basic trigger definition"
    (is (fn? (get-in (g/triggers BaseTriggerNode) [:added :added-trigger])))
    (is (fn? (get-in (g/triggers BaseTriggerNode) [:added :multiway-trigger])))
    (is (fn? (get-in (g/triggers BaseTriggerNode) [:deleted :multiway-trigger])))
    (is (fn? (get-in (g/triggers BaseTriggerNode) [:deleted :on-delete])))
    (is (fn? (get-in (g/triggers BaseTriggerNode) [:property-touched :on-property-touched])))
    (is (fn? (get-in (g/triggers BaseTriggerNode) [:input-connections :on-input-connections]))))

  (testing "triggers are inherited"
    (is (fn? (get-in (g/triggers InheritedTriggerNode) [:added :added-trigger])))
    (is (fn? (get-in (g/triggers InheritedTriggerNode) [:added :multiway-trigger])))
    (is (fn? (get-in (g/triggers InheritedTriggerNode) [:added :extra-added])))
    (is (fn? (get-in (g/triggers InheritedTriggerNode) [:deleted :multiway-trigger]))))

  (testing "inherited triggers can be overridden"
    (is (fn? (get-in (g/triggers InheritedTriggerNode) [:deleted :on-delete])))
    (is (= :override ((get-in (g/triggers InheritedTriggerNode) [:deleted :on-delete])))))

  (testing "disallows unknown trigger kinds"
    (is (thrown-with-msg? clojure.lang.Compiler$CompilerException #"Valid trigger kinds are"
          (eval '(dynamo.graph/defnode NoSuchTriggerNode
                   (trigger nope :not-a-real-trigger-kind (fn [& _] :nope))))))
    (is (thrown-with-msg? clojure.lang.Compiler$CompilerException #"Valid trigger kinds are"
          (eval '(dynamo.graph/defnode NoSuchTriggerNode
                   (trigger nope :modified (fn [& _] :nope)))))))

  (testing "triggers are named by symbols"
    (is (thrown? Compiler$CompilerException
                 (eval '(dynamo.graph/defnode BadTriggerName
                          (trigger :not-a-symbol :added (fn [& _] :ok)))))))

  (testing "triggers have actions"
    (is (thrown? Compiler$CompilerException
                 (eval '(dynamo.graph/defnode MissingAction
                          (trigger a-symbol :added)))))))

(g/defproperty DynamicExternalProperty g/Num
  (dynamic external-dynamic (g/fnk [an-input another-input] true)))

(g/defnode PropertyDynamicsNode
  (input an-input       g/Num)
  (input another-input  g/Num)
  (input third-input    g/Num)
  (input fourth-input   g/Num)
  (input fifth-input    g/Num)

  (property external-property DynamicExternalProperty)
  (property internal-property g/Num
            (dynamic internal-dynamic (g/fnk [third-input] false)))
  (property internal-multiple g/Num
            (dynamic one-dynamic (g/fnk [fourth-input] false))
            (dynamic two-dynamic (g/fnk [fourth-input fifth-input] "dynamics can return strings"))))

(defn affects-properties
  [node-type input]
  (contains? (get (g/input-dependencies node-type) input) :_properties))

(deftest node-properties-depend-on-dynamic-inputs
  (let [all-inputs [:an-input :another-input :third-input :fourth-input :fifth-input]]
   (is (= true (every? #(affects-properties PropertyDynamicsNode %) all-inputs)))))

(g/defproperty NeedsADifferentInput g/Num
  (dynamic a-dynamic (g/fnk [input-node-doesnt-have] false)))

(deftest compile-error-using-property-with-missing-argument-for-dynamic
  (is (thrown? AssertionError
               (eval '(dynamo.graph/defnode BadDynamicArgument
                        (property foo dynamo.defnode-test/NeedsADifferentInput))))))

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
