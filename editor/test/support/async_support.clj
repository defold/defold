(ns support.async-support
  (:require [clojure.core.async :as a]))

(defn eventually
  "Wait to take 1 value from the channel, failing on timeout (default 10 sec)"
  ([ch]
   (eventually ch (* 10 1000)))
  ([ch timeout-ms]
   (a/alt!!
     ch ([v] v)
     (a/timeout timeout-ms) (throw (ex-info "Timeout" {})))))