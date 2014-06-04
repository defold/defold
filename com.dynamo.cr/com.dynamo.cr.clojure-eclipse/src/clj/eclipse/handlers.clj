(ns eclipse.handlers
  (:require [eclipse.reflect :as r]
            [service.log :as log])
  (:import [org.eclipse.core.expressions IEvaluationContext]
           [org.eclipse.core.commands ExecutionEvent]
           [org.eclipse.ui ISources]))

(def ^:private variable-names
  (filter #(.endsWith % "_NAME") 
          (map :name (r/constants org.eclipse.ui.ISources))))

(defn- variable [^ExecutionEvent event ^String nm]
  (.getVariable (.getApplicationContext event) nm))

(defn- variables [event]
  (when (instance? IEvaluationContext (.getApplicationContext event))
    (map (fn [n] [n (variable event n)]) variable-names)))

(defn lookup [event]
  (log/info :event event)
  (log/info :variables (variables event))
  println)
