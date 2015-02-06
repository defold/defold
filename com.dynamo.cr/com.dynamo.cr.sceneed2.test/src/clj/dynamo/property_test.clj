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
    (is (= s/Any (-> property-defn var-get :value-type)))
    (is (t/property-visible SomeProperty)))
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
  (is (= 42 (t/property-default-value PropWithDefaultValue)))
  (is (nil? (t/property-default-value PropWithDefaultValueNil)))
  (is (=  2 (t/property-default-value PropWithMultipleDefaultValues)))
  (is (= 23 (t/property-default-value PropWithDefaultValueFn)))
  (is (= -5 (t/property-default-value PropWithDefaultValueVarAsSymbol)))
  (is (= -5 (t/property-default-value PropWithDefaultValueVarForm)))
  (binding [*default-value* 61]
    (is (= 61 (t/property-default-value PropWithDefaultValueVarAsSymbol)))
    (is (= 61 (t/property-default-value PropWithDefaultValueVarForm))))
  (is (= :some-keyword (t/property-default-value PropWithTypeKeyword)))
  (is (= 'some-symbol (t/property-default-value PropWithTypeSymbol)))
  (is (nil? (t/property-default-value PropWithoutDefaultValue)))
  (is (thrown-with-msg?
        clojure.lang.Compiler$CompilerException #"Unable to resolve symbol: non-existent-symbol in this context"
        (eval '(dynamo.property/defproperty BadProp schema.core/Num (default non-existent-symbol))))))

(defproperty PropWithoutValidation s/Any)

(defproperty PropWithValidationFnInline s/Num
  (validate pos #(pos? %)))

(defproperty PropWithValidationFnValue s/Num
  (validate pos (every-pred pos? even?)))

(defproperty PropWithValidationLiteralTrue s/Num
  (validate any-value true))

(defproperty PropWithValidationLiteralFalse s/Num
  (validate no-value false))

(defproperty PropWithMultipleValidations s/Num
  (validate same-label pos?)
  (validate same-label neg?))

(defn ^:dynamic *validation-fn* [v] (= 42 v))

(defproperty PropWithValidationVarAsSymbol s/Num
  (validate must-be-the-answer *validation-fn*))

(defproperty PropWithValidationVarForm s/Num
  (validate must-be-the-answer #'*validation-fn*))

(deftest property-type-validation
  (is (true?  (t/property-valid-value? PropWithoutValidation nil)))
  (is (true?  (t/property-valid-value? PropWithoutValidation 0)))
  (is (true?  (t/property-valid-value? PropWithoutValidation 1)))
  (is (false? (t/property-valid-value? PropWithValidationFnInline 0)))
  (is (true?  (t/property-valid-value? PropWithValidationFnInline 1)))
  (is (false? (t/property-valid-value? PropWithValidationFnValue 0)))
  (is (false? (t/property-valid-value? PropWithValidationFnValue 1)))
  (is (true?  (t/property-valid-value? PropWithValidationFnValue 2)))
  (is (true?  (t/property-valid-value? PropWithValidationLiteralTrue 0)))
  (is (true?  (t/property-valid-value? PropWithValidationLiteralTrue 1)))
  (is (false? (t/property-valid-value? PropWithValidationLiteralFalse 0)))
  (is (false? (t/property-valid-value? PropWithValidationLiteralFalse 1)))
  (is (false? (t/property-valid-value? PropWithMultipleValidations +1)))
  (is (true?  (t/property-valid-value? PropWithMultipleValidations -1)))
  (is (false? (t/property-valid-value? PropWithValidationVarAsSymbol 23)))
  (is (true?  (t/property-valid-value? PropWithValidationVarAsSymbol 42)))
  (is (false? (t/property-valid-value? PropWithValidationVarForm 23)))
  (is (true?  (t/property-valid-value? PropWithValidationVarForm 42)))
  (binding [*validation-fn* #(= 23 %)]
    (is (true?  (t/property-valid-value? PropWithValidationVarAsSymbol 23)))
    (is (false? (t/property-valid-value? PropWithValidationVarAsSymbol 42)))
    (is (true?  (t/property-valid-value? PropWithValidationVarForm 23)))
    (is (false? (t/property-valid-value? PropWithValidationVarForm 42))))
  (is (false? (t/property-valid-value? PropWithValidationFnInline "not an integer")))
  (is (thrown-with-msg?
        clojure.lang.Compiler$CompilerException #"Unable to resolve symbol: non-existent-symbol in this context"
        (eval '(dynamo.property/defproperty BadProp schema.core/Num (validate bad-case non-existent-symbol))))))

(defn- make-formatter [s]
  (fn [x] (str x s)))

(defproperty PropWithoutValidationFormatter s/Any
  (validate must-be-even even?))

(defproperty PropWithValidationFormatterFnInline s/Num
  (validate must-be-even :message #(str % "is not even") even?))

(defproperty PropWithValidationFormatterFnValue s/Num
  (validate must-be-even :message (make-formatter "is not even") even?))

(defproperty PropWithValidationFormatterLiteral s/Num
  (validate must-be-even :message "value must be even" even?))

(defn ^:dynamic *formatter-fn* [x] (str x "is not even"))

(defproperty PropWithValidationFormatterVarAsSymbol s/Num
  (validate must-be-even :message *formatter-fn* even?))

(defproperty PropWithValidationFormatterVarForm s/Num
  (validate must-be-even :message #'*formatter-fn* even?))

(deftest property-type-validation-messages
  (is (= []                (t/property-validate PropWithoutValidationFormatter 4)))
  (is (= ["invalid value"] (t/property-validate PropWithoutValidationFormatter 5)))
  (is (= ["invalid value"] (t/property-validate PropWithoutValidationFormatter "not a number")))

;  (is (true?  (t/property-valid-value? PropWithoutValidation 0)))
;  (is (true?  (t/property-valid-value? PropWithoutValidation 1)))
;  (is (false? (t/property-valid-value? PropWithValidationFnInline 0)))
;  (is (true?  (t/property-valid-value? PropWithValidationFnInline 1)))
;  (is (false? (t/property-valid-value? PropWithValidationFnValue 0)))
;  (is (false? (t/property-valid-value? PropWithValidationFnValue 1)))
;  (is (true?  (t/property-valid-value? PropWithValidationFnValue 2)))
  )

(defproperty BaseProp s/Num)

(defproperty BasePropWithDefault s/Num
  (default 42))

(defproperty BasePropWithValidation s/Num
  (validate must-be-pos pos?))

(defproperty BasePropWithDefaultAndValidation s/Num
  (default 42)
  (validate must-be-pos pos?))

(defproperty DerivedProp BaseProp)

(defproperty DerivedPropOverrideDefaultInheritValidation BasePropWithValidation
  (default 23))

(defproperty DerivedPropInheritDefaultOverrideValidation BasePropWithDefault
  (validate must-be-pos even?))

(defproperty DerivedPropInheritBothOverrideBoth BasePropWithDefaultAndValidation
  (default 23)
  (validate must-be-pos even?))

(deftest property-type-inheritance
  (is (= s/Num (:value-type DerivedProp)))
  (is (= s/Num (:value-type DerivedPropOverrideDefaultInheritValidation)))
  (is (= s/Num (:value-type DerivedPropInheritDefaultOverrideValidation)))
  (is (= s/Num (:value-type DerivedPropInheritBothOverrideBoth)))
  (is (= 23 (t/property-default-value DerivedPropOverrideDefaultInheritValidation)))
  (is (= 42 (t/property-default-value DerivedPropInheritDefaultOverrideValidation)))
  (is (= 23 (t/property-default-value DerivedPropInheritBothOverrideBoth)))
  (is (false? (t/property-valid-value? DerivedPropOverrideDefaultInheritValidation 0)))
  (is (true?  (t/property-valid-value? DerivedPropOverrideDefaultInheritValidation 1)))
  (is (true?  (t/property-valid-value? DerivedPropOverrideDefaultInheritValidation 2)))
  (is (true?  (t/property-valid-value? DerivedPropInheritDefaultOverrideValidation 0)))
  (is (false? (t/property-valid-value? DerivedPropInheritDefaultOverrideValidation 1)))
  (is (true?  (t/property-valid-value? DerivedPropInheritDefaultOverrideValidation 2)))
  (is (true?  (t/property-valid-value? DerivedPropInheritBothOverrideBoth 0)))
  (is (false? (t/property-valid-value? DerivedPropInheritBothOverrideBoth 1)))
  (is (true?  (t/property-valid-value? DerivedPropInheritBothOverrideBoth 2))))

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

(deftest presenter-event-map
  (are [event-map expected-presenter-event-map] (= expected-presenter-event-map (dp/presenter-event-map event-map))
    {}                                     {}
    {:widget "FILTER ME"}                  {}
    {:type :some-type}                     {:type :some-type}
    {:type :some-type :widget "FILTER ME"} {:type :some-type}
    {:character \d}                        {::dp/character \d}
    {:type :some-type :character \d}       {:type :some-type ::dp/character \d}))
