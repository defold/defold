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

(ns check-reflection
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [clojure.tools.namespace.find :as ns.find]
            [clojure.tools.namespace.parse :as ns.parse]))

(defn check-and-exit [project-root]
  (let [stderr-output (java.io.StringWriter.)
        proj-ns-set (into #{} (map str (ns.find/find-namespaces-in-dir
                                         (io/file project-root "src" "clj"))))]
    (binding [*err* (java.io.PrintWriter. stderr-output)
              *warn-on-reflection* true]
      (try
        (require 'editor.bootloader)
        (eval '(editor.bootloader/load-synchronous true))
        (catch ExceptionInInitializerError e
          (.printStackTrace e))))

    (let [reflection-warnings
          (->> (str stderr-output)
               (string/split-lines)
               (filter #(.startsWith % "Reflection warning, "))
               (keep (fn [warning]
                       (when-let [[_ file-path] (re-find #"Reflection warning, ([^:]+):" warning)]
                         (try
                           (let [full-path (io/file project-root "src" "clj" file-path)
                                 ns-decl (with-open [rdr (java.io.PushbackReader. (io/reader full-path))]
                                           (ns.parse/read-ns-decl rdr))
                                 ns-name (str (ns.parse/name-from-ns-decl ns-decl))]
                             (when (proj-ns-set ns-name)
                               warning))
                           (catch Exception _ nil))))))]
      (when (seq reflection-warnings)
        (println (str "Reflection Check Failed: Found " (count reflection-warnings) " reflection warning(s):"))
        (doseq [line reflection-warnings] (println line)))
      (System/exit (min (count reflection-warnings) 125)))))

(defn -main [& _args]
  (check-and-exit (System/getProperty "user.dir")))
