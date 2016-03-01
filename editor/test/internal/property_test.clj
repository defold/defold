(ns internal.property-test
  (:require [clojure.test :refer :all]
            [internal.util :as util]
            [editor.types :as types]
            [internal.graph.types :as gt]
            [dynamo.graph :as g]
            [plumbing.fnk.pfnk :as pf]
            [support.test-support :refer [with-clean-system tx-nodes]])
  (:import [clojure.lang Compiler$CompilerException]))

(defprotocol MyProtocol)

(deftest test-property-type-definition
  (let [property-defn (g/defproperty SomeProperty g/Any)]
    (is (var? property-defn))
    (is (identical? (resolve `SomeProperty) property-defn))
    (is (satisfies? gt/PropertyType (var-get property-defn)))
    (is (= g/Any (-> property-defn var-get :value-type)))
    (is (= "SomeProperty" (:name SomeProperty))))
  (is (thrown-with-msg? Compiler$CompilerException #"\(dynamo.graph/protocol internal.property-test/MyProtocol\)"
                        (eval '(dynamo.graph/defproperty BadProp internal.property-test/MyProtocol)))))

(g/defproperty PropWithDefaultValue g/Num
  (default 42))

(g/defproperty PropWithDefaultValueNil g/Any
  (default nil))

(g/defproperty PropWithMultipleDefaultValues g/Num
  (default 1)
  (default 2))

(g/defproperty PropWithDefaultValueFn g/Num
  (default (constantly 23)))

(defn ^:dynamic *default-value* [] -5)

(g/defproperty PropWithDefaultValueVarAsSymbol g/Num
  (default *default-value*))

(g/defproperty PropWithDefaultValueVarForm g/Num
  (default #'*default-value*))

(g/defproperty PropWithTypeKeyword g/Keyword
  (default :some-keyword))

(g/defproperty PropWithTypeSymbol g/Symbol
  (default 'some-symbol))

(g/defproperty PropWithoutDefaultValue g/Num)

(deftest test-property-type-default-value
  (is (= 42 (:default PropWithDefaultValue)))
  (is (= 42 (gt/property-default-value PropWithDefaultValue)))
  (is (nil? (gt/property-default-value PropWithDefaultValueNil)))
  (is (=  2 (gt/property-default-value PropWithMultipleDefaultValues)))
  (is (= 23 (gt/property-default-value PropWithDefaultValueFn)))
  (is (= -5 (gt/property-default-value PropWithDefaultValueVarAsSymbol)))
  (is (= -5 (gt/property-default-value PropWithDefaultValueVarForm)))
  (binding [*default-value* 61]
    (is (= -5 (gt/property-default-value PropWithDefaultValueVarAsSymbol)))
    (is (= 61 (gt/property-default-value PropWithDefaultValueVarForm))))
  (is (= :some-keyword (gt/property-default-value PropWithTypeKeyword)))
  (is (= 'some-symbol (gt/property-default-value PropWithTypeSymbol)))
  (is (nil? (gt/property-default-value PropWithoutDefaultValue)))
  (is (thrown-with-msg?
       clojure.lang.Compiler$CompilerException #"Unable to resolve symbol: non-existent-symbol in this context"
       (eval '(dynamo.graph/defproperty BadProp schema.core/Num (default non-existent-symbol))))))

(g/defproperty BaseProp g/Num)

(g/defproperty BasePropWithDefault g/Num
  (default 42))

(g/defproperty StringProp g/Str)

(g/defproperty Vec3Prop types/Vec3)

(g/defproperty UnregisteredProp [g/Keyword g/Str g/Num])

(g/defproperty PropWithoutEnablement g/Any)

(g/defproperty PropWithEnablementAlwaysTrue g/Num
  (dynamic enabled (g/always true)))

(g/defproperty PropWithEnablementAlwaysFalse g/Num
  (dynamic enabled (g/always false)))

(deftest test-enablement
  (is (thrown-with-msg?
       clojure.lang.Compiler$CompilerException #"property dynamic requires a symbol"
       (eval '(dynamo.graph/defproperty BadProp schema.core/Num (dynamic (enabled non-existent-symbol)))))))

(g/defproperty PropWithoutVisibility g/Any)

(g/defproperty PropWithVisibilityFn g/Num
  (dynamic visible (g/fnk [an-input] (pos? an-input))))

(deftest test-property-type-enablement
  (is (thrown-with-msg?
       clojure.lang.Compiler$CompilerException #"should be an fnk"
       (eval '(dynamo.graph/defproperty BadProp schema.core/Num (dynamic visible pos?))))))

(g/defproperty PropertyWithDynamicAttribute g/Any
  (dynamic fooable (g/fnk [an-input] (pos? an-input))))

(g/defnk fooable-fn [an-input another-input] (= an-input another-input))

(g/defproperty PropertyWithDynamicVarAsSymbol g/Num
  (dynamic fooable fooable-fn))

(g/defproperty PropertyWithDynamicVarForm g/Num
  (dynamic fooable #'fooable-fn))

(defn dynamic-arguments [property kind]
  (pf/input-schema
   (get (gt/dynamic-attributes property) kind)))

(defn argument-schema [& args]
  (assoc
   (zipmap args (repeat g/Any))
   g/Keyword g/Any))

(deftest test-property-dynamics
  (is (= (argument-schema :an-input) (dynamic-arguments PropertyWithDynamicAttribute :fooable)))
  (is (= (argument-schema :an-input :another-input) (dynamic-arguments PropertyWithDynamicVarAsSymbol :fooable)))
  (is (= (argument-schema :an-input :another-input) (dynamic-arguments PropertyWithDynamicVarForm :fooable))))

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
