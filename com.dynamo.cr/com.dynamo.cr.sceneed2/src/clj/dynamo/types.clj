(ns dynamo.types
  "Schema and type definitions. Refer to Prismatic's schema.core for s/* definitions."
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
       {#'as-schema            "applies schema metadata to x."
        #'has-schema?          "true if v has defined schema. That is, metadata includes a schema key."
        #'Icon                 "*schema* - schema for the representation of an Icon as s/Str"
        #'NodeRef              "*schema* - schema for the representation of a node reference as s/Int"
        #'number               "returns a schema requiring s/Num and options"
        #'string               "returns a schema requiring s/Str and options"
        #'icon                 "returns a schema requiring [[Icon]] and options"
        #'resource             "returns a schema requiring s/Str with default \"\""
        #'texture-image        "returns a schema requiring a byte array and opts"
        #'non-negative-integer "returns a schema requiring a [[number]] with default 0"
        #'isotropic-scale      "returns a schema requiring a [[number]] with default 1.0"
        }]
  (alter-meta! v assoc :doc doc))