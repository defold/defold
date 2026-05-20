(ns hooks.editor-git-test
  (:require [clj-kondo.hooks-api :as api]))

(defn with-git [{:keys [node]}]
  (let [[_ bindings-node & body] (:children node)]
    {:node
     (api/list-node
       (list*
         (api/token-node 'let)
         bindings-node
         body))}))
