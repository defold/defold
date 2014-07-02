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
