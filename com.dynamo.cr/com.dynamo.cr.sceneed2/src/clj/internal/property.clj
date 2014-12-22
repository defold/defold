(ns internal.property
  (:require [schema.core :as s]
            [schema.macros :as sm]
            [clojure.core.match :refer [match]]
            [dynamo.types :as t]))

(set! *warn-on-reflection* true)

(defn- get-default-value [property-type-descriptor]
  (some-> property-type-descriptor
          :default
          t/var-get-recursive
          t/apply-if-fn))

(def ^:private default-validation-fn (constantly true))

(defn- valid-value? [property-type-descriptor value]
  (s/validate (t/property-value-type property-type-descriptor) value)
  (-> property-type-descriptor
      (:validation default-validation-fn)
      t/var-get-recursive
      (t/apply-if-fn value)))

(sm/defrecord PropertyTypeImpl
  [value-type :- s/Schema]
  t/PropertyType
  (property-value-type    [this]   (:value-type this))
  (default-property-value [this]   (get-default-value this))
  (valid-property-value?  [this v] (valid-value? this v))
  (property-tags          [this]   (:tags this)))

(defn- resolve-if-symbol [sym]
  (if (symbol? sym)
    `(do
       ~sym ; eval to generate CompilerException when symbol cannot be resolved
       (resolve '~sym))
    sym))

(defn compile-defproperty-form [form]
  (match [form]
    [(['default default] :seq)]
    {:default (resolve-if-symbol default)}

    [(['validation validation-fn] :seq)]
    {:validation (resolve-if-symbol validation-fn)}

    [(['tag tag] :seq)]
    {:tags [tag]}

    :else
    (assert false (str "invalid form within property type definition: " (pr-str form)))))

(defn merge-props [props new-props]
  (-> (merge props new-props)
      (assoc :tags (concat (:tags new-props) (:tags props)))))

(defn property-type-descriptor [name-sym value-type body-forms]
  `(let [value-type#     ~value-type
         base-props#     (if (t/property-type? value-type#)
                           value-type#
                           {:value-type value-type# :tags []})
         override-props# ~(mapv compile-defproperty-form body-forms)
         props#          (reduce merge-props base-props# override-props#)
         ; protocol detection heuristic based on private function `clojure.core/protocol?`
         protocol?#      (fn [~'p] (and (map? ~'p) (contains? ~'p :on-interface)))]
     (assert (not (protocol?# value-type#)) (str "Property " '~name-sym " type " '~value-type " looks like a protocol; try (schema.core/protocol " '~value-type ") instead."))
     (map->PropertyTypeImpl props#)))

(defn def-property-type-descriptor [name-sym value-type & body-forms]
  `(def ~name-sym ~(property-type-descriptor name-sym value-type body-forms)))
