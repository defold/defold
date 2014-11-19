(ns suite
 (:require [clojure.test :as test]))

(def test-namespaces ['internal.node-test
                      'internal.scope-test
                      'internal.graph.graph-test
                      'internal.value-test
                      'internal.system-test
                      'internal.type-test
                      'internal.injection-test
                      'dynamo.transaction-test
                      'dynamo.texture-test
                      'dynamo.image-test
                      'dynamo.geom-test
                      'dynamo.gl.vertex-test
                      'dynamo.gl.translate-test
                      'dynamo.camera-test
                      'dynamo.protobuf-test
                      'docs])

(def test-namespaces-for-junit
  (into-array String (map name test-namespaces)))

(defn suite []
  (doseq [test-ns test-namespaces]
    (require test-ns)
    (test/run-tests test-ns)))
