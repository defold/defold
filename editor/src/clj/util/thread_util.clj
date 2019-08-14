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

(defn preset!
  "Sets the value of atom to newval without regard for the current value.
  Returns the previous value that was in atom before newval was swapped in."
  [atom newval]
  (let [*prev (volatile! nil)]
    (swap! atom (fn [prev]
                  (vreset! *prev prev)
                  newval))
    (deref *prev)))

(defn swap-rest!
  "Similar to core.swap!, but f is expected to return a sequence. The first
  element is swapped into the atom. Returns the remaining elements."
  [atom f & args]
  (let [*rest (volatile! nil)]
    (swap! atom (fn [val]
                  (let [[newval & rest] (apply f val args)]
                    (vreset! *rest rest)
                    newval)))
    (deref *rest)))
