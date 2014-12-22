(ns dynamo.property-test
  (:require [clojure.test :refer :all]
            [dynamo.property :as dp :refer [defproperty]]
            [dynamo.types :as t]
            [schema.core :as s])
  (:import [dynamo.property DefaultPresenter]))

(defprotocol MyProtocol)

(deftest property-type-definition
  (let [property-defn (defproperty SomeProperty s/Any)]
    (is (var? property-defn))
    (is (identical? (resolve `SomeProperty) property-defn))
    (is (satisfies? t/PropertyType (var-get property-defn)))
    (is (= s/Any (-> property-defn var-get :value-type))))
  (is (thrown-with-msg? clojure.lang.Compiler$CompilerException #"\(schema.core/protocol dynamo.property-test/MyProtocol\)"
        (eval '(dynamo.property/defproperty BadProp dynamo.property-test/MyProtocol)))))

(defproperty PropWithDefaultValue s/Num
  (default 42))

(defproperty PropWithDefaultValueNil s/Any
  (default nil))

(defproperty PropWithMultipleDefaultValues s/Num
  (default 1)
  (default 2))

(defproperty PropWithDefaultValueFn s/Num
  (default (constantly 23)))

(defn ^:dynamic *default-value* [] -5)

(defproperty PropWithDefaultValueVarAsSymbol s/Num
  (default *default-value*))

(defproperty PropWithDefaultValueVarForm s/Num
  (default #'*default-value*))

(defproperty PropWithTypeKeyword s/Keyword
  (default :some-keyword))

(defproperty PropWithTypeSymbol s/Symbol
  (default 'some-symbol))

(defproperty PropWithoutDefaultValue s/Num)

(deftest property-type-default-value
  (is (= 42 (:default PropWithDefaultValue)))
  (is (= 42 (t/default-property-value PropWithDefaultValue)))
  (is (nil? (t/default-property-value PropWithDefaultValueNil)))
  (is (=  2 (t/default-property-value PropWithMultipleDefaultValues)))
  (is (= 23 (t/default-property-value PropWithDefaultValueFn)))
  (is (= -5 (t/default-property-value PropWithDefaultValueVarAsSymbol)))
  (is (= -5 (t/default-property-value PropWithDefaultValueVarForm)))
  (binding [*default-value* 61]
    (is (= 61 (t/default-property-value PropWithDefaultValueVarAsSymbol)))
    (is (= 61 (t/default-property-value PropWithDefaultValueVarForm))))
  (is (= :some-keyword (t/default-property-value PropWithTypeKeyword)))
  (is (= 'some-symbol (t/default-property-value PropWithTypeSymbol)))
  (is (nil? (t/default-property-value PropWithoutDefaultValue)))
  (is (thrown-with-msg?
        clojure.lang.Compiler$CompilerException #"Unable to resolve symbol: non-existent-symbol in this context"
        (eval '(dynamo.property/defproperty BadProp schema.core/Num (default non-existent-symbol))))))

(defproperty PropWithoutValidation s/Any)

(defproperty PropWithValidationFnInline s/Num
  (validation #(pos? %)))

(defproperty PropWithValidationFnValue s/Num
  (validation (every-pred pos? even?)))

(defproperty PropWithValidationLiteralTrue s/Num
  (validation true))

(defproperty PropWithValidationLiteralFalse s/Num
  (validation false))

(defproperty PropWithMultipleValidations s/Num
  (validation pos?)
  (validation neg?))

(defn ^:dynamic *validation-fn* [v] (= 42 v))

(defproperty PropWithValidationVarAsSymbol s/Num
  (validation *validation-fn*))

(defproperty PropWithValidationVarForm s/Num
  (validation #'*validation-fn*))

(deftest property-type-validation
  (is (true?  (t/valid-property-value? PropWithoutValidation nil)))
  (is (true?  (t/valid-property-value? PropWithoutValidation 0)))
  (is (true?  (t/valid-property-value? PropWithoutValidation 1)))
  (is (false? (t/valid-property-value? PropWithValidationFnInline 0)))
  (is (true?  (t/valid-property-value? PropWithValidationFnInline 1)))
  (is (false? (t/valid-property-value? PropWithValidationFnValue 0)))
  (is (false? (t/valid-property-value? PropWithValidationFnValue 1)))
  (is (true?  (t/valid-property-value? PropWithValidationFnValue 2)))
  (is (true?  (t/valid-property-value? PropWithValidationLiteralTrue 0)))
  (is (true?  (t/valid-property-value? PropWithValidationLiteralTrue 1)))
  (is (false? (t/valid-property-value? PropWithValidationLiteralFalse 0)))
  (is (false? (t/valid-property-value? PropWithValidationLiteralFalse 1)))
  (is (false? (t/valid-property-value? PropWithMultipleValidations +1)))
  (is (true?  (t/valid-property-value? PropWithMultipleValidations -1)))
  (is (false? (t/valid-property-value? PropWithValidationVarAsSymbol 23)))
  (is (true?  (t/valid-property-value? PropWithValidationVarAsSymbol 42)))
  (is (false? (t/valid-property-value? PropWithValidationVarForm 23)))
  (is (true?  (t/valid-property-value? PropWithValidationVarForm 42)))
  (binding [*validation-fn* #(= 23 %)]
    (is (true?  (t/valid-property-value? PropWithValidationVarAsSymbol 23)))
    (is (false? (t/valid-property-value? PropWithValidationVarAsSymbol 42)))
    (is (true?  (t/valid-property-value? PropWithValidationVarForm 23)))
    (is (false? (t/valid-property-value? PropWithValidationVarForm 42))))
  (is (thrown-with-msg?
        clojure.lang.Compiler$CompilerException #"Unable to resolve symbol: non-existent-symbol in this context"
        (eval '(dynamo.property/defproperty BadProp schema.core/Num (validation non-existent-symbol))))))

(defproperty BaseProp s/Num)

(defproperty BasePropWithDefault s/Num
  (default 42))

(defproperty BasePropWithValidation s/Num
  (validation pos?))

(defproperty BasePropWithDefaultAndValidation s/Num
  (default 42)
  (validation pos?))

(defproperty DerivedProp BaseProp)

(defproperty DerivedPropOverrideDefaultInheritValidation BasePropWithValidation
  (default 23))

(defproperty DerivedPropInheritDefaultOverrideValidation BasePropWithDefault
  (validation even?))

(defproperty DerivedPropInheritBothOverrideBoth BasePropWithDefaultAndValidation
  (default 23)
  (validation even?))

(deftest property-type-inheritance
  (is (= s/Num (:value-type DerivedProp)))
  (is (= s/Num (:value-type DerivedPropOverrideDefaultInheritValidation)))
  (is (= s/Num (:value-type DerivedPropInheritDefaultOverrideValidation)))
  (is (= s/Num (:value-type DerivedPropInheritBothOverrideBoth)))
  (is (= 23 (t/default-property-value DerivedPropOverrideDefaultInheritValidation)))
  (is (= 42 (t/default-property-value DerivedPropInheritDefaultOverrideValidation)))
  (is (= 23 (t/default-property-value DerivedPropInheritBothOverrideBoth)))
  (is (false? (t/valid-property-value? DerivedPropOverrideDefaultInheritValidation 0)))
  (is (true?  (t/valid-property-value? DerivedPropOverrideDefaultInheritValidation 1)))
  (is (true?  (t/valid-property-value? DerivedPropOverrideDefaultInheritValidation 2)))
  (is (true?  (t/valid-property-value? DerivedPropInheritDefaultOverrideValidation 0)))
  (is (false? (t/valid-property-value? DerivedPropInheritDefaultOverrideValidation 1)))
  (is (true?  (t/valid-property-value? DerivedPropInheritDefaultOverrideValidation 2)))
  (is (true?  (t/valid-property-value? DerivedPropInheritBothOverrideBoth 0)))
  (is (false? (t/valid-property-value? DerivedPropInheritBothOverrideBoth 1)))
  (is (true?  (t/valid-property-value? DerivedPropInheritBothOverrideBoth 2))))

(defproperty UntaggedProp s/Keyword)

(defproperty TaggedProp s/Keyword
  (tag :first))

(defproperty ChildOfTaggedProp TaggedProp)

(defproperty ChildOfTaggedPropWithOverride TaggedProp)

(defproperty GrandchildOfTaggedProp ChildOfTaggedProp)

(defproperty TaggedChild TaggedProp
  (tag :second))

(defproperty MultipleTags TaggedProp
  (tag :second)
  (tag :third))

(defproperty ChildOfTaggedPropWithOverride TaggedProp
  (default :some-keyword))

(deftest property-tags
  (are [property tags] (= tags (t/property-tags property))
    UntaggedProp           []
    TaggedProp             [:first]
    ChildOfTaggedProp      [:first]
    GrandchildOfTaggedProp [:first]
    TaggedChild            [:second :first]
    MultipleTags           [:third :second :first])
  (is (vector? (t/property-tags ChildOfTaggedPropWithOverride))))

(defproperty StringProp s/Str)

(defproperty Vec3Prop t/Vec3)

(defproperty InheritsFromVec3Prop Vec3Prop)

(defproperty UnregisteredProp [s/Keyword s/Str s/Num])

(defrecord CustomPresenter []
  dp/Presenter)

(defrecord FirstCustomPresenter []
  dp/Presenter)

(defrecord SecondCustomPresenter []
  dp/Presenter)

(defrecord ThirdCustomPresenter []
  dp/Presenter)

(deftest lookup-presenter
  (testing "when the property is not registered, returns a default presenter"
    (is (instance? DefaultPresenter (dp/lookup-presenter {} StringProp)))
    (is (instance? DefaultPresenter (dp/lookup-presenter {} TaggedProp))))
  (testing "overriding registry"
    (are [type-to-register property-to-look-up expected-presenter]
      (let [registry (dp/register-presenter {} type-to-register (->CustomPresenter))
            actual-presenter (dp/lookup-presenter registry property-to-look-up)]
        (instance? expected-presenter actual-presenter))
      ;type-to-register    property-to-look-up  expected-presenter
      {:value-type s/Str}  StringProp           CustomPresenter
      {:value-type s/Str}  UnregisteredProp     DefaultPresenter
      {:value-type t/Vec3} InheritsFromVec3Prop CustomPresenter
      {:value-type t/Vec3} Vec3Prop             CustomPresenter))
  (testing "presenters with tags"
    (are [type-to-register property-to-look-up expected-presenter]
      (let [registry (dp/register-presenter {} type-to-register (->CustomPresenter))
            actual-presenter (dp/lookup-presenter registry property-to-look-up)]
        (instance? expected-presenter actual-presenter))
      ;type-to-register                    property-to-look-up  expected-presenter
      {:value-type s/Keyword}              UntaggedProp         CustomPresenter
      {:value-type s/Keyword}              TaggedProp           CustomPresenter
      {:value-type s/Keyword :tag :first}  UntaggedProp         DefaultPresenter
      {:value-type s/Keyword :tag :first}  TaggedProp           CustomPresenter
      {:value-type s/Str}                  UntaggedProp         DefaultPresenter
      {:value-type s/Str     :tag :first}  UntaggedProp         DefaultPresenter
      {:value-type s/Str     :tag :first}  TaggedProp           DefaultPresenter
      {:value-type s/Keyword :tag :second} TaggedProp           DefaultPresenter
      {:value-type s/Keyword :tag :second} TaggedChild          CustomPresenter
      {:value-type s/Keyword :tag :second} MultipleTags         CustomPresenter
      {:value-type s/Keyword :tag :third}  TaggedChild          DefaultPresenter
      {:value-type s/Keyword :tag :third}  MultipleTags         CustomPresenter))
  (testing "tag precedence"
    (let [registry (-> {}
                       (dp/register-presenter {:value-type s/Keyword}              (->CustomPresenter))
                       (dp/register-presenter {:value-type s/Keyword :tag :first}  (->FirstCustomPresenter))
                       (dp/register-presenter {:value-type s/Keyword :tag :second} (->SecondCustomPresenter))
                       (dp/register-presenter {:value-type s/Keyword :tag :third}  (->ThirdCustomPresenter)))]
      (are [property-to-look-up expected-presenter] (instance? expected-presenter (dp/lookup-presenter registry property-to-look-up))
        ;property-to-look-up          expected-presenter
        UntaggedProp                  CustomPresenter
        TaggedProp                    FirstCustomPresenter
        ChildOfTaggedProp             FirstCustomPresenter
        GrandchildOfTaggedProp        FirstCustomPresenter
        ChildOfTaggedPropWithOverride FirstCustomPresenter
        TaggedChild                   SecondCustomPresenter
        MultipleTags                  ThirdCustomPresenter))))
