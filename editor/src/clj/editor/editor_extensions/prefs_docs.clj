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

(ns editor.editor-extensions.prefs-docs
  (:require [clojure.string :as string]
            [editor.editor-extensions.coerce :as coerce]
            [editor.editor-extensions.ui-docs :as ui-docs]
            [util.coll :as coll]
            [util.eduction :as e]))

(set! *warn-on-reflection* true)

(defn- make-default-prop [type-string]
  (ui-docs/make-prop :default
                     ;; instead of coercing here, we produce the schema, and then
                     ;; coerce the default according to that schema
                     :coerce coerce/untouched
                     :types [type-string]
                     :doc "default value"))

(defn editor-script-defined-schema? [x]
  (= :schema (:type (meta x))))

(def schema-coercer
  (coerce/wrap-with-pred coerce/userdata editor-script-defined-schema? "is not a schema"))

(definline allowed-keyword-character? [ch]
  `(or (Character/isLetterOrDigit (char ~ch))
       (= \- ~ch)
       (= \. ~ch)
       (= \_ ~ch)
       (= \+ ~ch)))

(defn- edn-serializable-keyword-name? [^String s]
  (let [n (.length s)]
    (and (pos? n)
         (loop [i 0]
           (cond
             (= i n) true
             (allowed-keyword-character? (.charAt s i)) (recur (inc i))
             :else false)))))

(def serializable-keyword-coercer
  (-> coerce/string
      (coerce/wrap-with-pred edn-serializable-keyword-name? "is not a valid keyword")
      (coerce/wrap-transform keyword)))

(def schema-components
  (let [scope-prop (ui-docs/make-prop :scope
                                      :coerce (coerce/enum :global :project)
                                      :types ["string"]
                                      :doc (ui-docs/doc-with-ul-options "preference scope"
                                                                        ["<code>editor.prefs.SCOPE.GLOBAL</code>: same preference value is used in every project on this computer"
                                                                         "<code>editor.prefs.SCOPE.PROJECT</code>: a separate preference value per project"]))]
    [(ui-docs/component
       "boolean"
       :description "boolean schema"
       :props [(make-default-prop "boolean")
               scope-prop])
     (ui-docs/component
       "string"
       :description "string schema"
       :props [(make-default-prop "string")
               scope-prop])
     (ui-docs/component
       "password"
       :description "password schema\n\nA password is a string that is encrypted when stored in a preference file"
       :props [(make-default-prop "string")
               scope-prop])
     (ui-docs/component
       "keyword"
       :description "keyword schema\n\nA keyword is a short string that is interned within the editor runtime, useful e.g. for identifiers"
       :props [(make-default-prop "string")
               scope-prop])
     (ui-docs/component
       "integer"
       :description "integer schema"
       :props [(make-default-prop "integer")
               scope-prop])
     (ui-docs/component
       "number"
       :description "floating-point number schema"
       :props [(make-default-prop "number")
               scope-prop])
     (ui-docs/component
       "array"
       :description "array schema"
       :props [(ui-docs/make-prop :item
                                  :required true
                                  :coerce schema-coercer
                                  :types ["schema"]
                                  :doc "array item schema")
               (make-default-prop "item[]")
               scope-prop])
     (ui-docs/component
       "set"
       :description "set schema\n\nSet is represented as a lua table with <code>true</code> values"
       :props [(ui-docs/make-prop :item
                                  :required true
                                  :coerce schema-coercer
                                  :types ["schema"]
                                  :doc "set item schema")
               (make-default-prop "table&lt;item, true&gt;")
               scope-prop])
     (ui-docs/component
       "object"
       :description "heterogeneous object schema"
       :props [(ui-docs/make-prop :properties
                                  :required true
                                  :coerce (coerce/map-of serializable-keyword-coercer schema-coercer)
                                  :types ["table&lt;string, schema&gt;"]
                                  :doc "a table from property key (string) to value schema")
               (make-default-prop "table")
               scope-prop])
     (ui-docs/component
       "object_of"
       :description "homogeneous object schema"
       :props [(ui-docs/make-prop :key
                                  :required true
                                  :coerce schema-coercer
                                  :types ["schema"]
                                  :doc "table key schema")
               (ui-docs/make-prop :val
                                  :required true
                                  :coerce schema-coercer
                                  :types ["schema"]
                                  :doc "table value schema")
               (make-default-prop "table")
               scope-prop])
     (ui-docs/component
       "enum"
       :description "enum value schema"
       :props [(ui-docs/make-prop :values
                                  :required true
                                  :coerce (coerce/vector-of
                                            (coerce/one-of
                                              coerce/null
                                              coerce/boolean
                                              coerce/number
                                              coerce/string)
                                            :min-count 1
                                            :distinct true)
                                  :types ["any[]"]
                                  :doc "allowed values, must be scalar (nil, boolean, number or string)")
               (make-default-prop "any")
               scope-prop])
     (ui-docs/component
       "one_of"
       :description "one of schema"
       :props [(ui-docs/make-prop :schemas
                                  :required true
                                  :coerce (coerce/vector-of schema-coercer :min-count 2)
                                  :types ["schema[]"]
                                  :doc "alternative schemas")
               (make-default-prop "any")
               scope-prop])
     (ui-docs/component
       "tuple"
       :description "tuple schema\n\nA tuple is a fixed-length array where each item has its own defined type"
       :props [(ui-docs/make-prop :items
                                  :required true
                                  :coerce (coerce/vector-of schema-coercer :min-count 2)
                                  :types ["schema[]"]
                                  :doc "schemas for the items")
               (make-default-prop "any[]")
               scope-prop])]))

(defn- schema-component->script-doc [{:keys [name props description]}]
  (let [[req opt] (coll/separate-by :required props)
        optional (coll/empty? req)]
    {:name (str "editor.prefs.schema." name)
     :type :function
     :description description
     :parameters [{:name (if optional "[opts]" "opts")
                   :types ["table"]
                   :doc (str (when-not optional
                               (str "Required opts:\n"
                                    (ui-docs/props-doc-html req)
                                    "\n\n"))
                             "Optional opts:\n"
                             (ui-docs/props-doc-html opt))}]
     :returnvalues [{:name "value"
                     :types ["schema"]
                     :doc "Prefs schema"}]}))

(def enums
  {:scope [:global :project]})

(defn script-docs []
  (vec
    (e/concat
      [{:name "editor.prefs"
        :type :module
        :description "Reading and writing editor preferences"}
       {:name "editor.prefs.get"
        :type :function
        :parameters [{:name "key"
                      :types ["string"]
                      :doc "dot-separated preference key path"}]
        :returnvalues [{:name "value"
                        :types ["any"]
                        :doc "current pref value or default if a schema for the key path exists, nil otherwise"}]
        :description "Get preference value\n\nThe schema for the preference value should be defined beforehand."}
       {:name "editor.prefs.is_set"
        :type :function
        :parameters [{:name "key"
                      :types ["string"]
                      :doc "dot-separated preference key path"}]
        :returnvalues [{:name "value"
                        :types ["boolean"]
                        :doc "flag indicating if the value is explicitly set"}]
        :description "Check if preference value is explicitly set\n\nThe schema for the preference value should be defined beforehand."}
       {:name "editor.prefs.set"
        :type :function
        :parameters [{:name "key"
                      :types ["string"]
                      :doc "dot-separated preference key path"}
                     {:name "value"
                      :types ["any"]
                      :doc "new pref value to set"}]
        :description "Set preference value\n\nThe schema for the preference value should be defined beforehand."}
       {:name "editor.prefs.schema"
        :type :module
        :description "Schema for defining preferences"}]
      (e/map schema-component->script-doc schema-components)
      (e/mapcat (fn [[id-kw vs]]
                  (let [id (str "editor.prefs." (ui-docs/->screaming-snake-case id-kw))]
                    (e/concat
                      [{:name id
                        :type :module
                        :description (str "Constants for "
                                          (string/replace (name id-kw) \- \space)
                                          " enums")}]
                      (e/map
                        (fn [v-kw]
                          {:name (str id "." (ui-docs/->screaming-snake-case v-kw))
                           :type :constant
                           :description (format "`\"%s\"`" (name v-kw))})
                        vs))))
                enums))))