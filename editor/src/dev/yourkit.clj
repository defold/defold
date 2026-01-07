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

(ns yourkit
  "Helpers for interacting with the YourKit Java profiler.

  Prerequisites:
  * The JVM must have been started with `-agentpath:<path-to-libyjpagent>`.
  * `yjp-controller-api-redist.jar` must be on the classpath.

  You can achieve this by putting something like this in `~/.lein/profiles.clj`:

  {:user {:jvm-opts [\"-agentpath:/Applications/YourKit Java Profiler.app/Contents/Resources/bin/mac/libyjpagent.dylib\"]
          :resource-paths [\"/Applications/YourKit Java Profiler.app/Contents/Resources/lib/yjp-controller-api-redist.jar\"]}}"
  (:require [editor.process :as process])
  (:import [com.yourkit.api.controller.v2 AllocationProfilingMode AllocationProfilingSettings Controller CpuProfilingSettings]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

;; -----------------------------------------------------------------------------
;; Helpers
;; -----------------------------------------------------------------------------

(defonce ^:private controller-atom (atom nil))

(defonce ^:private snapshot-paths-atom (atom []))

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

(defn push-snapshot-path!
  ^String [^String snapshot-path]
  (swap! snapshot-paths-atom conj snapshot-path)
  snapshot-path)

(defn- with-scope-forms [clear-fn-sym start-fn-sym stop-fn-sym forms]
  `(let [controller# (controller)]
     (~clear-fn-sym controller#)
     (~start-fn-sym controller#)
     (try
       ~@forms
       (finally
         (~stop-fn-sym controller#)))))

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
;; YourKit utility functions
;; -----------------------------------------------------------------------------

(defmacro force-garbage-collection!
  "Forces a garbage collection."
  ([] `(force-garbage-collection! (controller)))
  ([^Controller controller]
   `(.forceGc ~(with-meta controller {:tag `Controller}))))

(defmacro advance-allocation-generation!
  "Advances object allocation generation number. Optionally, a string describing
  the new generation can be supplied. Note that capturing an allocation snapshot
  will always advance the generation number automatically."
  ([] `(advance-allocation-generation! (controller)))
  ([controller-or-description]
   `(let [controller-or-description# ~controller-or-description]
      (if (string? controller-or-description#)
        (advance-allocation-generation! (controller) controller-or-description#)
        (advance-allocation-generation! controller-or-description# nil))))
  ([^Controller controller ^String description]
   `(.advanceGeneration ~(with-meta controller {:tag `Controller}) ~description)))

(defmacro start-telemetry!
  "Starts collecting performance telemetry. If telemetry is already being
  collected, the call has no effect."
  ([] `(start-telemetry! (controller)))
  ([^Controller controller]
   `(.startTelemetry ~(with-meta controller {:tag `Controller}))))

(defmacro stop-telemetry!
  "Stops collecting performance telemetry. If telemetry is not being collected,
  the call has no effect."
  ([] `(stop-telemetry! (controller)))
  ([^Controller controller]
   `(.stopTelemetry ~(with-meta controller {:tag `Controller}))))

(defmacro clear-telemetry!
  "Clears telemetry. Note: when telemetry starts, collected telemetry is cleared
  automatically, so you do not have to explicitly call this function."
  ([] `(clear-telemetry! (controller)))
  ([^Controller controller]
   `(.resetTelemetry ~(with-meta controller {:tag `Controller}))))

;; -----------------------------------------------------------------------------
;; CPU profiling
;; -----------------------------------------------------------------------------

(def ^CpuProfilingSettings default-cpu-profiling-settings (CpuProfilingSettings.))

(defmacro start-cpu-sampling!
  "Starts CPU sampling. When sampling is used, the profiler periodically queries
  stacks and times of running threads to estimate the slowest parts of the code.
  In this mode method invocation counts are not available."
  ([] `(start-cpu-sampling! (controller)))
  ([^Controller controller]
   `(.startSampling ~(with-meta controller {:tag `Controller}) default-cpu-profiling-settings)))

(defmacro start-cpu-tracing!
  "Starts CPU tracing. When tracing is used, the profiler instruments the
  bytecode of the profiled application for recording time spent inside each
  profiled method. Both times and method invocation counts are available."
  ([] `(start-cpu-tracing! (controller)))
  ([^Controller controller]
   `(.startTracing ~(with-meta controller {:tag `Controller}) default-cpu-profiling-settings)))

(defmacro start-cpu-call-counting!
  "Starts CPU call counting. Unlike other CPU profiling modes only method
  invocations are counted, neither call stacks nor times are gathered."
  ([] `(start-cpu-call-counting! (controller)))
  ([^Controller controller]
   `(.startCallCounting ~(with-meta controller {:tag `Controller}))))

(defmacro stop-cpu-profiling!
  "Stops CPU profiling in any mode, be it sampling, tracing or call counting. If
  CPU profiling is not running, this call has no effect."
  ([] `(stop-cpu-profiling! (controller)))
  ([^Controller controller]
   `(.stopCpuProfiling ~(with-meta controller {:tag `Controller}))))

(defmacro capture-cpu-snapshot!
  "Writes the profiling results to a snapshot file on disk. Does not include any
  recorded memory allocations. Returns the path to the snapshot file."
  (^String [] `(capture-cpu-snapshot! (controller)))
  (^String [^Controller controller]
   `(let [snapshot-path# (.capturePerformanceSnapshot ~(with-meta controller {:tag `Controller}))]
      (push-snapshot-path! snapshot-path#))))

(defmacro clear-cpu-data!
  "Clears all results of CPU profiling in any mode, be it sampling, tracing or
  call counting. Note that when CPU profiling starts, collected CPU profiling
  data is cleared automatically, so you do not have to explicitly call this in
  that case."
  ([] (clear-cpu-data! (controller)))
  ([^Controller controller]
   `(.resetCpuProfiling ~(with-meta controller {:tag `Controller}))))

(defmacro with-cpu-sampling!
  "Runs body with CPU sampling. Returns the result of the last expression in
  body."
  [& body]
  (with-scope-forms `clear-allocation-data! `start-cpu-sampling! `stop-cpu-profiling! body))

(defmacro with-cpu-sampling-snapshot!
  "Runs body with CPU sampling and captures a snapshot. Returns the result of
  the last expression in body."
  [& body]
  (with-snapshot-forms `clear-allocation-data! `start-cpu-sampling! `stop-cpu-profiling! `capture-cpu-snapshot! body))

(defmacro with-cpu-tracing!
  "Runs body with CPU tracing. Returns the result of the last expression in
  body."
  [& body]
  (with-scope-forms `clear-allocation-data! `start-cpu-tracing! `stop-cpu-profiling! body))

(defmacro with-cpu-tracing-snapshot!
  "Runs body with CPU tracing and captures a snapshot. Returns the result of the
  last expression in body."
  [& body]
  (with-snapshot-forms `clear-allocation-data! `start-cpu-tracing! `stop-cpu-profiling! `capture-cpu-snapshot! body))

(defmacro with-cpu-call-counting!
  "Runs body with CPU call counting. Returns the result of the last expression
  in body."
  [& body]
  (with-scope-forms `clear-allocation-data! `start-cpu-call-counting! `stop-cpu-profiling! body))

(defmacro with-cpu-call-counting-snapshot!
  "Runs body with CPU call counting and captures a snapshot. Returns the result
  of the last expression in body."
  [& body]
  (with-snapshot-forms `clear-allocation-data! `start-cpu-call-counting! `stop-cpu-profiling! `capture-cpu-snapshot! body))

;; -----------------------------------------------------------------------------
;; Allocation recording
;; -----------------------------------------------------------------------------

(def ^AllocationProfilingSettings default-allocation-profiling-settings
  (doto (AllocationProfilingSettings.)
    (.setMode AllocationProfilingMode/HEAP_SAMPLING)))

(defmacro start-allocation-recording!
  "Starts recording memory allocations. The call clears previously recorded
  allocation data."
  ([] (start-allocation-recording! (controller)))
  ([^Controller controller]
   `(.startAllocationProfiling ~(with-meta controller {:tag `Controller}) default-allocation-profiling-settings)))

(defmacro stop-allocation-recording!
  "Stops recording memory allocations. Has no effect unless allocation recording
  is running."
  ([] (stop-allocation-recording! (controller)))
  ([^Controller controller]
   `(.stopAllocationProfiling ~(with-meta controller {:tag `Controller}))))

(defmacro capture-allocation-snapshot!
  "Writes the profiling results to a snapshot file on disk. Includes both any
  recorded CPU profiling results, and recorded memory allocations. Returns the
  path to the snapshot file."
  (^String [] (capture-allocation-snapshot! (controller)))
  (^String [^Controller controller]
   `(let [snapshot-path# (.captureMemorySnapshot ~(with-meta controller {:tag `Controller}))]
      (push-snapshot-path! snapshot-path#))))

(defmacro capture-garbage-collected-allocation-snapshot!
  "Forces a garbage collection, then writes the profiling results to a snapshot
  file on disk. Includes both any recorded CPU profiling results, and recorded
  memory allocations. Returns the path to the snapshot file."
  (^String [] (capture-garbage-collected-allocation-snapshot! (controller)))
  (^String [^Controller controller]
   `(let [^Controller controller# ~controller]
      (force-garbage-collection! controller#)
      (force-garbage-collection! controller#)
      (capture-allocation-snapshot! controller#))))

(defmacro clear-allocation-data!
  "Clears all recorded allocations. Note that when allocation recording starts,
  collected allocation data is cleared automatically, so you do not have to
  explicitly call this in that case."
  ([] (clear-allocation-data! (controller)))
  ([^Controller controller]
   `(.resetAllocationProfiling ~(with-meta controller {:tag `Controller}))))

(defmacro with-allocation-recording!
  "Runs body while recording memory allocations. Returns the result of the last
  expression in body."
  [& body]
  (with-scope-forms `clear-cpu-data! `start-allocation-recording! `stop-allocation-recording! body))

(defmacro with-allocation-snapshot!
  "Runs body while recording memory allocations and captures a snapshot. Returns
  the result of the last expression in body."
  [& body]
  (with-snapshot-forms `clear-cpu-data! `start-allocation-recording! `stop-allocation-recording! `capture-allocation-snapshot! body))

(defmacro with-garbage-collected-allocation-snapshot!
  "Runs body while recording memory allocations. Once complete, forces a
  garbage collection and captures a snapshot. Returns the result of the last
  expression in body."
  [& body]
  (with-snapshot-forms `clear-cpu-data! `start-allocation-recording! `stop-allocation-recording! `capture-garbage-collected-allocation-snapshot! body))

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
