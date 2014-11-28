(ns dynamo.property
  (:require [schema.core :as s]
            [dynamo.types :as t]
            [internal.property :as ip])
  (:import [org.eclipse.core.resources IResource]))

(set! *warn-on-reflection* true)

(defmacro defproperty [name value-type & body-forms]
  (apply ip/def-property-type-descriptor name value-type body-forms))

(defproperty Bool s/Bool)

(defproperty Keyword s/Keyword)

(defproperty Str s/Str
  (default ""))

(defproperty dynamo.property/Long java.lang.Long
  (default 0))

(defproperty dynamo.property/Double java.lang.Double
  (default 0.0))

(defproperty Vec3 t/Vec3
  (default [0.0 0.0 0.0]))

(defproperty Resource IResource)
