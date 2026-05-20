(ns hooks.integration-test-util
  (:require [clj-kondo.hooks-api :as api]))

(def implicit-loaded-project-bindings
  '#{app-view cache project world workspace})

(def implicit-scratch-project-bindings
  '#{app-view project workspace})

(def implicit-temp-project-bindings
  '#{app-view project project-path workspace})

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

(defn free-symbols-in-nodes [nodes]
  (into #{}
        (mapcat #(free-symbols #{} (api/sexpr %)))
        nodes))

(defn- option-nodes [nodes]
  (into []
        (comp (partition-all 2)
              (take-while (comp keyword? api/sexpr first))
              cat)
        nodes))

(defn- drop-option-nodes [nodes]
  (drop (count (option-nodes nodes)) nodes))

(defn implicit-binding-nodes [implicit-symbols body]
  (let [body-symbols (free-symbols-in-nodes body)]
    (into []
          (mapcat (fn [sym]
                    (when (contains? body-symbols sym)
                      [(api/token-node sym)
                       (api/token-node nil)])))
          implicit-symbols)))

(defn let-node [binding-nodes body]
  (api/list-node
    (list*
      (api/token-node 'let)
      (api/vector-node binding-nodes)
      body)))

(defn- identity-node [node]
  (api/list-node [(api/token-node 'identity)
                  node]))

(defn body-with-preserved-nodes [preserved-nodes body]
  (api/list-node
    (concat [(api/token-node 'do)]
            (map identity-node preserved-nodes)
            body)))

(defn with-loaded-project [{:keys [node]}]
  (let [[_ & forms] (:children node)
        first-form (first forms)
        custom-path? (let [sexpr (api/sexpr first-form)]
                       (or (string? sexpr) (symbol? sexpr)))
        path-nodes (if custom-path? [first-form] [])
        forms (cond-> forms custom-path? next)
        option-nodes (option-nodes forms)
        body (drop-option-nodes forms)
        preserved-nodes (cond-> path-nodes
                                (seq option-nodes)
                                (conj (api/map-node option-nodes)))
        binding-nodes (implicit-binding-nodes implicit-loaded-project-bindings body)]
    {:node (let-node binding-nodes [(body-with-preserved-nodes preserved-nodes body)])}))

(defn with-scratch-project [{:keys [node]}]
  (let [[_ project-path-node & forms] (:children node)
        option-nodes (option-nodes forms)
        body (drop-option-nodes forms)
        preserved-nodes (cond-> [project-path-node]
                                (seq option-nodes)
                                (conj (api/map-node option-nodes)))
        binding-nodes (implicit-binding-nodes implicit-scratch-project-bindings body)]
    {:node (let-node binding-nodes [(body-with-preserved-nodes preserved-nodes body)])}))

(defn with-temp-dir! [{:keys [node]}]
  (let [[_ name-node & body] (:children node)]
    {:node
     (let-node
       [name-node
        (api/token-node nil)]
       body)}))

(defn with-temp-project-content [{:keys [node]}]
  (let [[_ save-values-node & body] (:children node)
        binding-nodes (implicit-binding-nodes implicit-temp-project-bindings body)]
    {:node
     (let-node
       binding-nodes
       [(body-with-preserved-nodes [save-values-node] body)])}))
