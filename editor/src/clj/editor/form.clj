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

(ns editor.form
  (:require [dynamo.graph :as g]
            [editor.util :as util])
  (:import [java.net URI]))

(set! *warn-on-reflection* true)

(defn- ensure-does-not-transact [ret f]
  (when (g/tx-result? ret)
    (throw
      (IllegalStateException.
        (str "Invalid implementation of form-ops\nFunction: "
             (Compiler/demunge (.getName (class f)))
             "\nIt should return transaction steps instead of performing a transaction. Use, e.g.:\n- `g/set-property` instead of `g/set-property!`\n- `g/clear-property` instead of `g/clear-property!`"))))
  ret)

(defn set-value
  "Returns transaction steps for setting the value"
  [{:keys [user-data set]} path value]
  (ensure-does-not-transact (set user-data path value) set))

(defn can-clear? [{:keys [clear]}]
  (not (nil? clear)))

(defn clear-value
  "Returns transaction steps for clearing the value"
  [{:keys [user-data clear]} path]
  (ensure-does-not-transact (clear user-data path) clear))

(def ^:private type-defaults
  {:table []
   :string ""
   :resource nil
   :file ""
   :directory ""
   :url (URI. "https://example.com")
   :boolean false
   :integer 0
   :number 0.0
   :vec4 (vector-of :double 0.0 0.0 0.0 0.0)
   :mat4 (vector-of :double
           1.0 0.0 0.0 0.0
           0.0 1.0 0.0 0.0
           0.0 0.0 1.0 0.0
           0.0 0.0 0.0 1.0)
   :2panel [],
   :list []})

(defn has-default? [field-info]
  (or (contains? field-info :default)
      (contains? type-defaults (:type field-info))))

(defn field-default [field-info]
  (if (contains? field-info :default)
    (:default field-info)
    (type-defaults (:type field-info))))

(defn optional-field?
  "Whether this field can be cleared"
  [field-info]
  (:optional field-info))

(defn field-defaults [fields]
  (let [required-fields (remove optional-field? fields)]
    (when (every? has-default? required-fields)
      (reduce (fn [val field]
                (assoc-in val (:path field) (field-default field)))
              {}
              required-fields))))

(defn section-defaults [section]
  (field-defaults (:fields section)))

(defn table-row-defaults [table-field-info]
  (field-defaults (:columns table-field-info)))

(defn form-defaults [form]
  (let [sections-defaults (map section-defaults (:sections form))]
    (when-not (some nil? sections-defaults)
      (reduce merge
              {}
              sections-defaults))))

(defn two-panel-defaults [{:keys [panel-key panel-form panel-form-fn] :as _panel-field-info}]
  (let [panel-key-defaults (field-defaults [panel-key])
        panel-form-defaults (form-defaults (if panel-form-fn
                                             (panel-form-fn nil)
                                             panel-form))]
    (when (and panel-key-defaults panel-form-defaults)
      (merge panel-key-defaults panel-form-defaults))))

(defn update-section-setting [section path f]
  (if-let [index (first (util/positions #(= path (:path %)) (get section :fields)))]
    (update-in section [:fields index] f)
    section))

(defn update-form-setting [form-data path f]
  (update form-data :sections (fn [section] (mapv #(update-section-setting % path f) section))))
