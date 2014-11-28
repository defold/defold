(ns internal.property
  (:require [dynamo.types :as t]))

(set! *warn-on-reflection* true)

(defn property-type-descriptor [value-type body-forms]
  `(t/->PropertyTypeDescriptorImpl ~value-type))

(defn def-property-type-descriptor [name value-type & body-forms]
  `(def ~name ~(property-type-descriptor value-type body-forms)))
