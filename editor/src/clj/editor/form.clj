(ns editor.form
  (:require [dynamo.graph :as g]))

;; FIXME: copy-paste from properties.clj.
(defn- dissoc-in
  "Dissociates an entry from a nested associative structure returning a new
  nested structure. keys is a sequence of keys. Any empty maps that result
  will not be present in the new structure."
  [m [k & ks :as keys]]
  (if ks
    (if-let [nextmap (get m k)]
      (let [newmap (dissoc-in nextmap ks)]
        (if (empty? newmap)
          (dissoc m k)
          (assoc m k newmap)))
      m)
    (dissoc m k)))

;;; sample form data
#_{
 :ctxt [node-id [property & path-within-property]]
 :sections
 [
  {
   :section "project"
   :help "..."
   :title "..."
   :fields
   [
    {
     :field "name"
     :help "..."
     :type String
     :default "<my name>"
     }
    ]
   }
  {
   :section "sound"
   :help "..."
   :title "..."
   :fields
   [
    {
     :field "f"
     :help "freq"
     :type g/Int
     :default 22050
     }
    ]
   }
  ]
 :values
 {
  "project" {"name" "my fps"}
  "sound" {"f" 44100}
  }
}

(defn set-value! [target section field value]
  (let [[node-id [property & path]] target]
    (g/update-property! node-id property assoc-in (concat path [section field]) value)))

(defn clear-value! [target section field]
  (let [[node-id [property & path]] target]
    (g/update-property! node-id property dissoc-in (concat path [section field]))))

(defn- field-default [{:keys [field default]}]
  [field default])

(defn- section-defaults [{:keys [section fields]}]
  [section (into {} (map field-default fields))])

(defn- default-values [{:keys [sections]}]
  (into {} (map section-defaults sections)))

(defn- annotate-field-values [field-values k v]
  (into {} (map (fn [[field value]] [field {:value value k v}]) field-values)))

(defn- annotate-values [values k v]
  (into {} (map (fn [[section field-values]]
                  [section (annotate-field-values field-values k v)])
                values)))

(defn field-values-and-defaults [form-data]
  (let [defaults (annotate-values (default-values form-data) :source :default)
        explicit (annotate-values (:values form-data) :source :explicit)]
    (merge-with merge defaults explicit)))

