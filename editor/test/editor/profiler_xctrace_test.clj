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

(ns editor.profiler-xctrace-test
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [clojure.test :refer [deftest is]]
            [editor.engine :as engine]
            [editor.fs :as fs]
            [editor.os :as os]
            [editor.prefs :as prefs]
            [editor.process :as process]
            [editor.profiler-xctrace :as profiler-xctrace]
            [editor.ui :as ui])
  (:import [java.io InputStream]))

(set! *warn-on-reflection* true)

;; Engine child processes receive the tool, output folder and timing values;
;; UI-thread launches leave executable discovery and validation to the engine.
(deftest launch-recording-test
  (when (os/is-mac-os?)
    (let [directory (fs/create-temp-directory! "xctrace-launch")
          binary (io/file directory "engine with spaces.sh")
          settings (prefs/make :scopes {:global (io/file directory "preferences")
                                        :project (io/file directory "editor_settings")}
                               :schemas [:default])]
      (try
        (spit binary (str "#!/bin/sh\n"
                          "printf '%s\n' \"$DM_PROFILE_NATIVE_TRACE\" \"$DM_PROFILE_NATIVE_XCTRACE\" "
                          "\"$DM_PROFILE_NATIVE_TRACE_DELAY_MS\" \"$DM_PROFILE_NATIVE_TRACE_DURATION_MS\" \"$DM_PROFILE_NATIVE_TRACE_PATH\" \"$DM_PROFILE_NATIVE_TRACE_ENERGY\"\n"))
        (fs/set-executable! binary true)
        (is (= (.getAbsolutePath binary)
               (profiler-xctrace/resolve-executable! (.getAbsolutePath binary))))
        (is (nil? (profiler-xctrace/resolve-executable! (.getAbsolutePath directory))))
        (is (= {:path "" :output-directory "" :enabled false :delay-ms 0 :duration-ms 5000 :include-energy true :open-in-instruments false}
               (prefs/get settings [:profile :xctrace])))
        (prefs/set! settings [:profile :xctrace :path] (.getAbsolutePath binary))
        (prefs/set! settings [:profile :xctrace :enabled] true)
        (doseq [[delay duration energy output-directory]
                [[0 5000 true ""]
                 [123 0 false (str directory "/profile captures")]
                 [42 9876 true (str directory "/Traces + å&.trace")]]]
          (prefs/set! settings [:profile :xctrace :delay-ms] delay)
          (prefs/set! settings [:profile :xctrace :duration-ms] duration)
          (prefs/set! settings [:profile :xctrace :include-energy] energy)
          (prefs/set! settings [:profile :xctrace :output-directory] output-directory)
          (let [{:keys [process log-stream]} (ui/run-now (engine/launch! binary directory settings false 0 true))]
            (try
              (with-open [^InputStream log-stream log-stream]
                (is (= ["1" (.getAbsolutePath binary) (str delay) (str duration)
                        (if (string/blank? output-directory) "" (str output-directory "/")) (if energy "1" "0")]
                       (string/split-lines (process/capture! log-stream)))))
              (is (zero? (process/await-exit-code process)))
              (finally (.destroyForcibly ^Process process)))))
        (prefs/set! settings [:profile :xctrace :enabled] false)
        (prefs/set! settings [:profile :xctrace :output-directory] "")
        (is (= "0" (get (profiler-xctrace/launch-environment settings) "DM_PROFILE_NATIVE_TRACE")))
        (prefs/set! settings [:profile :xctrace :path] "")
        (is (= {"DM_PROFILE_NATIVE_TRACE" "0" "DM_PROFILE_NATIVE_TRACE_ENERGY" "1"} (profiler-xctrace/launch-environment settings)))
        (prefs/set! settings [:profile :xctrace :enabled] true)
        (is (= {"DM_PROFILE_NATIVE_TRACE" "1"
                "DM_PROFILE_NATIVE_TRACE_ENERGY" "1"
                "DM_PROFILE_NATIVE_TRACE_DELAY_MS" "42"
                "DM_PROFILE_NATIVE_TRACE_DURATION_MS" "9876"
                "DM_PROFILE_NATIVE_TRACE_PATH" nil}
               (ui/run-now (profiler-xctrace/launch-environment settings))))
        (prefs/set! settings [:profile :xctrace :path] (str directory "/missing"))
        (is (= (str directory "/missing")
               (get (profiler-xctrace/launch-environment settings) "DM_PROFILE_NATIVE_XCTRACE")))
        (finally (fs/delete-directory! directory))))))
