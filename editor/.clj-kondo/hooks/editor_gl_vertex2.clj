(ns hooks.editor-gl-vertex2
  (:require [clj-kondo.hooks-api :as api]))

(defn- ctor-symbol [name-sym]
  (symbol (str "->" name-sym)))

(defn- put-symbol [name-sym]
  (symbol (str name-sym "-put!")))

(defn- function-node []
  (api/list-node
    [(api/token-node 'fn)
     (api/vector-node [(api/token-node '&) (api/token-node '_args__)])
     (api/token-node nil)]))

(defn defvertex [{:keys [node]}]
  (let [[_ name-node] (:children node)
        name-sym (api/sexpr name-node)
        nodes (cond-> [(api/token-node 'do)
                       (api/list-node
                         [(api/token-node 'def)
                          name-node
                          (api/token-node nil)])
                       (api/list-node
                         [(api/token-node 'def)
                          (api/token-node (ctor-symbol name-sym))
                          (function-node)])]
                (not (:no-put (meta name-sym)))
                (conj
                  (api/list-node
                    [(api/token-node 'def)
                     (api/token-node (put-symbol name-sym))
                     (function-node)])))]
    {:node
     (api/list-node nodes)}))
