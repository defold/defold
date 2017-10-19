(ns editor.updater
  (:require [editor.system :as system]
            [clojure.string :as string]
            [service.log :as log])
  (:import [com.defold.editor Updater Updater$PendingUpdate]
           [java.io File IOException]
           [java.util.concurrent.atomic AtomicReference]
           [java.util Timer TimerTask]
           [javafx.scene Scene Parent]
           [javafx.stage Stage]))

(set! *warn-on-reflection* true)

(def ^:private initial-update-delay 1000)
(def ^:private update-delay 60000)

(defn make-update-context [update-url defold-sha1]
  {:updater (Updater. update-url)
   :last-update (AtomicReference.)
   :last-update-sha1 (AtomicReference. defold-sha1)})

(defn pending-update [update-context]
  (when-let [update ^Updater$PendingUpdate (.get ^AtomicReference (:last-update update-context))]
    (.sha1 update)))

(defn install-pending-update! [{:keys [^AtomicReference last-update] :as update-context} ^File resources-path]
  (when-let [update ^Updater$PendingUpdate (.getAndSet last-update nil)]
    (try
      (log/info :message "update installation requested")
      (.install update resources-path)
      (.deleteFiles update) ; maybe excessive since we're about to restart! anyhow
      (log/info :message "update installed")
      :installed
      (catch IOException e
        (log/debug :message "update installation failed" :exception e)
        nil))))

(defn restart! []
  (log/info :message "update restarting")
  (System/exit 17))

(defn check! [{:keys [^Updater updater ^AtomicReference last-update ^AtomicReference last-update-sha1] :as update-context}]
  (log/info :message "checking for updates")
  (if-let [new-update (.check updater (.get last-update-sha1))]
    (let [new-sha1 (.sha1 new-update)]
      (log/info :message (format "new version found: %s" new-sha1))
      (when-let [old-update ^Updater$PendingUpdate (.getAndSet last-update new-update)]
        (.deleteFiles old-update))
      (.set last-update-sha1 new-sha1)
      new-sha1)
    (do
      (log/info :message "no update found")
      nil)))

(defn- make-check-for-update-task ^TimerTask [^Timer update-timer update-delay update-context]
  (proxy [TimerTask] []
    (run []
      (try
        (check! update-context)
        (catch IOException e
          (log/warn :message "update check failed" :exception e))
        (finally
          (.schedule update-timer (make-check-for-update-task update-timer update-delay update-context) (long update-delay)))))))

(defn ^Timer start-update-timer! [update-context initial-update-delay update-delay]
  (let [update-timer (Timer.)]
    (.schedule update-timer (make-check-for-update-task update-timer update-delay update-context)
               (long initial-update-delay))
    update-timer))

(defn stop-update-timer! [^Timer timer]
  (doto timer
    (.cancel)
    (.purge)))

(defn start!
  ([] (start! (system/defold-update-url) (system/defold-editor-sha1) initial-update-delay update-delay))
  ([update-url sha1 initial-update-delay update-delay]
   (if (and (not (string/blank? update-url)) (not (string/blank? sha1)))
     (let [update-context (make-update-context update-url sha1)
           timer (start-update-timer! update-context initial-update-delay update-delay)]
       (log/info :message "automatic updates enabled")
       {:timer timer :update-context update-context})
     (do
       (log/info :message (format "automatic updates disabled (url='%s', sha1='%s')" update-url sha1))
       nil))))
