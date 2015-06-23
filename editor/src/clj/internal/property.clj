(ns internal.property
  (:require [clojure.core.match :refer [match]]
            [dynamo.util :refer [apply-if-fn var-get-recursive]]
            [internal.graph.types :as gt]
            [schema.core :as s]))

(def ^:private default-validation-fn (constantly true))

(defn- validation-problems
  [value-type validations value]
  (if (s/check value-type value)
    (list "invalid value type")
    (keep identity
      (reduce
        (fn [errs {:keys [fn formatter]}]
          (conj errs
            (let [valid? (try (apply-if-fn (var-get-recursive fn) value) (catch Exception e false))]
              (when-not valid?
                (apply-if-fn (var-get-recursive formatter) value)))))
        []
        validations))))

(defrecord PropertyTypeImpl
  [value-type default validation visible tags enabled]
  gt/PropertyType
  (property-value-type    [this]   value-type)
  (property-default-value [this]   (some-> default var-get-recursive apply-if-fn))
  (property-validate      [this v] (validation-problems value-type (map second validation) v))
  (property-valid-value?  [this v] (empty? (validation-problems value-type (map second validation) v)))
  (property-enabled?      [this v] (or (nil? enabled) (apply-if-fn (var-get-recursive enabled) v)))
  (property-visible?      [this v] (or (nil? visible) (apply-if-fn (var-get-recursive visible) v)))
  (property-tags          [this]   tags))

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

    [(['enabled enablement-fn] :seq)]
    {:enabled (resolve-if-symbol enablement-fn)}

    [(['visible visibility-fn] :seq)]
    {:visible (resolve-if-symbol visibility-fn)}

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
         base-props#     (if (gt/property-type? value-type#)
                           value-type#
                           {:value-type value-type# :tags []})
         override-props# ~(mapv compile-defproperty-form body-forms)
         props#          (reduce merge-props base-props# override-props#)
         ; protocol detection heuristic based on private function `clojure.core/protocol?`
         protocol?#      (fn [~'p] (gt/protocol? ~'p))]
     (assert (or (nil? (:visible props#)) (gt/pfnk? (:visible props#)))
             (str "Property " '~name-sym " type " '~value-type " has a visibility function that should be an fnk, but isn't. " (:visible props#)))
     (assert (not (protocol?# value-type#))
             (str "Property " '~name-sym " type " '~value-type " looks like a protocol; try (schema.core/protocol " '~value-type ") instead."))
     (assert (or (satisfies? gt/PropertyType value-type#) (satisfies? s/Schema value-type#))
             (str "Property " '~name-sym " is declared with type " '~value-type " but that doesn't seem like a real value type"))
     (map->PropertyTypeImpl props#)))

(defn def-property-type-descriptor [name-sym value-type & body-forms]
  `(def ~name-sym ~(property-type-descriptor name-sym value-type body-forms)))
