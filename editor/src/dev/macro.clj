(ns macro
  (:require [clojure.pprint :as pprint]
            [clojure.string :as string]
            [clojure.walk :as walk]
            [util.coll :as coll :refer [pair]]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn- ns->namespace-name
  ^String [ns]
  (name (ns-name ns)))

(defn- class->canonical-symbol [^Class class]
  (symbol (.getName class)))

(defn- make-alias-names-by-namespace-name [ns]
  (into {(ns->namespace-name 'clojure.core) nil
         (ns->namespace-name ns) nil}
        (map (fn [[alias-symbol referenced-ns]]
               (pair (ns->namespace-name referenced-ns)
                     (name alias-symbol))))
        (ns-aliases ns)))

(defn- make-simple-symbols-by-canonical-symbol [ns]
  (into {}
        (map (fn [[alias-symbol imported-class]]
               (pair (class->canonical-symbol imported-class)
                     alias-symbol)))
        (ns-imports ns)))

(defn- simplify-namespace-name
  ^String [namespace-name alias-names-by-namespace-name]
  {:pre [(or (nil? namespace-name) (string? namespace-name))
         (map? alias-names-by-namespace-name)]}
  (let [alias-name (get alias-names-by-namespace-name namespace-name ::not-found)]
    (case alias-name
      ::not-found namespace-name
      alias-name)))

(defn- claim-simplified-symbol-name!
  ^String [symbol-name name-without-suffix simple-names-by-gensym-name-atom]
  {:pre [(string? symbol-name)
         (not-empty symbol-name)
         (string? name-without-suffix)
         (not-empty name-without-suffix)]}
  (-> simple-names-by-gensym-name-atom
      (swap! (fn [simple-names-by-gensym-name]
               (if (contains? simple-names-by-gensym-name symbol-name)
                 simple-names-by-gensym-name
                 (assoc simple-names-by-gensym-name
                   symbol-name
                   (let [taken-simple-name? (set (vals simple-names-by-gensym-name))]
                     (loop [suffix 0]
                       (let [simple-name (case suffix
                                           0 (str name-without-suffix \#)
                                           (str name-without-suffix \# suffix))]
                         (if (taken-simple-name? simple-name)
                           (recur (inc suffix))
                           simple-name))))))))
      (get symbol-name)))

(defn- simplify-symbol-name [symbol-name simple-names-by-gensym-name-atom]
  (let [name-without-hash-auto-suffix (string/replace symbol-name
                                                      #"__(\d+)__auto__$"
                                                      "")]
    (if (not= symbol-name name-without-hash-auto-suffix)
      (claim-simplified-symbol-name! symbol-name name-without-hash-auto-suffix simple-names-by-gensym-name-atom)
      (let [name-without-gensym-suffix (string/replace symbol-name
                                                       #"__(\d+)$"
                                                       "")]
        (if (not= symbol-name name-without-gensym-suffix)
          (claim-simplified-symbol-name! symbol-name name-without-gensym-suffix simple-names-by-gensym-name-atom)
          symbol-name)))))

(defn- simplify-symbol [expression alias-names-by-namespace-name simple-names-by-gensym-name-atom]
  (-> expression
      (namespace)
      (simplify-namespace-name alias-names-by-namespace-name)
      (symbol (-> expression name (simplify-symbol-name simple-names-by-gensym-name-atom)))
      (with-meta (meta expression))))

(defn- simplify-keyword [expression alias-names-by-namespace-name]
  (-> expression
      (namespace)
      (simplify-namespace-name alias-names-by-namespace-name)
      (keyword (name expression))))

(defn- simplify-expression-impl [expression alias-names-by-namespace-name simple-symbols-by-canonical-symbol simple-names-by-gensym-name-atom]
  (cond
    (record? expression)
    expression

    (map? expression)
    (into (coll/empty-with-meta expression)
          (map (fn [[key value]]
                 (pair (simplify-expression-impl key alias-names-by-namespace-name simple-symbols-by-canonical-symbol simple-names-by-gensym-name-atom)
                       (simplify-expression-impl value alias-names-by-namespace-name simple-symbols-by-canonical-symbol simple-names-by-gensym-name-atom))))
          expression)

    (or (vector? expression)
        (set? expression))
    (into (coll/empty-with-meta expression)
          (map #(simplify-expression-impl % alias-names-by-namespace-name simple-symbols-by-canonical-symbol simple-names-by-gensym-name-atom))
          expression)

    (coll/list-or-cons? expression)
    (into (coll/empty-with-meta expression)
          (map #(simplify-expression-impl % alias-names-by-namespace-name simple-symbols-by-canonical-symbol simple-names-by-gensym-name-atom))
          (reverse expression))

    (symbol? expression)
    (or (prn expression)
        (get simple-symbols-by-canonical-symbol expression)
        (simplify-symbol expression alias-names-by-namespace-name simple-names-by-gensym-name-atom))

    (keyword? expression)
    (simplify-keyword expression alias-names-by-namespace-name)

    :else
    expression))

(defmacro simplify-expression
  ([expression]
   `(simplify-expression *ns* ~expression))
  ([ns expression]
   `(let [ns# ~ns]
      (#'simplify-expression-impl
        ~expression
        (#'make-alias-names-by-namespace-name ns#)
        (#'make-simple-symbols-by-canonical-symbol ns#)
        (atom {})))))

(defn- pprint-code-impl [expression]
  (binding [pprint/*print-suppress-namespaces* false
            pprint/*print-right-margin* 100
            pprint/*print-miser-width* 60]
    (pprint/with-pprint-dispatch
      pprint/code-dispatch
      (pprint/pprint expression))))

(defmacro pprint-code
  "Pretty-print the supplied code expression while attempting to retain readable
  formatting. Useful when developing macros."
  ([expression]
   `(#'pprint-code-impl (simplify-expression ~expression)))
  ([ns expression]
   `(#'pprint-code-impl (simplify-expression ~ns ~expression))))

(defmacro pprint-macroexpanded
  "Recursively macroexpand and pretty-print the supplied code expression while
  attempting to retain readable formatting. Useful when developing macros."
  [expression]
  `(pprint-code
     ~(walk/macroexpand-all expression)))

(defmacro macroexpanded
  "Recursively macroexpand the supplied code expression while trying to keep the
  output as human-readable as possible. Useful when developing macros."
  ([expression]
   `(simplify-expression ~(walk/macroexpand-all expression)))
  ([ns expression]
   `(simplify-expression ~ns ~(walk/macroexpand-all expression))))
