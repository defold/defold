(ns hooks.util-fn
  (:require [clj-kondo.hooks-api :as api]))

(defn defamong [{:keys [node]}]
  (let [[_ name-node valid-values-node] (:children node)]
    {:node
     (api/list-node
       [(api/token-node 'defn)
        name-node
        (api/vector-node [(api/token-node 'value)])
        (api/list-node
          [(api/token-node 'contains?)
           valid-values-node
           (api/token-node 'value)])])}))
