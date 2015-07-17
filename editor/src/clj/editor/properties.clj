(ns editor.properties
  (:require [camel-snake-kebab :as camel]
            [dynamo.graph :as g]))

(defn- property-edit-type [property]
  (or (get property :edit-type)
      {:type (g/property-value-type (:type property))}))

(defn- flatten-properties [properties]
  (let [pairs (seq properties)
        flat-pairs (filter #(not (contains? (second %) :link)) pairs)
        deep-pairs (filter #(contains? (second %) :link) pairs)]
    (reduce merge (into {} flat-pairs) (map #(flatten-properties (:link (second %))) deep-pairs))))

(defn coalesce [properties]
  (let [properties (mapv flatten-properties properties)
        node-count (count properties)
        ; Filter out invisible properties
        ; TODO - not= k :id is a hack since intrinsics are currently included in :properties output
        visible-props (mapcat (fn [p] (filter (fn [[k v]] (and (not= k :_id) (get v :visible true))) p)) properties)
        ; Filter out properties not common to *all* property sets
        ; Heuristic is to compare count and also type
        common-props (filter (fn [[k v]] (and (= node-count (count v)) (apply = (map property-edit-type v))))
                             (map (fn [[k v]] [k (mapv second v)]) (group-by first visible-props)))
        ; Coalesce into properties consumable by e.g. the properties view
        coalesced (into {} (map (fn [[k v]] [k {:key k
                                                :node-ids (mapv :node-id v)
                                                :values (mapv :value v)
                                                :edit-type (property-edit-type (first v))}])
                                common-props))]
    coalesced))

(defn set-value [property values]
  (for [[node-id value] (map vector (:node-ids property) values)
        :let [node (g/node-by-id node-id)]]
    (g/set-property node (:key property) value)))

(defn- niceify-label
  [k]
  (-> k
    name
    camel/->Camel_Snake_Case_String
    (clojure.string/replace "_" " ")))

(defn set-value! [property values]
  (g/transact
    (concat
      (g/operation-label (str "Set " (niceify-label (:key property))))
      (set-value property values))))

(defn unify-values [values]
  (loop [v0 (first values)
        values (rest values)]
   (if (not (empty? values))
     (let [v (first values)]
       (if (= v0 v)
         (recur v0 (rest values))
         nil))
     v0)))
