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

(ns editor.launcher
  (:require [clojure.java.io :as io]
            [editor.os :as os]
            [editor.process :as process]
            [editor.system :as system])
  (:import [java.lang.management ManagementFactory]))

(set! *warn-on-reflection* true)

(defn start!
  "Start another editor instance at the welcome window."
  []
  (if (system/defold-dev?)
    (apply process/start!
           {:dir (System/getProperty "user.dir")
            :out :inherit
            :err :inherit}
           (str (io/file (System/getProperty "java.home")
                         "bin"
                         (if (os/is-win32?) "java.exe" "java")))
           (into (vec (.getInputArguments (ManagementFactory/getRuntimeMXBean)))
                 ["-cp" (System/getProperty "java.class.path") "com.defold.editor.Main"]))
    (let [resources-path (system/defold-resourcespath)]
      (process/start!
        {:dir (.getCanonicalFile
                (case (os/os)
                  :macos
                  (io/file resources-path "../../")

                  (:linux :win32)
                  (io/file resources-path)))
         ;; Each editor writes its own log file. Unread pipes could fill up and
         ;; block the new instance while the original editor is still running.
         :out :discard
         :err :discard}
        (system/defold-launcherpath)))))
