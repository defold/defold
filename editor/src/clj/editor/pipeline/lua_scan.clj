(ns editor.pipeline.lua-scan
  (:require [clojure.java.io :as io]
            [editor.math :as math]
            [editor.protobuf :as protobuf])
  (:import [com.defold.editor.pipeline LuaScanner  LuaScanner$Property LuaScanner$Property$Status]))

(set! *warn-on-reflection* true)

(defn src->modules [source]
  (LuaScanner/scan source))

(defn- status->clj [^LuaScanner$Property$Status s]
  (cond
    (= LuaScanner$Property$Status/OK s) :ok
    (= LuaScanner$Property$Status/INVALID_ARGS s) :invalid-args
    (= LuaScanner$Property$Status/INVALID_VALUE s) :invalid-value))

(defn- value->clj [type value]
  (cond
    (#{:property-type-vector3 :property-type-vector4} type) (math/vecmath->clj value)
    (= :property-type-quat type) (math/quat->euler value)
    :else value))

(defn- prop->clj [^LuaScanner$Property property]
  (let [status (status->clj (.status property))
        type (some-> (.type property)
               (protobuf/pb-enum->val))]
    {:name (.name property)
     :type type
     :raw-value (.rawValue property)
     :value (when (= status :ok)
              (value->clj type (.value property)))
     :line (.line property)
     :status status}))

(defn src->properties [source]
  (let [properties (LuaScanner/scanProperties source)]
    (mapv prop->clj properties)))
