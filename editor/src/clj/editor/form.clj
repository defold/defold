(ns editor.form
  (:require [dynamo.graph :as g]))

(set! *warn-on-reflection* true)

(defn set-value! [{:keys [user-data set]} path value]
  (set user-data path value))

(defn can-clear? [{:keys [clear]}]
  (not (nil? clear)))

(defn clear-value! [{:keys [user-data clear]} path]
  (clear user-data path))

(def ^:private type-defaults
  {:table []
   :string ""
   :resource nil
   :boolean false
   :integer 0
   :number 0.0
   :vec4 [0.0 0.0 0.0 0.0]
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

(defn two-panel-defaults [panel-field-info]
  (let [panel-key-defaults (field-defaults [(:panel-key panel-field-info)])
        panel-form-defaults (form-defaults (:panel-form panel-field-info))]
    (when (and panel-key-defaults panel-form-defaults)
      (merge panel-key-defaults panel-form-defaults))))
