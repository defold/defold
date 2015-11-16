(ns editor.properties
  (:require [clojure.set :as set]
            [clojure.string :as str]
            [cognitect.transit :as transit]
            [camel-snake-kebab :as camel]
            [dynamo.graph :as g]
            [editor.types :as t]
            [editor.math :as math]
            [editor.protobuf :as protobuf]
            [editor.validation :as validation]
            [editor.core :as core])
  (:import [java.util StringTokenizer]
           [javax.vecmath Quat4d]))

(defprotocol Sampler
  (sample [this]))

(defrecord Curve [points]
  Sampler
  (sample [this] (:y (first points))))

(defrecord CurveSpread [points spread]
  Sampler
  (sample [this] (:y (first points))))

(core/register-read-handler!
 "curve"
 (transit/read-handler
  (fn [{:keys [points]}]
    (Curve. points))))

(core/register-write-handler!
 Curve
 (transit/write-handler
  (constantly "curve")
  (fn [^Curve c]
    {:points (:points c)})))

(core/register-read-handler!
 "curve-spread"
 (transit/read-handler
  (fn [{:keys [points spread]}]
    (CurveSpread. points spread))))

(core/register-write-handler!
 CurveSpread
 (transit/write-handler
  (constantly "curve-spread")
  (fn [^CurveSpread c]
    {:points (:points c)
     :spread (:spread c)})))

(def default-curve (map->Curve {:points [{:x 0 :y 0 :t-x 1 :t-y 0}]}))

(def default-curve-spread (map->CurveSpread {:points [{:x 0 :y 0 :t-x 1 :t-y 0}] :spread 0}))

(def go-prop-type->clj-type {:property-type-number g/Num
                             :property-type-hash String
                             :property-type-url String
                             :property-type-vector3 t/Vec3
                             :property-type-vector4 t/Vec4
                             :property-type-quat t/Vec3
                             :property-type-bool g/Bool})

(defn- q-round [v]
  (let [f 10e6]
    (/ (Math/round (* v f)) f)))

(defn go-prop->str [value type]
  (case type
    :property-type-vector3 (apply format "%s,%s,%s" (map str value))
    :property-type-vector4 (apply format "%s,%s,%s,%s" (map str value))
    :property-type-quat (let [q (math/euler->quat value)]
                          (apply format "%s,%s,%s,%s" (map (comp str q-round) (math/vecmath->clj q))))
    (str value)))

(defn- parse-num [s]
  (num (Double/parseDouble s)))

(defn- tokenize! [^StringTokenizer tokenizer parse-fn]
  (-> tokenizer (.nextToken) (.trim) (parse-fn)))

(defn- parse-vec [s count]
  (let [tokenizer (StringTokenizer. s ",")]
    (loop [result []
           counter 0]
      (if (< counter count)
        (recur (conj result (tokenize! tokenizer parse-num))
               (inc counter))
        result))))

(defn str->go-prop [s type]
  (case type
    :property-type-number (parse-num s)
    (:property-type-hash :property-type-url) s
    :property-type-vector3 (parse-vec s 3)
    :property-type-vector4 (parse-vec s 4)
    :property-type-quat (let [v (parse-vec s 4)
                              q (Quat4d. (double-array v))]
                          (math/quat->euler q))
    :property-type-boolean (Boolean/parseBoolean s)))

(defn- ->decl [keys]
  (into {} (map (fn [k] [k (transient [])]) keys)))

(def ^:private type->entry-keys {:property-type-number [:number-entries :float-values]
                                 :property-type-hash [:hash-entries :hash-values]
                                 :property-type-url [:url-entries :string-values]
                                 :property-type-vector3 [:vector3-entries :float-values]
                                 :property-type-vector4 [:vector4-entries :float-values]
                                 :property-type-quat [:quat-entries :float-values]
                                 :property-type-boolean [:bool-entries :float-values]})

