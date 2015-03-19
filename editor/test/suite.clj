(ns suite
  (:require [clojure.java.io :as io]
            [clojure.test :as test]
            [dynamo.buffers-test]
            [dynamo.camera-test]
            [dynamo.defnode-test]
            [dynamo.geom-test]
            [dynamo.gl.translate-test]
            [dynamo.gl.vertex-test]
            [dynamo.image-test]
            [dynamo.messages :as m]
            [dynamo.property-test]
            [dynamo.protobuf-test]
            [dynamo.transaction-test]
            [dynamo.ui.property-test]
            [dynamo.util-test]
            [editor.atlas-test]
            [internal.cache-test]
            [internal.dependency-test]
            [internal.either-test]
            [internal.graph.graph-test]
            [internal.injection-test]
            [internal.math-test]
            [internal.node-test]
            [internal.packing-test]
            [internal.scope-test]
            [internal.system-test]
            [internal.type-test]
            [internal.value-test])
  (:import [clojure.lang Compiler]))

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
  (test/run-all-tests))
