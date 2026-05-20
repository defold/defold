(ns hooks.editor-image-util
  (:require [clj-kondo.hooks-api :as api]))

(defn with-graphics [{:keys [node]}]
  (let [[_ binding-node & body] (:children node)]
    {:node
     (api/list-node
       (list*
         (api/token-node 'let)
         binding-node
         body))}))
