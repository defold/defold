
(ns editor.prefs
  (:require [clojure.edn :as edn]
            [clojure.data.json :as json]
            [clojure.java.io :as io]
            [service.log :as log]
            [cognitect.transit :as transit])
  (:import [java.util.prefs Preferences PreferenceChangeListener PreferenceChangeEvent]
           [java.io ByteArrayOutputStream StringBufferInputStream]
           [javafx.scene.paint Color]))

(set! *warn-on-reflection* true)

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
  (set-prefs [this key value])
  (make-listener [this listen-fn])
  (add-listener! [this listener])
  (remove-listener! [this listener]))

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
    value)

  (make-listener [_prefs listen-fn]
    (assert (ifn? listen-fn))
    (reify PreferenceChangeListener
      (preferenceChange [_this preference-change-event]
        (let [changed-prefs-key (.getKey preference-change-event)]
          (listen-fn changed-prefs-key)))))

  (add-listener! [prefs listener]
    (.addPreferenceChangeListener prefs ^PreferenceChangeListener listener))

  (remove-listener! [prefs listener]
    (.removePreferenceChangeListener prefs ^PreferenceChangeListener listener)))

(defrecord DevPreferences [path store listeners]
  PreferenceStore
  (get-prefs [_ key default]
    (get @store key default))

  (set-prefs [_ key value]
    (swap! store assoc key value)
    (doseq [listen-fn @listeners]
      (listen-fn key)))

  (make-listener [_ listen-fn]
    (assert (ifn? listen-fn))
    listen-fn)

  (add-listener! [_ listener]
    (swap! listeners conj listener))

  (remove-listener! [_ listener]
    (swap! listeners (partial filterv (partial not= listener)))))

(defn load-prefs [path]
  (with-open [reader (io/reader path)]
    (let [prefs (->> (json/read reader :key-fn keyword)
                     (reduce-kv (fn [m k v] (assoc m (name k) v)) {}))]
      (->DevPreferences path (atom prefs) (atom [])))))
