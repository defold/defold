(ns preflight.test
  (:require  [clojure.test :as t]
             [clojure.java.io :as io]
             [clojure.tools.namespace.dependency :as ns-deps]
             [clojure.tools.namespace.find :as ns-find]
             [clojure.tools.namespace.parse :as ns-parse]))

(defn run
  [_files]
  (with-out-str
    (binding [*ns* *ns*
              t/*test-out* *out*
              *err* *out*]
      (println "Collecting test namespaces")
      (let [namespaces (ns-find/find-namespaces-in-dir (io/file "test"))]
        (println "Loading test namespaces")
        (doall (map #(require %) namespaces))
        (println "Running tests")
        (apply t/run-tests namespaces)))))

