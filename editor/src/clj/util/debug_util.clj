;; Copyright 2020-2025 The Defold Foundation
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

(ns util.debug-util
  (:require [clojure.repl :as repl]
            [service.log :as log])
  (:import [java.util Locale]
           [org.apache.commons.lang3.time DurationFormatUtils]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def ^:private ^:const nanos-per-ms 1000000.0)
(def ^:private ^:const nanos-per-second 1000000000.0)
(def ^:private ^:const nanos-per-minute (* nanos-per-second 60.0))

(def ^:private ^:const bytes-per-kilobyte 1024.0)
(def ^:private ^:const bytes-per-megabyte (* 1024.0 bytes-per-kilobyte))
(def ^:private ^:const bytes-per-gigabyte (* 1024.0 bytes-per-megabyte))
(def ^:private ^:const bytes-per-terabyte (* 1024.0 bytes-per-gigabyte))

(defn- decimal-string
  ^String [num ^String unit]
  (String/format Locale/ROOT "%.2f %s" (to-array [num unit])))

(defn nanos->millis
  "Converts a nanosecond value into a double in milliseconds."
  ^double [^long nanos]
  (/ (double nanos) nanos-per-ms))

(defn nanos->seconds
  "Converts a nanosecond value into a double in seconds."
  ^double [^long nanos]
  (/ (double nanos) nanos-per-second))

(defn nanos->minutes
  "Converts a nanosecond value into a double in minutes."
  ^double [^long nanos]
  (/ (double nanos) nanos-per-minute))

(defn nanos->string
  "Converts a nanosecond value into a human-readable duration string."
  ^String [^long nanos]
  (let [ms (nanos->millis nanos)]
    (if (> 1000.0 ms)
      (decimal-string ms "ms")
      (let [seconds (nanos->seconds nanos)]
        (if (> 60.0 seconds)
          (decimal-string seconds "s")
          (let [minutes (nanos->minutes nanos)]
            (if (> 60.0 minutes)
              (DurationFormatUtils/formatDuration ms "m:ss")
              (DurationFormatUtils/formatDuration ms "H:mm:ss"))))))))

(defn bytes->string
  "Converts a byte size into a human-readable size string."
  ^String [^long bytes]
  (let [double-bytes (double bytes)]
    (condp > double-bytes
      bytes-per-kilobyte (str bytes " B")
      bytes-per-megabyte (decimal-string (/ double-bytes bytes-per-kilobyte) "KB")
      bytes-per-gigabyte (decimal-string (/ double-bytes bytes-per-megabyte) "MB")
      bytes-per-terabyte (decimal-string (/ double-bytes bytes-per-gigabyte) "GB")
      (decimal-string (/ double-bytes bytes-per-terabyte) "TB"))))

(defn- stack-trace-impl [^Thread thread trimmed-fn-class-names-set]
  {:pre [(set? trimmed-fn-class-names-set)]}
  (let [own-fn-class-name (.getName (class stack-trace-impl))
        trimmed-fn-class-names-set (conj trimmed-fn-class-names-set own-fn-class-name)]
    (into []
          (comp (drop 1) ; Trim the getStackTrace method call itself.
                (drop-while (fn [^StackTraceElement stack-trace-element]
                              (contains? trimmed-fn-class-names-set (.getClassName stack-trace-element))))
                (map repl/stack-element-str))
          (.getStackTrace thread))))

(defn stack-trace
  "Returns a human-readable stack trace as a vector of strings. Elements are
  ordered from the stack-trace function call site towards the outermost stack
  frame of the current Thread. Optionally, a different Thread can be specified."
  ([]
   (stack-trace (Thread/currentThread)))
  ([^Thread thread]
   (let [own-fn-class-name (.getName (class stack-trace))
         trimmed-fn-class-names-set #{own-fn-class-name}]
     (stack-trace-impl thread trimmed-fn-class-names-set))))

(defn print-stack-trace!
  "Prints the stack trace of the current thread to *out*. Optionally, a prefix
  string to append to the beginning of each line can be specified, as well as a
  different Thread."
  ([]
   (print-stack-trace! nil (Thread/currentThread)))
  ([^String line-prefix]
   (print-stack-trace! line-prefix (Thread/currentThread)))
  ([^String line-prefix ^Thread thread]
   (let [own-fn-class-name (.getName (class print-stack-trace!))
         trimmed-fn-class-names-set #{own-fn-class-name}
         stack-trace (stack-trace-impl thread trimmed-fn-class-names-set)
         print-line! (if (empty? line-prefix)
                       println
                       (fn print-fn [^String line]
                         (print line-prefix)
                         (println line)))]
     (run! print-line! stack-trace))))

(defn release-build?
  "Returns true if we're running a release build of the editor."
  []
  (and (nil? (System/getProperty "defold.dev"))
       (some? (System/getProperty "defold.version"))))

(defn running-tests?
  "Returns true if we're running with the defold.tests system property set."
  []
  (Boolean/getBoolean "defold.tests"))

(defn metrics-enabled?
  "Returns true if we're running with the defold.metrics system property set."
  []
  (Boolean/getBoolean "defold.metrics"))

(defmacro if-metrics
  "Evaluate metrics-expr if we're running with the defold.metrics system property set.
  Otherwise, evaluate no-metrics-expr."
  [metrics-expr no-metrics-expr]
  (if (metrics-enabled?)
    metrics-expr
    no-metrics-expr))

(defmacro when-metrics
  "Only evaluate body if we're running with the defold.metrics system property set.
  Returns the value of the last expr in body, or nil if the system property is not set."
  [& body]
  (if (metrics-enabled?)
    (cons 'do body)
    nil))

(defmacro metrics-time
  "Evaluates expr. Then prints the supplied label along with the time it took if
  we're running a metrics version. Regardless, returns the value of expr."
  ([expr]
   (metrics-time "Expression" expr))
  ([label expr]
   (if-not (metrics-enabled?)
     expr
     `(let [start# (System/nanoTime)
            ret# ~expr
            end# (System/nanoTime)]
        (println (str ~label " completed in " (nanos->string (- end# start#))))
        ret#))))

(defmacro log-time
  "Evaluates expr. Then logs the supplied label along with the time it took.
  Returns the value of expr."
  ([expr]
   (log-time "Expression" expr))
  ([label expr]
   ;; Disabled during tests to minimize log spam.
   (if (running-tests?)
     expr
     `(let [start# (System/nanoTime)
            ret# ~expr
            end# (System/nanoTime)]
        (log/info :message (str ~label " completed in " (nanos->string (- end# start#))))
        ret#))))

(defmacro allocated-bytes
  "Performs a garbage collection, then returns the number of bytes currently
  allocated in the JVM."
  ([]
   `(allocated-bytes (Runtime/getRuntime)))
  ([runtime-expr]
   `(long (let [^Runtime runtime# ~runtime-expr]
            (System/gc)
            (System/runFinalization)
            (- (.totalMemory runtime#)
               (.freeMemory runtime#))))))

(defmacro log-time-and-memory
  "Evaluates expr. Then logs the supplied label along with the time it took and
  the amount of memory allocated in the process. Returns the value of expr."
  ([expr]
   (log-time-and-memory "Expression" expr))
  ([label expr]
   ;; Disabled during tests to minimize log spam.
   (if (running-tests?)
     expr
     `(let [runtime# (Runtime/getRuntime)
            start-bytes# (allocated-bytes runtime#)
            start-ns# (System/nanoTime)
            ret# ~expr
            end-ns# (System/nanoTime)
            end-bytes# (allocated-bytes runtime#)
            allocated-bytes# (- end-bytes# start-bytes#)
            elapsed-ns# (- end-ns# start-ns#)]
        (if (pos? allocated-bytes#)
          (log/info :message ~label
                    :elapsed (nanos->string elapsed-ns#)
                    :allocated (bytes->string allocated-bytes#))
          (log/info :message ~label
                    :elapsed (nanos->string elapsed-ns#)))
        ret#))))

(defmacro log-statistics!
  "Gathers and logs statistics relevant to editor development."
  [label]
  (when-not (or (release-build?)
                (running-tests?))
    `(log/info :message ~label :allocated (bytes->string (allocated-bytes)))))

(defmacro make-metrics-collector
  "Returns a metrics-collector for use with the measuring macro if we're running
  a metrics version. Otherwise, returns nil."
  []
  (when (metrics-enabled?)
    '(volatile! {})))

(defn- update-task-metrics
  [metrics task-key ^long counter]
  (if-some [task-entry (find metrics task-key)]
    (let [^long task-counter (val task-entry)]
      (assoc metrics task-key (+ task-counter counter)))
    (assoc metrics task-key counter)))

(defn- update-subtask-metrics
  [metrics task-key subtask-key ^long counter]
  (if-some [task-entry (find metrics task-key)]
    (let [task-metrics (val task-entry)]
      (if-some [subtask-entry (find task-metrics subtask-key)]
        (let [^long subtask-counter (val subtask-entry)]
          (assoc metrics task-key (assoc task-metrics subtask-key (+ subtask-counter counter))))
        (assoc metrics task-key (assoc task-metrics subtask-key counter))))
    (assoc metrics task-key {subtask-key counter})))

(defmacro update-metrics
  "Adds to the specified counter in the provided metrics-collector."
  ([metrics-collector task-key counter]
   (when (metrics-enabled?)
     `(when ~metrics-collector
        (vswap! ~metrics-collector #'update-task-metrics ~task-key ~counter)
        nil)))
  ([metrics-collector task-key subtask-key counter]
   (when (metrics-enabled?)
     `(when ~metrics-collector
        (vswap! ~metrics-collector #'update-subtask-metrics ~task-key ~subtask-key ~counter)
        nil))))

(defmacro measuring
  "Evaluates expr. Then adds the time it took to the specified counter in the
  provided metrics-collector if we're running a metrics version. Regardless,
  returns the value of expr."
  ([metrics-collector task-key expr]
   (if-not (metrics-enabled?)
     expr
     `(let [start# (System/nanoTime)
            ret# ~expr
            end# (System/nanoTime)]
        (update-metrics ~metrics-collector ~task-key (- end# start#))
        ret#)))
  ([metrics-collector task-key subtask-key expr]
   (if-not (metrics-enabled?)
     expr
     `(let [start# (System/nanoTime)
            ret# ~expr
            end# (System/nanoTime)]
        (update-metrics ~metrics-collector ~task-key ~subtask-key (- end# start#))
        ret#))))
