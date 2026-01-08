;; Copyright 2020-2026 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;;
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;;
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns editor.lua-parser
  (:require [clojure.java.io :as io]
            [editor.code.data :as data]
            [editor.math :as math]
            [editor.workspace :as workspace])
  (:import [com.dynamo.bob.pipeline LuaScanner LuaScanner$ParseError LuaScanner$Property LuaScanner$Property$Status LuaScanner$Result]
           [com.dynamo.gameobject.proto GameObject$PropertyType]
           [java.io Reader]
           [java.util.function Predicate]
           [javax.vecmath Quat4d Vector3d Vector4d]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn lua-info [workspace valid-resource-kind? code evaluation-context]
  (let [^LuaScanner$Result result (if (string? code)
                                    (^[String boolean Predicate] LuaScanner/parse code true valid-resource-kind?)
                                    (^[Reader boolean Predicate] LuaScanner/parse (io/reader code) true valid-resource-kind?))]
    (cond->
      {:code (.code result)
       :modules (.modules result)
       :script-properties
       (mapv (fn [^LuaScanner$Property property]
               (let [value (.value property)
                     is-resource (.isResource property)]
                 (-> {:status (condp identical? (.status property)
                                LuaScanner$Property$Status/OK :ok
                                LuaScanner$Property$Status/INVALID_ARGS :invalid-args
                                LuaScanner$Property$Status/INVALID_VALUE :invalid-value
                                LuaScanner$Property$Status/INVALID_LOCATION :invalid-location)}

                     (with-meta
                       {:cursor-range (data/->CursorRange
                                        (data/->Cursor (.startLine property) (.startColumn property))
                                        (data/->Cursor (.endLine property) (.endColumn property)))})
                     (cond->
                       is-resource
                       (assoc :resource-kind (.resourceKind property))

                       (.name property)
                       (assoc :name (.name property))

                       (.type property)
                       (assoc :type (condp identical? (.type property)
                                      GameObject$PropertyType/PROPERTY_TYPE_NUMBER :script-property-type-number
                                      GameObject$PropertyType/PROPERTY_TYPE_HASH (if is-resource :script-property-type-resource :script-property-type-hash)
                                      GameObject$PropertyType/PROPERTY_TYPE_URL :script-property-type-url
                                      GameObject$PropertyType/PROPERTY_TYPE_VECTOR3 :script-property-type-vector3
                                      GameObject$PropertyType/PROPERTY_TYPE_VECTOR4 :script-property-type-vector4
                                      GameObject$PropertyType/PROPERTY_TYPE_QUAT :script-property-type-quat
                                      GameObject$PropertyType/PROPERTY_TYPE_BOOLEAN :script-property-type-boolean))

                       (some? value)
                       (assoc :value (if (and is-resource value)
                                       (workspace/resolve-workspace-resource workspace value evaluation-context)
                                       (condp instance? value
                                         Vector3d (math/vecmath->clj value)
                                         Vector4d (math/vecmath->clj value)
                                         Quat4d (math/quat->euler value)
                                         value)))))))
             (.properties result))}
      (not (.success result))
      (assoc :errors (mapv (fn [^LuaScanner$ParseError error]
                             {:message (.message error)
                              :cursor-range (data/->CursorRange
                                              (data/->Cursor (.startLine error) (.startColumn error))
                                              (data/->Cursor (.endLine error) (.endColumn error)))})
                           (.errors result))))))
