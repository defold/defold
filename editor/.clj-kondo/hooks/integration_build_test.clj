(ns hooks.integration-build-test
  (:require [clj-kondo.hooks-api :as api]
            [hooks.integration-test-util :as test-util]))

(def implicit-build-results-bindings
  '#{build-artifacts build-results content-by-source content-by-target path project resource-node workspace})

(defn- value-node [sym path-node]
  (case sym
    path path-node
    content-by-source (api/map-node [])
    content-by-target (api/map-node [])
    build-results (api/map-node [])
    build-artifacts (api/vector-node [])
    (api/token-node nil)))

(defn- binding-nodes [path-node body]
  (let [body-symbols (test-util/free-symbols-in-nodes body)]
    (into []
          (mapcat (fn [sym]
                    (when (contains? body-symbols sym)
                      [(api/token-node sym)
                       (value-node sym path-node)])))
          implicit-build-results-bindings)))

(defn with-build-results [{:keys [node]}]
  (let [[_ path-node & body] (:children node)
        binding-nodes (binding-nodes path-node body)
        path-bound? (some #(= 'path (api/sexpr %)) binding-nodes)]
    {:node
     (test-util/let-node
       binding-nodes
       [(test-util/body-with-preserved-nodes (if path-bound? [] [path-node]) body)])}))
