(ns util.debug-util)

(defn- system-property-set? [system-property-name]
  (if-some [string-value (System/getProperty system-property-name)]
    (Boolean/parseBoolean string-value)
    false))

(defn metrics-enabled? []
  (system-property-set? "defold.metrics"))

(defmacro when-metrics
  "Only evaluate expr if we're running with the defold.metrics system property set.
  Returns the value of expr, or nil if the system property is not set."
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
        (println (str ~label " completed in " (/ (double (- end# start#)) 1000000.0) " msecs"))
        ret#))))
