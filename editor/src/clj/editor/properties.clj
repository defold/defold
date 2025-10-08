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

(ns editor.properties
  (:require [camel-snake-kebab :as camel]
            [clojure.set :as set]
            [clojure.string :as string]
            [cognitect.transit :as transit]
            [dynamo.graph :as g]
            [editor.core :as core]
            [editor.graph-util :as gu]
            [editor.localization :as localization]
            [editor.math :as math]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.types :as t]
            [editor.util :as eutil]
            [editor.workspace :as workspace]
            [internal.util :as util]
            [schema.core :as s]
            [util.coll :as coll :refer [pair]]
            [util.defonce :as defonce]
            [util.eduction :as e]
            [util.fn :as fn]
            [util.id-vec :as iv]
            [util.murmur :as murmur]
            [util.text-util :as text-util])
  (:import [java.util StringTokenizer]
           [javax.vecmath Matrix4d Point3d Quat4d]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

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

(declare round-scalar round-scalar-float)

(defn spline-cp [spline ^double x]
  (let [x (min (max x 0.0) 1.0)
        [cp0 cp1] (some (fn [cps]
                          (and (<= ^double (ffirst cps) x)
                               (<= x ^double (first (second cps)))
                               cps))
                        (partition 2 1 spline))]
    (when (and cp0 cp1)
      (let [[^double x0 ^double y0 ^double s0 ^double t0] cp0
            [^double x1 ^double y1 ^double s1 ^double t1] cp1
            dx (- x1 x0)
            t (/ (- x x0) (- x1 x0))
            d0 (* dx (/ t0 s0))
            d1 (* dx (/ t1 s1))
            y (math/hermite y0 y1 d0 d1 t)
            ty (/ (math/hermite' y0 y1 d0 d1 t) dx)
            l (Math/sqrt (+ 1.0 (* ty ty)))
            ty (/ ty l)
            tx (/ 1.0 l)
            num-fn (if (math/float32? (first cp0))
                     round-scalar-float
                     round-scalar)]
        (-> (coll/empty-with-meta cp0)
            (conj (num-fn x))
            (conj (num-fn y))
            (conj (num-fn tx))
            (conj (num-fn ty)))))))

(defonce/protocol Sampler
  (sample [this])
  (sample-range [this]))

(defn- curve-aabbs
  ([curve]
   (curve-aabbs curve nil))
  ([curve ids]
   (let [points (if ids
                  (iv/iv-filter-ids (:points curve) ids)
                  (:points curve))
         id-vec-entries (iv/iv-entries points)
         [_id [cp-x]] (first id-vec-entries)
         zero (if (math/float32? cp-x)
                (float 0.0)
                0.0)]
     (into {}
           (map (fn [[id v]]
                  (let [[x y] v
                        v [x y zero]]
                    [id [v v]])))
           id-vec-entries))))

(defn- curve-insert [curve positions]
  (let [spline (-> curve :points iv/iv-vals ->spline)
        points (mapv (fn [[x]]
                       (spline-cp spline x))
                     positions)]
    (update curve :points iv/iv-into points)))

(defn- curve-delete [curve ids]
  (let [include (->> (:points curve)
                     (iv/iv-entries)
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
                    (into {}
                          (map (fn [[^double min x ^double max]]
                                 (pair x
                                       (pair (+ min margin)
                                             (- max margin)))))))
        limits (-> limits
                   (assoc (first cps) (pair 0.0 0.0)
                          (last cps) (pair 1.0 1.0)))]
    (curve-update curve ids (fn [v]
                              (let [[x y] v
                                    num-fn (if (math/float32? x)
                                             round-scalar-float
                                             round-scalar)]
                                (.set p x y 0.0)
                                (.transform transform p)
                                (let [[^double min-x ^double max-x] (limits x)
                                      x (max min-x (min max-x (.getX p)))
                                      y (.getY p)]
                                  (assoc v
                                    0 (num-fn x)
                                    1 (num-fn y))))))))

(defn curve-point-count
  ^long [curve]
  (iv/iv-count (:points curve)))

(defn curve-vals [curve]
  (iv/iv-vals (:points curve)))

(defn curve-range [curve]
  (let [curve-vals (curve-vals curve)]
    (assert (seq curve-vals) "Invalid curve")
    (if (= 1 (count curve-vals))
      (let [sample (curve-vals 0)
            value (sample 1)] ; Extract Y component.
        [value value])
      (let [spline (->spline curve-vals)]
        (loop [t 0.0
               min-value Double/MAX_VALUE
               max-value Double/MIN_VALUE]
          (let [sample (spline-cp spline t) ; Clamps t to [0, 1] internally.
                ^double value (sample 1)] ; Extract Y component.
            (if (>= t 1.0)
              [(min min-value value)
               (max max-value value)]
              (recur (+ t 0.01)
                     (min min-value value)
                     (max max-value value)))))))))

(defonce/record Curve [points]
  Sampler
  (sample [this] (second (first (iv/iv-vals points))))
  (sample-range [this] (curve-range this))
  t/GeomCloud
  (t/geom-aabbs [this] (curve-aabbs this))
  (t/geom-aabbs [this ids] (curve-aabbs this ids))
  (t/geom-insert [this positions] (curve-insert this positions))
  (t/geom-delete [this ids] (curve-delete this ids))
  (t/geom-update [this ids f] (curve-update this ids f))
  (t/geom-transform [this ids transform] (curve-transform this ids transform)))

(defonce/record CurveSpread [points ^float spread]
  Sampler
  (sample [this] (second (first (iv/iv-vals points))))
  (sample-range [this] (let [[^float min ^float max] (curve-range this)]
                         (pair (- min spread) (+ max spread))))
  t/GeomCloud
  (t/geom-aabbs [this] (curve-aabbs this))
  (t/geom-aabbs [this ids] (curve-aabbs this ids))
  (t/geom-insert [this positions] (curve-insert this positions))
  (t/geom-delete [this ids] (curve-delete this ids))
  (t/geom-update [this ids f] (curve-update this ids f))
  (t/geom-transform [this ids transform] (curve-transform this ids transform)))

(defn curve? [value]
  (or (instance? Curve value)
      (instance? CurveSpread value)))

(def default-control-point [protobuf/float-zero protobuf/float-zero protobuf/float-one protobuf/float-zero])

(def default-control-points [default-control-point])

(def default-spread (float 0.0))

(defn ->curve [control-points]
  (Curve. (iv/iv-vec (or control-points
                         default-control-points))))

(def default-curve (->curve default-control-points))

(defn ->curve-spread [control-points spread]
  (CurveSpread. (iv/iv-vec (or control-points
                               default-control-points))
                (or spread default-spread)))

(def default-curve-spread (->curve-spread default-control-points default-spread))

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

(defn- q-round [^double v]
  (let [f 10e6]
    (/ (Math/round (* v f)) f)))

(defn clj-value->go-prop-value [go-prop-type clj-value]
  (if (resource/resource? clj-value)
    (resource/resource->proj-path clj-value)
    (case go-prop-type
      :property-type-vector3 (apply format "%s, %s, %s" clj-value)
      :property-type-vector4 (apply format "%s, %s, %s, %s" clj-value)
      :property-type-quat (let [q (math/euler->quat clj-value)]
                            (apply format "%s, %s, %s, %s" (map q-round (math/vecmath->clj q))))
      (str clj-value))))

(defn- parse-num [s]
  (num (Double/parseDouble s)))

(defn- tokenize! [^StringTokenizer tokenizer parse-fn]
  (-> tokenizer (.nextToken) (.trim) (parse-fn)))

(defn- parse-vec [s ^long count]
  (let [tokenizer (StringTokenizer. s ",")]
    (loop [result []
           counter 0]
      (if (< counter count)
        (recur (conj result (tokenize! tokenizer parse-num))
               (inc counter))
        result))))

(defn- parse-quat-euler [s]
  (let [v (parse-vec s 4)
        q (Quat4d. (double-array v))]
    (math/quat->euler q)))

(defn- ->decl [keys]
  (into {} (map (fn [k] [k (transient [])]) keys)))

(def type->entry-keys {:property-type-number [:number-entries :float-values]
                       :property-type-hash [:hash-entries :hash-values]
                       :property-type-url [:url-entries :string-values]
                       :property-type-vector3 [:vector3-entries :float-values]
                       :property-type-vector4 [:vector4-entries :float-values]
                       :property-type-quat [:quat-entries :float-values]
                       :property-type-boolean [:bool-entries :float-values]})

(def ^:private go-prop-type? (partial contains? type->entry-keys))

(def TGoPropType (apply s/enum (keys type->entry-keys)))

(defn- go-prop? [value]
  (and (string? (:id value))
       (contains? value :clj-value)
       (contains? value :value)
       (go-prop-type? (:type value))))

(defn- append-values! [values vs]
  (loop [vs vs
         values values]
    (if-let [v (first vs)]
      (recur (rest vs) (conj! values v))
      values)))

(defn go-props->decls [go-props include-element-ids?]
  (loop [go-props go-props
         decl (->decl [:number-entries :hash-entries :url-entries :vector3-entries
                       :vector4-entries :quat-entries :bool-entries :float-values
                       :hash-values :string-values])]
    (if-some [{:keys [id type clj-value value] :as go-prop} (first go-props)]
      (let [_ (assert (go-prop? go-prop))
            values (case type
                     :property-type-number [clj-value]
                     :property-type-hash [(murmur/hash64 value)]
                     :property-type-url [value]
                     :property-type-vector3 clj-value
                     :property-type-vector4 clj-value
                     :property-type-quat (-> clj-value math/euler->quat math/vecmath->clj)
                     :property-type-boolean [(if clj-value 1.0 0.0)])
            [entry-key values-key] (type->entry-keys type)
            entry {:key id
                   :id (murmur/hash64 id)
                   :index (count (get decl values-key))}
            entry (cond
                    (not include-element-ids?)
                    entry

                    (= type :property-type-vector3)
                    (assoc entry :element-ids (mapv #(murmur/hash64 (str id %))
                                                    [".x" ".y" ".z"]))

                    (or (= type :property-type-vector4)
                        (= type :property-type-quat))
                    (assoc entry :element-ids (mapv #(murmur/hash64 (str id %))
                                                    [".x" ".y" ".z" ".w"]))
                    :else entry)]
        (recur (rest go-props)
               (-> decl
                 (update entry-key conj! entry)
                 (update values-key append-values! values))))
      (into {}
            (map (fn [[k v]]
                   [k (persistent! v)]))
            decl))))

(defn property-edit-type [property]
  (or (get property :edit-type)
      {:type (:type property)}))

(defn edit-type-id [edit-type]
  (let [t (:type edit-type)
        t (or (g/value-type-dispatch-value t) t)]
    (if (:on-interface t)
      (:on t)
      t)))

(defn property-edit-type-id [property]
  (edit-type-id (property-edit-type property)))

(defn value [property]
  ((get-in property [:edit-type :to-type] identity) (:value property)))

(def ^:private links #{:link :override})

(defn category? [v]
  (and (sequential? v)
       (let [v (first v)]
         (or (string? v)
             (localization/message-pattern? v)))))

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
  ;; TODO:
  ;; The (dynamic link) and (dynamic override) decorations supported here appear
  ;; unused outside of tests. Remove this processing? It seems to only be used
  ;; by the property editor, and we seem to override the `_properties` output
  ;; instead to achieve the same result.
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

(defn visible? [property]
  (:visible property true))

(defn coalesce [properties]
  (let [node-ids (mapv :node-id properties)
        properties (mapv flatten-properties properties)
        display-orders (mapv :display-order properties)
        node-count (count properties)
        ;; Filter out invisible properties
        visible-prop-colls (->> properties
                                (eduction
                                  (mapcat :properties)
                                  (filter #(visible? (val %))))
                                (util/group-into {} [] key val))
        coalesced (into {}
                        (comp
                          ;; Filter out properties not common to *all* property sets
                          ;; Heuristic is to compare count and also type
                          (filter
                            (fn [e]
                              (let [v (val e)]
                                (and (= node-count (count v))
                                     (apply = (map property-edit-type v))))))
                          ;; Coalesce into properties consumable by e.g. the properties view
                          (map (fn [[k v]]
                                 (let [prop {:key k
                                             :node-ids (mapv :node-id v)
                                             :prop-kws (mapv (fn [{:keys [prop-kw]}]
                                                               (cond (some? prop-kw) prop-kw
                                                                     (vector? k) (first k)
                                                                     :else k))
                                                             v)
                                             :tooltip (some :tooltip v)
                                             :values (mapv (fn [{:keys [value]}]
                                                             (when-not (g/error? value)
                                                               value))
                                                           v)
                                             :errors (mapv (fn [{:keys [value error]}]
                                                             (or error
                                                                 (when (g/error? value) value)))
                                                           v)
                                             :edit-type (property-edit-type (first v))
                                             :label (:label (first v))
                                             :read-only? (reduce
                                                           (fn [acc prop]
                                                             (if (:read-only? prop false)
                                                               (reduced true)
                                                               acc))
                                                           false
                                                           v)}
                                       original-values (mapv (fn [{:keys [original-value]}]
                                                               (when-not (g/error? original-value)
                                                                 original-value))
                                                             v)
                                       prop (cond-> prop
                                                    (not-every? nil? original-values)
                                                    (assoc :original-values original-values))]
                                   (pair k prop)))))
                        visible-prop-colls)]
    {:properties coalesced
     :display-order (prune-display-order (first display-orders) (set (keys coalesced)))
     :original-node-ids node-ids}))

(defn values [{:keys [edit-type original-values values]}]
  {:pre [(or (nil? original-values)
             (= (count values)
                (count original-values)))]}
  (let [to-type (:to-type edit-type identity)]
    (mapv (fn [value original-value]
            (to-type (if-not (nil? value)
                       value
                       original-value)))
          values
          (or original-values
              (repeat nil)))))

(defn- set-value-txs [evaluation-context node-id prop-kw key set-fn old-value new-value]
  (cond
    set-fn (set-fn evaluation-context node-id old-value new-value)
    (vector? key) (g/update-property node-id prop-kw assoc-in (rest key) new-value)
    :else (g/set-property node-id prop-kw new-value)))

(defn set-value [evaluation-context un-coalesced-property new-value]
  (let [{:keys [key value node-id edit-type prop-kw]} un-coalesced-property
        set-fn (:set-fn edit-type)
        from-fn (:from-type edit-type identity)
        new-value (from-fn new-value)]
    (set-value-txs evaluation-context node-id prop-kw key set-fn value new-value)))

(defn- resolve-set-operations
  "Resolves the supplied property and sequence of new values to a vector of
  individual [node-id prop-kw old-value new-value] vectors detailing the
  low-level property set operations that will be performed as a result."
  [property values]
  (let [node-ids (:node-ids property)
        prop-kws (:prop-kws property)
        old-values (:values property)
        new-values (if-some [from-fn (-> property :edit-type :from-type)]
                     (map from-fn values) ; The values might be an infinite sequence, so we can't use mapv here.
                     values)]
    (mapv vector node-ids prop-kws old-values new-values)))

(defn- edited-endpoints [set-operations]
  (into #{}
        (map (fn [[node-id prop-kw]]
               (g/endpoint node-id prop-kw)))
        set-operations))

(defn- set-values [evaluation-context property set-operations]
  (let [key (:key property)
        set-fn (get-in property [:edit-type :set-fn])]
    (for [[node-id prop-kw old-value new-value] set-operations]
      (set-value-txs evaluation-context node-id prop-kw key set-fn old-value new-value))))

(defn- keyword->name-raw [property-keyword]
  (-> property-keyword
      (name)
      (camel/->Camel_Snake_Case_String)
      (string/replace "_" " ")
      (string/trim)))

(def keyword->name (fn/memoize keyword->name-raw))

(def ^:private memoized-label-message
  (fn/memoize
    (fn create-label-message [domain k]
      {:pre [(or (nil? domain) (keyword? domain)) (keyword? k)]}
      (localization/message
        (str "property."
             (when domain (str (name domain) "."))
             (name k))
        {}
        (keyword->name k)))))

(defn label-dynamic
  "Create a fnk that returns a memoized property label MessagePattern

  Args:
    domain    optional \"domain\" of property, e.g. :gui; used for semantical
              localization grouping
    k         property key, e.g. :position

  Examples:
    (label-dynamic :position) => property.position
    (label-dynamic :particlefx :type) => property.particlefx.type"
  ([k]
   (label-dynamic nil k))
  ([domain k]
   (g/constantly (memoized-label-message domain k))))

(defn label
  [property]
  (or (:label property)
      (let [k (:key property)
            k (if (vector? k) (peek k) k)]
        (memoized-label-message nil k))))

(def ^:private memoized-tooltip-message
  (fn/memoize
    (fn create-tooltip-message [domain k]
      {:pre [(or (nil? domain) (keyword? domain)) (keyword? k)]}
      (localization/message
        (str "property." (when domain (str (name domain) ".")) (name k) ".tooltip")
        {}
        "none"))))

(defn tooltip-dynamic
  "Create a fnk that returns a memoized property tooltip MessagePattern

  Args:
    domain    optional \"domain\" of property, e.g. :gui; used for semantical
              localization grouping
    k         property key, e.g. :position

  Examples:
    (tooltip-dynamic :position) => property.position.tooltip
    (tooltip-dynamic :particlefx :type) => property.particlefx.type.tooltip"
  ([k]
   (tooltip-dynamic nil k))
  ([domain k]
   (g/constantly (memoized-tooltip-message domain k))))

(defn tooltip [property]
  (or (:tooltip property)
      (let [k (:key property)
            k (if (vector? k) (peek k) k)]
        (memoized-tooltip-message nil k))))

(defn read-only? [property]
  (:read-only? property))

(defn user-edit?
  "Callable from a property setter function. Returns true if the setter function
  is being invoked as a result of the user editing the specified property. It
  will return false if the property setter is being called as a result of other
  operations such as resource loading, script code being executed, etc."
  [node-id prop-kw evaluation-context]
  {:pre [(map? evaluation-context)]}
  (true?
    (some-> evaluation-context
            :tx-data-context
            deref
            :edited-endpoints
            (contains? (g/endpoint node-id prop-kw)))))

(defn set-values!
  "Set values as a result of a user-edit in the Property Editor."
  ([property values]
   (set-values! property values (gensym)))
  ([property values op-seq]
   (when (not (read-only? property))
     (let [evaluation-context (g/make-evaluation-context)
           set-operations (resolve-set-operations property values)
           edited-endpoints (edited-endpoints set-operations)]
       (g/transact
         {:tx-data-context-map {:edited-endpoints edited-endpoints}}
         (concat
           (g/operation-label (localization/message "operation.property.set" {"property" (label property)}))
           (g/operation-sequence op-seq)
           (set-values evaluation-context property set-operations)))))))

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
  (and (contains? property :original-values)
       (every? some? (:original-values property))
       (not-every? nil? (:values property))))

(defn overridden-uncoalesced? [property]
  (and (contains? property :original-value)
       (not (g/error? (:original-value property)))))

(defn error-aggregate [vals]
  (when-let [errors (seq (remove nil? (distinct (filter g/error? vals))))]
    (g/error-aggregate errors)))

(defn validation-message [property]
  (when-let [err (error-aggregate (:errors property))]
    {:severity (:severity err) :message (g/error-message err)}))

(defn clear-override-uncoalesced [property]
  ((-> property :edit-type (:clear-fn g/clear-property)) (:node-id property) (:prop-kw property)))

(defn clear-override! [property]
  (when (overridden? property)
    (g/transact
      (concat
        (g/operation-label (localization/message "operation.property.clear" {"property" (label property)}))
        (let [clear-fn (get-in property [:edit-type :clear-fn])]
          (map (fn [node-id prop-kw]
                 (if clear-fn
                   (clear-fn node-id prop-kw)
                   (g/clear-property node-id prop-kw)))
               (:node-ids property)
               (:prop-kws property)))))))

(definline round-scalar [num]
  `(math/round-with-precision ~num math/precision-general))

(definline round-scalar-float [num]
  `(float (round-scalar ~num)))

(definline round-scalar-coarse [num]
  `(math/round-with-precision ~num math/precision-coarse))

(definline round-scalar-coarse-float [num]
  `(float (round-scalar-coarse ~num)))

(definline round-vec [vec]
  `(mapv round-scalar ~vec))

(definline round-vec-coarse [vec]
  `(mapv round-scalar-coarse ~vec))

(defn scale-and-round [num ^double scale]
  (let [scaled-num (* (double num) scale)]
    (if (math/float32? num)
      (round-scalar-float scaled-num)
      (round-scalar scaled-num))))

(definline scale-and-round-vec [vec scale]
  `(math/zip-clj-v3 ~vec ~scale scale-and-round))

(definline scale-by-absolute-value-and-round [num scale]
  `(scale-and-round ~num (Math/abs (double ~scale))))

;; SDK api
(defn ->choicebox
  ([vals]
   (->choicebox vals true))
  ([vals apply-natural-sorting?]
   (let [sorted-vals (if apply-natural-sorting?
                       (sort eutil/natural-order vals)
                       vals)]
     {:type :choicebox
      :options (mapv (juxt identity identity) sorted-vals)})))

(defn ->pb-choicebox-raw [cls]
  (let [values (protobuf/enum-values cls)]
    {:type :choicebox
     :options (mapv (juxt first (comp :display-name second)) values)}))

;; SDK api
(def ->pb-choicebox (memoize ->pb-choicebox-raw))

;; SDK api
(defn quat->euler []
  {:type t/Vec3
   :from-type (fn [euler]
                (let [quat (math/euler->quat euler)]
                  (if-not (math/float32? (first euler))
                    (math/vecmath-into-clj quat (coll/empty-with-meta euler))
                    (into (coll/empty-with-meta euler)
                          (map float)
                          (math/vecmath->clj quat)))))
   :to-type (fn [clj-quat]
              (let [euler (math/clj-quat->euler clj-quat)]
                (into (coll/empty-with-meta clj-quat)
                      (map (if (math/float32? (first clj-quat))
                             round-scalar-coarse-float
                             round-scalar-coarse))
                      euler)))})

(def quat-rotation-edit-type (quat->euler))

(defn property-entry->go-prop [[key {:keys [go-prop-type value error]}]]
  (when (some? go-prop-type)
    (assert (keyword? key))
    (assert (go-prop-type? go-prop-type))
    {:id (key->user-name key)
     :type go-prop-type
     :clj-value value
     :value (clj-value->go-prop-value go-prop-type value)
     :error error}))

(defmulti go-prop-value->clj-value (fn [property-type go-prop-type _go-prop-value _workspace] [go-prop-type property-type]))
(defmethod go-prop-value->clj-value :default [property-type _ go-prop-value _]
  (if (= g/Str property-type)
    go-prop-value
    ::unsupported-conversion))

(defmethod go-prop-value->clj-value [:property-type-boolean g/Bool] [_ _ go-prop-value _] (Boolean/parseBoolean go-prop-value))
(defmethod go-prop-value->clj-value [:property-type-number g/Num]   [_ _ go-prop-value _] (parse-num go-prop-value))
(defmethod go-prop-value->clj-value [:property-type-number t/Vec3]  [_ _ go-prop-value _] [(parse-num go-prop-value) 0.0 0.0])
(defmethod go-prop-value->clj-value [:property-type-number t/Vec4]  [_ _ go-prop-value _] [(parse-num go-prop-value) 0.0 0.0 1.0])
(defmethod go-prop-value->clj-value [:property-type-vector3 g/Num]  [_ _ go-prop-value _] (first (parse-vec go-prop-value 1)))
(defmethod go-prop-value->clj-value [:property-type-vector3 t/Vec3] [_ _ go-prop-value _] (parse-vec go-prop-value 3))
(defmethod go-prop-value->clj-value [:property-type-vector3 t/Vec4] [_ _ go-prop-value _] (conj (parse-vec go-prop-value 3) 1.0))
(defmethod go-prop-value->clj-value [:property-type-vector4 g/Num]  [_ _ go-prop-value _] (first (parse-vec go-prop-value 1)))
(defmethod go-prop-value->clj-value [:property-type-vector4 t/Vec3] [_ _ go-prop-value _] (parse-vec go-prop-value 3))
(defmethod go-prop-value->clj-value [:property-type-vector4 t/Vec4] [_ _ go-prop-value _] (parse-vec go-prop-value 4))
(defmethod go-prop-value->clj-value [:property-type-quat t/Vec3]    [_ _ go-prop-value _] (parse-quat-euler go-prop-value))

(defmethod go-prop-value->clj-value [:property-type-hash resource/Resource] [_ _ go-prop-value workspace]
  (workspace/resolve-workspace-resource workspace go-prop-value))

(defn property-desc->resource [property-desc proj-path->resource]
  ;; This is not strictly correct. We cannot distinguish between a resource
  ;; property and other properties backed by hashed strings without looking at
  ;; the property declaration in the originating .script file.
  (case (:type property-desc)
    :property-type-hash (proj-path->resource (:value property-desc))
    nil))

(defn property-desc->clj-value [property-desc proj-path->resource]
  ;; This is not strictly correct. We cannot know the expected type without
  ;; looking at the property declaration in the originating .script file.
  (let [go-prop-value (:value property-desc)]
    (case (:type property-desc)
      :property-type-boolean (Boolean/parseBoolean go-prop-value)
      :property-type-number (parse-num go-prop-value)
      :property-type-vector3 (parse-vec go-prop-value 3)
      :property-type-vector4 (parse-vec go-prop-value 4)
      :property-type-quat (parse-quat-euler go-prop-value)
      :property-type-hash (or (proj-path->resource go-prop-value) go-prop-value)
      go-prop-value)))

(defn property-desc->go-prop [property-desc proj-path->resource]
  ;; GameObject$PropertyDesc in map format.
  (assoc property-desc :clj-value (property-desc->clj-value property-desc proj-path->resource)))

(defn go-prop->property-desc [go-prop]
  ;; GameObject$PropertyDesc in map format with additional internal keys.
  ;; We strip these out to get a "clean" GameObject$PropertyDesc in map format.
  (dissoc go-prop :clj-value :error))

(defmulti sanitize-go-prop-value (fn [go-prop-type _go-prop-value] go-prop-type))
(defmethod sanitize-go-prop-value :default [_go-prop-type go-prop-value]
  go-prop-value)

(defmethod sanitize-go-prop-value :property-type-vector3 [go-prop-type go-prop-value] (clj-value->go-prop-value go-prop-type (parse-vec go-prop-value 3)))
(defmethod sanitize-go-prop-value :property-type-vector4 [go-prop-type go-prop-value] (clj-value->go-prop-value go-prop-type (parse-vec go-prop-value 4)))
(defmethod sanitize-go-prop-value :property-type-quat [go-prop-type go-prop-value] (clj-value->go-prop-value go-prop-type (go-prop-value->clj-value t/Vec3 :property-type-quat go-prop-value nil)))

(defn sanitize-property-desc [property-desc]
  ;; GameObject$PropertyDesc in map format.
  (let [go-prop-value (:value property-desc)
        go-prop-type (:type property-desc)]
    (try
      (let [sanitized-go-prop-value (sanitize-go-prop-value go-prop-type go-prop-value)]
        (assert (string? sanitized-go-prop-value))
        (assoc property-desc :value sanitized-go-prop-value))
      (catch Exception _
        ;; Leave unsanitized.
        property-desc))))

(defn- apply-property-override [workspace id-mapping prop-kw prop property-desc]
  ;; This can be used with raw GameObject$PropertyDescs in map format. However,
  ;; we decorate these with a :clj-value field when they enter the graph, so if
  ;; that is already present we don't attempt conversion here.
  (let [clj-value-entry (find property-desc :clj-value)
        clj-value (if (some? clj-value-entry)
                    (val clj-value-entry)
                    (try
                      (go-prop-value->clj-value (:type prop) (:type property-desc) (:value property-desc) workspace)
                      (catch Exception _
                        ::unsupported-conversion)))]
    (when (not= ::unsupported-conversion clj-value)
      (let [resolved-node-id (id-mapping (:node-id prop))
            resolved-prop-kw (get prop :prop-kw prop-kw)]
        (g/set-property resolved-node-id resolved-prop-kw clj-value)))))

(defn apply-property-overrides
  "Returns transaction steps that applies the overrides from the supplied
  GameObject$PropertyDescs in map format to the specified node."
  [workspace id-mapping overridable-properties property-descs]
  {:pre [(or (nil? property-descs) (sequential? property-descs))
         (or (nil? overridable-properties) (map? overridable-properties))
         (every? (comp keyword? key) overridable-properties)]}
  (when (and (seq property-descs)
             (seq overridable-properties))
    (eduction
      (mapcat (fn [property-desc]
                (let [prop-kw (user-name->key (:id property-desc))
                      prop (get overridable-properties prop-kw)]
                  (when (some? prop)
                    (apply-property-override workspace id-mapping prop-kw prop property-desc)))))
      property-descs)))

(defn build-target-go-props
  "Convert go-props that may contain references to Resources inside the project
  (source-resources) into go-props that instead refers to the BuildResource
  produced from the source-resource. Returns a pair of vectors. The first is all
  the go-props with any source-resources now replaced with BuildResources in the
  original order. The second is a vector of dep-build-targets produced from the
  referenced source-resources, obtained from the resource-property-build-targets
  supplied to this function. You would typically call this function when
  producing a build-target. The resulting vectors can later be supplied to the
  build-go-props function inside the build-targets :build-fn in order to update
  the go-props with the final BuildResource references once equivalent
  build-targets have been fused.

  The term `go-prop` is used to describe a protobuf GameObject$PropertyDesc in
  map format with an additional :clj-value field that contains a more
  sophisticated representation of the :value. For example, the :value might be a
  string path, but the :clj-value is a Resource."
  [proj-path->resource-property-build-target go-props-with-source-resources]
  (loop [go-props-with-source-resources go-props-with-source-resources
         go-props-with-build-resources (transient [])
         dep-build-targets (transient [])
         seen-dep-build-resource-paths #{}]
    (if-some [go-prop (first go-props-with-source-resources)]
      (do
        (assert (go-prop? go-prop))
        (let [clj-value (:clj-value go-prop)

              dep-build-target
              (when (resource/resource? clj-value)
                (or (proj-path->resource-property-build-target (resource/proj-path clj-value))

                    ;; If this fails, it is likely because the collection of
                    ;; resource-property-build-targets supplied to this
                    ;; function does not include a resource that is referenced
                    ;; by one of the properties. This typically means that
                    ;; you've forgotten to connect a dependent build target to
                    ;; an array input in the graph.
                    (throw (ex-info (str "unable to resolve build target from value "
                                         (pr-str clj-value))
                                    {:property (:id go-prop)
                                     :resource-reference clj-value}))))

              build-resource (some-> dep-build-target :resource)
              build-resource-path (some-> build-resource resource/proj-path)
              go-prop (cond-> go-prop

                              (and (contains? go-prop :error)
                                   (nil? (:error go-prop)))
                              (dissoc :error)

                              (some? build-resource)
                              (assoc :clj-value build-resource
                                     :value build-resource-path))]
          (recur (next go-props-with-source-resources)
                 (conj! go-props-with-build-resources go-prop)
                 (if (and (some? dep-build-target)
                          (not (contains? seen-dep-build-resource-paths
                                          build-resource-path)))
                   (conj! dep-build-targets dep-build-target)
                   dep-build-targets)
                 (if (some? build-resource-path)
                   (conj seen-dep-build-resource-paths build-resource-path)
                   seen-dep-build-resource-paths))))
      [(persistent! go-props-with-build-resources)
       (persistent! dep-build-targets)])))

(defn build-go-props
  "Typically called from a :build-fn provided by a build-target. Takes a
  collection of go-props that were obtained from the build-target-go-props
  function and updates their BuildResource references to refer to the final
  BuildResource that resulted from the build-target fusing process."
  [dep-resources go-props-with-build-resources]
  (let [build-resource->fused-build-resource
        (fn [build-resource]
          (when (some? build-resource)
            (when-not (workspace/build-resource? build-resource)
              (throw (ex-info (format ":clj-value field in resource go-prop \"%s\" should be a build resource, got "
                                      (pr-str build-resource))
                              {:resource-reference build-resource})))
            (or (dep-resources build-resource)
                (throw (ex-info (str "unable to resolve fused build resource from referenced "
                                     (pr-str build-resource))
                                {:resource-reference build-resource})))))

        go-props-with-fused-build-resources
        (mapv (fn [{:keys [clj-value] :as go-prop}]
                (assert (go-prop? go-prop))
                (if-not (resource/resource? clj-value)
                  go-prop
                  (let [fused-build-resource (build-resource->fused-build-resource clj-value)]
                    (assoc go-prop
                      :clj-value fused-build-resource
                      :value (resource/resource->proj-path fused-build-resource)))))
              go-props-with-build-resources)]

    go-props-with-fused-build-resources))

(defn source-resource-go-prop
  "Resolve any reference to a build resource in the provided go-prop so that it
  refers to a source resource instead."
  [property-desc]
  (if (not= :property-type-hash (:type property-desc))
    property-desc
    (let [build-resource (:clj-value property-desc)]
      (if (not (workspace/build-resource? build-resource))
        property-desc
        (let [source-resource (:resource build-resource)
              source-proj-path (resource/proj-path source-resource)]
          (assoc property-desc
            :clj-value source-resource
            :value source-proj-path))))))

(defn try-get-go-prop-proj-path
  "Returns a non-empty string of the assigned proj-path, or nil if no
  resource was assigned or the go-prop is not a resource property."
  ^String [go-prop]
  (assert (go-prop? go-prop))
  (let [{:keys [value clj-value]} go-prop]
    (when (resource/resource? clj-value)
      ;; Sanity checks to ensure our go-prop is well-formed.
      (assert (workspace/build-resource? clj-value))
      (assert (= value (resource/proj-path clj-value)))
      value)))

(defn transferred-properties
  "Returns a source-prop-infos-by-prop-kw map containing properties that can be
  transferred from the specified source-node-id, intended for use with the
  transfer-overrides-plan function. The returned map may optionally be
  constrained to only include specific properties by supplying a seq of property
  keywords for the source-prop-kws argument. If :all is supplied in place of a
  seq, all overridden properties are included in the returned map. Returns nil
  in case there are no properties matching the criteria."
  [source-node-id source-prop-kws evaluation-context]
  (let [source-prop-infos-by-prop-kw
        (cond-> (:properties (g/node-value source-node-id :_properties evaluation-context))
                (not= :all source-prop-kws)
                (select-keys source-prop-kws))]
    (coll/not-empty
      (coll/transfer source-prop-infos-by-prop-kw {}
        (filter (fn [[_prop-kw prop-info]]
                  ;; Note that properties that have validation errors may be
                  ;; included in the transfer.
                  (and (contains? prop-info :original-value)
                       (:visible prop-info true)
                       (not (:read-only? prop-info)))))))))

(def ^:private property-transfer-target-status?
  #{:ok :owner-resource-not-editable :property-not-found})

(def ^:private override-transfer-type?
  #{:pull-up-overrides :push-down-overrides})

(defn- property-transfer-target-aspect?
  "Returns true if the supplied value is non-nil and valid for the
  :target-aspect of a property-transfer-target."
  [value]
  (and (vector? value)
       (= 2 (count value))
       (let [[aspect-name-message aspect-kind-message] value]
         (and (localization/message-pattern? aspect-name-message)
              (localization/message-pattern? aspect-kind-message)))))

(defn- property-transfer-target?
  "Returns true if the supplied value is a property-transfer-target."
  [{:keys [target-aspect target-status target-owner-resource target-node-id target-prop-node-id target-prop-kw]}]
  (and (or (nil? target-aspect) (property-transfer-target-aspect? target-aspect))
       (property-transfer-target-status? target-status)
       (resource/resource? target-owner-resource)
       (g/node-id? target-node-id)
       (g/node-id? target-prop-node-id)
       (keyword? target-prop-kw)))

(defn- property-transfer?
  "Returns true if the supplied value is a property-transfer."
  [{:keys [source-node-id source-prop-kw source-prop-label source-clear-fn targets] :as value}]
  (and (g/node-id? source-node-id)
       (keyword? source-prop-kw)
       (or (string? source-prop-label)
           (localization/message-pattern? source-prop-label))
       (ifn? source-clear-fn)
       (contains? value :source-value)
       (vector? targets)
       (every? property-transfer-target? targets)))

(defn transfer-overrides-plan?
  "Returns true if the supplied value is a transfer-overrides-plan."
  [{:keys [override-transfer-type property-transfers]}]
  (and (override-transfer-type? override-transfer-type)
       (vector? property-transfers)
       (every? property-transfer?
               property-transfers)))

(defn transfer-overrides-plan
  "Returns a transfer-overrides-plan, that when performed, will transfer the
  properties supplied as a source-prop-infos-by-prop-kw map onto all the targets
  specified in the target-infos vector. The source-prop-infos-by-prop-kw map can
  be obtained from the source-node-id using the transferred-properties function.
  Each target-info should be a map where a :target-node-id and a
  :target-prop-infos-by-prop-kw map must be specified. Optionally, a
  :target-aspect can also be specified as a pair of strings denoting the
  [target-aspect-name target-aspect-kind]. This will solely affect the output of
  the transfer-overrides-description function. For example, you could supply
  ['Default' 'Layout'] as the :target-aspect to denote that the transfer
  specifically targets the Default Layout of the target node. The second element
  of the pair is considered the aspect-kind, and may be presented in a
  pluralized form in the event that several aspect-names (first element) are
  involved."
  [basis override-transfer-type source-prop-infos-by-prop-kw target-infos]
  {:pre [(override-transfer-type? override-transfer-type)
         (map? source-prop-infos-by-prop-kw)
         (not (coll/empty? source-prop-infos-by-prop-kw))
         (vector? target-infos)
         (not (coll/empty? target-infos))]}
  (let [property-transfers
        (coll/transfer source-prop-infos-by-prop-kw []
          (map (fn [[source-prop-kw source-prop-info]]
                 (let [property-transfer-targets
                       (coll/transfer target-infos []
                         (map (fn [{:keys [target-aspect target-node-id target-prop-infos-by-prop-kw] :as target-info}]
                                {:pre [(or (nil? target-aspect) (property-transfer-target-aspect? target-aspect))
                                       (g/node-id? target-node-id)
                                       (map? target-prop-infos-by-prop-kw)
                                       (not (coll/empty? target-prop-infos-by-prop-kw))]}
                                (let [target-owner-resource (resource-node/owner-resource basis target-node-id)
                                      target-prop-info (get target-prop-infos-by-prop-kw source-prop-kw)

                                      property-transfer-target
                                      (-> target-info
                                          (select-keys [:target-node-id :target-aspect])
                                          (assoc :target-owner-resource target-owner-resource))]

                                  (if (nil? target-prop-info)
                                    ;; The property does not exist in the target.
                                    (assoc property-transfer-target
                                      :target-status :property-not-found
                                      :target-prop-node-id target-node-id
                                      :target-prop-kw source-prop-kw)

                                    ;; The property exists in the target.
                                    (let [target-prop-node-id (:node-id target-prop-info)
                                          target-prop-kw (get target-prop-info :prop-kw source-prop-kw)]
                                      (if (and (resource/file-resource? target-owner-resource)
                                               (resource/editable? target-owner-resource))

                                        ;; The property can be transferred to the target.
                                        (let [target-set-fn (-> target-prop-info :edit-type :set-fn)]
                                          (cond-> (assoc property-transfer-target
                                                    :target-status :ok
                                                    :target-prop-node-id target-prop-node-id
                                                    :target-prop-kw target-prop-kw)

                                                  target-set-fn
                                                  (assoc :target-set-fn target-set-fn
                                                         :target-value (:value target-prop-info))))

                                        ;; The resource owning the target node is not editable.
                                        (assoc property-transfer-target
                                          :target-status :owner-resource-not-editable
                                          :target-prop-node-id target-prop-node-id
                                          :target-prop-kw source-prop-kw))))))))]

                   {:source-node-id (:node-id source-prop-info)
                    :source-prop-kw (get source-prop-info :prop-kw source-prop-kw)
                    :source-prop-label (or (:label source-prop-info) (keyword->name source-prop-kw))
                    :source-clear-fn (or (-> source-prop-info :edit-type :clear-fn) g/clear-property)
                    :source-value (:value source-prop-info)
                    :targets property-transfer-targets}))))]

    {:override-transfer-type override-transfer-type
     :property-transfers property-transfers}))

(defn decorate-transfer-overrides-plan
  "Decorates the supplied transfer-overrides-plan with debug info. Useful during
  development."
  ([transfer-overrides-plan]
   (g/with-auto-evaluation-context evaluation-context
     (decorate-transfer-overrides-plan transfer-overrides-plan evaluation-context)))
  ([transfer-overrides-plan {:keys [basis] :as evaluation-context}]
   {:pre [(transfer-overrides-plan? transfer-overrides-plan)]
    :post [(transfer-overrides-plan? %)]}
   (update
     transfer-overrides-plan :property-transfers
     (fn [property-transfers]
       (coll/transfer property-transfers (coll/empty-with-meta property-transfers)
         (map (fn [{:keys [source-node-id] :as property-transfer}]
                {:pre [(g/node-id? source-node-id)]}
                (as-> property-transfer property-transfer
                      (merge (sorted-map
                               :source-node-type-kw (g/node-type-kw basis source-node-id)
                               :source-node-path (gu/node-debug-label-path source-node-id evaluation-context))
                             property-transfer)
                      (update property-transfer :targets
                              (fn [property-transfer-targets]
                                (coll/transfer property-transfer-targets (coll/empty-with-meta property-transfer-targets)
                                  (map (fn [{:keys [target-node-id target-prop-node-id] :as property-transfer-target}]
                                         {:pre [(g/node-id? target-node-id)
                                                (g/node-id? target-prop-node-id)]}
                                         (merge (sorted-map
                                                  :target-node-type-kw (g/node-type-kw basis target-node-id)
                                                  :target-node-path (gu/node-debug-label-path target-node-id evaluation-context)
                                                  :target-prop-node-type-kw (g/node-type-kw basis target-prop-node-id)
                                                  :target-prop-node-path (gu/node-debug-label-path target-prop-node-id evaluation-context))
                                                property-transfer-target))))))))))))))

(defn transfer-overrides-status
  "Returns :ok if the supplied transfer-overrides-plan can be executed,
  otherwise returns a different keyword signaling why we can't proceed with the
  transfer.

  Return values:
    :ok
    Everything appears fine. We should be able to execute the plan.

    :owner-resource-not-editable
    The owner resource of one of the target nodes is not editable.

    :property-not-found
    One or more transferred properties do not exist on one of the target nodes."
  [transfer-overrides-plan]
  (let [property-transfers (get transfer-overrides-plan :property-transfers ::not-found)]
    (assert (not= ::not-found property-transfers))
    (transduce
      (comp
        (mapcat :targets)
        (map :target-status))
      (fn
        ([result] result)
        ([_ target-status]
         {:pre [(property-transfer-target-status? target-status)]}
         (case target-status
           :ok :ok
           (reduced target-status))))
      :ok
      property-transfers)))

(defn can-transfer-overrides?
  "Returns whether the supplied transfer-overrides-plan can be executed.
  Suitable for use with user-interface elements such as menu items."
  [transfer-overrides-plan]
  (= :ok (transfer-overrides-status transfer-overrides-plan)))

(defn transfer-overrides-description
  "Given a transfer-overrides-plan, return a user-friendly localizable
  description of its effects if executed. The resulting MessagePattern is
  suitable for use with user-interface elements such as menu items or tooltips."
  [transfer-overrides-plan evaluation-context]
  {:pre [(transfer-overrides-plan? transfer-overrides-plan)]
   :post [(localization/message-pattern? %)]}
  (let [{:keys [property-transfers override-transfer-type]} transfer-overrides-plan
        property-transfer-targets (coll/transfer property-transfers []
                                    (mapcat :targets))
        target-node-ids (coll/transfer property-transfer-targets #{}
                          (keep :target-node-id))
        target-aspects (coll/transfer property-transfer-targets #{}
                         (keep :target-aspect))
        target-aspect-count (count target-aspects)
        target-proj-paths (coll/transfer property-transfer-targets #{}
                            (keep :target-owner-resource)
                            (map resource/proj-path))]
    (-> (case override-transfer-type
          :pull-up-overrides "command.edit.pull-up-overrides.option"
          :push-down-overrides "command.edit.push-down-overrides.option")
        (localization/message
          {"property_count" (count property-transfers)
           "property" (:source-prop-label (first property-transfers))
           "target_count" (count target-node-ids)
           "target" (or (gu/node-qualifier-label (first target-node-ids) evaluation-context) "undefined")
           "aspect_count" target-aspect-count
           "aspect" (if (= 1 target-aspect-count)
                      (first (first target-aspects))
                      (localization/vary-message-variables
                        (let [target-kinds (into #{} (map second) target-aspects)]
                          (if (= 1 (count target-kinds))
                            (first target-kinds)
                            (localization/message "override.aspect.generic.kind")))
                        assoc "count" target-aspect-count))
           "resource_count" (count target-proj-paths)
           "resource" (first target-proj-paths)})
        (localization/transform string/replace "_" "__"))))

(defn- set-property-tx-data
  [property-transfer-target new-value evaluation-context]
  {:pre [(property-transfer-target? property-transfer-target)]}
  (let [target-prop-node-id (:target-prop-node-id property-transfer-target)]
    (if-let [target-set-fn (:target-set-fn property-transfer-target)]
      (let [old-value (:target-value property-transfer-target)]
        (target-set-fn evaluation-context target-prop-node-id old-value new-value))
      (let [target-prop-kw (:target-prop-kw property-transfer-target)]
        (g/set-property target-prop-node-id target-prop-kw new-value)))))

(defn- clear-property-tx-data
  [{:keys [source-clear-fn source-node-id source-prop-kw]}]
  (assert (g/node-id? source-node-id))
  (assert (keyword? source-prop-kw))
  (source-clear-fn source-node-id source-prop-kw))

(defn- property-transfer-tx-data
  [property-transfer evaluation-context]
  (let [source-value (get property-transfer :source-value ::not-found)
        property-transfer-targets (get property-transfer :targets ::not-found)]
    (assert (not= ::not-found source-value))
    (assert (not= ::not-found property-transfer-targets))
    (e/concat
      (clear-property-tx-data property-transfer)
      (e/mapcat #(set-property-tx-data % source-value evaluation-context)
                property-transfer-targets))))

(defn transfer-overrides-tx-data
  "Given a transfer-overrides-plan, return a sequence of transaction steps that
  will transfer the overridden properties to the target nodes, or nil if there
  is nothing to transfer."
  [transfer-overrides-plan evaluation-context]
  (let [property-transfers (get transfer-overrides-plan :property-transfers ::not-found)]
    (assert (not= ::not-found property-transfers))
    (coll/not-empty
      (coll/transfer property-transfers []
        (mapcat #(property-transfer-tx-data % evaluation-context))))))

(defn transfer-overrides!
  "Given a transfer-overrides-plan, execute a transaction that will transfer the
  overridden properties to the target nodes. Does nothing if the plan is empty."
  [transfer-overrides-plan]
  (when (coll/not-empty transfer-overrides-plan)
    (when-let [tx-data (g/with-auto-evaluation-context evaluation-context
                         (transfer-overrides-tx-data transfer-overrides-plan evaluation-context))]
      (g/transact tx-data)
      nil)))

(defn- basic-transfer-overrides-plan
  [override-transfer-type source-prop-infos-by-prop-kw target-node-ids {:keys [basis] :as evaluation-context}]
  {:pre [(not (coll/empty? target-node-ids))]}
  (let [target-infos
        (coll/transfer target-node-ids []
          (map (fn [target-node-id]
                 (let [target-prop-infos-by-prop-kw (:properties (g/node-value target-node-id :_properties evaluation-context))]
                   {:target-node-id target-node-id
                    :target-prop-infos-by-prop-kw target-prop-infos-by-prop-kw}))))]
    (transfer-overrides-plan basis override-transfer-type source-prop-infos-by-prop-kw target-infos)))

(defmulti pull-up-overrides-plan-alternatives
  "Given a source-node-id and a source-prop-infos-by-prop-kw map, should return
  a vector of transfer-overrides-plans for transferring the properties from the
  source-node-id to each of its override-originals in succession, starting from
  the source-node-id and proceeding towards the override-root."
  {:arglists '([source-node-id source-prop-infos-by-prop-kw evaluation-context])}
  (fn [source-node-id _source-prop-infos-by-prop-kw evaluation-context]
    (g/node-type-kw (:basis evaluation-context) source-node-id)))

(defmethod pull-up-overrides-plan-alternatives :default
  [source-node-id source-prop-infos-by-prop-kw {:keys [basis] :as evaluation-context}]
  (when-let [original-node-id (g/override-original basis source-node-id)]
    (let [original-node-ids (iterate #(g/override-original basis %) original-node-id)]
      (coll/transfer
        original-node-ids []
        (take-while some?)
        (map (fn [original-node-id]
               (basic-transfer-overrides-plan :pull-up-overrides source-prop-infos-by-prop-kw [original-node-id] evaluation-context)))))))

(defmulti push-down-overrides-plan-alternatives
  "Given a source-node-id and a source-prop-infos-by-prop-kw map, should return
  a vector of transfer-overrides-plans for transferring the properties to each
  immediate override node of the source-node-id."
  {:arglists '([source-node-id source-prop-infos-by-prop-kw evaluation-context])}
  (fn [source-node-id _source-prop-infos-by-prop-kw evaluation-context]
    (g/node-type-kw (:basis evaluation-context) source-node-id)))

(defmethod push-down-overrides-plan-alternatives :default
  [source-node-id source-prop-infos-by-prop-kw {:keys [basis] :as evaluation-context}]
  (when-let [override-node-ids (coll/not-empty (g/overrides basis source-node-id))]
    (let [transfer-overrides-plan (basic-transfer-overrides-plan :push-down-overrides source-prop-infos-by-prop-kw override-node-ids evaluation-context)]
      [transfer-overrides-plan])))
