(ns hooks.support-test-support
  (:require [clj-kondo.hooks-api :as api]))

(def implicit-clean-system-bindings
  '#{cache world})

(defn- binding-symbols [form]
  (into #{}
        (filter symbol?)
        (tree-seq coll? seq form)))

(declare free-symbols)

(defn- let-free-symbols [bound [_ bindings & body]]
  (let [pairs (partition 2 bindings)
        [init-symbols body-bound]
        (reduce
          (fn [[init-symbols bound] [binding init]]
            [(into init-symbols (free-symbols bound init))
             (into bound (binding-symbols binding))])
          [#{} bound]
          pairs)]
    (into init-symbols (mapcat #(free-symbols body-bound %)) body)))

(defn- fn-free-symbols [bound [_ argv & body]]
  (let [body-bound (into bound (binding-symbols argv))]
    (into #{} (mapcat #(free-symbols body-bound %)) body)))

(defn- free-symbols [bound form]
  (cond
    (symbol? form)
    (cond-> #{} (not (contains? bound form)) (conj form))

    (seq? form)
    (case (first form)
      let (let-free-symbols bound form)
      fn (fn-free-symbols bound form)
      fn* (fn-free-symbols bound form)
      (into #{} (mapcat #(free-symbols bound %)) form))

    (coll? form)
    (into #{} (mapcat #(free-symbols bound %)) form)

    :else
    #{}))

(defn- free-symbols-in-nodes [nodes]
  (into #{}
        (mapcat #(free-symbols #{} (api/sexpr %)))
        nodes))

(defn- implicit-binding-nodes [body]
  (let [body-symbols (free-symbols-in-nodes body)]
    (into []
          (mapcat (fn [sym]
                    (when (contains? body-symbols sym)
                      [(api/token-node sym)
                       (api/token-node nil)])))
          implicit-clean-system-bindings)))

(defn- identity-node [node]
  (api/list-node [(api/token-node 'identity)
                  node]))

(defn with-clean-system [{:keys [node]}]
  (let [[_ & forms] (:children node)
        configuration? (map? (api/sexpr (first forms)))
        configuration-node (when configuration? (first forms))
        body (cond-> forms configuration? next)
        binding-nodes (implicit-binding-nodes body)]
    {:node
     (api/list-node
       (list*
         (api/token-node 'let)
         (api/vector-node binding-nodes)
         (cond-> body
                 configuration?
                 (conj (identity-node configuration-node)))))}))
