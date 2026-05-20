(ns hooks.editor-gl-shader
  (:require [clj-kondo.hooks-api :as api]))

(defn defshader [{:keys [node]}]
  (let [[_ name-node] (:children node)]
    {:node
     (api/list-node
       [(api/token-node 'do)
        (api/list-node
          [(api/token-node 'def)
           name-node
           (api/token-node nil)])
        name-node])}))
