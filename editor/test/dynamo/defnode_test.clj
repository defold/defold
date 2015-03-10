(ns dynamo.defnode-test
  (:require [clojure.test :refer :all]
            [schema.core :as s]
            [plumbing.core :refer [defnk fnk]]
            [dynamo.node :as n]
            [dynamo.property :as dp]
            [dynamo.system :as ds]
            [dynamo.system.test-support :as ts]
            [dynamo.types :as t]
            [internal.graph.dgraph :as dg]
            [internal.node :as in]))

(deftest nodetype
  (testing "is created from a data structure"
    (is (satisfies? t/NodeType (in/make-node-type {:inputs {:an-input s/Str}}))))
  (testing "supports direct inheritance"
    (let [super-type (in/make-node-type {:inputs {:an-input s/Str}})
          node-type  (in/make-node-type {:supertypes [super-type]})]
      (is (= [super-type] (t/supertypes node-type)))
      (is (= {:an-input s/Str} (t/inputs' node-type)))))
  (testing "supports multiple inheritance"
    (let [super-type (in/make-node-type {:inputs {:an-input s/Str}})
          mixin-type (in/make-node-type {:inputs {:mixin-input s/Str}})
          node-type  (in/make-node-type {:supertypes [super-type mixin-type]})]
      (is (= [super-type mixin-type] (t/supertypes node-type)))
      (is (= {:an-input s/Str :mixin-input s/Str} (t/inputs' node-type)))))
  (testing "supports inheritance hierarchy"
    (let [grandparent-type (in/make-node-type {:inputs {:grandparent-input s/Str}})
          parent-type      (in/make-node-type {:supertypes [grandparent-type] :inputs {:parent-input s/Str}})
          node-type        (in/make-node-type {:supertypes [parent-type]})]
      (is (= {:parent-input s/Str :grandparent-input s/Str} (t/inputs' node-type))))))

(n/defnode BasicNode)

(deftest basic-node-definition
  (is (satisfies? t/NodeType BasicNode))
  (is (= BasicNode (t/node-type (n/construct BasicNode)))))

(n/defnode IRootNode)
(n/defnode ChildNode
  (inherits IRootNode))
(n/defnode GChild
  (inherits ChildNode))
(n/defnode MixinNode)
(n/defnode GGChild
  (inherits ChildNode)
  (inherits MixinNode))

(deftest inheritance
 (is (= [IRootNode]           (t/supertypes ChildNode)))
 (is (= ChildNode             (t/node-type (n/construct ChildNode))))
 (is (= [ChildNode]           (t/supertypes GChild)))
 (is (= GChild                (t/node-type (n/construct GChild))))
 (is (= [ChildNode MixinNode] (t/supertypes GGChild)))
 (is (= GGChild               (t/node-type (n/construct GGChild)))))

(n/defnode OneInputNode
  (input an-input s/Str))

(n/defnode InheritedInputNode
  (inherits OneInputNode))

(n/defnode InjectableInputNode
  (input for-injection s/Int :inject))

(deftest nodes-can-have-inputs
  (testing "labeled input"
    (let [node (n/construct OneInputNode)]
      (is (:an-input (t/inputs' OneInputNode)))
      (is (= s/Str (:an-input (t/input-types node))))
      (is (:an-input (t/inputs node)))))
  (testing "inherited input"
    (let [node (n/construct InheritedInputNode)]
      (is (:an-input (t/inputs' InheritedInputNode)))
      (is (= s/Str (:an-input (t/input-types node))))
      (is (:an-input (t/inputs node)))))
  (testing "inputs can be flagged for injection"
    (let [node (n/construct InjectableInputNode)]
      (is (:for-injection (t/injectable-inputs' InjectableInputNode))))))

(definterface MarkerInterface)
(definterface SecondaryInterface)

(n/defnode MarkerInterfaceNode
  MarkerInterface)

(n/defnode MarkerAndSecondaryInterfaceNode
  MarkerInterface
  SecondaryInterface)

(n/defnode InheritedInterfaceNode
 (inherits MarkerInterfaceNode))

(definterface OneMethodInterface
  (oneMethod [^Long x]))

(defn- private-function [x] [x :ok])

(n/defnode OneMethodNode
  (input an-input s/Str)

  OneMethodInterface
  (oneMethod [this ^Long x] (private-function x)))

(n/defnode InheritedMethodNode
  (inherits OneMethodNode))

(n/defnode OverriddenMethodNode
  (inherits OneMethodNode)

  OneMethodInterface
  (oneMethod [this x] [x :overridden]))

(deftest nodes-can-implement-interfaces
  (testing "implement a single interface"
    (let [node (n/construct MarkerInterfaceNode)]
      (is (= #{`MarkerInterface} (t/interfaces MarkerInterfaceNode)))
      (is (instance? MarkerInterface node))
      (is (not (instance? SecondaryInterface node)))))
  (testing "implement two interfaces"
    (let [node (n/construct MarkerAndSecondaryInterfaceNode)]
      (is (= #{`MarkerInterface `SecondaryInterface} (t/interfaces MarkerAndSecondaryInterfaceNode)))
      (is (instance? MarkerInterface node))
      (is (instance? SecondaryInterface node))))
  (testing "implement interface with methods"
    (let [node (n/construct OneMethodNode)]
      (is (instance? OneMethodInterface node))
      (is (= [5 :ok] (.oneMethod node 5)))))
  (testing "interface inheritance"
    (let [node (n/construct InheritedInterfaceNode)]
      (is (instance? MarkerInterface node)))
    (let [node (n/construct InheritedMethodNode)]
      (is (instance? OneMethodInterface node))
      (is (= [5 :ok] (.oneMethod node 5))))
    (let [node (n/construct OverriddenMethodNode)]
      (is (instance? OneMethodInterface node))
      (is (= [42 :overridden] (.oneMethod node 42)))))
  (testing "preserves type hints"
    (let [[arglist _] (get (t/method-impls OneMethodNode) 'oneMethod)]
      (is (= ['this 'x] arglist))
      (is (= {:tag 'Long} (meta (second arglist)))))))

(defprotocol LocalProtocol
  (protocol-method [this x y]))

(n/defnode LocalProtocolNode
  LocalProtocol
  (protocol-method [this x y] [:ok x y]))

(n/defnode InheritedLocalProtocol
  (inherits LocalProtocolNode))

(n/defnode InheritedProtocolOverride
  (inherits LocalProtocolNode)
  (protocol-method [this x y] [:override-ok x y]))

(deftest nodes-can-support-protocols
  (testing "support a single protocol"
    (let [node (n/construct LocalProtocolNode)]
      (is (= #{`LocalProtocol} (t/protocols LocalProtocolNode)))
      (is (satisfies? LocalProtocol node))
      (is (= [:ok 5 10] (protocol-method node 5 10))))
    (let [node (n/construct InheritedLocalProtocol)]
      (is (satisfies? LocalProtocol node))
      (is (= [:ok 5 10] (protocol-method node 5 10))))
    (let [node (n/construct InheritedProtocolOverride)]
      (is (satisfies? LocalProtocol node))
      (is (= [:override-ok 5 10] (protocol-method node 5 10))))))

(n/defnode SinglePropertyNode
  (property a-property s/Str))

(n/defnode TwoPropertyNode
 (property a-property s/Str (default "default value"))
 (property another-property s/Int))

(n/defnode InheritedPropertyNode
  (inherits TwoPropertyNode)
  (property another-property s/Int (default -1)))

(deftest nodes-can-include-properties
  (testing "a single property"
    (let [node (n/construct SinglePropertyNode)]
      (is (:a-property (t/properties' SinglePropertyNode)))
      (is (:a-property (t/properties node)))
      (is (some #{:a-property} (keys node)))))
  (testing "two properties"
    (let [node (n/construct TwoPropertyNode)]
      (is (:a-property       (t/properties' TwoPropertyNode)))
      (is (:another-property (t/properties node)))
      (is (:a-property       (t/properties node)))
      (is (some #{:a-property}       (keys node)))
      (is (some #{:another-property} (keys node)))))
  (testing "properties can have defaults"
    (let [node (n/construct TwoPropertyNode)]
      (is (= "default value" (:a-property node)))))
  (testing "properties are inherited"
    (let [node (n/construct InheritedPropertyNode)]
      (is (:a-property       (t/properties' InheritedPropertyNode)))
      (is (:another-property (t/properties node)))
      (is (:a-property       (t/properties node)))
      (is (some #{:a-property}       (keys node)))
      (is (some #{:another-property} (keys node)))))
  (testing "property defaults can be inherited or overridden"
    (let [node (n/construct InheritedPropertyNode)]
      (is (= "default value" (:a-property node)))
      (is (= -1              (:another-property node)))))
  (testing "output dependencies include properties"
    (let [node (n/construct InheritedPropertyNode)]
      (is (= {:another-property #{:properties :another-property}
              :a-property #{:properties :a-property}}
            (t/output-dependencies node))))))

(defnk string-production-fnk [this integer-input] "produced string")
(defnk integer-production-fnk [this g project] 42)
(defn schemaless-production-fn [this g] "schemaless fn produced string")
(defn substitute-value-fn [& _] "substitute value")

(dp/defproperty IntegerProperty s/Int (validate positive? (comp not neg?)))

(n/defnode MultipleOutputNode
  (input integer-input s/Int)
  (input string-input s/Str)

  (output string-output         s/Str                                                 string-production-fnk)
  (output integer-output        IntegerProperty                                       integer-production-fnk)
  (output cached-output         s/Str           :cached                               string-production-fnk)
  (output inline-string         s/Str           :on-update                            (fnk [string-input] "inline-string"))
  (output schemaless-production s/Str                                                 schemaless-production-fn)
  (output with-substitute       s/Str           :substitute-value substitute-value-fn string-production-fnk))

(n/defnode AbstractOutputNode
  (output abstract-output s/Str :abstract))

(n/defnode InheritedOutputNode
  (inherits MultipleOutputNode)
  (inherits AbstractOutputNode)

  (output abstract-output s/Str string-production-fnk))

(n/defnode TwoLayerDependencyNode
  (property a-property s/Str)

  (output direct-calculation s/Str (fnk [a-property] a-property))
  (output indirect-calculation s/Str (fnk [direct-calculation] direct-calculation)))

(deftest nodes-can-have-outputs
  (testing "basic output definition"
    (let [node (n/construct MultipleOutputNode)]
      (doseq [expected-output [:string-output :integer-output :cached-output :inline-string :schemaless-production :with-substitute]]
        (is (get (t/outputs' MultipleOutputNode) expected-output))
        (is (get (t/outputs  node)               expected-output)))
      (doseq [[label expected-schema] {:string-output s/Str :integer-output IntegerProperty :cached-output s/Str :inline-string s/Str :schemaless-production s/Str :with-substitute s/Str}]
        (is (= expected-schema (get-in MultipleOutputNode [:transform-types label]))))
      (is (:cached-output (t/cached-outputs' MultipleOutputNode)))
      (is (:cached-output (t/cached-outputs node)))
      (is (:inline-string (t/auto-update-outputs node)))))
  (testing "output inheritance"
    (let [node (n/construct InheritedOutputNode)]
      (doseq [expected-output [:string-output :integer-output :cached-output :inline-string :schemaless-production :with-substitute :abstract-output]]
        (is (get (t/outputs' InheritedOutputNode) expected-output))
        (is (get (t/outputs  node)               expected-output)))
      (doseq [[label expected-schema] {:string-output s/Str :integer-output IntegerProperty :cached-output s/Str :inline-string s/Str :schemaless-production s/Str :with-substitute s/Str :abstract-output s/Str}]
        (is (= expected-schema (get-in InheritedOutputNode [:transform-types label]))))
      (is (:cached-output (t/cached-outputs' InheritedOutputNode)))
      (is (:cached-output (t/cached-outputs node)))))
  (testing "output dependencies include transforms and their inputs"
    (let [node (n/construct MultipleOutputNode)]
      (is (= {:project #{:integer-output}
              :string-input #{:inline-string}
              :integer-input #{:string-output :cached-output :with-substitute}}
            (t/output-dependencies node))))
    (let [node (n/construct InheritedOutputNode)]
      (is (= {:project #{:integer-output}
              :string-input #{:inline-string}
              :integer-input #{:string-output :abstract-output :cached-output :with-substitute}}
            (t/output-dependencies node)))))
  (testing "output dependencies are the transitive closure of their inputs"
    (let [node (n/construct TwoLayerDependencyNode)]
      (is (= {:a-property #{:direct-calculation :indirect-calculation :properties :a-property}
              :direct-calculation #{:indirect-calculation}}
            (t/output-dependencies node))))))

(n/defnode OneEventNode
  (on :an-event
    :ok))

(n/defnode EventlessNode)

(n/defnode MixinEventNode
  (on :mixin-event
    :mixin-ok))

(n/defnode InheritedEventNode
  (inherits OneEventNode)
  (inherits MixinEventNode)

  (on :another-event
    :another-ok))

(deftest nodes-can-handle-events
  (ts/with-clean-system
    (testing "nodes with event handlers implement MessageTarget"
      (let [node (n/construct OneEventNode)]
        (is (:an-event (t/event-handlers' OneEventNode)))
        (is (satisfies? t/MessageTarget node))
        (is (= :ok (n/dispatch-message node :an-event)))))
    (testing "nodes without event handlers do not implement MessageTarget"
      (let [node (n/construct EventlessNode)]
        (is (not (satisfies? t/MessageTarget node)))))
    (testing "nodes can inherit handlers from their supertypes"
      (let [node (n/construct InheritedEventNode)]
        (is ((every-pred :an-event :mixin-event :another-event) (t/event-handlers' InheritedEventNode)))
        (is (= :ok         (n/dispatch-message node :an-event)))
        (is (= :mixin-ok   (n/dispatch-message node :mixin-event)))
        (is (= :another-ok (n/dispatch-message node :another-event)))))))

(defn- not-neg? [x] (not (neg? x)))

(dp/defproperty TypedProperty s/Int)
(dp/defproperty DerivedProperty TypedProperty)
(dp/defproperty DefaultProperty DerivedProperty
  (default 0))
(dp/defproperty ValidatedProperty DefaultProperty
  (validate must-be-positive not-neg?))

(n/defnode NodeWithPropertyVariations
  (property typed-external TypedProperty)
  (property derived-external DerivedProperty)
  (property default-external DefaultProperty)
  (property validated-external ValidatedProperty)

  (property typed-internal s/Int)
  (property derived-internal TypedProperty)
  (property default-internal TypedProperty
    (default 0))
  (property validated-internal DefaultProperty
    (validate always-valid (fn [value] true))))

(n/defnode InheritsPropertyVariations
  (inherits NodeWithPropertyVariations))

(def original-node-definition
  '(dynamo.node/defnode MutagenicNode
     (property a-property schema.core/Str  (default "a-string"))
     (property b-property schema.core/Bool (default true))))

(def replacement-node-definition
  '(dynamo.node/defnode MutagenicNode
     (property a-property schema.core/Str  (default "Genosha"))
     (property b-property schema.core/Bool (default false))
     (property c-property schema.core/Int  (default 42))
     dynamo.defnode_test.MarkerInterface))

(deftest redefining-nodes-updates-existing-world-instances
  (ts/with-clean-system
    (binding [*ns* (find-ns 'dynamo.defnode-test)]
      (eval original-node-definition))

    (let [node-type-var        (resolve 'dynamo.defnode-test/MutagenicNode)
          node-type            (var-get node-type-var)
          node-before-mutation (ds/transactional (ds/add (n/construct node-type)))
          original-node-id     (:_id node-before-mutation)]
      (binding [*ns* (find-ns 'dynamo.defnode-test)]
        (eval replacement-node-definition))

      (let [node-after-mutation (ds/refresh node-before-mutation)]
        (is (not (instance? MarkerInterface node-before-mutation)))
        (is (= "a-string" (:a-property node-after-mutation)))
        (is (= true       (:b-property node-after-mutation)))
        (is (= 42         (:c-property node-after-mutation)))
        (is (instance? MarkerInterface node-after-mutation))))))

(n/defnode BaseTriggerNode
  (trigger added-trigger        :added             (fn [& _] :ok))
  (trigger multiway-trigger     :added :deleted    (fn [& _] :ok))
  (trigger on-delete            :deleted           (fn [& _] :ok))
  (trigger on-property-touched  :property-touched  (fn [& _] :ok))
  (trigger on-input-connections :input-connections (fn [& _] :ok)))

(n/defnode InheritedTriggerNode
  (inherits BaseTriggerNode)

  (trigger extra-added      :added           (fn [& _] :extra))
  (trigger on-delete        :deleted         (fn [& _] :override)))

(deftest nodes-can-have-triggers
  (testing "basic trigger definition"
    (is (fn? (get-in (t/triggers BaseTriggerNode) [:added :added-trigger])))
    (is (fn? (get-in (t/triggers BaseTriggerNode) [:added :multiway-trigger])))
    (is (fn? (get-in (t/triggers BaseTriggerNode) [:deleted :multiway-trigger])))
    (is (fn? (get-in (t/triggers BaseTriggerNode) [:deleted :on-delete])))
    (is (fn? (get-in (t/triggers BaseTriggerNode) [:property-touched :on-property-touched])))
    (is (fn? (get-in (t/triggers BaseTriggerNode) [:input-connections :on-input-connections]))))

  (testing "triggers are inherited"
    (is (fn? (get-in (t/triggers InheritedTriggerNode) [:added :added-trigger])))
    (is (fn? (get-in (t/triggers InheritedTriggerNode) [:added :multiway-trigger])))
    (is (fn? (get-in (t/triggers InheritedTriggerNode) [:added :extra-added])))
    (is (fn? (get-in (t/triggers InheritedTriggerNode) [:deleted :multiway-trigger]))))

  (testing "inherited triggers can be overridden"
    (is (fn? (get-in (t/triggers InheritedTriggerNode) [:deleted :on-delete])))
    (is (= :override ((get-in (t/triggers InheritedTriggerNode) [:deleted :on-delete])))))

  (testing "disallows unknown trigger kinds"
    (is (thrown-with-msg? clojure.lang.Compiler$CompilerException #"Valid trigger kinds are"
          (eval '(dynamo.node/defnode NoSuchTriggerNode
                   (trigger nope :not-a-real-trigger-kind (fn [& _] :nope))))))
    (is (thrown-with-msg? clojure.lang.Compiler$CompilerException #"Valid trigger kinds are"
          (eval '(dynamo.node/defnode NoSuchTriggerNode
                   (trigger nope :modified (fn [& _] :nope))))))))
