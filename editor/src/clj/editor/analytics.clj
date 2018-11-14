(ns editor.analytics
  (:require [clojure.data.json :as json]
            [clojure.java.io :as io]
            [clojure.string :as string]
            [editor.system :as sys]
            [service.log :as log])
  (:import (clojure.lang PersistentQueue)
           (com.defold.editor Editor)
           (java.io File)
           (java.net HttpURLConnection MalformedURLException URL)
           (java.nio.charset StandardCharsets)
           (java.util UUID)
           (java.util.concurrent CancellationException)))

(set! *warn-on-reflection* true)

(defonce ^:private batch-size 16)
(defonce ^:private cid-regex #"[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}")
(defonce ^:private config-atom (atom nil))
(defonce ^:private event-queue-atom (atom PersistentQueue/EMPTY))
(defonce ^:private max-failed-send-attempts 5)
(defonce ^:private payload-content-type "application/x-www-form-urlencoded; charset=UTF-8")
(defonce ^:private send-remaining-interval 30)
(defonce ^:private shutdown-timeout 1000)
(defonce ^:private uid-regex #"[0-9A-F]{16}")
(defonce ^:private worker-atom (atom nil))

;; -----------------------------------------------------------------------------
;; Validation
;; -----------------------------------------------------------------------------

(defn- valid-analytics-url? [value]
  (and (string? value)
       (try
         (let [url (URL. value)]
           (and (nil? (.getQuery url))
                (nil? (.getRef url))
                (let [protocol (.getProtocol url)]
                  (or (= "http" protocol)
                      (= "https" protocol)))))
         (catch MalformedURLException _
           false))))

(defn- valid-cid? [value]
  (and (string? value)
       (.matches (re-matcher cid-regex value))))

(defn- valid-uid? [value]
  (and (string? value)
       (.matches (re-matcher uid-regex value))))

(defn- valid-config? [config]
  (and (map? config)
       (= #{:cid :uid} (set (keys config)))
       (let [{:keys [cid uid]} config]
         (and (valid-cid? cid)
              (or (nil? uid)
                  (valid-uid? uid))))))

(def ^:private valid-event?
  (every-pred map?
              (comp (every-pred string? not-empty) :t)
              (partial every?
                       (every-pred (comp keyword? key)
                                   (comp string? val)
                                   (comp not-empty val)))))

;; -----------------------------------------------------------------------------
;; Configuration
;; -----------------------------------------------------------------------------

(defn- make-config []
  {:post [(valid-config? %)]}
  {:cid (str (UUID/randomUUID))
   :uid nil})

(defn- config-file
  ^File []
  (.toFile (.resolve (Editor/getSupportPath) ".defold-analytics")))

(defn- write-config-to-file! [{:keys [cid uid]} ^File config-file]
  {:pre [(valid-cid? cid)
         (or (nil? uid) (valid-uid? uid))]}
  (let [json (if (some? uid)
               {"cid" cid "uid" uid}
               {"cid" cid})]
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
    (let [{cid "cid" uid "uid"} (json/read reader)]
      {:cid cid :uid uid})))

(defn- read-config! [invalidate-uid?]
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
                 (cond-> config-from-file
                         (and invalidate-uid?
                              (contains? config-from-file :uid))
                         (assoc :uid nil)))]
    (when (not= config-from-file config)
      (write-config! config))
    config))

;; -----------------------------------------------------------------------------
;; Internals
;; -----------------------------------------------------------------------------

(defn- batch->payload
  ^bytes [batch {:keys [cid uid] :as _config}]
  {:pre [(vector? batch)
         (not-empty batch)
         (valid-cid? cid)
         (or (nil? uid) (valid-uid? uid))]}
  (let [common-pairs (cond-> ["v=1" "tid=UA-83690-7" (str "cid=" cid)]
                             (some? uid) (conj (str "uid=" uid)))
        lines (map (fn [event]
                     (let [pairs (into common-pairs
                                       (map (fn [[k v]]
                                              (str (name k) "=" v)))
                                       event)]
                       (string/join "&" pairs)))
                   batch)
        text (string/join "\n" lines)]
    (.getBytes text StandardCharsets/UTF_8)))

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
      (.setFixedLengthStreamingMode (count payload))
      (.setRequestProperty "Content-Type" content-type)
      (.connect))
    (with-open [output-stream (.getOutputStream connection)]
      (.write output-stream payload))
    connection))

(defn- ok-response-code? [^long response-code]
  (<= 200 response-code 299))

(defn- send-payload! [analytics-url ^bytes payload]
  (try
    (let [response-code (get-response-code! (post! analytics-url payload-content-type payload))]
      (if (ok-response-code? response-code)
        true
        (do
          (log/warn :msg (str "Analytics server sent non-OK response code " response-code))
          false)))
    (catch Exception error
      (log/warn :msg "An exception was thrown when sending analytics data" :exception error)
      false)))

(defn- pop-count [queue ^long count]
  (loop [remaining-count count
         shortened-queue queue]
    (if (zero? remaining-count)
      shortened-queue
      (recur (dec remaining-count)
             (pop shortened-queue)))))

(defn- send-one-batch!
  "Sends one batch of events from the queue. Returns false if there were events
  on the queue that could not be sent. Otherwise removes the successfully sent
  events from the queue and returns true."
  [analytics-url]
  (let [config @config-atom]
    (if (nil? config)
      true
      (let [event-queue @event-queue-atom
            batch (into [] (take batch-size) event-queue)]
        (if (empty? batch)
          true
          (if-not (send-payload! analytics-url (batch->payload batch config))
            false
            (do
              (swap! event-queue-atom pop-count (count batch))
              true)))))))

(defn- send-remaining-batches! [analytics-url]
  (when-some [config @config-atom]
    (let [event-queue @event-queue-atom]
      (loop [event-queue event-queue]
        (when-some [batch (not-empty (into [] (take batch-size) event-queue))]
          (Thread/sleep send-remaining-interval)
          (send-payload! analytics-url (batch->payload batch config))
          (recur (pop-count event-queue (count batch)))))
      (swap! event-queue-atom pop-count (count event-queue))
      nil)))

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
  (assert (valid-event? event))
  (when (enabled?)
    (swap! event-queue-atom conj event)
    nil))

;; -----------------------------------------------------------------------------
;; Public interface
;; -----------------------------------------------------------------------------

(defn start! [^String analytics-url send-interval invalidate-uid?]
  {:pre [(valid-analytics-url? analytics-url)]}
  (when (some? (sys/defold-version))
    (reset! config-atom (read-config! invalidate-uid?))
    (swap! worker-atom
           (fn [started-worker]
             (when (some? started-worker)
               (shutdown-worker! started-worker 0))
             (start-worker! analytics-url send-interval)))))

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

(defn set-uid! [^String uid]
  {:pre [(or (nil? uid) (valid-uid? uid))]}
  (swap! config-atom
         (fn [config]
           (let [config (or config (read-config! false))
                 updated-config (assoc config :uid uid)]
             (when (not= config updated-config)
               (write-config! updated-config))
             updated-config))))

(defn track-event!
  ([^String category ^String action]
   (append-event! {:t "event"
                   :ec category
                   :ea action}))
  ([^String category ^String action ^String label]
   (append-event! {:t "event"
                   :ec category
                   :ea action
                   :el label})))

(defn track-exception! [^Throwable exception]
  (append-event! {:t "exception"
                  :exd (.getSimpleName (class exception))}))

(defn track-screen! [^String screen-name]
  (append-event! {:t "screenview"
                  :an "defold"
                  :av (sys/defold-version)
                  :cd screen-name}))
