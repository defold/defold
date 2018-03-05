(ns editor.properties
  (:require [clojure.set :as set]
            [clojure.string :as str]
            [cognitect.transit :as transit]
            [camel-snake-kebab :as camel]
            [dynamo.graph :as g]
            [util.murmur :as murmur]
            [editor.types :as t]
            [editor.math :as math]
            [editor.protobuf :as protobuf]
            [editor.core :as core]
            [schema.core :as s]
            [util.id-vec :as iv])
  (:import [java.util StringTokenizer]
           [javax.vecmath Quat4d Point3d Matrix4d Vector3d]))

(set! *warn-on-reflection* true)

(defn user-name->key [name]
  (->> name
    (str "__")
    keyword))

(defn key->user-name [k]
  (-> k
    name
    (subs 2)))

(defn ->spline [points]
  (->> points
    (sort-by first)
    vec))

(defn spline-cp [spline x]
  (let [x (min (max x 0.0) 1.0)
        [cp0 cp1] (some (fn [cps] (and (<= (ffirst cps) x) (<= x (first (second cps))) cps)) (partition 2 1 spline))]
    (when (and cp0 cp1)
      (let [[x0 y0 s0 t0] cp0
            [x1 y1 s1 t1] cp1
            dx (- x1 x0)
            t (/ (- x x0) (- x1 x0))
            d0 (* dx (/ t0 s0))
            d1 (* dx (/ t1 s1))
            y (math/hermite y0 y1 d0 d1 t)
            ty (/ (math/hermite' y0 y1 d0 d1 t) dx)
            l (Math/sqrt (+ 1.0 (* ty ty)))
            ty (/ ty l)
            tx (/ 1.0 l)]
        [x y tx ty]))))

(defprotocol Sampler
  (sample [this]))

(defn- curve-aabbs
  ([curve]
    (curve-aabbs curve nil))
  ([curve ids]
    (->> (iv/iv-filter-ids (:points curve) ids)
      (iv/iv-mapv (fn [[id v]] (let [[x y] v
                                     v [x y 0.0]]
                                 [id [v v]])))
      (into {}))))

(defn- curve-insert [curve positions]
  (let [spline (->> (:points curve)
                 (iv/iv-mapv second)
                 ->spline)
        points (mapv (fn [[x]] (-> spline
                                 (spline-cp x))) positions)]
    (update curve :points iv/iv-into points)))

(defn- curve-delete [curve ids]
  (let [include (->> (iv/iv-mapv identity (:points curve))
                  (sort-by (comp first second))
                  (map first)
                  (drop 1)
                  (butlast)
                  (into #{}))]
    (update curve :points iv/iv-remove-ids (filter include ids))))

(defn- curve-update [curve ids f]
  (let [ids (set ids)]
    (update curve :points (partial iv/iv-update-ids f) ids)))

(defn- curve-transform [curve ids ^Matrix4d transform]
  (let [p (Point3d.)
        cps (->> (:points curve) iv/iv-vals (mapv first) sort vec)
        margin 0.01
        limits (->> cps
                 (partition 3 1)
                 (mapv (fn [[min x max]] [x [(+ min margin) (- max margin)]]))
                 (into {}))
        limits (-> limits
                 (assoc (first cps) [0.0 0.0]
                        (last cps) [1.0 1.0]))]
    (curve-update curve ids (fn [v]
                              (let [[x y] v]
                                (.set p x y 0.0)
                                (.transform transform p)
                                (let [[min-x max-x] (limits x)
                                      x (max min-x (min max-x (.getX p)))
                                      y (.getY p)]
                                  (assoc v 0 x 1 y)))))))

(defrecord Curve [points]
  Sampler
  (sample [this] (second (first (iv/iv-vals points))))
  t/GeomCloud
  (t/geom-aabbs [this] (curve-aabbs this))
  (t/geom-aabbs [this ids] (curve-aabbs this ids))
  (t/geom-insert [this positions] (curve-insert this positions))
  (t/geom-delete [this ids] (curve-delete this ids))
  (t/geom-update [this ids f] (curve-update this ids f))
  (t/geom-transform [this ids transform] (curve-transform this ids transform)))

(defrecord CurveSpread [points spread]
  Sampler
  (sample [this] (second (first (iv/iv-vals points))))
  t/GeomCloud
  (t/geom-aabbs [this] (curve-aabbs this))
  (t/geom-aabbs [this ids] (curve-aabbs this ids))
  (t/geom-insert [this positions] (curve-insert this positions))
  (t/geom-delete [this ids] (curve-delete this ids))
  (t/geom-update [this ids f] (curve-update this ids f))
  (t/geom-transform [this ids transform] (curve-transform this ids transform)))

(defn ->curve [control-points]
  (Curve. (iv/iv-vec control-points)))

(def default-curve (->curve [[0 0 1 0]]))

(defn ->curve-spread [control-points spread]
  (CurveSpread. (iv/iv-vec control-points) spread))

(def default-curve-spread (->curve-spread [[0 0 1 0]] 0))

(defn curve-vals [curve]
  (iv/iv-vals (:points curve)))

(core/register-read-handler!
 (.getName Curve)
 (transit/read-handler
  (fn [{:keys [points]}]
    (->curve points))))

(core/register-write-handler!
 Curve
 (transit/write-handler
  (constantly (.getName Curve))
  (fn [^Curve c]
    {:points (curve-vals c)})))

(core/register-read-handler!
 (.getName CurveSpread)
 (transit/read-handler
  (fn [{:keys [points spread]}]
    (->curve-spread points spread))))

(core/register-write-handler!
 CurveSpread
 (transit/write-handler
  (constantly (.getName CurveSpread))
  (fn [^CurveSpread c]
    {:points (curve-vals c)
     :spread (:spread c)})))

(def default-curve (map->Curve {:points [{:x 0 :y 0 :t-x 1 :t-y 0}]}))

(def default-curve-spread (map->CurveSpread {:points [{:x 0 :y 0 :t-x 1 :t-y 0}] :spread 0}))

(defn- q-round [v]
  (let [f 10e6]
    (/ (Math/round (* v f)) f)))

(defn go-prop->str [value type]
  (case type
    :property-type-vector3 (apply format "%s, %s, %s" value)
    :property-type-vector4 (apply format "%s, %s, %s, %s" value)
    :property-type-quat (let [q (math/euler->quat value)]
                          (apply format "%s, %s, %s, %s" (map q-round (math/vecmath->clj q))))
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
            value (str->go-prop (:value prop) type)
            value (case type
                    (:property-type-number :property-type-url) [value]
                    :property-type-hash [(murmur/hash64 value)]
                    :property-type-boolean [(if value 1.0 0.0)]
                    :property-type-quat (-> value (math/euler->quat) (math/vecmath->clj))
                    value)
            [entry-key values-key] (type->entry-keys type)
            entry (cond-> {:key (:id prop)
                           :id (murmur/hash64 (:id prop))
                           :index (count (get decl values-key))}

                          (case type
                            (:property-type-vector3 :property-type-vector4 :property-type-quat) true
                            false)
                          (assoc :element-ids (mapv #(murmur/hash64 (str (:id prop) %))
                                                    [".x" ".y" ".z"]))

                          (case type
                            (:property-type-vector4 :property-type-quat) true
                            false)
                          (update :element-ids conj (murmur/hash64 (str (:id prop) ".w"))))]
        (recur (rest properties)
               (-> decl
                 (update entry-key conj! entry)
                 (update values-key append-values! value))))
      (into {} (map (fn [[k v]] [k (persistent! v)]) decl)))))

(defn- property-edit-type [property]
  (or (get property :edit-type)
      {:type (:type property)}))

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
                                                                                   (set/rename-keys {:value :original-value})
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
                                              :values (mapv (fn [{:keys [value]}]
                                                             (when-not (g/error? value)
                                                               value)) v)
                                              :errors  (mapv (fn [{:keys [value error]}]
                                                               (or error
                                                                   (when (g/error? value) value))) v)
                                              :edit-type (property-edit-type (first v))
                                              :label (:label (first v))
                                              :read-only? (reduce (fn [res read-only] (or res read-only)) false (map #(get % :read-only? false) v))}
                                        default-vals (mapv :original-value (filter #(contains? % :original-value) v))
                                        prop (if (empty? default-vals) prop (assoc prop :original-values default-vals))]
                                    [k prop]))
                                common-props))]
    {:properties coalesced
     :display-order (prune-display-order (first display-orders) (set (keys coalesced)))}))

