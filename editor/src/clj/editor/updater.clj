(ns editor.updater
  (:require [clojure.data.json :as json]
            [clojure.java.io :as io]
            [clojure.string :as string]
            [editor.process :as process]
            [editor.progress :as progress]
            [editor.system :as system]
            [service.log :as log]
            [util.net :as net])
  (:import [com.defold.editor Platform]
           [java.io File IOException]
           [java.nio.file Files CopyOption StandardCopyOption]
           [java.nio.file.attribute FileAttribute]
           [java.util Timer TimerTask]
           [org.apache.commons.compress.archivers.zip ZipArchiveEntry ZipFile]
           [org.apache.commons.io FilenameUtils FileUtils]))

(set! *warn-on-reflection* true)

(defn- download-url [sha1 ^Platform platform]
  (format "https://d.defold.com/editor2/%s/editor2/Defold-%s.zip" sha1 (.getPair platform)))

(defn- update-url [channel]
  (format "https://d.defold.com/editor2/channels/%s/update-v2.json" channel))

(defn- make-updater [channel editor-sha1 downloaded-sha1 platform install-dir launcher-path]
  {:channel channel
   :platform platform
   :install-dir install-dir
   :launcher-path launcher-path
   :editor-sha1 editor-sha1
   :state-atom (atom {:downloaded-sha1 downloaded-sha1
                      :server-sha1 editor-sha1})})

(defn add-progress-watch
  "Adds a watch that gets notified on download and extraction progress of
  updater + immediately if download is currently in progress.
  `f` will receive progress as an argument. Unsubscribe by passing same
  `f` to `remove-progress-watch`"
  [updater f]
  (let [{:keys [state-atom]} updater
        {:keys [current-download]} @state-atom]
    (add-watch state-atom
               [:progress f]
               (fn [_ _ old-state new-state]
                 (let [old-progress (-> old-state :current-download :progress)
                       new-progress (-> new-state :current-download :progress)]
                   (when-not (= old-progress new-progress)
                     (f (or new-progress progress/done))))))
    (when (some? current-download)
      (f (:progress current-download)))))

(defn remove-progress-watch [updater f]
  (remove-watch (:state-atom updater) [:progress f]))

(defn add-state-watch
  "Adds watch that gets notified immediately + whenever result of
  `can-download-update?` or `can-install-update?` may change.
  `f` will receive updater as an argument. Unsubscribe by passing same `f` to
  `remove-state-watch`"
  [updater f]
  (add-watch (:state-atom updater)
             [:state f]
             (fn [_ _ old-state new-state]
               (let [old-state-sha1s (update old-state :current-download :sha1)
                     new-state-sha1s (update new-state :current-download :sha1)]
                 (when-not (= old-state-sha1s new-state-sha1s)
                   (f updater)))))
  (f updater))

(defn remove-state-watch [updater f]
  (remove-watch (:state-atom updater) [:state f]))

(defn can-download-update? [updater]
  (let [{:keys [state-atom editor-sha1]} updater
        {:keys [downloaded-sha1 server-sha1 current-download installed-sha1]} @state-atom]
    (cond
      (some? installed-sha1) (not= installed-sha1 server-sha1)
      (some? current-download) (not= (:sha1 current-download) server-sha1)
      (some? downloaded-sha1) (not= downloaded-sha1 server-sha1)
      :else (not= editor-sha1 server-sha1))))

(defn can-install-update? [updater]
  (some? (:downloaded-sha1 @(:state-atom updater))))

(defn- ^File create-temp-zip-file []
  (.toFile (Files/createTempFile "defold-update" ".zip" (into-array FileAttribute []))))

(defn- download! [url ^File zip-file track-download-progress! cancelled-atom]
  (log/info :message "Downloading update" :url url :file (.getAbsolutePath zip-file))
  (net/download! url zip-file
                 :progress-callback (fn [current total]
                                      (track-download-progress!
                                        (progress/make "Downloading update" total current)))
                 :chunk-size (* 1024 1024)
                 :cancelled-atom cancelled-atom))

(def ^:private execute-permission-flag
  "9 bits in 3 triples: [rwx][rwx][rwx]
  r is read, w is write, x is execute
  1st triple is owner, 2nd is group, 3rd is others"
  2r001000000)

(defn- executable? [unix-mode]
  (not (zero? (bit-and unix-mode execute-permission-flag))))

