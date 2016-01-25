(ns suite
  (:require [clojure.java.io :as io]
            [clojure.test :as test]
            [editor.messages :as m])
  (:import [clojure.lang Compiler]))

(def test-namespaces '[dynamo.integration.defective-nodes
                       dynamo.integration.dependencies
                       dynamo.integration.error-substitute-values
                       dynamo.integration.garbage-collection
                       dynamo.integration.graph-functions
                       dynamo.integration.node-become
                       dynamo.integration.node-value-options
                       dynamo.integration.property-setters
                       dynamo.integration.schema-validation
                       dynamo.integration.validation
                       dynamo.integration.visibility-enablement
                       editor.buffers-test
                       editor.camera-test
                       editor.diff-view-test
                       editor.fs-watch-test
                       editor.geom-test
                       editor.git-test
                       editor.gl.translate-test
                       editor.gl.vertex-test
                       editor.handler-test
                       editor.image-test
                       editor.injection-test
                       editor.math-test
                       editor.pipeline.tex-gen-test
                       editor.prefs-test
                       editor.project-test
                       editor.properties-test
                       editor.protobuf-test
                       editor.texture.math-test
                       editor.texture.packing-test
                       editor.project-test
                       editor.ui-test
                       editor.updater-test
                       integration.asset-browser-test
                       integration.benchmark-test
                       integration.build-test
                       integration.collection-test
                       integration.game-project-test
                       integration.outline-test
                       integration.reload-test
                       integration.save-test
                       integration.scene-test
                       integration.scope-test
                       integration.tex-packing-test
                       integration.undo-test
                       internal.cache-test
                       internal.connection-rules
                       internal.copy-paste-test
                       internal.dependency-test
                       internal.graph.graph-test
                       internal.graph.types-test
                       internal.graph.error-values-test
                       internal.node-test
                       internal.paper-tape-test
                       internal.property-test
                       internal.system-test
                       internal.value-test
                       internal.defnode-test
                       internal.transaction-test
                       internal.util-test
                       potemkin.imports-test
                       potemkin.namespaces-test])

(def builtin-basedir (io/file "../com.dynamo.cr/com.dynamo.cr.builtins"))

(defn file?      [f]   (.isFile ^java.io.File f))
(defn extension? [f e] (.endsWith (.getName ^java.io.File f) e))
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
