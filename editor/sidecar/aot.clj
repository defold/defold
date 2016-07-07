(ns aot
  "Build tool to compile everything, in dependency order."
  (:require [clojure.java.io :as io]
            [clojure.tools.namespace.dependency :as dep]
            [clojure.tools.namespace.file :as file]
            [clojure.tools.namespace.find :as find]
            [clojure.tools.namespace.track :as track]))

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
         schema.core]))

(defn compile-order
  [srcdirs]
  (->> srcdirs
       all-sources-tracker
       sorted-deps
       (remove core-namespaces)))

(def srcdirs (map io/file ["src/clj"]))

(defn compile-clj
  [namespaces]
  (binding [*compiler-options* {:elide-meta #{:file :line :column}}]
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
