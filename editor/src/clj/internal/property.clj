(ns internal.property
  (:require [clojure.core.match :refer [match]]
            [dynamo.types :as t]))

(defn- var-get-recursive [var-or-value]
  (if (var? var-or-value)
    (recur (var-get var-or-value))
    var-or-value))

(defn- apply-if-fn [f & args]
  (if (fn? f)
    (apply f args)
    f))

(defn- get-default-value [property-type-descriptor]
  (some-> property-type-descriptor
          :default
          var-get-recursive
          apply-if-fn))

(def ^:private default-validation-fn (constantly true))

(defn- validations
  [property-type-descriptor]
  (->> property-type-descriptor
    :validation
    (map second)))

(defn- validation-problems
  [property-type-descriptor value]
  (if (t/check (t/property-value-type property-type-descriptor) value)
    (list "invalid value type")
    (keep identity
      (reduce
        (fn [errs {:keys [fn formatter]}]
          (conj errs
            (let [valid? (try (apply-if-fn (var-get-recursive fn) value) (catch Exception e false))]
              (when-not valid?
                (apply-if-fn (var-get-recursive formatter) value)))))
        []
        (validations property-type-descriptor)))))

(defn- valid-value?
  [property-type-descriptor value]
  (empty? (validation-problems property-type-descriptor value)))

(defrecord PropertyTypeImpl
  [value-type]
  t/PropertyType
  (property-value-type    [this]   (:value-type this))
  (property-default-value [this]   (get-default-value this))
  (property-validate      [this v] (validation-problems this v))
  (property-valid-value?  [this v] (valid-value? this v))
  (property-visible       [this]   (if (contains? this :visible) (:visible this) true))
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

    [(['validate label :message formatter-fn validation-fn] :seq)]
    {:validation {(keyword label) {:fn (resolve-if-symbol validation-fn)
                                   :formatter (resolve-if-symbol formatter-fn)}}}

    [(['validate label validation-fn] :seq)]
    {:validation [[(keyword label) {:fn (resolve-if-symbol validation-fn)
                                   :formatter "invalid value"}]]}

    [(['validation validation-fn] :seq)]
    {:validation [[(keyword (gensym)) {:fn (resolve-if-symbol validation-fn)
                                    :formatter "invalid value"}]]}

    [(['visible visibility] :seq)]
    {:visible (resolve-if-symbol visibility)}

    [(['tag tag] :seq)]
    {:tags [tag]}

    :else
    (assert false (str "invalid form within property type definition: " (pr-str form)))))

(defn merge-props [props new-props]
  (-> (merge (dissoc props :validation) (dissoc new-props :validation))
    (assoc :validation (concat (:validation props) (:validation new-props)))
    (assoc :tags (into (vec (:tags new-props)) (:tags props)))))

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
