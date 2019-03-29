(ns service.smoke-log
  "Logger for logging extra events for interaction with the smoke test robot.
  Enable this logger by passing -Ddefold.smoke.log=true as a JVM parameter."
  (:import [org.slf4j LoggerFactory Logger]))

(set! *warn-on-reflection* true)

(def ^:private enabled-atom (atom nil))

(defn- enabled?
  []
  (let [enabled @enabled-atom]
    (if (nil? enabled)
      (let [enabled (Boolean/valueOf (System/getProperty "defold.smoke.log"))]
        (reset! enabled-atom enabled)
        enabled)
      enabled)))

(defn smoke-log
  [^String message]
  (when (enabled?)
    (let [logger (LoggerFactory/getLogger "DEFOLD-SMOKE-LOG")]
      (.info logger message))))

