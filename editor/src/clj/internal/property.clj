(ns internal.property
  (:require [clojure.tools.macro :as ctm]
            [clojure.core.match :refer [match]]
            [dynamo.util :as util]
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
            (let [valid? (try (util/apply-if-fn (util/var-get-recursive fn) value) (catch Exception e false))]
              (when-not valid?
                (util/apply-if-fn (util/var-get-recursive formatter) value)))))
        []
        validations))))

(defn property-validate      [p v] (validation-problems (:value-type p) (map second (:validation p)) v))
(defn property-default-value [p]   (some-> (:default p) util/var-get-recursive util/apply-if-fn))
(defn property-value-type    [p]   (:value-type p))
(defn dynamic-attributes     [p]   (util/map-vals util/var-get-recursive (:dynamic p)))

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

         :else
         (assert false (str "invalid form within property definition: " (pr-str form)))))

(defn attach-value-type
  [description value-type]
  (assert-schema "property" value-type)
  (assoc description :value-type value-type))

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
     (assert (satisfies? s/Schema ~value-type)
             (str "Property " ~name-str " is declared with type " '~value-type " but that doesn't seem like a real value type"))
     description#))
