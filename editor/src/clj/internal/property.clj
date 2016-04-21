(ns internal.property
  (:require [internal.util :as util]
            [internal.graph :as ig]
            [internal.graph.types :as gt]
            [internal.property :as ip]
            [plumbing.core :as p]
            [schema.core :as s]))

(set! *warn-on-reflection* true)

;;; ----------------------------------------
;;; Runtime API
;;; ----------------------------------------

(defn property-default-setter
  [basis node property _ new-value]
  (first (gt/replace-node basis node (gt/set-property (ig/node-by-id-at basis node) basis property new-value))))

(defn property-default-value [this] (some-> this ::default util/var-get-recursive util/apply-if-fn))
(defn default-getter?        [this] (not (contains? this ::value)))

(def  value-type   ::value-type)
(def  dynamics     ::dynamic)
(def  default      ::default)
(def  validation   ::validate)
(def  getter-for   ::value)
(def  setter-for   ::setter)
(def  dependencies ::dependencies)

(defn has-dynamics?       [x]   (not (empty? (dynamics x))))
(defn dynamic             [x d] (-> x dynamics (get d)))
(defn dynamic-arguments   [x d] (-> x (dynamic d) :arguments))
(defn dynamic-evaluator   [x d] (-> x (dynamic d) :fn))

(defn validation-evaluator [x]  (-> x validation :fn))
(defn validation-arguments [x]  (-> x validation :arguments))

;; ----------------------------------------
;; Compiling
;; ----------------------------------------

;; During compilation, it is important to leave symbols as
;; symbols. That way they can be emitted by the defnode macro as
;; symbols for the compiler to resolve. Otherwise, we get function
;; values being emitted, which can't be used as input to the compiler.

;; Despite that, we need to collect some compile time information by
;; either extracting it from the source (for a standalone property
;; definition) or from the supertype (in the case of inheritance.)

(defn- make-fnky-call [form]
  {:fn        (if (or (seq? form) (symbol? form) (var? form))
                form
                `(p/fnk [] ~form))
   :arguments (util/inputs-needed form)})

(defn- property-form [description form]
  (case (first form)
    default
    (assoc description ::default (second form))

    validate
    (assoc description ::validate (make-fnky-call (second form)))

    dynamic
    (let [[_ kind remainder] form]
      (util/assert-form-kind "property" "symbol" symbol? "dynamic" kind)
      (assoc-in description [::dynamic (keyword kind)] (make-fnky-call remainder)))

    set
    (assoc description ::setter (second form))

    value
    (assoc description ::value (second form))

    (assert false (str "invalid form within property type definition: " (pr-str form)))))

(defn- property-dependencies
  [property]
  (reduce into #{} [(util/inputs-needed (getter-for property))
                    (util/inputs-needed (:fn (validation property)))
                    (mapcat (comp util/inputs-needed :fn) (vals (dynamics property)))]))

(defn- wrap-protocol
  [property tp-form]
  (cond-> property
    (util/protocol? (value-type property))
    (assoc ::value-type `(s/protocol ~tp-form))))

(defn- check-for-invalid-type
  [{:keys [::value-type ::value-type-name]}]
  (assert (util/schema?   value-type)
          (str value-type-name " doesn't seem like a real value type")))

(defn resolve-value-type
  [x]
  (cond
    (symbol? x)                     (resolve-value-type (resolve x))
    (var? x)                        (resolve-value-type (var-get x))
    (vector? x)                     (mapv resolve-value-type x)
    :else                           x))

(defn- update-dependencies
  [prop]
  (update prop ::dependencies into (property-dependencies prop)))

(defn assert-schema
  [form tp]
  (assert (util/schema? tp) (str (pr-str form) " doesn't seem like a real schema"))
  (assert (not (nil? tp))   (str (pr-str form) " doesn't refer to a schema in this context.")))

(defn property-type-descriptor
  "value-type may be one of:
   - symbol that refers to a Java class
   - symbol that refers to a Prismatic schema
   - var that refers to a Prismatic schema
   - a literal schema (including vector and map forms)"
  [tp-form body-forms]
  (let [tp   (resolve-value-type tp-form)
        prop {::value-type   tp
              ::dependencies #{}}]
    (assert-schema tp-form tp)
    (-> (reduce property-form prop body-forms)
        (wrap-protocol tp-form)
        update-dependencies)))
