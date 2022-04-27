(ns util.debug-util)

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn counter->ms
  "Converts a nanosecond counter value into a double in milliseconds."
  ^double [^long counter]
  (/ (double counter) 1000000.0))

(defn counter->seconds
  "Converts a nanosecond counter value into a double in seconds."
  ^double [^long counter]
  (/ (double counter) 1000000000.0))

(defn counter->minutes
  "Converts a nanosecond counter value into a double in minutes."
  ^double [^long counter]
  (/ (double counter) 6.0E10))

(defn- system-property-set?
  [system-property-name]
  (if-some [string-value (System/getProperty system-property-name)]
    (Boolean/parseBoolean string-value)
    false))

(defn metrics-enabled?
  "Returns true if we're running with the defold.metrics system property set."
  []
  (system-property-set? "defold.metrics"))

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
        (println (str ~label " completed in " (counter->ms (- end# start#)) " ms"))
        ret#))))

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
