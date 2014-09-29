(ns dynamo.resource)

(set! *warn-on-reflection* true)

(defprotocol IDisposable
  (dispose [this] "Clean up a value, including thread-jumping as needed"))

(defn disposable? [x] (satisfies? IDisposable x))