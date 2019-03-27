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
            [clojure.java.io :as io]))


(defn check-and-exit
  "Check syntax, warn on reflection and then run Platform/exit."
  ([project]
   (let [source-files (map io/file (:source-paths project))
         nses (b/namespaces-on-classpath :classpath source-files
                                         :ignore-unreadable? false)
         action `(let [failures# (atom 0)]
                   (doseq [ns# '~nses]
                     ;; load will add the .clj, so can't use ns/path-for.
                     (let [ns-file# (-> (str ns#)
                                        (.replace \- \_)
                                        (.replace \. \/))]
                       (binding [*out* *err*]
                         (println "Compiling namespace" ns#))
                       (try
                         (binding [*warn-on-reflection* true]
                           (load ns-file#))
                         (catch ExceptionInInitializerError e#
                           (swap! failures# inc)
                           (.printStackTrace e#)))))
                   (javafx.application.Platform/exit)
                   (if-not (zero? @failures#)
                     (System/exit @failures#)))]
     (try
       (binding [eval/*pump-in* false]
         (eval/eval-in-project project action))
       (catch clojure.lang.ExceptionInfo e
         (main/abort "Failed."))))))