(defn append-values! [values vs]
  (loop [vs vs
         values values]
    (if-let [v (first vs)]
      (recur (rest vs) (conj! values v))
      values)))

(defn properties->decls [properties]
  (loop [properties properties
         decl (->decl [:number-entries :hash-entries :url-entries :vector3-entries
                       :vector4-entries :quat-entries :bool-entries :float-values
                       :hash-values :string-values])]
    (if-let [prop (first properties)]
      (let [type (:type prop)
            value (:value prop)
            value (case type
                    (:property-type-number :property-type-url) [value]
                    :property-type-hash [(protobuf/hash64 value)]
                    :property-type-boolean [(if value 1.0 0.0)]
                    :property-type-quat (-> value (math/euler->quat) (math/vecmath->clj))
                    value)
            [entry-key values-key] (type->entry-keys type)
            entry {:key (:id prop)
                   :id (protobuf/hash64 (:id prop))
                   :index (count (get decl values-key))}]
        (recur (rest properties)
               (-> decl
                 (update entry-key conj! entry)
                 (update values-key append-values! value))))
      (into {} (map (fn [[k v]] [k (persistent! v)]) decl)))))

(defn- property-edit-type [property]
  (or (get property :edit-type)
      {:type (g/property-value-type (:type property))}))

(def ^:private links #{:link :override})

(defn category? [v]
  (and (sequential? v) (string? (first v))))

(defn- inject-display-order [display-order key injected-display-order]
  (let [injected-display-order (loop [keys injected-display-order
                                      result []]
                                 (if-let [k (first keys)]
                                   (let [result (if (category? k)
                                                  (apply conj result (rest k))
                                                  (conj result k))]
                                     (recur (rest keys) result))
                                   result))]
    (loop [v display-order
           result []]
      (if-let [k (first v)]
        (let [result (if (= k key)
                       (apply conj result injected-display-order)
                       (let [k (if (category? k)
                                 (inject-display-order k key injected-display-order)
                                 k)]
                         (conj result k)))]
          (recur (rest v) result))
        result))))

(defn- replace-display-order [display-order new-keys]
  (mapv (fn [k]
          (if (category? k)
            (vec (cons (first k) (replace-display-order (rest k) new-keys)))
            (new-keys k)))
        display-order))

(defn- flatten-properties [properties]
  (let [pairs (seq (:properties properties))
        flat-pairs (filter #(not-any? links (keys (second %))) pairs)
        link-pairs (filter #(contains? (second %) :link) pairs)
        link->props (into {} (map (fn [[k v]] [k (flatten-properties (:link v))]) link-pairs))
        override-pairs (filter #(contains? (second %) :override) pairs)
        override->props (into {} (map (fn [[k v]]
                                     (let [v (let [k (if (vector? k) k (vector k))
                                                 properties (flatten-properties (:override v))
                                                 new-keys (into {} (map #(do [% (conj k %)]) (keys (:properties properties))))]
                                               {:properties (into {} (map (fn [[o-k o-v]]
                                                                      (let [prop (-> o-v
                                                                                   (set/rename-keys {:value :default-value})
                                                                                   (assoc :node-id (:node-id v)
                                                                                          :value (get (:value v) o-k)))]
                                                                        [(conj k o-k) prop]))
                                                                   (:properties properties)))
                                                  :display-order (replace-display-order (:display-order properties) new-keys)})]
                                       [k v]))
                                   override-pairs))
        key->display-order (apply merge-with concat (mapv (fn [m] (into {} (map (fn [[k v]] [k (:display-order v)]) m))) [link->props override->props]))
        display-order (reduce (fn [display-order [key injected]] (inject-display-order display-order key injected))
                              (:display-order properties)
                              key->display-order)]
    {:properties (reduce merge
                         (into {} flat-pairs)
                         (concat
                           (map :properties (vals link->props))
                           (map :properties (vals override->props))))
     :display-order display-order}))

(defn- prune-display-order [display-order key-set]
  (loop [display-order display-order
         result []]
    (if-let [k (first display-order)]
      (let [result (if (category? k)
                     (let [keys (prune-display-order (rest k) key-set)]
                       (if (empty? keys)
                         result
                         (conj result (vec (cons (first k) keys)))))
                     (if (contains? key-set k)
                       (conj result k)
                       result))]
        (recur (rest display-order) result))
      result)))

(defn coalesce [properties]
  (let [properties (mapv flatten-properties properties)
        display-orders (mapv :display-order properties)
        properties (mapv :properties properties)
        node-count (count properties)
        ; Filter out invisible properties
        visible-props (mapcat (fn [p] (filter (fn [[k v]] (get v :visible true)) p)) properties)
        ; Filter out properties not common to *all* property sets
        ; Heuristic is to compare count and also type
        common-props (filter (fn [[k v]] (and (= node-count (count v)) (apply = (map property-edit-type v))))
                             (map (fn [[k v]] [k (mapv second v)]) (group-by first visible-props)))
        ; Coalesce into properties consumable by e.g. the properties view
        coalesced (into {} (map (fn [[k v]]
                                  (let [prop {:key k
                                              :node-ids (mapv :node-id v)
                                              :values (mapv :value v)
                                              :validation-problems (mapv :validation-problems v)
                                              :edit-type (property-edit-type (first v))
                                              :label (:label (first v))}
                                        default-vals (mapv :default-value (filter #(contains? % :default-value) v))
                                        prop (if (empty? default-vals) prop (assoc prop :default-values default-vals))]
                                    [k prop]))
                                common-props))]
    {:properties coalesced
     :display-order (prune-display-order (first display-orders) (set (keys coalesced)))}))

(defn values [property]
  (mapv (fn [value default-value validation-problem]
          (if-not (nil? validation-problem)
            (:value validation-problem)
            (if-not (nil? value)
              value
              default-value)))
        (:values property)
        (:default-values property (repeat nil))
        (:validation-problems property)))

(defn- set-values [property values]
  (let [key (:key property)]
    (for [[node-id value] (map vector (:node-ids property) values)]
      (if (vector? key)
        (g/update-property node-id (first key) assoc-in (rest key) value)
        (g/set-property node-id (:key property) value)))))

(defn label
  [property]
  (or (:label property)
      (let [k (:key property)
           k (if (vector? k) (last k) k)]
       (-> k
         name
         camel/->Camel_Snake_Case_String
         (clojure.string/replace "_" " ")))))

(defn set-values!
  ([property values]
    (set-values! property values (gensym)))
  ([property values op-seq]
    (g/transact
      (concat
        (g/operation-label (str "Set " (label property)))
        (g/operation-sequence op-seq)
        (set-values property values)))))

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

(defn unify-values [values]
  (loop [v0 (first values)
         values (rest values)]
    (if (not (empty? values))
      (let [v (first values)]
        (if (= v0 v)
          (recur v0 (rest values))
          nil))
      v0)))

(defn overridden? [property]
  (and (contains? property :default-values) (not-every? nil? (:values property))))

(defn validation-message [property]
  (when-let [err (validation/error-aggregate (:validation-problems property))]
    {:severity (:severity err) :message (validation/error-message err)}))

(defn clear-override! [property]
  (when (overridden? property)
    (g/transact
      (concat
        (g/operation-label (str "Set " (label property)))
        (let [key (:key property)]
          (for [node-id (:node-ids property)]
            (g/update-property node-id (first key) dissoc-in (rest key))))))))

(defn ->choicebox [vals]
  {:type :choicebox
   :options (zipmap vals vals)})

(defn ->pb-choicebox [cls]
  (let [options (protobuf/enum-values cls)]
    {:type :choicebox
     :options (zipmap (map first options)
                      (map (comp :display-name second) options))}))
