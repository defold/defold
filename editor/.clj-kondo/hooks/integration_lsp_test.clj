(ns hooks.integration-lsp-test
  (:require [hooks.integration-test-util :as test-util]))

(def implicit-scratch-project-bindings
  '#{app-view project workspace})

(defn with-scratch-project [{:keys [node]}]
  (let [[_ project-path-node & body] (:children node)
        binding-nodes (test-util/implicit-binding-nodes implicit-scratch-project-bindings body)]
    {:node
     (test-util/let-node
       binding-nodes
       [(test-util/body-with-preserved-nodes [project-path-node] body)])}))
