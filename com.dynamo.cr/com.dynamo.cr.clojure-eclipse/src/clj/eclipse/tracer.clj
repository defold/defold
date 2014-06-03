(ns eclipse.tracer
  (:import [com.dynamo.cr.clojure_eclipse ClojureEclipse ITracer]))

(defn- log-expr [form option bits]
  `(.trace (ClojureEclipse/getTracer) ~option (object-array (list :line ~(:line (meta form)) ~@bits))))

(defmacro info  [& bits] (log-expr &form "log/info"  bits))
(defmacro warn  [& bits] (log-expr &form "log/warn"  bits))
(defmacro error [& bits] (log-expr &form "log/error" bits))
