(ns basement
  (:require [dynamo.ui :as ui]))

(comment

  (def property-update-debouncer (display-debouncer 100 refresh-property-page))

  (signal property-update-debouncer)
  (cancel property-update-debouncer)

  )

(comment

  (defn now [] (System/currentTimeMillis))

  (defprotocol Condition
    (signal [this] "Notify a deferred action of something"))

  (defprotocol Cancelable
    (cancel [this] "Cancel a thing."))

  (deftype DisplayDebouncer [delay action ^:volatile-mutable signaled ^:volatile-mutable last-signaled ^:volatile-mutable run-count]
    Condition
    (signal [this]
      (prn :signal this)
      (set! last-signaled (now))
      (set! signaled true)
      (.asyncExec (ui/display) this))

    Cancelable
    (cancel [this]
      (prn :cancel this)
      (set! signaled false))

    Runnable
    (run [this]
      (set! run-count (inc run-count)) ; possible race condition here (probably not an issue since run is scheduled on the display thread)
      (when signaled
        (if (< delay (- (now) last-signaled))
          (do
            (action)
            (set! signaled false)
            (prn :run-count run-count :delay (- (now) last-signaled)))
          (.asyncExec (ui/display) this)))))

  (defn display-debouncer
    [when f]
    (->DisplayDebouncer when f nil false 0))

  )