(defn values [property]
  (let [f (get-in property [:edit-type :to-type] identity)]
    (mapv (fn [value default-value]
            (f (if-not (nil? value)
                 value
                 default-value)))
          (:values property)
          (:original-values property (repeat nil)))))

(defn- set-values [evaluation-context property values]
  (let [key (:key property)
        set-fn (get-in property [:edit-type :set-fn])
        from-fn (get-in property [:edit-type :from-type] identity)]
    (for [[node-id old-value new-value] (map vector (:node-ids property) (:values property) (map from-fn values))]
      (cond
        set-fn (set-fn evaluation-context node-id old-value new-value)
        (vector? key) (g/update-property node-id (first key) assoc-in (rest key) new-value)
        true (g/set-property node-id key new-value)))))

(defn keyword->name [kw]
  (-> kw
    name
    camel/->Camel_Snake_Case_String
    (clojure.string/replace "_" " ")
    clojure.string/trim))

(defn label
  [property]
  (or (:label property)
      (let [k (:key property)
           k (if (vector? k) (last k) k)]
       (keyword->name k))))

(defn read-only? [property]
  (:read-only? property))

(defn set-values!
  ([property values]
    (set-values! property values (gensym)))
  ([property values op-seq]
    (when (not (read-only? property))
      (let [evaluation-context (g/make-evaluation-context)]
        (g/transact
          (concat
            (g/operation-label (str "Set " (label property)))
            (g/operation-sequence op-seq)
            (set-values evaluation-context property values)))))))

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
  (and (contains? property :original-values) (not-every? nil? (:values property))))

