(ns editor.form
  (:require [dynamo.graph :as g]))

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

(defn has-explicit-default [field-info]
  (contains? field-info :default))

(defn field-default [field-info]
  (if (has-explicit-default field-info)
    (:default field-info)
    (type-defaults (:type field-info))))

(defn field-defaults [fields]
  (when (every? #(not (nil? (field-default %))) fields)
    (reduce (fn [val field]
              (assoc-in val (:path field) (field-default field)))
            {}
            fields)))

(defn section-defaults [section]
  (field-defaults (:fields section)))

(defn table-row-defaults [table-field-info]
  (field-defaults (:columns table-field-info)))

(defn form-defaults [form]
  (let [s-defaults (section-defaults (:sections form))]
    (when-not (some nil? s-defaults)
      (reduce merge
              {}
              (map section-defaults (:sections form))))))


