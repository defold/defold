(ns internal.property
  (:require [clojure.core.match :refer [match]]
            [clojure.tools.macro :as ctm]
            [dynamo.util :as util]
            [internal.graph :as ig]
            [internal.graph.types :as gt]
            [plumbing.core :refer [fnk]]
            [schema.core :as s]))

(def ^:private default-validation-fn (constantly true))

;; TODO - stop being redundant with the property argument to these.
(defn- default-getter
  [property]
  (eval `(fnk [~'this ~(symbol (name property))] (get ~'this ~property))))

(defn property-default-setter
  [basis node property value]
  (first (gt/replace-node basis (gt/node-id node) (assoc node property value))))

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

         [(['value & remainder] :seq)]
         `(attach-dynamic :internal.property/value ~@remainder)

         :else
         (assert false (str "invalid form within property type definition: " (pr-str form)))))

(defn attach-value-type
  [description value-type]
  (if (gt/property-type? value-type)
    (merge description value-type)
    (do
      (assert-schema "defproperty" value-type)
      (assoc description :value-type value-type))))

(defn kernel [name-kw]
  {:name (name name-kw)})

(defn getter-for [property] (get-in property [:dynamic :internal.property/value]))
(defn setter-for [property] (get property :setter))

(defn attach-default-getter
  [description name-kw]
  (assoc-in description [:dynamic :internal.property/value] (default-getter name-kw)))

(defn property-type-forms
  [name-kw value-type body-forms]
  (concat [`-> `(kernel ~name-kw)
           `(attach-value-type ~value-type)
           `(attach-default-getter ~name-kw)]
          (map property-form body-forms)))

(defn property-type-descriptor
  [name-kw value-type body-forms]
  (let [name-str (name name-kw)]
    `(let [description# ~(property-type-forms name-kw value-type body-forms)]
       (assert (or (nil? (:dynamic description#)) (every? gt/pfnk? (vals (:dynamic description#))))
               (str "Property " ~name-str " type " '~value-type " has a dynamic function that should be an fnk, but isn't. " (-> (:dynamic description#) vals first)))
       (assert (not (gt/protocol? ~value-type))
               (str "Property " ~name-str " type " '~value-type " looks like a protocol; try (dynamo.graph/protocol " '~value-type ") instead."))
       (assert (or (satisfies? gt/PropertyType ~value-type) (satisfies? s/Schema ~value-type))
               (str "Property " ~name-str " is declared with type " '~value-type " but that doesn't seem like a real value type"))
       (map->PropertyTypeImpl description#))))

(defn def-property-type-descriptor
  [name-sym & body-forms]
  (let [[name-sym [value-type & body-forms]] (ctm/name-with-attributes name-sym body-forms)]
    `(def ~name-sym ~(property-type-descriptor (keyword name-sym) value-type body-forms))))
