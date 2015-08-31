(ns internal.property
  (:require [clojure.core.match :refer [match]]
            [clojure.tools.macro :as ctm]
            [dynamo.util :as util]
            [internal.graph :as ig]
            [internal.graph.types :as gt]
            [schema.core :as s]))

(def ^:private default-validation-fn (constantly true))

;; TODO - stop being redundant with the property argument to these.
(defn property-default-getter
  [basis node property]
  (get node property))

(defn property-default-setter
  [basis node property value]
  (first (gt/replace-node basis (gt/node-id node) (assoc node property value))))

(defn invoke-getter
  [basis node property]
  (when node
    (let [getter (get-in (gt/property-types node) [property :getter] property-default-getter)]
      (getter basis node property))))

(defn invoke-setter
  [basis node property new-value]
  (let [setter (get-in (gt/property-types node) [property :setter] property-default-setter)]
    (if-let [new-basis (setter basis node property new-value)]
      new-basis
      (do
        (println "WARNING: setter for " property " on " (gt/node-type node) " returned nil. It should return an updated basis.")
        basis))))

(defn- validation-problems
  [value-type validations value]
  (if (s/check value-type value)
    (list "invalid value type")
    (keep identity
      (reduce
        (fn [errs {:keys [fn formatter]}]
          (conj errs
            (let [valid? (try (util/apply-if-fn (util/var-get-recursive fn) value) (catch Exception e false))]
              (when-not valid?
                (util/apply-if-fn (util/var-get-recursive formatter) value)))))
        []
        validations))))

(defrecord PropertyTypeImpl
  [name value-type default validation tags dynamic]
  gt/PropertyType
  (property-value-type    [this]   value-type)
  (property-default-value [this]   (some-> default util/var-get-recursive util/apply-if-fn))
  (property-validate      [this v] (validation-problems value-type (map second validation) v))
  (property-valid-value?  [this v] (empty? (validation-problems value-type (map second validation) v)))
  (property-tags          [this]   tags)

  gt/Dynamics
  (dynamic-attributes     [this]   (util/map-vals util/var-get-recursive dynamic)))

(defn- assert-form-kind [kind-label required-kind label form]
  (assert (required-kind form) (str "property " label " requires a " kind-label " not a " (class form) " of " form)))

(def assert-symbol (partial assert-form-kind "symbol" symbol?))
(def assert-schema (partial assert-form-kind "schema" util/schema?))

(defn- resolve-if-symbol [sym]
  (if (symbol? sym)
    `(do
       ~sym ; eval to generate CompilerException when symbol cannot be resolved
       (resolve '~sym))
    sym))

(defn attach-default
  [description provider]
  (assoc description :default provider))

(defn attach-validation
  [description label formatter validation]
  (update description
          :validation #(conj (or %1 []) %2) [label {:fn validation :formatter formatter}]))

(defn attach-dynamic
  [description kind evaluation]
  (assoc-in description [:dynamic kind] evaluation))

(defn attach-setter
  [description evaluation]
  (assoc description :setter evaluation))

(defn attach-getter
  [description evaluation]
  (assoc description :getter evaluation))

(defn- property-form [form]
  (match [form]
         [(['default default] :seq)]
         `(attach-default ~default)

         [(['validate label :message formatter-fn & remainder] :seq)]
         (do
           (assert-symbol "validate" label)
           `(attach-validation ~(keyword label) ~formatter-fn ~@remainder))

         [(['validate label & remainder] :seq)]
         (do
           (assert-symbol "validate" label)
           `(attach-validation ~(keyword label) (fn [& _#] "invalid value") ~@remainder))

         [(['dynamic kind & remainder] :seq)]
         (do
           (assert-symbol "dynamic" kind)
           `(attach-dynamic ~(keyword kind) ~@remainder))

         [(['set & remainder] :seq)]
         `(attach-setter ~@remainder)

         [(['get & remainder] :seq)]
         `(attach-getter ~@remainder)

         :else
         (assert false (str "invalid form within property type definition: " (pr-str form)))))

(defn attach-value-type
  [description value-type]
  (if (gt/property-type? value-type)
    (merge description value-type)
    (do
      (assert-schema "defproperty" value-type)
      (assoc description :value-type value-type))))

(defn property-type-forms
  [name-str value-type body-forms]
  (concat [`-> {:name name-str}
           `(attach-value-type ~value-type)]
          (map property-form body-forms)))

(defn merge-props [props new-props]
  (let [merged (merge-with merge  (:dynamic props)    (:dynamic new-props))
        joined (merge-with concat (:validation props) (:validation new-props))
        tagged {:tags (into (vec (:tags new-props)) (:tags props))}]
    (merge props new-props merged joined tagged)))

(defn property-type-descriptor
  [name-str value-type body-forms]
  `(let [description# ~(property-type-forms name-str value-type body-forms)]
     (assert (or (nil? (:dynamic description#)) (gt/pfnk? (-> (:dynamic description#) vals first)))
             (str "Property " ~name-str " type " '~value-type " has a dynamic function that should be an fnk, but isn't. " (-> (:dynamic description#) vals first)))
     (assert (not (gt/protocol? ~value-type))
             (str "Property " ~name-str " type " '~value-type " looks like a protocol; try (dynamo.graph/protocol " '~value-type ") instead."))
     (assert (or (satisfies? gt/PropertyType ~value-type) (satisfies? s/Schema ~value-type))
             (str "Property " ~name-str " is declared with type " '~value-type " but that doesn't seem like a real value type"))
     (map->PropertyTypeImpl description#)))

(defn def-property-type-descriptor
  [name-sym & body-forms]
  (let [[name-sym [value-type & body-forms]] (ctm/name-with-attributes name-sym body-forms)]
    `(def ~name-sym ~(property-type-descriptor (str name-sym) value-type body-forms))))
