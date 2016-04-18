(ns internal.property
  (:require [internal.util :as util]
            [internal.graph :as ig]
            [internal.graph.types :as gt]))

(set! *warn-on-reflection* true)

(defn property-default-setter
  [basis node property _ new-value]
  (first (gt/replace-node basis node (gt/set-property (ig/node-by-id-at basis node) basis property new-value))))

(defn property-value-type
  [x]
  (cond
    (and (map? x)
         (contains? x :value-type)) x
    (symbol? x)                     (property-value-type (resolve x))
    (var? x)                        (property-value-type (var-get x))
    :else                           {:value-type x}))

(def  property-tags          :tags)
(defn property-default-value [this] (some-> this :default util/var-get-recursive util/apply-if-fn))
(defn dynamic-attributes     [this] (util/map-vals util/var-get-recursive (:dynamic this)))
(defn default-getter?        [this] (not (contains? this ::value)))

(def assert-symbol (partial util/assert-form-kind "property" "symbol" symbol?))

(defn assert-schema
  [place label form]
  (if-not (util/schema? form)
    (let [resolved-schema   (util/resolve-schema form)
          underlying-schema (if (util/property? resolved-schema) (:value-type resolved-schema) resolved-schema)]
      (util/assert-form-kind place label util/schema? "schema" (util/resolve-schema underlying-schema)))))

(defn- property-form [description form]
  (case (first form)
    default
    (assoc description :default (second form))

    validate
    (assoc description ::validate (second form))

    dynamic
    (let [[_ kind remainder] form]
      (assert-symbol "dynamic" kind)
      (assoc-in description [:dynamic (keyword kind)] remainder))

    set
    (assoc description ::setter (second form))

    value
    (assoc description ::value (second form))

    (assert false (str "invalid form within property type definition: " (pr-str form)))))

(def validation ::validate)
(def getter-for ::value)
(def setter-for ::setter)

(defn property-dependencies
  [property]
  (reduce into #{} [(util/inputs-needed (getter-for property))
                    (util/inputs-needed (validation property))
                    (mapcat util/inputs-needed (vals (dynamic-attributes property)))]))

(defn- check-for-protocol-type
  [label prop]
  (let [value-type (:value-type prop)]
    (assert (not (gt/protocol? value-type))
            (str "Property " label " type " value-type " looks like a protocol; try (dynamo.graph/protocol " value-type ") instead."))))

(defn- check-for-invalid-type
  [label value-type]
  (assert (or (util/property? value-type)
              (util/schema?   value-type))
          (str "Property " label " is declared with type " value-type " but that doesn't seem like a real value type")))

(defn property-type-descriptor
  [label value-type body-forms]
  (let [prop (-> (property-value-type value-type)
                 (assoc :name (str label)))]
    (assert-schema "property" label value-type)
    (check-for-protocol-type label prop)
    (check-for-invalid-type label prop)
    (reduce property-form prop body-forms)))

(defn def-property-type-descriptor
  [name-sym value-type & body-forms]
  `(def ~name-sym ~(property-type-descriptor name-sym value-type body-forms)))