(defn- error-seq [e]
  (tree-seq :causes :causes e))

(defn- error-messages [e]
  (distinct (keep :message (error-seq e))))

(defn error-message [e]
  (str/join "\n" (error-messages e)))

(defn error-aggregate [vals]
  (when-let [errors (seq (remove nil? (distinct (filter g/error? vals))))]
    (g/error-aggregate errors)))

(defn validation-message [property]
  (when-let [err (error-aggregate (:errors property))]
    {:severity (:severity err) :message (error-message err)}))

(defn clear-override! [property]
  (when (overridden? property)
    (g/transact
      (concat
        (g/operation-label (str "Clear " (label property)))
        (let [key (:key property)]
          (for [node-id (:node-ids property)]
            (g/clear-property node-id key)))))))

(defn round-scalar [n]
  (math/round-with-precision n 0.001))

(defn round-vec [v]
  (mapv round-scalar v))

(defn ->choicebox [vals]
  {:type :choicebox
   :options (map (juxt identity identity) vals)})

(defn ->pb-choicebox [cls]
  (let [values (protobuf/enum-values cls)]
    {:type :choicebox
     :options (map (juxt first (comp :display-name second)) values)}))

(defn vec3->vec2 [default-z]
  {:type t/Vec2
   :from-type (fn [[x y _]] [x y])
   :to-type (fn [[x y]] [x y default-z])})

(defn quat->euler []
  {:type t/Vec3
   :from-type (fn [v] (-> v math/euler->quat math/vecmath->clj))
   :to-type (fn [v] (round-vec (math/quat->euler (doto (Quat4d.) (math/clj->vecmath v)))))})
