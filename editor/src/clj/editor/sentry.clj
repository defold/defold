(ns editor.sentry
  (:require
   [clojure.data.json :as json]
   [clojure.java.io :as io]
   [clojure.string :as string]
   [editor.gl :as gl]
   [editor.system :as system]
   [schema.utils :as su]
   [service.log :as log]
   [util.http-client :as http])
  (:import
   (java.util.concurrent LinkedBlockingQueue TimeUnit)
   (java.time LocalDateTime ZoneOffset)))

(set! *warn-on-reflection* true)

(def user-agent "sentry-defold/1.0")

(defn- cleanup-anonymous-fn-class-name
  [s]
  (-> s
      (string/replace #"fn__\d+" "fn")
      (string/replace #"reify__\d+" "reify")))

(defn- stacktrace-data
  [^Exception ex]
  {:frames (vec (for [^StackTraceElement frame (reverse (.getStackTrace ex))]
                  {:filename (.getFileName frame)
                   :lineno (.getLineNumber frame)
                   :function (.getMethodName frame)
                   :module (some-> (.getClassName frame) cleanup-anonymous-fn-class-name)}))})

(defn module-name
  [frames]
  (when-let [^StackTraceElement top (first (seq frames))]
    (str (.getClassName top) "." (.getMethodName top))))

(defn- exception-data
  [^Exception ex ^Thread thread]
  (loop [ex ex
         data []]
    (if (nil? ex)
      data
      (recur (.getCause ex) (conj data {:type (str (.getClass ex))
                                        :value (.getMessage ex)
                                        :module (module-name (.getStackTrace ex))
                                        :stacktrace (stacktrace-data ex)
                                        :thread_id (str (.getId thread))})))))

(defn- thread-data
  [^Thread thread ^Exception ex]
  [{:id (str (.getId thread))
    :current true
    :crashed (= (.getState thread) Thread$State/TERMINATED)
    :name (.getName thread)}])

(defn to-safe-json-value
  [value]
  (cond
    (set? value)
    (into [] (sort (map to-safe-json-value value)))

    (instance? java.util.Map value)
    (into {} (map (fn [[k v]] [k (to-safe-json-value v)])) value)

    (instance? java.util.Collection value)
    (into [] (map to-safe-json-value) value)

    (class? value)
    (str value)

    (fn? value)
    (str value)

    (instance? schema.utils.ValidationError value)
    (su/validation-error-explain value)

    :else
    value))

(defn make-event
  [^Exception ex ^Thread thread]
  (let [id (string/replace (str (java.util.UUID/randomUUID)) "-" "")
        environment (if (system/defold-version) "release" "dev")
        gl-info (gl/gl-info)
        event {:event_id    id
               :message     (.getMessage ex)
               :timestamp   (LocalDateTime/now ZoneOffset/UTC)
               :level       "error"
               :logger      ""
               :platform    "java"
               :sdk         {:name "sentry-defold" :version "1.0"}
               :device      {:name (system/os-name) :version (system/os-version)}
               :culprit     (module-name (.getStackTrace ex))
               :release     (or (system/defold-version) "dev")
               :tags        {:id id
                             :defold-sha1 (system/defold-sha1)
                             :defold-version (or (system/defold-version) "dev")
                             :os-name (system/os-name)
                             :os-arch (system/os-arch)
                             :os-version (system/os-version)
                             :java-version (system/java-runtime-version)}
               :environment environment
               :extra       (merge {:java-home (system/java-home)}
                              (to-safe-json-value (ex-data ex)))
               :fingerprint ["{{ default }}" environment]
               :exception   (exception-data ex thread)
               :threads     (thread-data thread ex)}]
    (cond-> event
      gl-info (-> (update :tags assoc :gpu-vendor (:vendor gl-info))
                (update :extra assoc
                  :gpu (:renderer gl-info)
                  :gpu-version (:version gl-info)
                  :gpu-info (:desc gl-info))))))

(defn x-sentry-auth
  [^LocalDateTime ts key secret]
  (format "Sentry sentry_version=7, sentry_timestamp=%s, sentry_key=%s, sentry_secret=%s"
          (.toEpochSecond (.atZone ts ZoneOffset/UTC)) key secret))

(extend-protocol json/JSONWriter
  java.time.LocalDateTime
  (-write [object out] (json/-write (str object) out)))

(defn make-request-data
  [{:keys [project-id key secret]} event]
  {:request-method :post
   :scheme         "https"
   :server-name    "sentry.io"
   :uri            (format "/api/%s/store/" project-id)
   :content-type   "application/json"
   :headers        {"X-Sentry-Auth" (x-sentry-auth (:timestamp event) key secret)
                    "User-Agent"    user-agent
                    "Accept"        "application/json"}
   :body           (try
                     (json/write-str event)
                     (catch Exception e
                       ;; The :extra field in the event data returned by make-event can potentially
                       ;; contain anything, since it includes ex-data from the exception.
                       ;; We attempt to convert unrepresentable values into an appropriate json value
                       ;; using the to-safe-json-value function above, but new types might appear.
                       ;; In case conversion fails, we replace the :extra data with safe data that
                       ;; will convert successfully, and include info about the conversion failure
                       ;; so we can add conversions to the to-safe-json-value function as needed.
                       (json/write-str (assoc event :extra {:java-home (system/java-home)
                                                            :conversion-failure (exception-data e (Thread/currentThread))}))))})

(defn report-exception
  [options exception thread]
  (let [event (make-event exception thread)
        request (make-request-data options event)
        response (http/request request)]
    (when (= 200 (:status response))
      (with-open [reader (io/reader (:body response))]
        (-> reader (json/read :key-fn keyword) :id)))))

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
                          (if-let [[exception thread id-promise] (.poll queue 1 TimeUnit/SECONDS)]
                            (if (< (- (System/currentTimeMillis) last-report) 1000)
                              (recur last-report)
                              (do
                                (try
                                  (let [id (report-exception options exception thread)]
                                    (deliver id-promise id))
                                  (catch Exception e
                                    (log/error :exception e :msg (format "Error reporting exception to sentry: %s" (.getMessage e)))))
                                (recur (System/currentTimeMillis))))
                            (recur last-report))))) ]
    (doto (Thread. reporter-fn)
      (.setDaemon true)
      (.setName "sentry-reporter-thread")
      (.start))
    (fn
      ([]
       (vreset! run? false))
      ([exception thread]
       (let [id-promise (promise)]
         (.offer queue [exception thread id-promise])
         id-promise)))))
