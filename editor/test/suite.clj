(ns suite
  (:require [clojure.java.io :as io]
            [clojure.test :as test]
            [editor.messages :as m])
  (:import [clojure.lang Compiler]))

(def test-namespaces '[editor.buffers-test
                       dynamo.defnode-test
                       editor.geom-test
                       editor.gl.translate-test
                       editor.gl.vertex-test
                       editor.image-test
                       dynamo.transaction-test
                       dynamo.util-test
                       dynamo.integration.error-substitute-values
                       dynamo.integration.garbage-collection
                       dynamo.integration.graph-functions
                       dynamo.integration.node-become
                       dynamo.integration.schema-validation
                       dynamo.integration.value-disposal
                       dynamo.integration.visibility-enablement
                       editor.handler-test
                       editor.injection-test
                       editor.math-test
                       editor.project-test
                       editor.ui-test
                       integration.undo-test
                       integration.asset-browser-test
                       integration.save-test
                       integration.scene-test
                       integration.scope-test
                       integration.collection-test
                       integration.copy-paste-test
                       internal.cache-test
                       internal.connection-rules
                       internal.copy-paste-test
                       internal.dependency-test
                       internal.graph.graph-test
                       internal.graph.types-test
                       internal.math-test
                       internal.node-test
                       internal.packing-test
                       internal.paper-tape-test
                       internal.property-test
                       internal.system-test
                       internal.type-test
                       internal.value-test
                       potemkin.imports-test
                       potemkin.namespaces-test])

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
