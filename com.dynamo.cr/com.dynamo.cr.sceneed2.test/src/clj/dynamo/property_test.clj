(ns dynamo.property-test
  (:require [clojure.test :refer :all]
            [dynamo.property :as dp :refer [defproperty]]
            [dynamo.types :as t]
            [schema.core :as s]))

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
