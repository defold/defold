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

(defn make-cached-future-fn
  "Returns a cached version of a referentially transparent function that
  performs an expensive operation. The returned function will return a future.
  If invoked with the same argument list, the same future will be returned.
  If invoked with a different argument list, calls future-cancel on any pending
  future, then returns a new future with the result of calling f with the new
  arguments. The f function should call Thread/interrupted at regular intervals
  to check whether or not it has been cancelled."
  [f]
  (let [current (atom {:args nil :future nil})]
    (with-meta (fn [& new-args]
                 (let [{current-args :args current-future :future} @current]
                   (if (= new-args current-args)
                     current-future
                     (do (when current-future
                           (future-cancel current-future))
                         (let [new-future (future-call #(apply f new-args))]
                           (reset! current {:args new-args :future new-future})
                           new-future)))))
               {::current current})))

(defn cancel-cached-future-fn
  "Given a function returned by make-cached-future-fn, call future-cancel on
  the last pending future produced by the returned function, if any."
  [cached-future-fn]
  (let [current (-> cached-future-fn meta ::current)]
    (swap! current (fn [{current-future :future}]
                     (when current-future
                       (future-cancel current-future))
                     {:args nil :future nil})))
  nil)
