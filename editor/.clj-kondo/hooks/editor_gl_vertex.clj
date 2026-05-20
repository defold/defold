(ns hooks.editor-gl-vertex
  (:require [clj-kondo.hooks-api :as api]))

(defn- ctor-symbol [name-sym]
  (symbol (str "->" name-sym)))

(defn defvertex [{:keys [node]}]
  (let [[_ name-node] (:children node)
        name-sym (api/sexpr name-node)
        nodes [(api/token-node 'do)
               (api/list-node
                 [(api/token-node 'def)
                  name-node
                  (api/token-node nil)])
               (api/list-node
                 [(api/token-node 'def)
                  (api/token-node (ctor-symbol name-sym))
                  (api/token-node nil)])]]
    {:node
     (api/list-node nodes)}))
