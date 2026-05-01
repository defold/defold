;; Copyright 2020-2026 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;;
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;;
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns aot
  "Build tool to compile everything, in dependency order."
  (:require [clojure.java.io :as io]
            [clojure.tools.namespace.dependency :as dep]
            [clojure.tools.namespace.file :as file]
            [clojure.tools.namespace.find :as find]
            [clojure.tools.namespace.track :as track]))

(defn all-sources-tracker
  [build-type srcdirs]
  (reduce
    (fn [tracker dir]
      (->> dir
           (find/find-clojure-sources-in-dir)
           (file/add-files tracker)))
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
  [srcdirs build-type]
  (->> srcdirs
       (all-sources-tracker build-type)
       sorted-deps
       (remove core-namespaces)))

(def srcdirs (map io/file ["src/clj"]))

(def build-type (System/getProperty "defold.build" "development"))

(def build->compiler-options
  {"development"
   {:elide-meta #{:added :column :doc :file :line}
    :defold/check-schemas true}

   "release"
   {:elide-meta #{:added :column :doc :file :line}
    :direct-linking true
    :defold/check-schemas false}})

(def default-compiler-options (build->compiler-options "development"))

(defn compile-clj
  [namespaces]
  (binding [*compiler-options* (build->compiler-options build-type default-compiler-options)]
    (println "Using build profile:" build-type)
    (println "Compiler options:" *compiler-options*)
    (doseq [n namespaces]
      (println "Compiling" n)
      (compile n))))

(defn -main [& args]
  (defonce force-toolkit-init (javafx.application.Platform/startup (fn [])))
  (let [order (compile-order srcdirs build-type)]
    (println "Compiling in order" order)
    (compile-clj order))
  (println "Done compiling")
  (System/exit 0))
