(ns suite
 (:require [clojure.test :as test]))

(def test-namespaces ['dynamo.project-test
                      'atlas.core-test
                      'dynamo.node-test
                      'internal.graph.graph-test
                      'internal.cache-test
                      'internal.node-test])

(defn suite []
  (doseq [test-ns test-namespaces]
    (require test-ns))
  (apply test/run-tests test-namespaces))
