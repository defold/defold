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
