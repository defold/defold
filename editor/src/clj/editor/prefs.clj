(ns editor.prefs
  (:require [clojure.edn :as edn]
            [service.log :as log])
  (:import [java.util.prefs Preferences]))

(defprotocol PreferenceStore
  (get-prefs [this key default])
  (set-prefs [this key default]))

(defn make-prefs [namespace]
  (.node (Preferences/userRoot) namespace))

(extend-type Preferences
  PreferenceStore
  (get-prefs [prefs key default]
    (if-let [val (.get prefs key nil)]
      (try
        (edn/read-string val)
        (catch Exception e
          (log/error :msg (format "failed to fetch preference for %s with value %s" key val) :exception e)
          default))
      default))

  (set-prefs [prefs key value]
    (.put prefs key (pr-str value))
    value))
