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

(def int-ref (in/->ValueTypeRef :internal.defnode-test/Int))

(deftest parsing-type-forms
  (testing "aliases for the same type ref"
    (are [x] (= {:value-type int-ref :flags #{}}
                (in/parse-type-form "test" x))
      `Int
      :internal.defnode-test/Int
      'internal.defnode-test/Int))

  (testing "multivalued cases"
    (are [x] (= {:value-type int-ref :flags #{:collection}}
                (in/parse-type-form "test" x))
      `[Int]
      [:internal.defnode-test/Int]
      '[internal.defnode-test/Int]))

  (testing "unhappy path"
    (are [x] (thrown? AssertionError (in/parse-type-form "sadpath test" x))
      'NoSuchSymbol
      `NoSuchSymbol)))

(defprotocol Prot
  (a-method [this]))
(defrecord   Rec [])

(g/deftype W s/Any)
(g/deftype X java.lang.String)
(g/deftype Y Prot)
(g/deftype Z Rec)

(g/defnode DispatchValuesNode
  (property any-deftyped      W)
  (property string-deftyped   X)
  (property string-direct     String)
  (property protocol-deftyped Y)
  (property protocol-direct   Prot)
  (property record-deftyped   Z)
  (property record-direct     Rec))

(deftest dispatch-values-depend-on-usage
  (are [expected tp] (= expected (g/value-type-dispatch-value tp))
    W W
    X X
    Y Y
    Z Z)
  (are [expected prop] (= expected (-> @DispatchValuesNode :property prop :value-type g/value-type-dispatch-value))
    W      :any-deftyped
    X      :string-deftyped
    String :string-direct
    Y      :protocol-deftyped
    Prot   :protocol-direct
    Z      :record-deftyped
    Rec    :record-direct))

(g/defnode JavaClassesTypeNode
  (input an-input java.util.concurrent.BlockingDeque)
  (property a-property java.util.Vector)
  (output an-output java.util.concurrent.BlockingDeque (g/fnk [an-input] an-input)))

(defmulti class-based-dispatch (fn [value-type] (in/value-type-dispatch-value value-type)))

(defmethod class-based-dispatch java.util.Collection [_] :collection)
(defmethod class-based-dispatch java.util.concurrent.BlockingDeque [_] :blocking-deque)
(defmethod class-based-dispatch java.util.AbstractList [_] :abstract-list)
(defmethod class-based-dispatch :default [_] :fail)

(deftest java-classes-are-types
  (testing "classes can appear as type forms"
    (is (= (in/->ValueTypeRef :java.lang.String)
           (:value-type (in/parse-type-form (str 'java-classes-are-types) 'java.lang.String)))))

  (testing "classes are registered after parsing"
    (in/unregister-value-type :java.util.LinkedList)
    (eval '(dynamo.graph/defnode JavaClassNode (input a java.util.LinkedList)))
    (is (in/value-type-resolve :java.util.LinkedList))
    (is (= java.util.LinkedList (in/schema (in/value-type-resolve :java.util.LinkedList)))))

  (testing "classes can be used as inputs, outputs, and properties"
    (are [expected kind label] (= expected (-> @JavaClassesTypeNode kind label :value-type in/value-type-schema))
      java.util.concurrent.BlockingDeque :input    :an-input
      java.util.concurrent.BlockingDeque :output   :an-output
      java.util.Vector                   :property :a-property))

  (testing "classes can be used as multimethod dispatch values"
    (are [expected kind label] (= expected (class-based-dispatch (-> @JavaClassesTypeNode kind label :value-type)))
      :blocking-deque :input    :an-input
      :blocking-deque :output   :an-output
      :abstract-list  :property :a-property)))

(defprotocol CustomProtocol)
(defrecord CustomProtocolImplementerA []
  CustomProtocol)
(defrecord CustomProtocolImplementerB []
  CustomProtocol)
(defrecord CustomProtocolImplementerC []
  CustomProtocol)

(g/defnode ProtocolTypeNode
  (input an-input CustomProtocolImplementerA)
  (property a-property CustomProtocol)
  (output an-output CustomProtocol (g/fnk [an-input] an-input)))

(defmulti protocol-based-dispatch (fn [value-type] (in/value-type-dispatch-value value-type)))

(defmethod protocol-based-dispatch CustomProtocol [_] :protocol)
(defmethod protocol-based-dispatch CustomProtocolImplementerA [_] :a)
(defmethod protocol-based-dispatch CustomProtocolImplementerB [_] :b)
(defmethod protocol-based-dispatch :default [_] :fail)

(defprotocol AutoRegisteredProtocol)

(deftest protocols-are-parsed-as-types
  (testing "protocols can appear as type forms"
    (is (= (in/->ValueTypeRef :internal.defnode-test/AutoRegisteredProtocol)
           (:value-type (in/parse-type-form (str 'protocols-are-types) 'internal.defnode-test/AutoRegisteredProtocol)))))

  (testing "protocols are registered after defnode is parsed"
    (in/unregister-value-type :internal.defnode-test/AutoRegisteredProtocol)
    (eval '(dynamo.graph/defnode AutoRegisteredInputNode (input a internal.defnode-test/AutoRegisteredProtocol)))
    (is (in/value-type-resolve :internal.defnode-test/AutoRegisteredProtocol))))

(deftest protocols-are-types
  (testing "protocols can be used as inputs, outputs, and properties"
    (are [expected kind label] (= expected (-> @ProtocolTypeNode kind label :value-type in/value-type-schema :p))
      CustomProtocol :output   :an-output
      CustomProtocol :property :a-property)

    (are [expected kind label] (= expected (-> @ProtocolTypeNode kind label :value-type in/value-type-schema))
      CustomProtocolImplementerA :input    :an-input))

  (testing "protocols can be used as multimethod dispatch values"
    (are [expected kind label] (= expected (protocol-based-dispatch (-> @ProtocolTypeNode kind label :value-type)))
      :a        :input    :an-input
      :protocol :output   :an-output
      :protocol :property :a-property)))

(defrecord CustomRecord [])

(deftest records-are-types
  (testing "records can appear as type forms"
    (is (= (in/->ValueTypeRef :internal.defnode_test.CustomRecord)
           (:value-type (in/parse-type-form (str 'records-are-types) 'internal.defnode_test.CustomRecord))))))

(g/deftype Str java.lang.String)

(defmulti type-based-dispatch (fn [x] (in/value-type-dispatch-value x)))
(defmethod type-based-dispatch Str [_] :str)
(defmethod type-based-dispatch java.lang.String [_] :java.lang.String)
(defmethod type-based-dispatch :default [_] :fail)

(deftest deftypes-with-classes-dispatch-as-type-not-class
  (is (= :str (type-based-dispatch Str))))

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
  (is (= [IRootNode]                       (g/supertypes ChildNode)))
  (is (= ChildNode                         (g/node-type (g/construct ChildNode))))
  (is (= [ChildNode IRootNode]             (g/supertypes GChild)))
  (is (= GChild                            (g/node-type (g/construct GChild))))
  (is (= [ChildNode MixinNode IRootNode]   (g/supertypes GGChild)))
  (is (= GGChild                           (g/node-type (g/construct GGChild))))
  (is (thrown? AssertionError              (eval '(dynamo.graph/defnode BadInheritance (inherits :not-a-symbol)))))
  (is (thrown? AssertionError              (eval '(dynamo.graph/defnode BadInheritance (inherits DoesntExist))))))

(g/defnode OneInputNode
  (input an-input g/Str))

(g/defnode InheritedInputNode
  (inherits OneInputNode))

(g/defnode SubstitutingInputNode
  (input substitute-fn  g/Str :substitute substitute-value-fn)
  (input var-to-fn      g/Str :substitute #'substitute-value-fn)
  (input inline-literal g/Str :substitute "inline literal"))

(g/defnode CardinalityInputNode
  (input single-scalar-value        String)
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
            (set (fn [_evaluation-context self old-value new-value]
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

(g/defnode ReflexivePropertyValueFnNode
  (property zort g/Int
            (value (g/fnk [zort] zort))))

(deftest nodes-can-include-properties
  (testing "a single property"
    (let [node (g/construct SinglePropertyNode)]
      (is (:a-property (g/declared-property-labels SinglePropertyNode)))
      (is (:a-property (-> node g/node-type g/declared-property-labels)))
      (is (some #{:a-property} (keys node)))))

  (testing "_node-id is an internal property"
    (is (= #{:_node-id :_output-jammers} (g/internal-property-labels SinglePropertyNode)))
    (is (= #{:a-property} (g/declared-property-labels SinglePropertyNode))))

  (testing "two properties"
    (let [node (g/construct TwoPropertyNode)]
      (is (contains? (g/declared-property-labels TwoPropertyNode) :a-property))
      (is (contains? (g/declared-property-labels TwoPropertyNode) :another-property))
      (is (some #{:a-property}       (keys node)))
      (is (some #{:another-property} (keys node)))))

  (testing "properties can have defaults"
    (let [node (g/construct TwoPropertyNode)]
      (is (= "default value" (:a-property node)))))

  (testing "properties are inherited"
    (let [node (g/construct InheritedPropertyNode)]
      (is (contains? (g/declared-property-labels InheritedPropertyNode) :a-property))
      (is (contains? (g/declared-property-labels InheritedPropertyNode) :another-property))
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

  (testing "empty property set can still be fetched"
    (with-clean-system
      (let [basic-id (first (g/tx-nodes-added
                              (g/transact
                                (g/make-node world BasicNode))))]
        (is (= {:properties {} :display-order [] :node-id 0} (g/node-value basic-id :_properties))))))

  (testing "getter functions are invoked when supplying values"
    (with-clean-system
      (let [[getter-node] (g/tx-nodes-added
                           (g/transact
                            (g/make-node world GetterFnPropertyNode :reports-higher 0)))]
        (is (= 1 (g/node-value getter-node :reports-higher))))))

  (testing "getter functions are used when supplying :_declared-properties"
    (with-clean-system
      (let [[getter-node] (g/tx-nodes-added
                           (g/transact
                            (g/make-node world GetterFnPropertyNode :reports-higher 0)))]
        (is (= 1 (->  (g/node-value getter-node :_declared-properties) :properties :reports-higher :value))))))

  (testing "do not allow a property to shadow an input of the same name"
    (is (thrown? AssertionError
                 (eval '(dynamo.graph/defnode ReflexiveFeedbackPropertySingularToSingular
                          (property port dynamo.graph/Keyword (default :x))
                          (input port dynamo.graph/Keyword))))))

  (testing "visibility dependencies include properties"
    (let [node (g/construct VisibiltyFunctionPropertyNode)]
      (is (= {:a-property #{:_declared-properties :_properties :a-property}}
             (select-keys (-> node g/node-type g/input-dependencies) [:a-property])))))

  (testing "properties are named by symbols"
    (is (thrown? AssertionError
                 (eval '(dynamo.graph/defnode BadProperty
                          (property :not-a-symbol dynamo.graph/Keyword))))))

  (testing "property value types must exist when referenced"
    (is (thrown? AssertionError
                 (eval '(dynamo.graph/defnode BadPropertyType
                          (property a-property (in/->ValueTypeRef :foo.bar/baz)))))))

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
      (is (contains? (g/declared-property-labels ExternNode) :external-resource))
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
      (are [label expected-schema] (= expected-schema (g/output-type MultipleOutputNode label))
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

    (are [t o vt] (= vt (g/output-type t o))
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
            (dynamic error (g/fnk [an-input] (when (nil? an-input) (g/error-info "Select a resource")))))
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
(g/deftype Vec3 [(s/one g/Num "x")
                 (s/one g/Num "y")
                 (s/one g/Num "z")])

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

(g/defnode ProtocolPropertyNode
  (property protocol-property AProtocol)
  (property protocol-property-multi [AProtocol]))

(g/defnode InheritsProtocolPropertyNode
  (inherits ProtocolPropertyNode))

(g/defnode ProtocolOutputNode
  (output protocol-output AProtocol (g/fnk [] nil))
  (output protocol-output-multi [AProtocol] (g/fnk [] nil)))

(g/defnode InheritsProtocolOutputNode
  (inherits ProtocolOutputNode))

(g/defnode ProtocolInputNode
  (input protocol-input AProtocol)
  (input protocol-input-multi [AProtocol] :array))

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
  (output arguments-from-fnk-as-var          g/Any #'fnk-with-arguments))

(deftest output-function-arguments
  (testing "arguments with inline function definition"
    (are [o x] (= x (in/output-arguments OutputFunctionArgumentsNode o))
      :inline-arguments                   #{:in1 :in2 :in3}
      :arguments-from-nilary-fnk          #{}
      :arguments-from-fnk                 #{:a :b :c :d}
      :arguments-from-fnk-as-var          #{:a :b :c :d})))

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

  (property valid-on-single-valued-input g/Any)

  (property valid-on-multi-valued-input g/Any)

  (property valid-on-single-valued-with-sub g/Any)

  (input  overloading-single-valued g/Any)
  (output overloading-single-valued g/Any :cached (g/fnk [overloading-single-valued] overloading-single-valued))
  (property valid-on-overloading-single-valued g/Any))

(g/defnode PropertyWithSetter
  (property string-property g/Str
            (set (fn [evaluation-context self old-value new-value]))))

(g/defnode InheritingSetter
  (inherits PropertyWithSetter))

(deftest inherited-property-setters-work
  (with-clean-system
    (let [[nid] (tx-nodes (g/make-node world InheritingSetter))]
      (g/set-property! nid :string-property "bang!"))))

(defmacro fnk-generator
  ([x]
   `(g/fnk [] ~x))
  ([x y]
   `(g/fnk [~x] ~y))
  ([x y z]
   `(g/fnk [~x ~y] ~z)))

(g/defnode MacroMembers
  (input input-one g/Any)
  (input input-two g/Any)
  (input input-three g/Any)

  (property a-property g/Any
            (default (fnk-generator false))
            (value   (fnk-generator input-two (inc input-two))))

  (output an-output g/Any
          (fnk-generator input-one a-property :ok)))

(deftest fnks-from-macros
  (testing "as output"
    (are [x] (contains? (get (g/input-dependencies MacroMembers) x) :an-output)
      :input-one
      :input-two
      :a-property)))

(g/defnode PropertiesWithDynamics
  (input input-one g/Any)
  (input input-two g/Any)
  (input input-three g/Any)

  (property a-property g/Any
            (default  (g/constantly false))
            (dynamic error (g/fnk [input-three] (nil? input-three)))
            (value    (g/fnk [input-two] (inc input-two))))

  (output an-output g/Any
          (g/fnk [input-one a-property] :ok)))

(defn- affected-by? [out in]
  (let [affected-outputs (-> PropertiesWithDynamics g/input-dependencies (get in))]
    (contains? affected-outputs out)))

(deftest properties-depend-on-their-dynamic-inputs
  (testing "as output"
    (is (affected-by? :an-output :input-three)))

  (testing "as property"
    (are [input] (affected-by? :a-property input)
      :input-two
      :input-three)

    (are [input] (not (affected-by? :a-property input))
      :input-one
      :_properties
      :_declared-properties)))

(g/defnode CustomPropertiesOutput
  (output _properties g/Properties :cached
          (g/fnk [_declared-properties]
                 (assoc-in _declared-properties [:properties :from-custom-output] {:node-id 0
                                                                                   :type    g/Bool
                                                                                   :value true}))))

(g/defnode InheritFromCustomProperties
  (inherits CustomPropertiesOutput))

(g/defnode InheritAndOverrideProperties
  (output _properties g/Properties
          (g/fnk [_declared-properties] _declared-properties)))

(deftest inheriting-properties-output
  (testing "custom _properties function is invoked"
    (with-clean-system
      (let [[n] (tx-nodes (g/make-node world CustomPropertiesOutput))]
        (is (some-> n (g/node-value :_properties) :properties :from-custom-output :value)))))

  (testing "inherited function is invoked"
    (with-clean-system
      (let [[n] (tx-nodes (g/make-node world InheritFromCustomProperties))]
        (is (some-> n (g/node-value :_properties) :properties :from-custom-output :value)))))

  (testing "cached flag is not inherited"
    (is (contains? (-> @CustomPropertiesOutput :output :_properties :flags) :cached))
    (is (not (contains? (-> @InheritAndOverrideProperties :output :_properties :flags) :cached)))))

(deftest declared-properties-is-cached
  (is (contains? (in/cached-outputs CustomPropertiesOutput) :_declared-properties)))

(defn- override [node-id]
  (-> (g/override node-id {})
    :tx-data
    tx-nodes))

(deftest overridden-properties
  (testing "is empty for an original node"
    (with-clean-system
      (let [[n] (tx-nodes (g/make-node world BasicNode))]
        (is (empty? (g/node-value n :_overridden-properties))))))

  (testing "is empty for an override node with no properties set"
    (with-clean-system
      (let [[n] (tx-nodes (g/make-node world BasicNode))
            [onode] (override n)]
        (is (empty? (g/node-value onode :_overridden-properties))))))

  (testing "contains only properties with an override value"
    (with-clean-system
      (let [[n]     (tx-nodes (g/make-node world BasicNode))
            [onode] (override n)
            _       (g/set-property! onode :dynamic-property 99)
            v1      (g/node-value onode :_overridden-properties)
            _       (g/set-property! onode :background-color "cornflower blue")
            v2      (g/node-value onode :_overridden-properties)
            _       (g/clear-property! onode :dynamic-property)
            v3      (g/node-value onode :_overridden-properties)
            _       (g/clear-property! onode :background-color)
            v4      (g/node-value onode :_overridden-properties)]
        (is (= v1 {:dynamic-property 99} ))
        (is (= v2 {:dynamic-property 99 :background-color "cornflower blue"} ))
        (is (= v3 {:background-color "cornflower blue"}))
        (is (= v4 {}))))))

(g/defnk produce-all [test :as all]
  all)

(g/defnk produce-all-intrinsics [test _node-id _basis :as all]
  all)

(g/defnode AsAllNode
  (property test g/Str (default "test"))
  (output inline g/Any (g/fnk [test :as all] all))
  (output inline-intrinsics g/Any (g/fnk [test _node-id _basis :as all] all))
  (output defnk g/Any produce-all)
  (output defnk-intrinsics g/Any produce-all-intrinsics))

(deftest as-all
  (with-clean-system
    (let [[n] (tx-nodes (g/make-node world AsAllNode))]
      (is (= {:test "test"} (g/node-value n :inline)))
      (is (= #{:test :_node-id :_basis} (set (keys (g/node-value n :inline-intrinsics)))))
      (is (= {:test "test"} (g/node-value n :defnk)))
      (is (= #{:test :_node-id :_basis} (set (keys (g/node-value n :defnk-intrinsics))))))))
