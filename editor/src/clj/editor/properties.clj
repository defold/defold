(ns editor.properties
  (:require [camel-snake-kebab :as camel]
            [clojure.set :as set]
            [cognitect.transit :as transit]
            [dynamo.graph :as g]
            [editor.core :as core]
            [editor.defold-project :as project]
            [editor.image :as image]
            [editor.math :as math]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.types :as t]
            [editor.workspace :as workspace]
            [util.id-vec :as iv]
            [util.murmur :as murmur])
  (:import [com.defold.editor.pipeline LuaScanner$Property]
           [com.dynamo.textureset.proto TextureSetProto$TextureSet]
           [java.util StringTokenizer]
           [javax.vecmath Quat4d Point3d Matrix4d]))

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
    :property-type-resource (resource/resource->proj-path value)
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

(defn str->go-prop [base-resource s type]
  (case type
    :property-type-number (parse-num s)
    (:property-type-hash :property-type-url) s
    :property-type-vector3 (parse-vec s 3)
    :property-type-vector4 (parse-vec s 4)
    :property-type-quat (let [v (parse-vec s 4)
                              q (Quat4d. (double-array v))]
                          (math/quat->euler q))
    :property-type-boolean (Boolean/parseBoolean s)
    :property-type-resource (workspace/resolve-resource base-resource s)))

(defn go-prop-type->property-type [go-prop-type]
  (case go-prop-type
    :property-type-number   g/Num
    :property-type-hash     g/Str
    :property-type-url      g/Str
    :property-type-vector3  t/Vec3
    :property-type-vector4  t/Vec4
    :property-type-quat     t/Vec3
    :property-type-boolean  g/Bool
    :property-type-resource resource/Resource))

;; Supported resource property sub-types.
;; TODO: Use murmur/hash64 directly instead of LuaScanner constants once we remove the legacy code editor.
(defonce sub-type-material LuaScanner$Property/subTypeMaterial)
(defonce sub-type-texture LuaScanner$Property/subTypeTexture)
(defonce sub-type-textureset LuaScanner$Property/subTypeTextureSet)

(defonce sub-type-material-ext "material")
(defonce sub-type-textureset-ext ["atlas" "tilesource"])
(defonce sub-type-texture-ext (into sub-type-textureset-ext (cons "cubemap" image/exts)))

(defn go-prop-sub-type->ext [go-prop-sub-type]
  (condp = go-prop-sub-type
    sub-type-material sub-type-material-ext
    sub-type-texture sub-type-texture-ext
    sub-type-textureset sub-type-textureset-ext))

(defn go-prop-edit-type [prop-kw go-prop-type go-prop-sub-type]
  (if (not= :property-type-resource go-prop-type)
    {:type (go-prop-type->property-type go-prop-type)}
    {:type (go-prop-type->property-type go-prop-type)
     :ext (go-prop-sub-type->ext go-prop-sub-type)
     :set-fn (fn set-resource-property [_evaluation-context self _old-value new-value]
               (concat
                 (g/set-property self prop-kw new-value)
                 (g/update-property self :property-resources assoc prop-kw new-value)))}))

(defn- ->decl [keys]
  (into {} (map (fn [k] [k (transient [])]) keys)))

(def ^:private type->entry-keys {:property-type-number [:number-entries :float-values]
                                 :property-type-hash [:hash-entries :hash-values]
                                 :property-type-url [:url-entries :string-values]
                                 :property-type-vector3 [:vector3-entries :float-values]
                                 :property-type-vector4 [:vector4-entries :float-values]
                                 :property-type-quat [:quat-entries :float-values]
                                 :property-type-boolean [:bool-entries :float-values]
                                 :property-type-resource [:resource-entries :string-values]})

(defn append-values! [values vs]
  (loop [vs vs
         values values]
    (if-let [v (first vs)]
      (recur (rest vs) (conj! values v))
      values)))

