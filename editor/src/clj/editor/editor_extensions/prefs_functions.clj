;; Copyright 2020-2025 The Defold Foundation
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

(ns editor.editor-extensions.prefs-functions
  (:require [clojure.spec.alpha :as s]
            [clojure.string :as string]
            [editor.editor-extensions.coerce :as coerce]
            [editor.editor-extensions.prefs-docs :as prefs-docs]
            [editor.editor-extensions.runtime :as rt]
            [editor.editor-extensions.ui-docs :as ui-docs]
            [editor.prefs :as prefs]
            [internal.util :as iutil]
            [util.coll :as coll])
  (:import [org.luaj.vm2 LuaError]))

(set! *warn-on-reflection* true)

(defn- map-keys->set [m]
  (persistent!
    (reduce-kv
      (fn [acc k _]
        (conj! acc k))
      (transient #{})
      m)))

(def ^:private number->double-coercer
  (coerce/wrap-transform coerce/number double))

(defn- schema->coercer [schema]
  (case (:type schema)
    :any coerce/any
    :boolean coerce/boolean
    :string coerce/string
    :password coerce/string
    :locale coerce/string
    :keyword prefs-docs/serializable-keyword-coercer
    :integer coerce/integer
    :number number->double-coercer
    :array (coerce/vector-of (schema->coercer (:item schema)))
    :set (coerce/wrap-transform
           (coerce/map-of (schema->coercer (:item schema)) (coerce/const true))
           map-keys->set)
    :object-of (coerce/map-of (schema->coercer (:key schema)) (schema->coercer (:val schema)))
    :object (coerce/hash-map :opt (coll/pair-map-by key (comp schema->coercer val) (:properties schema))
                             :extra-keys false)
    :enum (apply coerce/enum (:values schema))
    :tuple (apply coerce/tuple (mapv schema->coercer (:items schema)))))

(def ^:private schema-lua-env
  (coll/pair-map-by
    :name
    (fn [{:keys [name props]}]
      (let [schema-type (keyword (string/replace name \_ \-))
            string-representation (str "editor.prefs.schema." name "(...)")
            {req true opt false} (iutil/group-into {} {} :required (coll/pair-fn :name :coerce) props)
            coercer (coerce/hash-map :req req :opt opt)
            make-schema-lua-value
            (fn make-schema-lua-value [rt m]
              (let [schema (assoc m :type schema-type)
                    schema (if-let [default-lua-value (:default schema)]
                             (assoc schema :default (rt/->clj rt (schema->coercer schema) default-lua-value))
                             schema)]
                (s/assert ::prefs/schema schema)
                (rt/wrap-userdata (vary-meta schema assoc :type :schema) string-representation)))]
        (if req
          (rt/lua-fn schema-factory [{:keys [rt]} lua-arg]
            (make-schema-lua-value rt (rt/->clj rt coercer lua-arg)))
          ;; all keys are optional
          (rt/lua-fn schema-factory
            ([{:keys [rt]}]
             (make-schema-lua-value rt {}))
            ([{:keys [rt]} lua-arg]
             (make-schema-lua-value rt (rt/->clj rt coercer lua-arg)))))))
    prefs-docs/schema-components))

(defn parse-dot-separated-path [^String s]
  (let [n (.length s)
        non-empty! (fn [^String s]
                     (if (.isEmpty s)
                       (throw (LuaError. "Key path element cannot be empty"))
                       s))]
    (loop [acc (transient [])
           from 0
           to 0]
      (if (= n to)
        (persistent! (conj! acc (keyword (non-empty! (.substring s from to)))))
        (let [ch (.charAt s to)]
          (cond
            (= ch \.)
            (recur (conj! acc (keyword (non-empty! (.substring s from to)))) (inc to) (inc to))

            (prefs-docs/allowed-keyword-character? ch)
            (recur acc from (inc to))

            :else
            (throw (LuaError. (str "Invalid identifier character: '" ch "'")))))))))

(defn- make-get-fn [prefs]
  (rt/lua-fn ext-get
    [{:keys [rt]} lua-path]
    (let [path-str (rt/->clj rt coerce/string lua-path)
          path (parse-dot-separated-path path-str)]
      (try
        (prefs/get prefs path)
        (catch Exception e
          (case (::prefs/error (ex-data e))
            :path (throw (LuaError. (str "No schema defined for prefs path '" path-str "'")))
            (throw e)))))))

(defn- make-is-set-fn [prefs]
  (rt/lua-fn ext-is-set
    [{:keys [rt]} lua-path]
    (let [path-str (rt/->clj rt coerce/string lua-path)
          path (parse-dot-separated-path path-str)]
      (try
        (prefs/set? prefs path)
        (catch Exception e
          (case (::prefs/error (ex-data e))
            :path (throw (LuaError. (str "No schema defined for prefs path '" path-str "'")))
            (throw e)))))))

(defn- make-set-fn [prefs]
  (rt/lua-fn ext-set [{:keys [rt]} lua-path lua-value]
    (let [path-str (rt/->clj rt coerce/string lua-path)
          path (parse-dot-separated-path path-str)]
      (try
        (let [schema (prefs/schema prefs path)
              coercer (schema->coercer schema)
              value (rt/->clj rt coercer lua-value)]
          (prefs/set! prefs path value))
        (catch Exception e
          (let [data (ex-data e)]
            (case (::prefs/error data)
              ;; we don't catch :value errors here because coercer already
              ;; verifies the right data shape and presents Lua-specific error
              ;; messages to the user
              :path (throw (LuaError. (str "No schema defined for prefs path '" path-str "'")))
              (throw e))))))))

(def ^:private enum-env
  (into {}
        (map (fn [[id values]]
               [(ui-docs/->screaming-snake-case id)
                (coll/pair-map-by ui-docs/->screaming-snake-case coerce/enum-lua-value-cache values)]))
        prefs-docs/enums))

(def schema-definition-coercer
  (coerce/one-of
    ;; simple
    prefs-docs/schema-coercer
    ;; easy: strings are dot-separated paths
    (coerce/map-of coerce/string prefs-docs/schema-coercer)))

(defn- fail-on-conflict [_ _ path]
  (throw (LuaError. (str "Conflicting schemas defined at path '" (string/join "." (map name path)) "'"))))

(defn lua-schema-definition->schema [rt lua-schema-definition]
  (let [m (rt/->clj rt schema-definition-coercer lua-schema-definition)]
    (if (prefs-docs/editor-script-defined-schema? m)
      m
      (reduce-kv
        (fn [acc path-str schema]
          (let [path (parse-dot-separated-path path-str)
                schema (loop [i (dec (count path))
                              acc schema]
                         (if (neg? i)
                           acc
                           (recur (dec i) {:type :object :properties {(path i) acc}})))]
            (prefs/merge-schemas fail-on-conflict acc schema)))
        {:type :object :properties {}}
        m))))

(defn env [prefs]
  (merge
    {"get" (make-get-fn prefs)
     "set" (make-set-fn prefs)
     "is_set" (make-is-set-fn prefs)
     "schema" schema-lua-env}
    enum-env))
