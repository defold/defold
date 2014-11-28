(ns dynamo.property
  (:require [dynamo.types :as t]))

(set! *warn-on-reflection* true)

(defmacro defproperty [name value-type]
  `(def ~name (t/->PropertyTypeDescriptorImpl ~value-type)))
