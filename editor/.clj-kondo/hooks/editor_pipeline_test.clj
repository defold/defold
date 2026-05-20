(ns hooks.editor-pipeline-test
  (:require [hooks.integration-test-util :as test-util]))

(def implicit-clean-system-bindings
  '#{project workspace})

(defn with-clean-system [{:keys [node]}]
  (let [[_ & body] (:children node)]
    {:node
     (test-util/let-node
       (test-util/implicit-binding-nodes implicit-clean-system-bindings body)
       body)}))