(defn- extract! [updater ^File zip-file server-sha1 track-extract-progress! cancelled-atom]
  (let [{:keys [state-atom install-dir]} updater
        {:keys [downloaded-sha1]} @state-atom
        update-dir (io/file install-dir "update")
        update-sha1-file (io/file install-dir "update.sha1")]
    (when (some? downloaded-sha1)
      (log/info :message "Removing previously downloaded update")
      (swap! state-atom dissoc :downloaded-sha1)
      (FileUtils/deleteQuietly update-dir)
      (FileUtils/deleteQuietly update-sha1-file))
    (log/info :message "Extracting update" :dir update-dir)
    (with-open [zip (ZipFile. zip-file)]
      (let [entries (enumeration-seq (.getEntries zip))
            entry-count (count entries)]
        (doseq [[i ^ZipArchiveEntry e] (map-indexed vector entries)
                :when (and (not (.isDirectory e))
                           (not @cancelled-atom))
                :let [file-name-parts (-> e
                                          .getName
                                          (FilenameUtils/separatorsToUnix)
                                          (string/split #"/")
                                          next)
                      target-dir-parts (butlast file-name-parts)
                      target-file ^File (apply io/file update-dir file-name-parts)]]
          (.mkdirs ^File (apply io/file update-dir target-dir-parts))
          (with-open [in (.getInputStream zip e)]
            (io/copy in target-file))
          (when (executable? (.getUnixMode e))
            (.setExecutable target-file true))
          (track-extract-progress! (progress/make "Extracting update" entry-count (inc i)))))
      (if @cancelled-atom
        (do (FileUtils/deleteQuietly update-dir)
            false)
        (do
          (spit update-sha1-file server-sha1)
          (swap! state-atom (fn [state]
                              (-> state
                                  (assoc :downloaded-sha1 server-sha1)
                                  (dissoc :current-download))))
          (log/info :message "Update extracted")
          true)))))

(defn download-and-extract!
  "Asynchronously downloads newest zip distribution to temporary directory,
  extracts it to `{install-dir}/update` and creates `{install-dir}/update.sha1`
  file containing downloaded update's sha1

  Returns future that eventually will contain boolean indicating the success of
  operation"
  [updater]
  {:pre [(can-download-update? updater)]}
  (let [{:keys [state-atom platform]} updater
        {:keys [current-download server-sha1]} @state-atom
        url (download-url server-sha1 platform)
        zip-file (create-temp-zip-file)
        cancelled-atom (atom false)
        track-progress! (fn [progress]
                          (swap! state-atom assoc-in [:current-download :progress] progress))
        track-download-progress! (progress/nest-render-progress track-progress! (progress/make "" 2 0))
        track-extract-progress! (progress/nest-render-progress track-progress! (progress/make "" 2 1))]
    (when (some? current-download)
      (reset! (:cancelled-atom current-download) true))
    (swap! state-atom assoc :current-download {:sha1 server-sha1
                                               :progress (progress/make "" 2 0)
                                               :cancelled-atom cancelled-atom})
    (future
      (try
        (download! url zip-file track-download-progress! cancelled-atom)
        (if @cancelled-atom
          (do (.delete zip-file)
              false)
          (extract! updater zip-file server-sha1 track-extract-progress! cancelled-atom))
        (catch Exception e
          (log/info :message "Update download failed" :exception e)
          (swap! state-atom dissoc :current-download)
          false)))))

(defn- move-file! [^File source-file ^File target-file]
  (Files/move (.toPath source-file)
              (.toPath target-file)
              (into-array CopyOption [StandardCopyOption/REPLACE_EXISTING])))

(defn- install-unix-file! [^File source-file ^File target-file]
  (when (.exists target-file)
    (.delete target-file))
  (move-file! source-file target-file))

(defn- install-windows-file! [^File source-file ^File target-file editor-sha1]
  (if (and (.exists target-file)
           (not (.delete target-file)))
    ;; delete may fail if we are trying to replace running file
    ;; renaming it works as a workaround
    (do
      (.renameTo target-file
                 (io/file (format "%s-%s.backup" target-file editor-sha1)))
      (io/copy source-file target-file))
    (move-file! source-file target-file)))

(defn- install-file! [updater source-file target-file]
  (let [{:keys [^Platform platform editor-sha1]} updater]
    (case (.getOs platform)
      ("linux" "darwin") (install-unix-file! source-file target-file)
      "win32" (install-windows-file! source-file target-file editor-sha1))))

(defn install!
  "Installs previously downloaded update"
  [updater]
  {:pre [(can-install-update? updater)]}
  (let [{:keys [install-dir state-atom editor-sha1 ^Platform platform]} updater
        {:keys [current-download downloaded-sha1]} @state-atom
        update-dir (io/file install-dir "update")
        update-sha1-file (io/file install-dir "update.sha1")]
    (when (some? current-download)
      (reset! (:cancelled-atom current-download) true)
      (swap! state-atom dissoc :current-download))
    (log/info :message "Installing update")
    (doseq [^File source-file (FileUtils/listFiles update-dir nil true)
            :let [relative-path (.relativize (.toPath update-dir) (.toPath source-file))
                  target-file (io/file install-dir (.toFile relative-path))]]
      (io/make-parents target-file)
      (install-file! updater source-file target-file))
    (FileUtils/deleteQuietly update-sha1-file)
    (FileUtils/deleteQuietly update-dir)
    (swap! state-atom (fn [state]
                        (-> state
                            (dissoc :downloaded-sha1)
                            (assoc :installed-sha1 downloaded-sha1))))
    (log/info :message "Update installed")))

(defn restart! [updater]
  (let [{:keys [launcher-path install-dir]} updater]
    (log/info :message "Restarting editor")
    (process/start! launcher-path *command-line-args* {:directory install-dir})
    (javafx.application.Platform/exit)))

(defn delete-backup-files!
  "Delete files left from previous update, has effect only on windows since only
  windows creates backup files"
  [updater]
  (when (= "win32" (.getOs ^Platform (:platform updater)))
    (let [{:keys [^File install-dir]} updater
          backup-files (FileUtils/listFiles
                         install-dir
                         ^"[Ljava.lang.String;" (into-array ["backup"])
                         true)]
      (doseq [^File file backup-files]
        (.delete file)))))

(defn- check! [updater]
  (let [{:keys [channel state-atom]} updater
        url (update-url channel)]
    (try
      (log/info :message "Checking for updates" :url url)
      (with-open [reader (io/reader url)]
        (let [update (json/read reader :key-fn keyword)
              update-sha1 (:sha1 update)]
          (swap! state-atom assoc :server-sha1 update-sha1)
          (if (can-download-update? updater)
            (log/info :message "New version found" :sha1 update-sha1)
            (log/info :message "No update found"))))
      (catch IOException e
        (log/warn :message "Update check failed" :exception e)))))

(defn- make-check-for-update-task ^TimerTask [^Timer timer updater update-delay]
  (proxy [TimerTask] []
    (run []
      (check! updater)
      (.schedule timer
                 (make-check-for-update-task timer updater update-delay)
                 (long update-delay)))))

(defn- start-timer! [updater initial-update-delay update-delay]
  (let [timer (Timer.)]
    (doto timer
      (.schedule (make-check-for-update-task timer updater update-delay)
                 (long initial-update-delay)))))

(defn start!
  "Starts a timer that polls for updates periodically, returns updater which can be passed
  to other public functions in this namespace"
  []
  (let [channel (system/defold-channel)
        sha1 (system/defold-editor-sha1)
        platform (Platform/getHostPlatform)
        os (.getOs platform)
        install-dir (.getAbsoluteFile
                      (if-let [path (system/defold-resourcespath)]
                        (case os
                          "darwin" (io/file path "../../")
                          ("linux" "win32") (io/file path))
                        (io/file "")))
        launcher-path (or (system/defold-launcherpath)
                          (case os
                            "win32" "./Defold.exe"
                            "linux" "./Defold"
                            "darwin" "./Contents/MacOS/Defold"))
        update-sha1-file (io/file install-dir "update.sha1")
        downloaded-sha1 (when (.exists update-sha1-file)
                          (slurp update-sha1-file))
        initial-update-delay 1000
        update-delay 60000]
    (if (or (string/blank? channel) (string/blank? sha1))
      (do
        (log/info :message "Automatic updates disabled" :channel channel :sha1 sha1)
        nil)
      (doto (make-updater channel sha1 downloaded-sha1 platform install-dir launcher-path)
        (start-timer! initial-update-delay update-delay)))))
