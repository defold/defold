(ns dynamo.property-test
  (:require [clojure.test :refer :all]
            [dynamo.property :as dp :refer [defproperty]]
            [dynamo.types :as t]
            [schema.core :as s]))

(deftest property-type-definition
  (let [property-defn (defproperty SomeProperty s/Any)]
    (is (var? property-defn))
    (is (identical? (resolve `SomeProperty) property-defn))
    (is (satisfies? t/PropertyTypeDescriptor (var-get property-defn)))
    (is (= s/Any (-> property-defn var-get :value-type)))))

(defproperty PropWithDefaultValue Integer
  (default 42))

(defproperty PropWithDefaultValueNil Integer
  (default nil))

(defproperty PropWithMultipleDefaultValues Integer
  (default 1)
  (default 2))

(defproperty PropWithDefaultValueFn Integer
  (default (constantly 23)))

(defn ^:dynamic *default-value* [] -5)

(defproperty PropWithDefaultValueVarAsSymbol Integer
  (default *default-value*))

(defproperty PropWithDefaultValueVarForm Integer
  (default #'*default-value*))

(defproperty PropWithTypeKeyword s/Keyword
  (default :some-keyword))

(defproperty PropWithTypeSymbol s/Any #_s/Symbol
  (default 'some-symbol))

(defproperty PropWithoutDefaultValue Integer)

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
  (is (thrown? java.lang.AssertionError (t/default-property-value PropWithoutDefaultValue)))
  ;; TODO: `eval` below does not work as expected when run as part of "JUnit Plug-in Test".
  (is (thrown-with-msg?
        clojure.lang.Compiler$CompilerException #"Unable to resolve symbol: non-existent-symbol in this context"
        (eval '(defproperty BadProp Integer (default non-existent-symbol))))))
