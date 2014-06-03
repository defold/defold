(ns eclipse.handlers
  (:require [eclipse.tracer :as log]))

(defn lookup [event]
  (log/info "Event " event " received in clojure lookup function")
  println)
