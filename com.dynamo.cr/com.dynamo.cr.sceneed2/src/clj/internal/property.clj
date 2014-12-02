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

(sm/defrecord PropertyTypeDescriptorImpl
  [name       :- String
   value-type :- s/Schema]
  t/PropertyTypeDescriptor
  (property-value-type    [this]   (:value-type this))
  (default-property-value [this]   (get-default-value this))
  (valid-property-value?  [this v] (valid-value? this v)))

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

    :else
    (assert false (str "invalid form within property type definition: " (pr-str form)))))

(defn property-type-descriptor [value-type body-forms]
  `(let [value-type#     ~value-type
         parent#         (when (satisfies? t/PropertyTypeDescriptor value-type#) value-type#)
         base-props#     (merge {:value-type value-type#} parent#)
         override-props# ~(mapv compile-defproperty-form body-forms)
         props#          (reduce merge base-props# override-props#)]
     (map->PropertyTypeDescriptorImpl props#)))

(defn def-property-type-descriptor [name-sym value-type & body-forms]
  `(def ~name-sym ~(property-type-descriptor value-type body-forms)))
