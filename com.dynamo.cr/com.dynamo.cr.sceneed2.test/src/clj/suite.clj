(ns suite
 (:require [clojure.java.io :as io]
           [clojure.osgi.core :as o]
           [clojure.test :as test])
 (:import [clojure.lang Compiler]
          [java.io StringReader]))

(def test-namespaces ['internal.either-test
                      'internal.injection-test
                      'internal.math-test
                      'internal.node-test
                      'internal.packing-test
                      'internal.refresh-test
                      'internal.scope-test
                      'internal.system-test
                      'internal.type-test
                      'internal.value-test
                      'internal.graph.graph-test
                      'dynamo.buffers-test
                      'dynamo.camera-test
                      'dynamo.defnode-test
                      'dynamo.geom-test
                      'dynamo.image-test
                      'dynamo.project-test
                      'dynamo.property-test
                      'dynamo.protobuf-test
                      'dynamo.transaction-test
                      'dynamo.ui.property-test
                      'dynamo.util-test
                      'dynamo.gl.translate-test
                      'dynamo.gl.vertex-test
                      'docs])

;; TODO - Once we support dependencies among tool code better, put this in alpha order.
;; For now, editors.image-node must be loaded before editors.atlas (ick)
(def builtins-files ["/builtins/tools/editors/image_node.clj"
                     "/builtins/tools/editors/atlas.clj"
                     "/builtins/tools/editors/cubemap.clj"
                     "/builtins/tools/editors/particlefx.clj"
                     "/builtins/tools/editors/presenters.clj"])

(def builtins-tests [["/test/tools/editors/atlas_test.clj" 'editors.atlas-test]])


;; We do a dance and a wiggle here to compile the builtins within this
;; plugin, despite the fact they aren't on an OSGi classpath. This
;; models the runtime situation better, because these will ultimately
;; be loaded as project code and not as part of an OSGi plugin.
(defn builtin-bundle []
  (o/get-bundle "com.dynamo.cr.builtins"))

(defn resource-from-bundle
  [b f]
  (o/with-bundle b
    (io/resource f)))

(defn builtin [f]
  (slurp
    (resource-from-bundle (builtin-bundle) f)))

(defn load-builtin [f]
  (Compiler/load (StringReader. (builtin f)) f f))

(defn compile-builtins []
  (let [all-builtins (concat builtins-files (map first builtins-tests))]
    (doseq [f all-builtins]
      (println "Compiling" f)
      (load-builtin f))))

(def test-namespaces-for-junit
  (into-array String (concat
                       (map name test-namespaces)
                       (map (comp str second) builtins-tests))))

(defn suite []
  (doseq [test-ns test-namespaces]
    (require test-ns)
    (test/run-tests test-ns))

  (compile-builtins)

  (doseq [[_ n] builtins-tests]
    (test/run-tests n)))
