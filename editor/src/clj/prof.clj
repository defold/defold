(ns prof
  (:require [clj-async-profiler.core :as prof])
  (:import [java.awt Desktop]
           [java.io File]
           [java.net URI]))

(set! *warn-on-reflection* true)

(defn- open-uri! [^URI uri]
  ;; NOTE: Running Desktop methods on JavaFX application thread locks the
  ;; application on Linux, so we do it on a new thread.
  (future
    (try
      (.browse (Desktop/getDesktop) uri)
      nil
      (catch Throwable error
        (println (str "Failed to open URI " uri))
        (println (.getMessage error))))))

(defn list-event-types
  "Print all event types that can be sampled by the profiler. Available options:

  :pid - process to attach to (default: current process)"
  ([] (prof/list-event-types))
  ([options] (prof/list-event-types options)))

(defn profile-start
  "Start the profiler. Available options:

  :pid - process to attach to (default: current process)
  :interval - sampling interval in nanoseconds (default: 1000000 - 1ms)
  :threads - profile each thread separately
  :event - event to profile, see `list-event-types` (default: :cpu)"
  ([] (profile-start {}))
  ([options] (print (prof/start options))))

(defn profile-stop
  "Stop the currently running profiler and and open the resulting flamegraph in
  the system-configured browser. Available options:

  :pid - process to attach to (default: current process)
  :min-width - minimum width in pixels for a frame to be shown on a flamegraph.
               Use this if the resulting flamegraph is too big and hangs your
               browser (default: nil, recommended: 1-5)
  :reverse? - if true, generate the reverse flamegraph which grows from callees
              up to callers (default: false)
  :icicle? - if true, invert the flamegraph upside down (default: false for
             regular flamegraph, true for reverse)"
  ([] (profile-stop {}))
  ([options]
   (let [augmented-options (assoc options :generate-flamegraph? true)
         ^File flamegraph-svg-file (prof/stop augmented-options)]
     (println "Stopped profiling")
     (open-uri! (.toURI flamegraph-svg-file)))))

(defn profile-for
  "Run the profiler for the specified duration. Open the resulting flamegraph in
  the system-configured browser. For available options, see `profile-start` and
  `profile-stop`."
  ([duration-in-seconds]
   (profile-for duration-in-seconds {}))
  ([duration-in-seconds options]
   (profile-start options)
   (Thread/sleep (* duration-in-seconds 1000))
   (profile-stop options)))

(defmacro profile
  "Profile the execution of `body`. If the first argument is a map, treat it as
  options. For available options, see `profile-start` and `profile-stop`. The
  `:pid` option is ignored, current process is always profiled."
  [options? & body]
  (let [[options body] (if (map? options?)
                         [(dissoc options? :pid) body]
                         [{} (cons options? body)])]
    `(let [options# ~options]
       (profile-start options#)
       (try
         ~@body
         (finally
           (profile-stop options#))))))

(defn instrument!
  "Wrap the specified function so that calling it will profile and open the
  resulting flamegraph in the system-configured browser. For available options,
  see `profile-start` and `profile-stop`. The `:pid` option is ignored, current
  process is always profiled."
  ([fn-sym] (instrument! fn-sym {}))
  ([fn-sym options]
   (let [augmented-options (dissoc options :pid)]
     (alter-var-root
       (resolve fn-sym)
       (fn [fn-var]
         (assert (ifn? fn-var) "Symbol must be bound to a function.")
         (let [fn-meta (meta fn-var)]
           (if (some? (::uninstrumented-fn fn-meta))
             fn-var
             (with-meta
               (fn profile-instrumentation [& args]
                 (profile-start augmented-options)
                 (try
                   (apply fn-var args)
                   (finally
                     (profile-stop augmented-options))))
               {::uninstrumented-fn fn-var}))))))))

(defn uninstrument!
  "Undo instrumentation of the specified function."
  [fn-sym]
  (alter-var-root
    (resolve fn-sym)
    (fn [fn-var]
      (assert (ifn? fn-var) "Symbol must be bound to a function.")
      (or (::uninstrumented-fn (meta fn-var))
          fn-var))))
