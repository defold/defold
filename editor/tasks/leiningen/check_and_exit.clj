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
   (try
     (binding [eval/*pump-in* false]
       (eval/eval-in-project project
                             `(let [failures# (atom 0)]
                                (try
                                  (binding [*warn-on-reflection* true]
                                    (require 'editor.bootloader)
                                    (eval '(editor.bootloader/load-synchronous true)))
                                  (catch ExceptionInInitializerError e#
                                    (swap! failures# inc)
                                    (.printStackTrace e#)))
                                (javafx.application.Platform/exit)
                                (shutdown-agents)
                                (if-not (zero? @failures#)
                                  (System/exit @failures#)))))
     (catch clojure.lang.ExceptionInfo e
       (main/abort "Failed.")))))
