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

(ns editor.profiler-xctrace
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [editor.future :as future]
            [editor.os :as os]
            [editor.prefs :as prefs]
            [editor.process :as process]
            [service.log :as log])
  (:import [java.io IOException]
           [java.util.concurrent TimeUnit]))

(set! *warn-on-reflection* true)

(defn resolve-executable!
  "Resolve an executable or ask the selected Xcode. Call off the UI thread."
  [path]
  (when (os/is-mac-os?)
    (let [path (if-not (string/blank? path)
                 path
                 (try
                   (let [child (process/start! {:err :discard} "/usr/bin/xcrun" "--find" "xctrace")]
                     (try
                       (when (and (.waitFor child 5 TimeUnit/SECONDS)
                                  (zero? (.exitValue child)))
                         (process/capture! (process/out child)))
                       (finally
                         (.destroyForcibly child)
                         (.close (process/out child)))))
                   (catch IOException _ nil)))]
      (when path
        (let [file (io/file path)]
          (when (and (.isFile file) (.canExecute file))
            (.getAbsolutePath file)))))))

(defn launch-environment
  "Build the recording environment. The engine resolves xctrace."
  [settings]
  (when (os/is-mac-os?)
    (let [{:keys [path output-directory enabled delay-ms duration-ms include-energy]} (prefs/get settings [:profile :xctrace])]
      (cond-> {"DM_PROFILE_NATIVE_TRACE" (if enabled "1" "0")
               "DM_PROFILE_NATIVE_TRACE_ENERGY" (if include-energy "1" "0")}
        (not (string/blank? path))
        (assoc "DM_PROFILE_NATIVE_XCTRACE" (.getAbsolutePath (io/file path)))

        enabled
        (assoc "DM_PROFILE_NATIVE_TRACE_DELAY_MS" (str delay-ms)
               "DM_PROFILE_NATIVE_TRACE_DURATION_MS" (str duration-ms)
               "DM_PROFILE_NATIVE_TRACE_PATH" (when-not (string/blank? output-directory)
                                                (str output-directory "/")))))))

(defn open-completed-trace!
  "Open a completed local engine trace if enabled in the preferences."
  [settings line]
  (when-let [[_ path] (re-matches #"INFO:(?:ENGINE|PROFILER): Native trace complete: (/.+\.trace)" line)]
    (when (and (os/is-mac-os?) (prefs/get settings [:profile :xctrace :open-in-instruments]))
      (future/io
        (try
          (process/exec! {:err :stdout} "/usr/bin/open" "-a" "Instruments" path)
          (catch Exception e
            (log/warn :message "Could not open trace in Instruments" :path path :exception e)))))))
