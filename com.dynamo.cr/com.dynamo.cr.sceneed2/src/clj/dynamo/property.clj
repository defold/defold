(ns dynamo.property
  (:require [internal.property :as ip]))

(set! *warn-on-reflection* true)

(defmacro defproperty [name value-type & body-forms]
  (apply ip/def-property-type-descriptor name value-type body-forms))
