(ns internal.property
  (:require [schema.core :as s]
            [schema.macros :as sm]
            [clojure.core.match :refer [match]]
            [dynamo.types :as t]))

(set! *warn-on-reflection* true)

(defn- get-default-value [property-type-descriptor]
  (assert (contains? property-type-descriptor :default)
          (str "No default defined for property " (:name property-type-descriptor)))
  (-> property-type-descriptor
      :default
      t/var-get-recursive
      t/apply-if-fn))

(sm/defrecord PropertyTypeDescriptorImpl
  [name       :- String
   value-type :- s/Schema]
  t/PropertyTypeDescriptor
  (default-property-value [this] (get-default-value this)))

(defn compile-defproperty-form [form]
  (match [form]
    [(['default default] :seq)]
    {:default (if (symbol? default)
                `(do
                   ~default ; eval to generate CompilerException when symbol cannot be resolved
                   (resolve '~default))
                default)}

    :else
    (assert false (str "invalid form within property type definition: " (pr-str form)))))

(defn property-type-descriptor [name-str value-type body-forms]
  (let [props (->> body-forms
                (map compile-defproperty-form)
                (reduce merge {:name name-str :value-type value-type}))]
    `(map->PropertyTypeDescriptorImpl ~props)))

(defn def-property-type-descriptor [name-sym value-type & body-forms]
  `(def ~name-sym ~(property-type-descriptor (str name-sym) value-type body-forms)))
