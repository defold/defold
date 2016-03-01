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
   :resource ""
   :boolean false
   :integer 0
   :number 0.0
   :vec4 [0.0 0.0 0.0 0.0]
   :2panel []})

(defn field-default [field-info]
  (get field-info :default (type-defaults (:type field-info))))

(defn field-defaults [fields]
  (let [required-fields (remove :optional fields)]
    (when (every? #(not (nil? (field-default %))) required-fields)
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
