(ns editor.prefs
  (:require [service.log :as log]
            [clojure.edn :as edn])
  (:import [java.util.prefs Preferences]))

(defn get-prefs [key default]
  (let [prefs (.node (Preferences/userRoot) "defold")]
    (if-let [val (.get prefs key nil)]
      (try
        (edn/read-string val)
        (catch Exception e
          (log/error :msg (format "failed to fetch preference for %s with value %s" key val) :exception e)
          default))
      default)))

(defn set-prefs [key value]
  (let [prefs (.node (Preferences/userRoot) "defold")]
    (.put prefs key (pr-str value))
    value))

