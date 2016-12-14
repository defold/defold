(ns util.thread-util)

(defn throw-if-interrupted!
  "Checks if the current thread has been interrupted, and if so throws an
  InterruptedException. Calling this function clears the interrupted status
  of the executing thread."
  []
  (if (Thread/interrupted)
    (throw (InterruptedException.))))

(defn abortable-identity!
  "Like core.identity, but throws an InterruptedException if the executing
  thread was interrupted. Suitable for use with map in a transducer or lazy
  sequence transformation."
  [value]
  (throw-if-interrupted!)
  value)
