(ns preflight.test
  (:require  [clojure.test :as t]
             [clojure.java.io :as io]
             [clojure.tools.namespace.find :as ns-find])
  (:import [java.io ByteArrayOutputStream PrintStream PrintWriter]
           [java.nio.charset StandardCharsets]))

(defn run
  [_files]
  ;; Have to redirect the standard Java streams as well as the Clojure vars
  ;; because some code will print directly to the Java streams.
  (let [orig-out ^PrintStream System/out
        orig-err ^PrintStream System/err
        baos (ByteArrayOutputStream.)
        out (PrintStream. baos true "UTF-8")
        outwriter (PrintWriter. out)]
    (try
      (System/setOut out)
      (try
        (System/setErr out)

        (binding [*ns* *ns* ;; Needed for a test to work
                  *out* outwriter
                  *err* outwriter
                  t/*test-out* outwriter]

          (let [namespaces (ns-find/find-namespaces-in-dir (io/file "test"))]
            (doall (map #(require %) namespaces))
            (apply t/run-tests namespaces)))

        (finally
          (System/setErr orig-err)))
      (finally
        (System/setOut orig-out)))
    (String. (.toByteArray baos) StandardCharsets/UTF_8)))
