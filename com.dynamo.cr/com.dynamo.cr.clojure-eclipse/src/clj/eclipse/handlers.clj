(ns eclipse.handlers
  (:require [eclipse.tracer :as log])
  (:import [org.eclipse.core.expressions IEvaluationContext]
           [org.eclipse.ui ISources]))

(def ^:private variable-names
  )

(defn- variables [event]
  (when (instance? IEvaluationContext (.getApplicationContext event))
    (map (fn [n] [n (.getVariable event n)]) (variable-names))))

(defn- log-variables [event]
  (doseq [[n v] (variables event)]
    (log/info n " = " v)))

(defn lookup [event]
  (log/info "Event " event " received in clojure lookup function")
  (log-variables event)
  println)

(comment
  		if (event.getApplicationContext() instanceof IEvaluationContext) {
			Object var = ((IEvaluationContext) event.getApplicationContext())
					.getVariable(name);
			return var == IEvaluationContext.UNDEFINED_VARIABLE ? null : var;
		}
		return null;
  )