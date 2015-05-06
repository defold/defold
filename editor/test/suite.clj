(ns suite
  (:require [clojure.java.io :as io]
            [clojure.test :as test]
            [dynamo.messages :as m])
  (:import [clojure.lang Compiler]))

(def test-namespaces '[dynamo.buffers-test
                       dynamo.camera-test
                       dynamo.defnode-test
                       dynamo.geom-test
                       dynamo.gl.translate-test
                       dynamo.gl.vertex-test
                       dynamo.image-test
                       dynamo.property-test
                       dynamo.transaction-test
                       dynamo.util-test
                       editor.handler-test
                       editor.injection-test
                       editor.math-test
                       editor.project-test
                       editor.scope-test
                       editor.ui-test
                       internal.cache-test
                       internal.connection-rules
                       internal.dependency-test
                       internal.graph.graph-test
                       internal.graph.types-test
                       internal.math-test
                       internal.node-test
                       internal.packing-test
                       internal.paper-tape-test
                       internal.system-test
                       internal.type-test
                       internal.value-test
                       potemkin.imports-test
                       potemkin.namespaces-test])

(def integration-test-namespaces '[integration.undo-test
                                   integration.asset-browser-test
                                   integration.save-test
                                   integration.scene-test])

(def builtin-basedir (io/file "../com.dynamo.cr/com.dynamo.cr.builtins"))

(defn file?      [f]   (.isFile f))
(defn extension? [f e] (.endsWith (.getName f) e))
(defn source?    [f]   (and (file? f) (extension? f ".clj")))

(defn clojure-sources [dir]
  (filter source? (file-seq dir)))

(defn compile-files [fs]
  (doseq [f fs]
    (println "Compiling" (str f))
    (Compiler/loadFile (str f))))

(defn suite []
  (doseq [n test-namespaces]
    (require n))
  (apply test/run-tests test-namespaces))

(defn integration-tests []
  (doseq [n integration-test-namespaces]
    (require n))
  (apply test/run-tests integration-test-namespaces))
