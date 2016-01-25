(ns internal.property
  (:require [clojure.core.match :refer [match]]
            [clojure.set :as set]
            [clojure.tools.macro :as ctm]
            [internal.util :as util]
            [internal.graph :as ig]
            [internal.graph.error-values :as ie]
            [internal.graph.types :as gt]
            [plumbing.core :refer [fnk]]
            [plumbing.fnk.pfnk :as pf]
            [schema.core :as s]))

(def ^:private default-validation-fn (constantly true))

(defn property-default-setter
  [basis node property _ new-value]
  (first (gt/replace-node basis node (gt/set-property (ig/node-by-id-at basis node) basis property new-value))))

(defrecord PropertyTypeImpl
  [name value-type default tags dynamic]
  gt/PropertyType
  (property-value-type    [this]   value-type)
  (property-default-value [this]   (some-> default util/var-get-recursive util/apply-if-fn))
  (property-tags          [this]   tags)

  gt/Dynamics
  (dynamic-attributes     [this]   (util/map-vals util/var-get-recursive dynamic)))

(defn make-property-type [key type]
  (->PropertyTypeImpl (name key) type nil nil nil))

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

(defn- property-form [form]
  (match [form]
         [(['default default] :seq)]
         `(assoc :default ~default)

         [(['validate & remainder] :seq)]
         `(assoc ::validate ~@remainder)

         [(['dynamic kind & remainder] :seq)]
         (do
           (assert-symbol "dynamic" kind)
           `(assoc-in [:dynamic ~(keyword kind)] ~@remainder))

         [(['set & remainder] :seq)]
         `(assoc ::setter ~@remainder)

         [(['value & remainder] :seq)]
         `(assoc ::value ~@remainder)

         :else
         (assert false (str "invalid form within property type definition: " (pr-str form)))))

(defn attach-value-type
  [description value-type]
  (if (gt/property-type? value-type)
    (merge description value-type)
    (do
      (assert-schema "defproperty" value-type)
      (assoc description :value-type value-type))))

(defn validation [property] (get property ::validate))
(defn getter-for [property] (get property ::value))
(defn setter-for [property] (get property ::setter))

(defn property-dependencies
  [property]
  (concat (or (util/fnk-arguments (getter-for property)) #{(keyword (:name property))})
          (util/fnk-arguments (validation property))
          (mapcat util/fnk-arguments (vals (gt/dynamic-attributes property)))))

(defn parse-forms
  [desc-sym name-kw value-type body-forms & tail]
  `(let [~desc-sym (-> {:name ~(name name-kw)}
                       (attach-value-type ~value-type)
                       ~@(map property-form body-forms))]
     ~@tail))

(defn- check-dynamics-for-schemas
  [desc-sym name-str value-type]
  `(assert (every? gt/pfnk? (vals (:dynamic ~desc-sym)))
           (str "Property " ~name-str " type " '~value-type " has a dynamic function that should be an fnk, but isn't.")))

(defn- check-for-protocol-type
  [desc-sym name-str value-type]
  `(assert (not (gt/protocol? ~value-type))
           (str "Property " ~name-str " type " '~value-type " looks like a protocol; try (dynamo.graph/protocol " '~value-type ") instead.")))

(defn- check-for-invalid-type
  [desc-sym name-str value-type]
  `(assert (or (satisfies? gt/PropertyType ~value-type) (satisfies? s/Schema ~value-type))
               (str "Property " ~name-str " is declared with type " '~value-type " but that doesn't seem like a real value type")))

(defn- form-tail
  [body-forms match]
  (let [[[_ tail] & _] (filter #(= match (first %)) body-forms)]
    tail))

(defn default-getter
  [name-kw]
  `(fnk [~'this ~(symbol (name name-kw))] (get ~'this ~name-kw)))

(defn default-getter?
  [property-type]
  (boolean (::default-getter property-type)))

(defn attach-evaluation
  [desc-sym name-kw name-str value-type body-forms & tail]
  (let [validation-fnk (form-tail body-forms 'validate)
        value-fnk      (form-tail body-forms 'value)]
    (if value-fnk
      `(let [~desc-sym (assoc ~desc-sym ::value ~value-fnk)]
         ~@tail)
      `(let [~desc-sym (assoc ~desc-sym ::default-getter true)]
         ~@tail))))

(defn build-impl
  [desc-sym]
  `(map->PropertyTypeImpl ~desc-sym))

(defn property-type-descriptor
  [name-kw value-type body-forms]
  (let [name-str        (name name-kw)
        description-sym (gensym "description")]
    (parse-forms description-sym name-kw value-type body-forms
                 (check-dynamics-for-schemas description-sym name-str value-type)
                 (check-for-protocol-type description-sym name-str value-type)
                 (check-for-invalid-type description-sym name-str value-type)
                 (attach-evaluation description-sym name-kw name-str value-type body-forms
                                    (build-impl description-sym)))))

(defn def-property-type-descriptor
  [name-sym & body-forms]
  (let [[name-sym [value-type & body-forms]] (ctm/name-with-attributes name-sym body-forms)]
    `(def ~name-sym ~(property-type-descriptor (keyword name-sym) value-type body-forms))))
