(ns editor.updater
  (:require [clojure.data.json :as json]
            [clojure.java.io :as io]
            [clojure.string :as string]
            [editor.system :as system]
            [service.log :as log]
            [util.net :as net])
  (:import [com.defold.editor Platform]
           [java.io IOException File]
           [java.nio.file Files]
           [java.nio.file.attribute FileAttribute]
           [java.util Timer TimerTask]
           [org.apache.commons.io FilenameUtils]
           [org.apache.commons.compress.archivers.zip ZipArchiveEntry ZipFile]))

(set! *warn-on-reflection* true)

(defn- make-updater [channel editor-sha1 platform install-dir]
  {:channel channel
   :platform platform
   :install-dir install-dir
   :state-atom (atom {:installed-sha1 editor-sha1
                      :newest-sha1 editor-sha1})})

(defn- download-url [sha1 platform]
  (format "https://d.defold.com/editor2/%s/editor2/Defold-%s.zip" sha1 platform)
  ;; TODO remove temporary code
  "http://localhost:8000/Defold-x86_64-linux.zip")

(defn- update-url [channel]
  (format "https://d.defold.com/editor2/channels/%s/update-v2.json" channel)
  ;; TODO remove temporary code
  "http://localhost:8000/update-v2.json")

(defn has-update? [updater]
  (let [state @(:state-atom updater)]
    (not= (:installed-sha1 state)
          (:newest-sha1 state))))

(def execute-permission-flag
  "9 bits in 3 triples: [rwx][rwx][rwx]
  r is read, w is write, x is execute
  1st triple is owner, 2nd is group, 3rd is others"
  2r001000000)

(defn executable? [unix-mode]
  (not (zero? (bit-and unix-mode execute-permission-flag))))

(defn download-and-install! [updater & {:keys [progress-callback cancelled-atom]}]
  (let [{:keys [platform state-atom install-dir]} updater
        {:keys [newest-sha1 installed-sha1]} @state-atom
        url (download-url newest-sha1 platform)
        zip-file (.toFile (Files/createTempFile "defold-update" ".zip" (into-array FileAttribute [])))
        backup-dir (io/file install-dir (str installed-sha1 "-backup"))]
    (.deleteOnExit zip-file)
    (log/info :message "Downloading update" :url url :file (.getAbsolutePath zip-file))
    (net/download! url zip-file
                   :progress-callback progress-callback
                   :chunk-size (* 1024 1024)
                   :cancelled-atom cancelled-atom)
    (if (some-> cancelled-atom deref)
      (do (.delete zip-file)
          false)
      (with-open [zip (ZipFile. zip-file)]
        (log/info :message "Installing update")
        (let [es (.getEntries zip)]
          (while (.hasMoreElements es)
            (let [e ^ZipArchiveEntry (.nextElement es)]
              (when-not (.isDirectory e)
                (let [file-name-parts (-> e
                                          .getName
                                          (FilenameUtils/separatorsToUnix)
                                          (string/split #"/")
                                          next)
                      target-file ^File (apply io/file install-dir file-name-parts)]
                  ;; TODO remove logging spam
                  (log/info :message "Extracting file"
                            :from (.getName e)
                            :to (str target-file))
                  (when-let [target-dir-parts (butlast file-name-parts)]
                    (.mkdirs ^File (apply io/file install-dir target-dir-parts)))
                  (when (.exists target-file)
                    (try
                      (.delete target-file)
                      (catch IOException e
                        (.printStackTrace e)
                        (.mkdirs backup-dir)
                        (.renameTo target-file
                                   (io/file backup-dir target-file)))))
                  (with-open [out (io/output-stream target-file)]
                    ;; TODO replace launcher on windows
                    (io/copy (.getInputStream zip e) out))
                  (when (executable? (.getUnixMode e))
                    (.setExecutable target-file true)))))))
        (swap! state-atom assoc :installed-sha1 newest-sha1)
        (log/info :message "Update installed")
        true))))

(defn restart! []
  (log/info :message "Restarting editor")
  ;; magic exit code that restarts editor
  (System/exit 17))

(defn- check! [updater]
  (let [{:keys [channel state-atom]} updater
        url (update-url channel)]
    (try
      (log/info :message "Checking for updates" :url url)
      (let [update (json/read (io/reader url) :key-fn keyword)
            update-sha1 (:sha1 update)]
        (swap! state-atom assoc :newest-sha1 update-sha1)
        (if (has-update? updater)
          (log/info :message "New version found" :sha1 update-sha1)
          (log/info :message "No update found")))
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
        resources-path (system/defold-resourcespath)
        install-dir (if system/mac?
                      (io/file resources-path "../../")
                      (io/file resources-path))
        initial-update-delay 1000
        update-delay 60000]
    (if (or (string/blank? channel) (string/blank? sha1))
      (do
        (log/info :message "Automatic updates disabled" :channel channel :sha1 sha1)
        nil)
      (doto (make-updater channel sha1 (.getPair (Platform/getHostPlatform)) install-dir)
        (start-timer! initial-update-delay update-delay)))))