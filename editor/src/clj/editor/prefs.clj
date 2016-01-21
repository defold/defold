(ns editor.prefs
  (:require [clojure.edn :as edn]
            [service.log :as log]
            [cognitect.transit :as transit])
  (:import [java.util.prefs Preferences]
           [java.io ByteArrayOutputStream StringBufferInputStream]
           [javafx.scene.paint Color]))

(defn- color->web [^Color c]
  (let [v [(.getRed c) (.getGreen c) (.getBlue c) (.getOpacity c)]]
    (apply format "0x%02x%02x%02x%02x" (map #(long (Math/round (double (* 255 %)))) v))))

(def ^:private write-handlers {Color (transit/write-handler "Color" (fn [x] (color->web x)))})
(def ^:private read-handlers {"Color" (transit/read-handler (fn [x] (Color/web x)))})

(defn- transit-str [value]
  (let [out      (ByteArrayOutputStream. 4096)
        writer   (transit/writer out :json {:handlers write-handlers})]
     (transit/write writer value)
     (.toString out)))

(defn- read-transit [s]
  (let [reader (transit/reader (StringBufferInputStream. s) :json {:handlers read-handlers})]
    (transit/read reader)))

(defprotocol PreferenceStore
  (get-prefs [this key default])
  (set-prefs [this key value]))

(defn make-prefs [namespace]
  (.node (Preferences/userRoot) namespace))

(extend-type Preferences
  PreferenceStore
  (get-prefs [prefs key default]
    (if-let [val (.get prefs key nil)]
      (try
        (read-transit val)
        (catch Exception e
          (log/error :msg (format "failed to fetch preference for %s with value %s" key val) :exception e)
          (.remove prefs key)
          default))
      default))

  (set-prefs [prefs key value]
    (.put prefs key (transit-str value))
    value))
