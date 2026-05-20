(ns hooks.editor-gl
  (:require [clj-kondo.hooks-api :as api]))

(declare free-symbols)

(defn- binding-symbols [form]
  (into #{}
        (filter symbol?)
        (tree-seq coll? seq form)))

(defn- let-free-symbols [bound [_ bindings & body]]
  (let [pairs (partition 2 bindings)
        init-symbols (into #{}
                           (mapcat #(free-symbols bound (second %)))
                           pairs)
        body-bound (into bound (mapcat (comp binding-symbols first)) pairs)]
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

(defn- doto-body-node [gl-node body-node]
  (if (seq? (api/sexpr body-node))
    (let [[head & args] (:children body-node)]
      (api/list-node (list* head gl-node args)))
    body-node))

(defn- gl-begin-node [gl-node type-node body]
  (let [gl-sym (gensym "gl__")]
    (api/list-node
      (list
        (api/token-node 'let)
        (api/vector-node [(api/token-node gl-sym)
                          gl-node])
        (api/list-node
          (concat [(api/token-node 'do)
                   (api/list-node [(api/token-node 'identity)
                                   (api/token-node gl-sym)])
                   (api/list-node [(api/token-node 'identity)
                                   type-node])]
                  (map #(doto-body-node (api/token-node gl-sym) %) body)))))))

(defn- gl-doto-node [gl-node body]
  (let [gl-sym (gensym "gl__")]
    (api/list-node
      (list
        (api/token-node 'let)
        (api/vector-node [(api/token-node gl-sym)
                          gl-node])
        (api/list-node
          (list*
            (api/token-node 'do)
            (api/list-node [(api/token-node 'identity)
                            (api/token-node gl-sym)])
            (map #(doto-body-node (api/token-node gl-sym) %) body)))))))

(defn gl-begin [{:keys [node]}]
  (let [[_ gl-node type-node & body] (:children node)]
    {:node (gl-begin-node gl-node type-node body)}))

(defn gl-quads [{:keys [node]}]
  (let [[_ gl-node & body] (:children node)]
    {:node (gl-doto-node gl-node body)}))

(defn gl-lines [{:keys [node]}]
  (let [[_ gl-node & body] (:children node)]
    {:node (gl-doto-node gl-node body)}))

(defn gl-triangles [{:keys [node]}]
  (let [[_ gl-node & body] (:children node)]
    {:node (gl-doto-node gl-node body)}))

(defn with-drawable-as-current [{:keys [node]}]
  (let [[_ drawable-node & body] (:children node)
        body-symbols (free-symbols-in-nodes body)]
    {:node
     (api/list-node
       (list*
         (api/token-node 'let)
         (api/vector-node
           (into [(api/token-node (gensym "_drawable__"))
                  drawable-node]
                 (cond-> []
                         (contains? body-symbols 'gl-context)
                         (into [(api/token-node 'gl-context)
                                (api/token-node nil)])

                         (contains? body-symbols 'gl)
                         (into [(api/token-node 'gl)
                                (api/token-node nil)]))))
         body))}))
