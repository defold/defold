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

(ns leiningen.check-and-exit
  "Check syntax, warn on reflection and then run Platform/exit

  JavaFX has to be initialized during compilation even when we are not actually
  running the editor. To properly generate reflection-less code, clojure
  compiler loads classes and searches for fitting methods while compiling it.
  Loading javafx.scene.control.Control class requires application to be running,
  because it sets default platform stylesheet, and this requires Application to
  be running."
  (:require [leiningen.core.eval :as eval]
            [leiningen.core.main :as main]
            [bultitude.core :as b]
            [clojure.tools.namespace.find :as ns.find]
            [clojure.java.io :as io]))

(defn check-and-exit [project]
  (let [project-root (:root project)
        proj-ns-set (into #{} (map str (ns.find/find-namespaces-in-dir (io/file project-root "src" "clj"))))]
    (try
      (binding [eval/*pump-in* false]
        (eval/eval-in-project
          project
          `(let [stderr-output# (java.io.StringWriter.)
                 proj-ns-set# ~proj-ns-set]
             (binding [*err* (java.io.PrintWriter. stderr-output#)
                       *warn-on-reflection* true]
               (require 'editor.bootloader)
               (eval '(editor.bootloader/load-synchronous true)))
             (do
               (require 'clojure.tools.namespace.parse)
               (let [read-ns-decl# (resolve 'clojure.tools.namespace.parse/read-ns-decl)
                     name-from-ns-decl# (resolve 'clojure.tools.namespace.parse/name-from-ns-decl)
                     reflection-count#
                     (->> (str stderr-output#)
                          (clojure.string/split-lines)
                          (filter #(.startsWith % "Reflection warning, "))
                          (keep (fn [warning#]
                                  (when-let [[_# file-path#] (re-find #"Reflection warning, ([^:]+):" warning#)]
                                    (try
                                      (let [full-path# (clojure.java.io/file ~project-root "src" "clj" file-path#)
                                            ns-decl# (with-open [rdr# (java.io.PushbackReader. (io/reader full-path#))]
                                                       (read-ns-decl# rdr#))
                                            ns-name# (str (name-from-ns-decl# ns-decl#))]
                                        (when (proj-ns-set# ns-name#)
                                          warning#))
                                      (catch Exception e#
                                        nil))))))]
                 (when (seq reflection-count#)
                   (println (str "Reflection Check Failed: Found " (count reflection-count#) " reflection warning(s):")))
                 (doseq [line# reflection-count#] (println line#))
                 (System/exit (min (count reflection-count#) 125)))))))
      (catch clojure.lang.ExceptionInfo e
        (System/exit (or (some-> e ex-data :exit-code) -1))))))
