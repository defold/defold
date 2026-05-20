(ns hooks.editor-handler
  (:require [clj-kondo.hooks-api :as api]))

(defn- call-node? [node]
  (seq (:children node)))

(defn- function-clause-node? [node]
  (and (call-node? node)
       (symbol? (api/sexpr (first (:children node))))
       (vector? (api/sexpr (second (:children node))))))

(defn- get-node [map-sym dep]
  (api/list-node
    [(api/token-node 'get)
     (api/token-node map-sym)
     (api/token-node (keyword dep))]))

(defn- binding-nodes [map-sym argv-node]
  (let [deps (vec (api/sexpr argv-node))]
    (into []
          (concat
            (mapcat
              (fn [dep]
                [(api/token-node dep)
                 (get-node map-sym dep)])
              deps)
            (when (seq deps)
              [(api/token-node (gensym "_handler_deps__"))
               (api/vector-node (mapv api/token-node deps))])))))

(defn- fnk-node [clause-node]
  (let [[_ argv-node & body] (:children clause-node)
        env-sym (gensym (if (seq (api/sexpr argv-node))
                          "handler-env__"
                          "_handler-env__"))]
    (api/list-node
      [(api/token-node 'fn)
       (api/vector-node [(api/token-node env-sym)])
       (api/list-node
         (list*
           (api/token-node 'let)
           (api/vector-node (binding-nodes env-sym argv-node))
           body))])))

(defn- body-nodes [body]
  (loop [result []
         [node & more] body]
    (cond
      (nil? node)
      result

      (keyword? (api/sexpr node))
      (recur (cond-> result (first more) (conj (first more)))
             (rest more))

      (function-clause-node? node)
      (recur (conj result (fnk-node node)) more)

      :else
      (recur (conj result node) more))))

(defn defhandler [{:keys [node]}]
  (let [[_ _command-node _context-node & body] (:children node)]
    {:node
     (api/list-node
       (list*
         (api/token-node 'do)
         (body-nodes body)))}))
