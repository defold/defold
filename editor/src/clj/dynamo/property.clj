(ns dynamo.property
  (:require [dynamo.types :as t]
            [dynamo.ui :as ui]
            [dynamo.util :refer [map-keys]]
            [internal.property :as ip]
            [schema.core :as s]))

(defmacro defproperty [name value-type & body-forms]
  (apply ip/def-property-type-descriptor name value-type body-forms))

(defproperty NonNegativeInt t/Int
  (validate positive? :message "must be equal to or greater than zero" (comp not neg?)))

(defproperty Resource t/Str (tag ::resource))
(defproperty ImageResource Resource (tag ::image))

(defproperty ResourceList [t/Str] (tag ::resource) (default []))
(defproperty ImageResourceList ResourceList (tag ::image))

(defproperty Vec3 t/Vec3
  (default [0.0 0.0 0.0]))

(defproperty Color Vec3
  (tag :color))
