(ns hooks.clojure-test-check-clojure-test
  (:require [clj-kondo.hooks-api :as api]))

(defn defspec [{:keys [node]}]
  (let [[_ name-node & body] (:children node)]
    {:node
     (api/list-node
       [(api/token-node 'do)
        (api/list-node
          [(api/token-node 'clojure.test/deftest)
           name-node])
        (api/list-node
          [(api/token-node 'let)
           (api/vector-node
             [(api/token-node '_)
              (api/list-node
                (list*
                  (api/token-node 'do)
                  body))])
           (api/token-node nil)])])}))
