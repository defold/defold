(ns aot
  "Build tool to compile everything, in dependency order."
  (:require [clojure.java.io :as io]
            [clojure.tools.namespace.dependency :as dep]
            [clojure.tools.namespace.file :as file]
            [clojure.tools.namespace.find :as find]
            [clojure.tools.namespace.track :as track]
            [clojure.walk :as walk]))

(defn all-sources-tracker
  [srcdirs]
  (reduce
   (fn [tracker dir]
     (->> dir find/find-clojure-sources-in-dir (file/add-files tracker)))
   (track/tracker)
   srcdirs))

(defn sorted-deps
  [tracker]
  (dep/topo-sort (::track/deps tracker)))

(def core-namespaces
  (set '[clojure.core
         clojure.data
         clojure.edn
         clojure.inspector
         clojure.instant
         clojure.java.browse
         clojure.java.io
         clojure.java.javadoc
         clojure.java.shell
         clojure.main
         clojure.pprint
         clojure.reflect
         clojure.repl
         clojure.set
         clojure.stacktrace
         clojure.string
         clojure.template
         clojure.test
         clojure.walk
         clojure.xml
         clojure.zip
         schema.core
         schema.utils]))

(defn compile-order
  [srcdirs]
  (->> srcdirs
       all-sources-tracker
       sorted-deps
       (remove core-namespaces)))

(def srcdirs (map io/file ["src/clj"]))

(defn system-properties
  []
  (walk/keywordize-keys (into {} (System/getProperties))))

(def build-type (atom (get (system-properties) :defold.build "development")))

(def build->compiler-options
  {"development"
   {:elide-meta           #{:file :line :column}
    :defold/check-schemas true}

   "release"
   {:elide-meta     #{:file :line :column}
    :direct-linking true
    :defold/check-schemas  false}})

(def default-compiler-options (build->compiler-options "development"))

(defn compile-clj
  [namespaces]
  (binding [*compiler-options* (build->compiler-options @build-type default-compiler-options)]
    (println "Using build profile " @build-type)
    (println "Compiler options: " *compiler-options*)
    (doseq [n namespaces]
      (println "Compiling " n)
      (compile n))))

(defn -main [& args]
  (defonce force-toolkit-init (javafx.embed.swing.JFXPanel.))
  (let [order (compile-order srcdirs)]
    (println "Compiling in order " order)
    (compile-clj order))
  (println "Done compiling")
  (System/exit 0))
