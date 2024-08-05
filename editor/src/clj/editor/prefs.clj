;; Copyright 2020-2024 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;; 
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;; 
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.


(ns editor.prefs
  (:require [clojure.data.json :as json]
            [clojure.java.io :as io]
            [dynamo.graph :as g]
            [service.log :as log]
            [cognitect.transit :as transit])
  (:import [java.util.prefs Preferences]
           [java.io ByteArrayOutputStream ByteArrayInputStream StringBufferInputStream]
           [javafx.scene.paint Color]
           [java.nio.charset StandardCharsets]))

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

(defn- read-transit [^String s]
  (let [stream (ByteArrayInputStream. (.getBytes s StandardCharsets/UTF_8))
        reader (transit/reader stream :json {:handlers read-handlers})]
    (transit/read reader)))

(defprotocol PreferenceStore
  (get-prefs [this key default])
  (set-prefs [this key value]))

(defn- sanitize-prefs! [prefs]
  (doto prefs
    (set-prefs "email" nil)
    (set-prefs "first-name" nil)
    (set-prefs "last-name" nil)
    (set-prefs "server-url" nil)
    (set-prefs "token" nil)))

(defn make-prefs [namespace]
  (-> (Preferences/userRoot)
      (.node namespace)
      sanitize-prefs!))

(defn make-project-specific-key
  ([key workspace]
   (g/with-auto-evaluation-context evaluation-context
     (make-project-specific-key key workspace evaluation-context)))
  ([key workspace evaluation-context]
   (str key "-" (hash (g/node-value workspace :root evaluation-context)))))

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

(defn update-prefs [this key f & args]
  (let [value (get-prefs this key ::not-found)]
    (when-not (= ::not-found value)
      (set-prefs this key (apply f value args)))))

(defrecord DevPreferences [path store]
  PreferenceStore
  (get-prefs [_ key default]
    (get @store key default))
  (set-prefs [_ key value]
    (swap! store assoc key value)))

(defn load-prefs [path]
  (with-open [reader (io/reader path)]
    (let [prefs (->> (json/read reader :key-fn keyword)
                  (reduce-kv (fn [m k v] (assoc m (name k) v)) {}))]
      (sanitize-prefs!
        (->DevPreferences path (atom prefs))))))
