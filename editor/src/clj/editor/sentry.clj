(ns editor.sentry
  (:require
   [clojure.data.json :as json]
   [clojure.java.io :as io]
   [clojure.string :as string]
   [editor.system :as system]
   [service.log :as log]
   [util.http-client :as http])
  (:import
   (java.util.concurrent LinkedBlockingQueue TimeUnit)
   (java.time LocalDateTime ZoneOffset)))

(set! *warn-on-reflection* true)

(def user-agent "sentry-defold/1.0")

(defn- stacktrace-data
  [^Exception ex]
  {:frames (vec (for [^StackTraceElement frame (reverse (.getStackTrace ex))]
                  {:filename (.getFileName frame)
                   :lineo (.getLineNumber frame)
                   :function (.getMethodName frame)
                   :module (.getClassName frame)}))})

(defn- exception-data
  [^Exception ex ^Thread thread]
  (let [frames (seq (.getStackTrace ex))
        ^StackTraceElement top (first frames)]
    [{:type (str (.getClass ex))
      :value (.getMessage ex)
      :module (str (.getClassName top) "." (.getMethodName top))
      :thread_id (.getId thread)}]))

(defn- thread-data
  [^Thread thread ^Exception ex]
  [{:id (str (.getId thread))
    :current true
    :name (.getName thread)
    :stacktrace (stacktrace-data ex)}])

(defn make-event
  [^Exception ex ^Thread thread]
  {:event_id (string/replace (str (java.util.UUID/randomUUID)) "-" "")
   :message (.getMessage ex)
   :timestamp (LocalDateTime/now ZoneOffset/UTC)
   :level "error"
   :logger ""
   :platform "java"
   :sdk {:name "sentry-defold" :version "1.0"}
   :device {:name (system/os-name)
            :version (system/os-version)}
   :release (or (system/defold-version) "dev")
   :tags {:sha1 (system/defold-sha1)}
   :environment (if (system/defold-version) "release" "dev")
   :extra (ex-data ex)
   ;;:fingerprint []
   :exception (exception-data ex thread)
   :threads (thread-data thread ex)})

(defn x-sentry-auth
  [^LocalDateTime ts key secret]
  (format "Sentry sentry_version=7, sentry_timestamp=%s, sentry_key=%s, sentry_secret=%s"
          (.toEpochSecond (.atZone ts ZoneOffset/UTC)) key secret))

(extend-protocol json/JSONWriter
  java.time.LocalDateTime
  (-write [object out] (json/-write (str object) out)))

(defn report-exception
  [{:keys [project-id key secret] :as options} ^Exception ex ^Thread thread]
  (let [event (make-event ex thread)]
    (http/request {:request-method :post
                   :scheme         "https"
                   :server-name    "sentry.io"
                   :uri            (format "/api/%s/store/" project-id)
                   :content-type   "application/json"
                   :headers        {"X-Sentry-Auth" (x-sentry-auth (:timestamp event) key secret)
                                    "User-Agent"    user-agent
                                    "Accept"        "application/json"}
                   :body           (json/write-str event)})))


;; report exceptions on separate thread
;;
;; - pass data using bounded queue
;; - immediately drop exception if queue is full (will still be logged)
;; - limit rate of reporting

(defn make-exception-reporter
  [{:keys [project-id key secret] :as options}]
  (let [queue (LinkedBlockingQueue. 1000)
        run? (volatile! true)
        reporter-fn (fn []
                      ;; very simple time-based rate-limiting, send at most 1 exception/s.
                      (loop [last-report (System/currentTimeMillis)]
                        (when @run?
                          (if-let [[exception thread] (.poll queue 1 TimeUnit/SECONDS)]
                            (if (< (- (System/currentTimeMillis) last-report) 1000)
                              (recur last-report)
                              (do
                                (try
                                  (report-exception options exception thread)
                                  (catch Exception e
                                    (log/error :exception e :msg (format "Error reporting exception to sentry: %s" (.getMessage e)))))
                                (recur (System/currentTimeMillis))))
                            (recur last-report))))) ]
    (doto (Thread. reporter-fn)
      (.setName "sentry-reporter-thread")
      (.start))
    (fn
      ([]
       (vreset! run? false))
      ([exception thread]
       (.offer queue [exception thread])))))

