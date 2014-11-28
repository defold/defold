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
