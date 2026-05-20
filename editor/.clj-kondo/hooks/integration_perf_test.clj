(ns hooks.integration-perf-test
  (:require [clj-kondo.hooks-api :as api]))

(defn measure [{:keys [node]}]
  (let [[_ binding-node & body] (:children node)]
    {:node
     (api/list-node
       (list*
         (api/token-node 'dotimes)
         binding-node
         body))}))
