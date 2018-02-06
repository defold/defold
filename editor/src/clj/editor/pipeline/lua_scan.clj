(ns editor.pipeline.lua-scan
  (:require [editor.math :as math]
            [editor.protobuf :as protobuf]
            [editor.workspace :as workspace])
  (:import [com.defold.editor.pipeline LuaScanner  LuaScanner$Property LuaScanner$Property$Status]))

(set! *warn-on-reflection* true)

(defn src->modules [source]
  (LuaScanner/scan source))

(defn- status->clj [^LuaScanner$Property$Status s]
  (cond
    (= LuaScanner$Property$Status/OK s) :ok
    (= LuaScanner$Property$Status/INVALID_ARGS s) :invalid-args
    (= LuaScanner$Property$Status/INVALID_VALUE s) :invalid-value))

(defn- value->clj [base-resource type value]
  (case type
    (:property-type-vector3 :property-type-vector4) (math/vecmath->clj value)
    :property-type-quat (math/quat->euler value)
    :property-type-resource (workspace/resolve-resource base-resource value)
    value))

(defn- prop->clj [base-resource ^LuaScanner$Property property]
  (let [status (status->clj (.status property))
        type (some-> (.type property)
               (protobuf/pb-enum->val))]
    {:name (.name property)
     :type type
     :sub-type (.subType property)
     :raw-value (.rawValue property)
     :value (when (= status :ok)
              (value->clj base-resource type (.value property)))
     :line (.line property)
     :status status}))

(defn src->properties [base-resource source]
  (let [properties (LuaScanner/scanProperties source)]
    (mapv (partial prop->clj base-resource) properties)))
