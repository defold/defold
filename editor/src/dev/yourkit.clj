;; Copyright 2020-2024 The Defold Foundation
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

(ns yourkit
  "Helpers for interacting with the YourKit Java profiler.

  Prerequisites:
  * The JVM must have been started with `-agentpath:<path-to-libyjpagent+opts>`.
  * `yjp-controller-api-redist.jar` must be on the classpath.

  You can achieve this by putting something like this in `~/.lein/profiles.clj`:
  ```
  {:user {:jvm-opts [\"-agentpath:/Applications/YourKit-Java-Profiler-2023.5.app/Contents/Resources/bin/mac/libyjpagent.dylib=disablestacktelemetry,exceptions=disable\"]
          :resource-paths [\"/Applications/YourKit-Java-Profiler-2023.5.app/Contents/Resources/lib/yjp-controller-api-redist.jar\"]}}
  ```"
  (:require [editor.process :as process])
  (:import [com.yourkit.api.controller AllocationRecordingSettings Controller CpuProfilingSettings]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

;; -----------------------------------------------------------------------------
;; Helpers
;; -----------------------------------------------------------------------------

(defonce ^:private controller-atom (atom nil))

(defonce snapshot-paths-atom (atom []))

(def ^:private profiler-command-path-delay
  (delay
    (-> Controller
        (.getProtectionDomain)
        (.getCodeSource)
        (.getLocation)
        (.toURI)
        (.resolve "../bin/profiler.sh")
        (.getPath))))

(defn controller
  "Returns the shared instance of the YourKit profiling agent controller."
  ^Controller []
  (or (deref controller-atom)
      (swap! controller-atom
             (fn [existing-controller]
               (or existing-controller
                   (-> (Controller/newBuilder)
                       (.self)
                       (.build)))))))

(defn- with-snapshot-forms [clear-fn-sym start-fn-sym stop-fn-sym capture-fn-sym forms]
  `(let [controller# (controller)]
     (~clear-fn-sym controller#)
     (~start-fn-sym controller#)
     (let [result# (try
                     ~@forms
                     (finally
                       (~stop-fn-sym controller#)))
           snapshot-path# (~capture-fn-sym controller#)]
       (println "Wrote" snapshot-path#)
       result#)))

;; -----------------------------------------------------------------------------
;; CPU profiling
;; -----------------------------------------------------------------------------

(def default-cpu-profiling-settings (CpuProfilingSettings.))

(definline start-cpu-sampling!
  "Starts CPU sampling. When sampling is used, the profiler periodically queries
  stacks and times of running threads to estimate the slowest parts of the code.
  In this mode method invocation counts are not available."
  [^Controller controller]
  `(.startSampling ~controller default-cpu-profiling-settings))

(definline start-cpu-tracing!
  "Starts CPU tracing. When tracing is used, the profiler instruments the
  bytecode of the profiled application for recording time spent inside each
  profiled method. Both times and method invocation counts are available."
  [^Controller controller]
  `(.startTracing ~controller default-cpu-profiling-settings))

(definline start-cpu-call-counting!
  "Starts CPU call counting. Unlike other CPU profiling modes only method
  invocations are counted, neither call stacks nor times are gathered."
  [^Controller controller]
  `(.startCallCounting ~controller))

(definline stop-cpu-profiling!
  "Stops CPU profiling in any mode, be it sampling, tracing or call counting. If
  CPU profiling is not running, this call has no effect."
  [^Controller controller]
  `(.stopCpuProfiling ~controller))

(definline capture-cpu-snapshot!
  "Writes the profiling results to a snapshot file on disk. Does not include any
  recorded memory allocations. Returns the path to the snapshot file."
  ^String [^Controller controller]
  `(let [snapshot-path# (.capturePerformanceSnapshot ~controller)]
     (swap! snapshot-paths-atom conj snapshot-path#)
     snapshot-path#))

(definline clear-cpu-data!
  "Clears all results of CPU profiling in any mode, be it sampling, tracing or
  call counting. Note that when CPU profiling starts, collected CPU profiling
  data is cleared automatically, so you do not have to explicitly call this in
  that case."
  [^Controller controller]
  `(.clearCpuData ~controller))

(defmacro with-cpu-sampling!
  "Runs body with CPU sampling and captures a snapshot. Returns the result of
  the last expression in body."
  [& body]
  (with-snapshot-forms `clear-allocation-data! `start-cpu-sampling! `stop-cpu-profiling! `capture-cpu-snapshot! body))

(defmacro with-cpu-tracing!
  "Runs body with CPU tracing and captures a snapshot. Returns the result of the
  last expression in body."
  [& body]
  (with-snapshot-forms `clear-allocation-data! `start-cpu-tracing! `stop-cpu-profiling! `capture-cpu-snapshot! body))

(defmacro with-cpu-call-counting!
  "Runs body with CPU call counting and captures a snapshot. Returns the result
  of the last expression in body."
  [& body]
  (with-snapshot-forms `clear-allocation-data! `start-cpu-call-counting! `stop-cpu-profiling! `capture-cpu-snapshot! body))

;; -----------------------------------------------------------------------------
;; Allocation recording
;; -----------------------------------------------------------------------------

(def default-allocation-recording-settings (AllocationRecordingSettings.))

(definline start-allocation-recording!
  "Starts recording memory allocations. The call clears previously recorded
  allocation data."
  [^Controller controller]
  `(.startAllocationRecording ~controller default-allocation-recording-settings))

(definline stop-allocation-recording!
  "Stops recording memory allocations. Has no effect unless allocation recording
  is running."
  [^Controller controller]
  `(.stopAllocationRecording ~controller))

(definline capture-allocation-snapshot!
  "Writes the profiling results to a snapshot file on disk. Includes both any
  recorded CPU profiling results, and recorded memory allocations. Returns the
  path to the snapshot file."
  ^String [^Controller controller]
  `(let [snapshot-path# (.captureMemorySnapshot ~controller)]
     (swap! snapshot-paths-atom conj snapshot-path#)
     snapshot-path#))

(definline clear-allocation-data!
  "Clears all recorded allocations. Note that when allocation recording starts,
  collected allocation data is cleared automatically, so you do not have to
  explicitly call this in that case."
  [^Controller controller]
  `(.clearAllocationData ~controller))

(defmacro with-allocation-recording!
  "Runs body while recording memory allocations and captures a snapshot. Returns
  the result of the last expression in body."
  [& body]
  (with-snapshot-forms `clear-cpu-data! `start-allocation-recording! `stop-allocation-recording! `capture-allocation-snapshot! body))

;; -----------------------------------------------------------------------------
;; Utility functions
;; -----------------------------------------------------------------------------

(defn snapshot-paths
  "Returns a vector of all snapshot file paths captured during the session."
  []
  (deref snapshot-paths-atom))

(defn open-snapshot!
  "Opens the snapshot file at the specified path in a new instance of the
  YourKit UI. If no snapshot file path is specified, opens the latest captured
  snapshot from this session. Currently only works on macOS and Linux."
  (^Process []
   (when-some [latest-snapshot-path (peek (snapshot-paths))]
     (open-snapshot! latest-snapshot-path)))
  (^Process [^String snapshot-path]
   (let [command-path (deref profiler-command-path-delay)]
     (process/start! command-path "-open" snapshot-path))))
