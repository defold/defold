(ns suite
 (:require [clojure.test :as test]))

(def test-namespaces ['dynamo.project-test
                      'internal.graph.graph-test
                      'internal.value-test
                      'internal.node-test
                      'atlas.core-test])

(defn suite []
  (doseq [test-ns test-namespaces]
    (require test-ns))
  (apply test/run-tests test-namespaces))
