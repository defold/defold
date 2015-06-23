(ns dynamo.property-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.types :as types]
            [internal.graph.types :as gt]))

(defprotocol MyProtocol)

(deftest property-type-definition
  (let [property-defn (g/defproperty SomeProperty g/Any)]
    (is (var? property-defn))
    (is (identical? (resolve `SomeProperty) property-defn))
    (is (satisfies? gt/PropertyType (var-get property-defn)))
    (is (= g/Any (-> property-defn var-get :value-type)))
    (is (gt/property-visible? SomeProperty {})))
  (is (thrown-with-msg? clojure.lang.Compiler$CompilerException #"\(schema.core/protocol dynamo.property-test/MyProtocol\)"
                        (eval '(dynamo.graph/defproperty BadProp dynamo.property-test/MyProtocol)))))

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

(deftest property-type-default-value
  (is (= 42 (:default PropWithDefaultValue)))
  (is (= 42 (gt/property-default-value PropWithDefaultValue)))
  (is (nil? (gt/property-default-value PropWithDefaultValueNil)))
  (is (=  2 (gt/property-default-value PropWithMultipleDefaultValues)))
  (is (= 23 (gt/property-default-value PropWithDefaultValueFn)))
  (is (= -5 (gt/property-default-value PropWithDefaultValueVarAsSymbol)))
  (is (= -5 (gt/property-default-value PropWithDefaultValueVarForm)))
  (binding [*default-value* 61]
    (is (= 61 (gt/property-default-value PropWithDefaultValueVarAsSymbol)))
    (is (= 61 (gt/property-default-value PropWithDefaultValueVarForm))))
  (is (= :some-keyword (gt/property-default-value PropWithTypeKeyword)))
  (is (= 'some-symbol (gt/property-default-value PropWithTypeSymbol)))
  (is (nil? (gt/property-default-value PropWithoutDefaultValue)))
  (is (thrown-with-msg?
       clojure.lang.Compiler$CompilerException #"Unable to resolve symbol: non-existent-symbol in this context"
       (eval '(dynamo.graph/defproperty BadProp schema.core/Num (default non-existent-symbol))))))

(g/defproperty PropWithoutValidation g/Any)

(g/defproperty PropWithValidationFnInline g/Num
  (validate pos #(pos? %)))

(g/defproperty PropWithValidationFnValue g/Num
  (validate pos (every-pred pos? even?)))

(g/defproperty PropWithValidationLiteralTrue g/Num
  (validate any-value true))

(g/defproperty PropWithValidationLiteralFalse g/Num
  (validate no-value false))

(g/defproperty PropWithMultipleValidations g/Num
  (validate same-label pos?)
  (validate same-label neg?))

(defn ^:dynamic *validation-fn* [v] (= 42 v))

(g/defproperty PropWithValidationVarAsSymbol g/Num
  (validate must-be-the-answer *validation-fn*))

(g/defproperty PropWithValidationVarForm g/Num
  (validate must-be-the-answer #'*validation-fn*))

(deftest property-type-validation
  (is (true?  (gt/property-valid-value? PropWithoutValidation nil)))
  (is (true?  (gt/property-valid-value? PropWithoutValidation 0)))
  (is (true?  (gt/property-valid-value? PropWithoutValidation 1)))
  (is (false? (gt/property-valid-value? PropWithValidationFnInline 0)))
  (is (true?  (gt/property-valid-value? PropWithValidationFnInline 1)))
  (is (false? (gt/property-valid-value? PropWithValidationFnValue 0)))
  (is (false? (gt/property-valid-value? PropWithValidationFnValue 1)))
  (is (true?  (gt/property-valid-value? PropWithValidationFnValue 2)))
  (is (true?  (gt/property-valid-value? PropWithValidationLiteralTrue 0)))
  (is (true?  (gt/property-valid-value? PropWithValidationLiteralTrue 1)))
  (is (false? (gt/property-valid-value? PropWithValidationLiteralFalse 0)))
  (is (false? (gt/property-valid-value? PropWithValidationLiteralFalse 1)))
  (is (false? (gt/property-valid-value? PropWithMultipleValidations +1)))
  (is (false? (gt/property-valid-value? PropWithMultipleValidations -1)))
  (is (false? (gt/property-valid-value? PropWithValidationVarAsSymbol 23)))
  (is (true?  (gt/property-valid-value? PropWithValidationVarAsSymbol 42)))
  (is (false? (gt/property-valid-value? PropWithValidationVarForm 23)))
  (is (true?  (gt/property-valid-value? PropWithValidationVarForm 42)))
  (binding [*validation-fn* #(= 23 %)]
    (is (true?  (gt/property-valid-value? PropWithValidationVarAsSymbol 23)))
    (is (false? (gt/property-valid-value? PropWithValidationVarAsSymbol 42)))
    (is (true?  (gt/property-valid-value? PropWithValidationVarForm 23)))
    (is (false? (gt/property-valid-value? PropWithValidationVarForm 42))))
  (is (false? (gt/property-valid-value? PropWithValidationFnInline "not an integer")))
  (is (thrown-with-msg?
       clojure.lang.Compiler$CompilerException #"Unable to resolve symbol: non-existent-symbol in this context"
       (eval '(dynamo.graph/defproperty BadProp schema.core/Num (validate bad-case non-existent-symbol))))))

(defn- make-formatter [s]
  (fn [x] (str x s)))

(g/defproperty PropWithoutValidationFormatter g/Any
  (validate must-be-even even?))

(g/defproperty PropWithValidationFormatterFnInline g/Num
  (validate must-be-even :message #(str % " is not even") even?))

(g/defproperty PropWithValidationFormatterFnValue g/Num
  (validate must-be-even :message (make-formatter " is not even") even?))

(g/defproperty PropWithValidationFormatterLiteral g/Num
  (validate must-be-even :message "value must be even" even?))

(g/defproperty PropWithMultipleValidationsAndFormatters g/Num
  (validate must-be-even     :message "value must be even" even?)
  (validate must-be-positive :message "value must be positive" pos?))

(defn ^:dynamic *formatter-fn* [x] (str x " is not even"))

(g/defproperty PropWithValidationFormatterVarAsSymbol g/Num
  (validate must-be-even :message *formatter-fn* even?))

(g/defproperty PropWithValidationFormatterVarForm g/Num
  (validate must-be-even :message #'*formatter-fn* even?))

(deftest property-type-validation-messages
  (is (= []                                              (gt/property-validate PropWithoutValidationFormatter 4)))
  (is (= ["invalid value"]                               (gt/property-validate PropWithoutValidationFormatter 5)))
  (is (= ["invalid value"]                               (gt/property-validate PropWithoutValidationFormatter "not a number")))
  (is (= ["5 is not even"]                               (gt/property-validate PropWithValidationFormatterFnInline 5)))
  (is (= ["5 is not even"]                               (gt/property-validate PropWithValidationFormatterFnValue 5)))
  (is (= ["value must be even"]                          (gt/property-validate PropWithValidationFormatterLiteral 5)))
  (is (= ["value must be even"]                          (gt/property-validate PropWithMultipleValidationsAndFormatters 5)))
  (is (= ["value must be positive"]                      (gt/property-validate PropWithMultipleValidationsAndFormatters -4)))
  (is (= ["value must be even" "value must be positive"] (gt/property-validate PropWithMultipleValidationsAndFormatters -5)))
  (is (= ["5 is not even"]                               (gt/property-validate PropWithValidationFormatterVarAsSymbol 5)))
  (is (= ["5 is not even"]                               (gt/property-validate PropWithValidationFormatterVarForm 5)))
  (binding [*formatter-fn* (fn [x] (str x " is odd"))]
    (is (= ["5 is odd"]                                  (gt/property-validate PropWithValidationFormatterVarAsSymbol 5)))
    (is (= ["5 is odd"]                                  (gt/property-validate PropWithValidationFormatterVarForm 5)))))

(g/defproperty BaseProp g/Num)

(g/defproperty BasePropWithDefault g/Num
  (default 42))

(g/defproperty BasePropWithValidation g/Num
  (validate must-be-pos pos?))

(g/defproperty BasePropWithDefaultAndValidation g/Num
  (default 42)
  (validate must-be-pos pos?))

(g/defproperty DerivedProp BaseProp)

(g/defproperty DerivedPropOverrideDefaultInheritValidation BasePropWithValidation
  (default 23))

(g/defproperty DerivedPropInheritDefaultOverrideValidation BasePropWithDefault
  (validate must-be-pos even?))

(g/defproperty DerivedPropInheritBothOverrideBoth BasePropWithDefaultAndValidation
  (default 23)
  (validate must-be-pos even?))

(deftest property-type-inheritance
  (is (= g/Num (:value-type DerivedProp)))
  (is (= g/Num (:value-type DerivedPropOverrideDefaultInheritValidation)))
  (is (= g/Num (:value-type DerivedPropInheritDefaultOverrideValidation)))
  (is (= g/Num (:value-type DerivedPropInheritBothOverrideBoth)))
  (is (= 23 (gt/property-default-value DerivedPropOverrideDefaultInheritValidation)))
  (is (= 42 (gt/property-default-value DerivedPropInheritDefaultOverrideValidation)))
  (is (= 23 (gt/property-default-value DerivedPropInheritBothOverrideBoth)))
  (is (false? (gt/property-valid-value? DerivedPropOverrideDefaultInheritValidation 0)))
  (is (true?  (gt/property-valid-value? DerivedPropOverrideDefaultInheritValidation 1)))
  (is (true?  (gt/property-valid-value? DerivedPropOverrideDefaultInheritValidation 2)))
  (is (true?  (gt/property-valid-value? DerivedPropInheritDefaultOverrideValidation 0)))
  (is (false? (gt/property-valid-value? DerivedPropInheritDefaultOverrideValidation 1)))
  (is (true?  (gt/property-valid-value? DerivedPropInheritDefaultOverrideValidation 2)))
  (is (false? (gt/property-valid-value? DerivedPropInheritBothOverrideBoth 0)))
  (is (false? (gt/property-valid-value? DerivedPropInheritBothOverrideBoth 1)))
  (is (true?  (gt/property-valid-value? DerivedPropInheritBothOverrideBoth 2))))

(g/defproperty UntaggedProp g/Keyword)

(g/defproperty TaggedProp g/Keyword
  (tag :first))

(g/defproperty ChildOfTaggedProp TaggedProp)

(g/defproperty ChildOfTaggedPropWithOverride TaggedProp)

(g/defproperty GrandchildOfTaggedProp ChildOfTaggedProp)

(g/defproperty TaggedChild TaggedProp
  (tag :second))

(g/defproperty MultipleTags TaggedProp
  (tag :second)
  (tag :third))

(g/defproperty ChildOfTaggedPropWithOverride TaggedProp
  (default :some-keyword))

(deftest property-tags
  (are [property tags] (= tags (gt/property-tags property))
       UntaggedProp           []
       TaggedProp             [:first]
       ChildOfTaggedProp      [:first]
       GrandchildOfTaggedProp [:first]
       TaggedChild            [:second :first]
       MultipleTags           [:third :second :first])
  (is (vector? (gt/property-tags ChildOfTaggedPropWithOverride))))

(g/defproperty StringProp g/Str)

(g/defproperty Vec3Prop types/Vec3)

(g/defproperty InheritsFromVec3Prop Vec3Prop)

(g/defproperty UnregisteredProp [g/Keyword g/Str g/Num])


(g/defproperty PropWithoutEnablement g/Any)

(g/defproperty PropWithEnablementFnInline g/Num
  (enabled #(pos? %)))

(g/defproperty PropWithEnablementFnValue g/Num
  (enabled (every-pred pos? even?)))

(g/defproperty PropWithEnablementLiteralTrue g/Num
  (enabled true))

(g/defproperty PropWithEnablementLiteralFalse g/Num
  (enabled false))

(defn ^:dynamic *enablement-fn* [v] (= 42 v))

(g/defproperty PropWithEnablementVarAsSymbol g/Num
  (enabled *enablement-fn*))

(g/defproperty PropWithEnablementVarForm g/Num
  (enabled #'*enablement-fn*))

(deftest property-type-enablement
  (is (true?  (gt/property-enabled? PropWithoutEnablement nil)))
  (is (true?  (gt/property-enabled? PropWithoutEnablement 0)))
  (is (true?  (gt/property-enabled? PropWithoutEnablement 1)))
  (is (false? (gt/property-enabled? PropWithEnablementFnInline 0)))
  (is (true?  (gt/property-enabled? PropWithEnablementFnInline 1)))
  (is (false? (gt/property-enabled? PropWithEnablementFnValue 0)))
  (is (false? (gt/property-enabled? PropWithEnablementFnValue 1)))
  (is (true?  (gt/property-enabled? PropWithEnablementFnValue 2)))
  (is (true?  (gt/property-enabled? PropWithEnablementLiteralTrue 0)))
  (is (true?  (gt/property-enabled? PropWithEnablementLiteralTrue 1)))
  (is (false? (gt/property-enabled? PropWithEnablementLiteralFalse 0)))
  (is (false? (gt/property-enabled? PropWithEnablementLiteralFalse 1)))
  (is (false? (gt/property-enabled? PropWithEnablementVarAsSymbol 23)))
  (is (true?  (gt/property-enabled? PropWithEnablementVarAsSymbol 42)))
  (is (false? (gt/property-enabled? PropWithEnablementVarForm 23)))
  (is (true?  (gt/property-enabled? PropWithEnablementVarForm 42)))
  (binding [*enablement-fn* #(= 23 %)]
    (is (true?  (gt/property-enabled? PropWithEnablementVarAsSymbol 23)))
    (is (false? (gt/property-enabled? PropWithEnablementVarAsSymbol 42)))
    (is (true?  (gt/property-enabled? PropWithEnablementVarForm 23)))
    (is (false? (gt/property-enabled? PropWithEnablementVarForm 42))))
  (is (thrown-with-msg?
       clojure.lang.Compiler$CompilerException #"Unable to resolve symbol: non-existent-symbol in this context"
       (eval '(dynamo.graph/defproperty BadProp schema.core/Num (enabled non-existent-symbol))))))

(g/defproperty PropWithoutVisibility g/Any)

(g/defproperty PropWithVisibilityFnInline g/Num
  (visible (g/fnk [an-input] (pos? an-input))))

(g/defnk enablement-fn [an-input another-input] (= an-input another-input))

(g/defproperty PropWithVisibilityVarAsSymbol g/Num
  (visible enablement-fn))

(g/defproperty PropWithVisibilityVarForm g/Num
  (visible #'enablement-fn))

(deftest property-type-enablement
  (is (true?  (gt/property-visible? PropWithoutVisibility {})))
  (is (false? (gt/property-visible? PropWithVisibilityFnInline {:an-input 0})))
  (is (true?  (gt/property-visible? PropWithVisibilityFnInline {:an-input 1})))
  (is (false? (gt/property-visible? PropWithVisibilityVarAsSymbol {:an-input 42 :another-input 23})))
  (is (true?  (gt/property-visible? PropWithVisibilityVarAsSymbol {:an-input 42 :another-input 42})))
  (is (false? (gt/property-visible? PropWithVisibilityVarForm {:an-input 42 :another-input 23})))
  (is (true?  (gt/property-visible? PropWithVisibilityVarForm {:an-input 42 :another-input 42})))
  (is (thrown-with-msg?
       clojure.lang.Compiler$CompilerException #"should be an fnk"
       (eval '(dynamo.graph/defproperty BadProp schema.core/Num (visible pos?))))))
