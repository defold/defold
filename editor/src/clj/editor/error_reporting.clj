(ns editor.error-reporting
  (:require
   [editor.sentry :as sentry]
   [editor.system :as system]
   [service.log :as log])
  (:import
   (java.util.concurrent LinkedBlockingQueue)
   (org.apache.commons.lang.exception ExceptionUtils)))

(set! *warn-on-reflection* true)

;;--------------------------------------------------------------------
;; user notification

(defonce ^:private ^:redef exception-notifier
  ;; default noop notifier
  (fn
    ([])
    ([ex-map sentry-id-promise])))

(defn make-exception-notifier
  [notify-fn]
  (let [queue       (LinkedBlockingQueue. 32)
        notifier-fn (fn []
                      (try
                        (while true
                          (when-let [[ex-map sentry-id-promise] (.take queue)]
                            (notify-fn ex-map sentry-id-promise)))
                        (catch InterruptedException _ nil)))
        thread      (doto (Thread. notifier-fn)
                      (.setDaemon true)
                      (.setName "exception-notifier-thread")
                      (.start))]
    (fn
      ([]
       (.interrupt thread))
      ([exception sentry-id-promise]
       (.offer queue [exception sentry-id-promise])
       nil))))

(defn init-exception-notifier!
  [notify-fn]
  (alter-var-root #'exception-notifier
                  (fn [val]
                    (when val (val))
                    (make-exception-notifier notify-fn))))

;;--------------------------------------------------------------------
;; sentry

(defonce ^:private ^:redef sentry-reporter
  ;; default noop reporter
  (fn
    ([])
    ([exception thread] (promise))))

(defn init-sentry-reporter!
  [options]
  (alter-var-root #'sentry-reporter
                  (fn [val]
                    (when val (val))
                    (sentry/make-exception-reporter options))))

;;--------------------------------------------------------------------

(defonce ^:private exception-stats (atom {}))

(def ^:const suppress-exception-threshold-ms 1000)

(defn- update-stats
  [{:keys [last-seen] :as stats}]
  (let [now (System/currentTimeMillis)]
    {:last-seen now
     :suppressed? (and last-seen (< (- now last-seen) suppress-exception-threshold-ms))}))

(defn- ex-identity
  [{:keys [cause trace]}]
  {:cause cause
   :trace (when (and trace (pos? (count trace)))
            (subvec trace 0 1))})

(defn- record-exception!
  [exception]
  (let [ex-map (Throwable->map exception)
        ex-id (ex-identity ex-map)
        ret (swap! exception-stats update ex-id update-stats)]
    (assoc (get ret ex-id) :ex-map ex-map)))

(defn report-exception!
  ([exception]
   (report-exception! exception (Thread/currentThread)))
  ([^Throwable exception thread]
   (try
     (let [{:keys [ex-map suppressed?]} (record-exception! exception)]
       (if suppressed?
         (when (system/defold-dev?)
           (log/debug :msg "Suppressed unhandled" :exception exception))
         (do (log/error :exception exception)
             (let [sentry-id-promise (sentry-reporter exception thread)]
               (exception-notifier ex-map sentry-id-promise)
               nil))))
     (catch Throwable t
       (println (format "Fatal error reporting unhandled exception: '%s'\n%s" (.getMessage t) (pr-str (Throwable->map t))))))))

(defn setup-error-reporting!
  [{:keys [sentry notifier]}]
  (when sentry (init-sentry-reporter! sentry))
  (when notifier (init-exception-notifier! (:notify-fn notifier)))
  (Thread/setDefaultUncaughtExceptionHandler
    (reify Thread$UncaughtExceptionHandler
      (uncaughtException [_ thread exception]
        (report-exception! exception thread)))))

(defmacro catch-all!
  "Executes body, catching and reporting any unhandled exceptions."
  [& body]
  `(try
     ~@body
     (catch Throwable throwable#
       (report-exception! throwable#))))
