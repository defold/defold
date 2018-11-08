(ns editor.analytics
  (:require [clojure.data.json :as json]
            [clojure.java.io :as io]
            [clojure.string :as string]
            [editor.system :as sys]
            [service.log :as log])
  (:import (clojure.lang PersistentQueue)
           (com.defold.editor Editor)
           (java.io File IOException)
           (java.net HttpURLConnection MalformedURLException URL)
           (java.nio.charset StandardCharsets)
           (java.util UUID)))

(set! *warn-on-reflection* true)

(defonce ^:private batch-size 16)
(defonce ^:private cid-regex #"[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}")
(defonce ^:private config-atom (atom nil))
(defonce ^:private event-queue-atom (atom PersistentQueue/EMPTY))
(defonce ^:private payload-content-type "application/x-www-form-urlencoded; charset=UTF-8")
(defonce ^:private send-interval 300)
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

(defn- write-config! [{:keys [cid uid]}]
  {:pre [(valid-cid? cid)
         (or (nil? uid) (valid-uid? uid))]}
  (let [config-file (config-file)
        json (if (some? uid)
               {"cid" cid "uid" uid}
               {"cid" cid})]
    (try
      (with-open [writer (io/writer config-file)]
        (json/write json writer))
      (catch Throwable error
        (log/error :msg (str "Failed to write analytics config: " config-file) :exception error)))))

(defn- read-config! [signed-in?]
  {:post [(valid-config? %)]}
  (let [config-file (config-file)
        config-from-file (when (.exists config-file)
                           (try
                             (with-open [reader (io/reader config-file)]
                               (let [{cid "cid" uid "uid"} (json/read reader)]
                                 {:cid cid :uid uid}))
                             (catch Throwable error
                               (log/error :msg (str "Failed to read analytics config: " config-file) :exception error)
                               nil)))
        config (if-not (valid-config? config-from-file)
                 (make-config)
                 (cond-> config-from-file
                         (and (not signed-in?)
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

(defn- post! [^String url-string ^String content-type ^bytes payload]
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
    (.getResponseCode connection)))

(defn- ok-response-code? [^long response-code]
  (<= 200 response-code 299))

(defn- send-payload! [analytics-url ^bytes payload]
  (try
    (ok-response-code? (post! analytics-url payload-content-type payload))
    (catch IOException _
      ;; NOTE: We don't log on errors as we don't have backoff inplace.
      ;; Same for non-ok response code.
      false)))

(defn- pop-count [queue ^long count]
  (loop [remaining-count count
         shortened-queue queue]
    (if (zero? remaining-count)
      shortened-queue
      (recur (dec remaining-count)
             (pop shortened-queue)))))

(defn- send-one-batch! [analytics-url]
  (when-some [config @config-atom]
    (let [event-queue @event-queue-atom
          batch (into [] (take batch-size) event-queue)]
      (when (and (seq batch)
                 (send-payload! analytics-url (batch->payload batch config)))
        (swap! event-queue-atom pop-count (count batch))))))

(defn- start-worker! [analytics-url]
  (let [stopped-atom (atom false)
        thread (future
                 (try
                   (loop []
                     (when-not @stopped-atom
                       (Thread/sleep send-interval)
                       (send-one-batch! analytics-url)
                       (recur)))
                   (catch Throwable error
                     (log/error :msg "Abnormal worker thread termination" :exception error))))]
    {:stopped-atom stopped-atom
     :thread thread}))

(defn- shutdown-worker! [{:keys [stopped-atom thread]}]
  (reset! stopped-atom true)
  (deref thread) ; Block until graceful exit.
  nil)

(declare enabled?)

(defn- append-event! [event]
  (when (enabled?)
    (assert (valid-event? event))
    (swap! event-queue-atom conj event)
    nil))

;; -----------------------------------------------------------------------------
;; Public interface
;; -----------------------------------------------------------------------------

(defn start! [^String analytics-url signed-in?]
  {:pre [(valid-analytics-url? analytics-url)]}
  (when (some? (sys/defold-version))
    (reset! config-atom (read-config! signed-in?))
    (swap! worker-atom
           (fn [started-worker]
             (when (some? started-worker)
               (shutdown-worker! started-worker))
             (start-worker! analytics-url)))))

(defn stop! []
  (swap! worker-atom shutdown-worker!))

(defn enabled? []
  (some? @worker-atom))

(defn set-uid! [^String uid]
  {:pre [(or (nil? uid) (valid-uid? uid))]}
  (swap! config-atom
         (fn [config]
           (when (some? config)
             (let [updated-config (assoc config :uid uid)]
               (when (not= config updated-config)
                 (write-config! updated-config))
               updated-config)))))

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
