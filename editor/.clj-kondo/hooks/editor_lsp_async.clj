(ns hooks.editor-lsp-async
  (:require [clj-kondo.hooks-api :as api]))

(defn with-auto-evaluation-context [{:keys [node]}]
  (let [[_ evaluation-context-node & body] (:children node)]
    {:node
     (api/list-node
       (list*
         (api/token-node 'let)
         (api/vector-node
           [evaluation-context-node
            (api/list-node
              [(api/token-node 'dynamo.graph/make-evaluation-context)])])
         body))}))
