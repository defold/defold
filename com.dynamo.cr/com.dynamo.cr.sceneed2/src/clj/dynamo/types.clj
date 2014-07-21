(ns dynamo.types
  (:require [schema.core :as s]))

; ----------------------------------------
; Functions to create basic value types
; ----------------------------------------

(defn as-schema   [x] (with-meta x {:schema true}))
(defn has-schema? [v] (and (fn? v) (:schema (meta v))))

(def Icon s/Str)
(def NodeRef s/Int)

(defn number               [& {:as opts}] (merge {:schema s/Num} opts))
(defn string               [& {:as opts}] (merge {:schema s/Str :default ""} opts))
(defn icon                 [& {:as opts}] (merge {:schema Icon} opts))
(defn resource             [& {:as opts}] (merge {:schema s/Str :default ""} opts))
(defn texture-image        [& {:as opts}] (merge {:schema bytes} opts))
(defn non-negative-integer [& {:as opts}] (merge (number :default 0) opts))
(defn isotropic-scale      [& {:as opts}] (merge (number :default 1.0) opts))

(doseq [[v doc]
       {*ns*                   "Schema and type definitions. Refer to Prismatic's schema.core for s/* definitions."
        #'as-schema            "applies schema metadata to x."
        #'has-schema?          "true if v has defined schema. That is, metadata includes a schema key."
        #'Icon                 "*schema* - schema for the representation of an Icon as s/Str"
        #'NodeRef              "*schema* - schema for the representation of a node reference as s/Int"
        #'number               "creates a property definition for a numeric property"
        #'string               "creates a property definition for a string property"
        #'icon                 "creates a property definition for an [[Icon]] property"
        #'resource             "creates a property definition for a resource (file)"
        #'texture-image        "creates a property definition for a byte array property to be used as an image"
        #'non-negative-integer "creates a property definition for a numeric property that must be zero or greater"
        #'isotropic-scale      "creates a property definition for a uniform scaling factor"}]
  (alter-meta! v assoc :doc doc))