(defn go-props->decls [properties]
  (loop [properties properties
         decl (->decl [:number-entries :hash-entries :url-entries :resource-entries
                       :vector3-entries :vector4-entries :quat-entries :bool-entries
                       :float-values :hash-values :string-values])]
    (if-let [prop (first properties)]
      (let [type (:type prop)
            clj-value (:clj-value prop)
            values (case type
                     :property-type-number [clj-value]
                     :property-type-hash [(murmur/hash64 clj-value)]
                     :property-type-url [clj-value]
                     :property-type-vector3 clj-value
                     :property-type-vector4 clj-value
                     :property-type-quat (-> clj-value math/euler->quat math/vecmath->clj)
                     :property-type-boolean [(if clj-value 1.0 0.0)]
                     :property-type-resource [(resource/resource->proj-path clj-value)])
            [entry-key values-key] (type->entry-keys type)
            entry {:key (:id prop)
                   :id (murmur/hash64 (:id prop))
                   :index (count (get decl values-key))}]
        (recur (rest properties)
               (-> decl
                 (update entry-key conj! entry)
                 (update values-key append-values! values))))
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

(defn error-aggregate [vals]
  (when-let [errors (seq (remove nil? (distinct (filter g/error? vals))))]
    (g/error-aggregate errors)))

(defn validation-message [property]
  (when-let [err (error-aggregate (:errors property))]
    {:severity (:severity err) :message (g/error-message err)}))

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

(defn- resolve-texture-build-target [{:keys [user-data] :as build-target}]
  (let [{:keys [pb-class pb-msg]} user-data
        texture-build-target (if-not (= TextureSetProto$TextureSet pb-class)
                               build-target
                               (let [texture-build-resource (:texture pb-msg)]
                                 (assert (some? texture-build-resource))
                                 (some (fn [dep-build-target]
                                         (when (= texture-build-resource (:resource dep-build-target))
                                           dep-build-target))
                                       (:deps build-target))))]
    (if (= "texturec" (resource/ext (:resource texture-build-target)))
      texture-build-target
      (throw (ex-info (str "unable to resolve texture build target from build target for " (pr-str (:resource build-target)))
                      {:resource-reference (:resource build-target)
                       :build-target build-target})))))

(defn property-entry->go-prop [[key {:keys [go-prop-type go-prop-sub-type value]}]]
  (when (some? go-prop-type)
    (assert (keyword? key))
    (assert (contains? type->entry-keys go-prop-type))
    (assert (or (not= :property-type-resource go-prop-type) (integer? go-prop-sub-type)))
    {:id (key->user-name key)
     :type go-prop-type
     :sub-type go-prop-sub-type
     :clj-value value
     :value (go-prop->str value go-prop-type)}))

(defn- go-prop? [value]
  (and (string? (:id value))
       (contains? value :clj-value)
       (contains? value :value)
       (contains? type->entry-keys (:type value))
       (or (not= :property-type-resource (:type value))
           (integer? (:sub-type value)))))

(defn build-target-go-props [resource-property-build-targets go-props-with-source-resources]
  (let [build-targets-by-source-resource (into {}
                                               (map (juxt (comp :resource :resource) identity))
                                               (flatten resource-property-build-targets))]
    (loop [go-props-with-source-resources go-props-with-source-resources
           go-props-with-build-resources (transient [])
           dep-build-targets (transient [])]
      (if-some [{:keys [clj-value sub-type type] :as go-prop} (first go-props-with-source-resources)]
        (do
          (assert (go-prop? go-prop))
          (let [dep-build-target (when (and (= :property-type-resource type) (some? clj-value))
                                   (if-some [build-target (build-targets-by-source-resource clj-value)]
                                     ;; Resolve .atlas reference into built .texturesetc or .texturec build target depending on property sub-type.
                                     ;; Doing this here means we can correctly handle sub-type changes. The texture set build targets are
                                     ;; invalidated whenever a dependent texture changes, so it should be safe to do so.
                                     (if (= sub-type-texture sub-type)
                                       (resolve-texture-build-target build-target)
                                       build-target)

                                     ;; If this fails, it is likely because resource-property-build-targets does not contain a referenced resource.
                                     (throw (ex-info (str "unable to resolve build target from value " (pr-str clj-value))
                                                     {:property (:id go-prop)
                                                      :resource-reference clj-value}))))
                build-resource (some-> dep-build-target :resource)
                go-prop (if (nil? build-resource)
                          go-prop
                          (assoc go-prop
                            :clj-value build-resource
                            :value (go-prop->str build-resource type)))]
            (recur (next go-props-with-source-resources)
                   (conj! go-props-with-build-resources go-prop)
                   (if (some? dep-build-target)
                     (conj! dep-build-targets dep-build-target)
                     dep-build-targets))))
        [(persistent! go-props-with-build-resources)
         (persistent! dep-build-targets)]))))

(defn build-go-props [dep-resources go-props-with-build-resources]
  (let [build-resource->fused-build-resource (fn [build-resource]
                                               (when (some? build-resource)
                                                 (or (dep-resources build-resource)
                                                     (throw (ex-info (str "unable to resolve fused build resource from referenced " (pr-str build-resource))
                                                                     {:resource-reference build-resource})))))
        go-props-with-fused-build-resources (mapv (fn [{:keys [clj-value type] :as go-prop}]
                                                    (assert (go-prop? go-prop))
                                                    (if (not= :property-type-resource type)
                                                      go-prop
                                                      (let [fused-build-resource (build-resource->fused-build-resource clj-value)]
                                                        (assoc go-prop
                                                          :clj-value fused-build-resource
                                                          :value (go-prop->str fused-build-resource type)))))
                                                  go-props-with-build-resources)]
    go-props-with-fused-build-resources))

(defn go-prop-resource-paths [go-props]
  (assert (every? go-prop? go-props))
  (into (sorted-set)
        (comp (filter #(= :property-type-resource (:type %)))
              (keep (comp not-empty :value)))
        go-props))
