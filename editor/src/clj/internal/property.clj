(ns internal.property
  (:require [clojure.core.match :refer [match]]
            [clojure.set :as set]
            [clojure.tools.macro :as ctm]
            [dynamo.util :as util]
            [internal.graph :as ig]
            [internal.graph.error-values :as ie]
            [internal.graph.types :as gt]
            [plumbing.core :refer [fnk]]
            [plumbing.fnk.pfnk :as pf]
            [schema.core :as s]))

(def ^:private default-validation-fn (constantly true))

(defn property-default-setter
  [basis node property value]
  (first (gt/replace-node basis (gt/node-id node) (assoc node property value))))

(defrecord PropertyTypeImpl
  [name value-type default tags dynamic]
  gt/PropertyType
  (property-value-type    [this]   value-type)
  (property-default-value [this]   (some-> default util/var-get-recursive util/apply-if-fn))
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

(defn kernel [name-kw]
  {:name (name name-kw)})

(defn validation [property] (get property ::validate))
(defn getter-for [property] (get property ::value))
(defn setter-for [property] (get property ::setter))

(defn property-dependencies
  [property]
  (concat (util/fnk-arguments (getter-for property))
          (util/fnk-arguments (validation property))
          (mapcat util/fnk-arguments (vals (gt/dynamic-attributes property)))))

(defn property-type-forms
  [name-kw value-type body-forms]
  (concat [`-> `(kernel ~name-kw)
           `(attach-value-type ~value-type)]
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
       (map->PropertyTypeImpl
        (cond-> description#
          (not (::value description#))
          (assoc ::value (fnk [~'this ~(symbol name-str)] (get ~'this ~name-kw))))))))

(defn def-property-type-descriptor
  [name-sym & body-forms]
  (let [[name-sym [value-type & body-forms]] (ctm/name-with-attributes name-sym body-forms)]
    `(def ~name-sym ~(property-type-descriptor (keyword name-sym) value-type body-forms))))
