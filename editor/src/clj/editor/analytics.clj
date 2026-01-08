;; Copyright 2020-2026 The Defold Foundation
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

(ns editor.analytics
  (:require [clojure.data.json :as json]
            [clojure.java.io :as io]
            [editor.system :as sys]
            [service.log :as log])
  (:import [clojure.lang PersistentQueue]
           [com.defold.editor Editor]
           [java.io File]
           [java.net HttpURLConnection MalformedURLException URL]
           [java.nio.charset StandardCharsets]
           [java.util UUID]
           [java.util.concurrent CancellationException]))

(set! *warn-on-reflection* true)

;; Set this to true to see the events that get sent when testing.
(defonce ^:private log-events? false)

(defonce ^:private batch-size 25)
(defonce ^:private cid-regex #"[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}")
(defonce ^:private config-atom (atom nil))
(defonce ^:private event-queue-atom (atom PersistentQueue/EMPTY))
(defonce ^:private max-failed-send-attempts 5)
(defonce ^:private payload-content-type "application/json")
(defonce ^:private shutdown-timeout 1000)
(defonce ^:private worker-atom (atom nil))
(defonce ^:private session-id (rand))

;; -----------------------------------------------------------------------------
;; Validation
;; -----------------------------------------------------------------------------

(defn- valid-analytics-url? [value]
  (and (string? value)
       (try
         (let [url (URL. value)]
           (and (nil? (.getRef url))
                (let [protocol (.getProtocol url)]
                  (or (= "http" protocol)
                      (= "https" protocol)))))
         (catch MalformedURLException _
           false))))

(defn- valid-cid? [value]
  (and (string? value)
       (.matches (re-matcher cid-regex value))))

(defn- valid-config? [config]
  (and (map? config)
       (= #{:cid} (set (keys config)))
       (let [{:keys [cid]} config]
         (valid-cid? cid))))

;; -----------------------------------------------------------------------------
;; Configuration
;; -----------------------------------------------------------------------------

(defn- make-config []
  {:post [(valid-config? %)]}
  {:cid (str (UUID/randomUUID))})

(defn- config-file
  ^File []
  (.toFile (.resolve (Editor/getSupportPath) ".defold-analytics")))

(defn- write-config-to-file! [{:keys [cid]} ^File config-file]
  {:pre [(valid-cid? cid)]}
  (let [json {"cid" cid}]
    (with-open [writer (io/writer config-file)]
      (json/write json writer))))

(defn- write-config! [config]
  (let [config-file (config-file)]
    (try
      (write-config-to-file! config config-file)
      (catch Throwable error
        (log/warn :msg (str "Failed to write analytics config: " config-file) :exception error)))))

(defn- read-config-from-file! [^File config-file]
  (with-open [reader (io/reader config-file)]
    (let [{cid "cid"} (json/read reader)]
      {:cid cid})))

(defn- read-config! []
  {:post [(valid-config? %)]}
  (let [config-file (config-file)
        config-from-file (try
                           (when (.exists config-file)
                             (read-config-from-file! config-file))
                           (catch Throwable error
                             (log/warn :msg (str "Failed to read analytics config: " config-file) :exception error)
                             nil))
        config (if-not (valid-config? config-from-file)
                 (make-config)
                 config-from-file)]
    (when (not= config-from-file config)
      (write-config! config))
    config))

;; -----------------------------------------------------------------------------
;; Internals
;; https://developers.google.com/analytics/devguides/collection/protocol/ga4/reference?client_type=gtag
;; -----------------------------------------------------------------------------

(defn- batch->payload
  ^bytes [batch]
  (let [config @config-atom
        cid (get config :cid)
        payload {:client_id cid :events batch}
        ^String payload-json (json/write-str payload)]
    (.getBytes payload-json StandardCharsets/UTF_8)))

(defn- get-response-code!
  "Wrapper rebound to verify response from dev server in tests."
  [^HttpURLConnection connection]
  (.getResponseCode connection))

(defn- post!
  ^HttpURLConnection [^String url-string ^String content-type ^bytes payload]
  {:pre [(not-empty payload)]}
  (let [^HttpURLConnection connection (.openConnection (URL. url-string))]
    (doto connection
      (.setRequestMethod "POST")
      (.setDoOutput true)
      (.setRequestProperty "Content-Type" content-type)
      (.connect))
    (with-open [output-stream (.getOutputStream connection)]
      (.write output-stream payload))
    connection))

(defn- ok-response-code? [^long response-code]
  (<= 200 response-code 299))

(defn- send-payload! [analytics-url ^bytes payload-json]
  (try
    (let [connection (post! analytics-url payload-content-type payload-json)
          response-message (.getResponseMessage connection)
          response-code (get-response-code! connection)
          response-body (slurp (.getInputStream connection))]
      (if (ok-response-code? response-code)
        true
        (do
          (log/warn :msg (str "Analytics server sent non-OK response code " response-code " " response-message " " response-body))
          false)))
    (catch Exception error
      (log/warn :msg "An exception was thrown when sending analytics data" :exception error)
      false)))

(defn- pop-count [queue ^long count]
  (nth (iterate pop queue) count))

(defn- send-one-batch!
  "Sends one batch of events from the queue. Returns false if there were events
  on the queue that could not be sent. Otherwise removes the successfully sent
  events from the queue and returns true."
  [analytics-url]
  (let [event-queue @event-queue-atom
        batch (into [] (take batch-size) event-queue)]
    (if (empty? batch)
      true
      (if-not (send-payload! analytics-url (batch->payload batch))
        false
        (do
          (swap! event-queue-atom pop-count (count batch))
          true)))))

(defn- send-remaining-batches! [analytics-url]
  (let [event-queue @event-queue-atom]
    (loop [event-queue event-queue]
      (when-some [batch (not-empty (into [] (take batch-size) event-queue))]
        (send-payload! analytics-url (batch->payload batch))
        (recur (pop-count event-queue (count batch)))))
    (swap! event-queue-atom pop-count (count event-queue))
    nil))

(declare shutdown!)

(defn- start-worker! [analytics-url ^long send-interval]
  (let [stopped-atom (atom false)
        thread (future
                 (try
                   (loop [failed-send-attempts 0]
                     (cond
                       (>= failed-send-attempts max-failed-send-attempts)
                       (do
                         (log/warn :msg (str "Analytics shut down after " max-failed-send-attempts " failed send attempts"))
                         (shutdown! 0)
                         (reset! event-queue-atom PersistentQueue/EMPTY)
                         nil)

                       @stopped-atom
                       (send-remaining-batches! analytics-url)

                       :else
                       (do
                         (Thread/sleep send-interval)
                         (if (send-one-batch! analytics-url)
                           (recur 0)
                           (recur (inc failed-send-attempts))))))
                   (catch CancellationException _
                     nil)
                   (catch InterruptedException _
                     nil)
                   (catch Throwable error
                     (log/warn :msg "Abnormal worker thread termination" :exception error))))]
    {:stopped-atom stopped-atom
     :thread thread}))

(defn- shutdown-worker! [{:keys [stopped-atom thread]} timeout-ms]
  (if-not (pos? timeout-ms)
    (future-cancel thread)
    (do
      ;; Try to shut down the worker thread gracefully, otherwise cancel the thread.
      (reset! stopped-atom true)
      (when (= ::timeout (deref thread timeout-ms ::timeout))
        (future-cancel thread))))
  nil)

(declare enabled?)

(defn- append-event! [event]
  (when-some [config @config-atom]
    (when (enabled?)
      (swap! event-queue-atom conj event))
    (when log-events?
      (log/info :msg event))))

;; -----------------------------------------------------------------------------
;; Public interface
;; -----------------------------------------------------------------------------

(defn start! [^String analytics-url send-interval]
  {:pre [(valid-analytics-url? analytics-url)]}
  (let [config (reset! config-atom (read-config!))]
    (when (some? (sys/defold-version))
      (swap! worker-atom
             (fn [started-worker]
               (when (some? started-worker)
                 (shutdown-worker! started-worker 0))
               (start-worker! analytics-url send-interval))))
    (:cid config)))

(defn shutdown!
  ([]
   (shutdown! shutdown-timeout))
  ([timeout-ms]
   (swap! worker-atom
          (fn [started-worker]
            (when (some? started-worker)
              (shutdown-worker! started-worker timeout-ms))))))

(defn enabled? []
  (some? @worker-atom))

(defn track-event!
  ([^String category ^String action]
   (append-event! {:name "select_content"
                   :params {:content_type (str category "-" action)
                            :engagement_time_msec "100"
                            :session_id session-id}}))
  ([^String category ^String action ^String label]
   (append-event! {:name "select_content"
                   :params {:content_type (str category "-" action)
                            :item_id label
                            :engagement_time_msec "100"
                            :session_id session-id}})))

(defn track-exception! [^Throwable exception]
  (append-event! {:name "exception"
                  :params {:name (.getSimpleName (class exception)) 
                           :engagement_time_msec "100"
                           :session_id session-id}}))

(defn track-screen! [^String screen-name]
    (append-event! {:name "page_view"
                    :params {:page_location screen-name
                             :engagement_time_msec "100"
                             :session_id session-id}}))

