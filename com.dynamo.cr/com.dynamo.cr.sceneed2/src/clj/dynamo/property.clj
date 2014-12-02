(ns dynamo.property
  (:require [schema.core :as s]
            [dynamo.types :as t]
            [internal.property :as ip])
  (:import [org.eclipse.core.resources IResource]))

(set! *warn-on-reflection* true)

(defmacro defproperty [name value-type & body-forms]
  (apply ip/def-property-type-descriptor name value-type body-forms))

(defproperty NonNegativeInt s/Int
  (validation (comp not neg?)))

(defproperty Resource IResource)

(defproperty Vec3 t/Vec3
  (default [0.0 0.0 0.0]))
