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

(ns editor.updater
  (:require [clojure.data.json :as json]
            [clojure.java.io :as io]
            [clojure.string :as string]
            [editor.connection-properties :refer [connection-properties]]
            [editor.localization :as localization]
            [editor.process :as process]
            [editor.progress :as progress]
            [editor.system :as system]
            [service.log :as log]
            [util.coll :as coll]
            [util.net :as net])
  (:import [com.defold.editor Editor]
           [com.dynamo.bob Platform]
           [java.io ByteArrayOutputStream File IOException]
           [java.nio.file Files CopyOption StandardCopyOption]
           [java.nio.file.attribute FileAttribute]
           [java.util Timer TimerTask]
           [org.apache.commons.compress.archivers.zip ZipArchiveEntry ZipFile]
           [org.apache.commons.io FilenameUtils FileUtils]))

(set! *warn-on-reflection* true)

(defn- download-url [archive-domain sha1 channel ^Platform platform]
  (format (get-in connection-properties [:updater :download-url-template]) archive-domain sha1 channel (.getPair platform)))

(defn- update-url [archive-domain channel]
  (format (get-in connection-properties [:updater :update-url-template]) archive-domain channel))

(defonce dev-updater (atom nil))

(defn release-notes->markdown
  ^String [release-notes]
  (let [issue-types ["BREAKING CHANGE" "NEW" "FIX"]
        issue->closed-issues (fn [{:keys [closed_issues repository]}]
                               (string/join ","
                                            (into []
                                                  (map (fn [issue-number]
                                                         (let [issue-text (if (= "defold" repository)
                                                                            (str "#" issue-number)
                                                                            (str repository "#" issue-number))
                                                               issue-url (format "https://github.com/defold/%s/issues/%s"
                                                                                 repository
                                                                                 issue-number)]
                                                           (format "[%s](%s)" issue-text issue-url))))
                                                  closed_issues)))
        issue->markdown (fn [{:keys [author pr_number title type url] :as issue}]
                          (let [closed-issues (issue->closed-issues issue)]
                            (format "* __%s__: (%s) %s (by %s) (PR [#%s](%s))\n"
                                    type
                                    closed-issues
                                    title
                                    author
                                    pr_number
                                    url)))
        summary-markdown (fn [sections]
                           (reduce (fn [markdown issue-type]
                                     (reduce (fn [markdown issues]
                                               (reduce (fn [markdown issue]
                                                         (if (and (= issue-type (:type issue))
                                                                  (not (:duplicate issue)))
                                                           (str markdown (issue->markdown issue))
                                                           markdown))
                                                       markdown
                                                       issues))
                                             markdown
                                             sections))
                                   ""
                                   issue-types))
        {:keys [version issues]} release-notes
        {:keys [engine editor other]}
        (reduce (fn [sections issue]
                  (cond
                    (not= "defold" (:repository issue))
                    (update sections :other conj issue)

                    (contains? (set (:labels issue)) "editor")
                    (update sections :editor conj issue)

                    :else
                    (update sections :engine conj issue)))
                {:engine []
                 :editor []
                 :other []}
                issues)]
    (str "# Defold Release Summary - Version " version "\n"
         "\nFor release notes, please visit our forum at " (:external-link release-notes)
         "\n" (summary-markdown [engine editor other]))))

(def ^:private release-notes-range-limit
  "When the user is several versions behind, show at most this many of the most
  recent releases in the update dialog. Each shown release links to its full
  notes online, so older ones stay reachable."
  5)

(defn- parse-version
  "\"1.13.2\" -> [1 13 2]; nil or anything non-numeric -> nil. Equal-length int
  vectors compare with clojure.core/compare exactly as semver expects."
  [s]
  (when (string? s)
    (let [parts (string/split s #"\.")]
      (when (and (seq parts) (every? #(re-matches #"\d+" %) parts))
        (mapv #(Long/parseLong %) parts)))))

(defn- versions-to-fetch
  "From the channel manifest's version strings, picks the ones to show for the
  running editor version: those strictly newer than `current-version`, newest
  first, capped at `release-notes-range-limit`. Unparseable entries are dropped.
  When `current-version` is unknown (e.g. dev builds) every version is a
  candidate, so this yields the most recent N."
  [manifest-versions current-version]
  (let [current (parse-version current-version)]
    (->> manifest-versions
         (keep (fn [v] (when-some [parsed (parse-version v)] [v parsed])))
         (filter (fn [[_ parsed]] (or (nil? current) (pos? (compare parsed current)))))
         (sort-by second #(compare %2 %1)) ; newest first
         (mapv first)
         (into [] (take release-notes-range-limit)))))

(defn- fetch-json!
  "GETs `url` and parses the body as JSON with keyword keys, or nil on failure."
  [url]
  (let [out (ByteArrayOutputStream.)]
    (try
      (net/download! url out :read-timeout 10000 :connect-timeout 5000)
      (json/read-str (.toString out "UTF-8") :key-fn keyword)
      (catch Exception e
        (log/warn :message "Failed to fetch release notes resource" :url url :exception e)
        nil))))

(defn- fetch-manifest! [archive-domain channel]
  (fetch-json! (format (get-in connection-properties [:updater :release-notes-manifest-url-template])
                       archive-domain channel)))

(defn- fetch-version-notes! [archive-domain channel version]
  (fetch-json! (format (get-in connection-properties [:updater :release-notes-version-url-template])
                       archive-domain channel version)))

(defn fetch-release-notes!
  "Downloads the release notes to show for the available update. Reads the
  channel manifest, selects the versions between the running editor version and
  the update (newest first, capped at `release-notes-range-limit`), and fetches
  those per-version notes in parallel. Returns a vector of slots, newest first,
  one per selected version: {:version <string> :notes <map-or-nil>} (:notes is
  nil when that version's file couldn't be fetched). Returns nil when the
  manifest itself can't be fetched."
  [archive-domain channel]
  (when-some [manifest (fetch-manifest! archive-domain channel)]
    (let [versions (versions-to-fetch (filter string? manifest) (system/defold-version))]
      (coll/pmapv (fn [version]
                    {:version version
                     :notes (fetch-version-notes! archive-domain channel version)})
                  versions))))

(def ^:private ^File support-dir
  (.getCanonicalFile (.toFile (Editor/getSupportPath))))

(def ^:private ^File update-dir
  (io/file support-dir "update"))

(def ^:private ^File update-sha1-file
  (io/file support-dir "update.sha1"))

(defn- make-updater [channel editor-sha1 downloaded-sha1 platform install-dir launcher-path protected-dirs]
  {:channel channel
   :platform platform
   :install-dir install-dir
   :launcher-path launcher-path
   :editor-sha1 editor-sha1
   :protected-dirs protected-dirs
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
    (not= server-sha1
          (or installed-sha1
              (:sha1 current-download)
              downloaded-sha1
              editor-sha1))))

(defn release-notes
  "Returns the in-memory release notes markdown for the available update, or nil
  if none have been fetched yet (or the fetch failed). When the user is several
  versions behind, each version in range is rendered newest first. A version
  whose notes failed to download renders its heading followed by a short
  'failed to download' notice in place, so the user sees the gap in order."
  ^String
  [updater]
  (let [updater (if @dev-updater @dev-updater updater)
        slots (:release-notes @(:state-atom updater))]
    (when-not (coll/empty? slots)
      (string/join "\n\n---\n\n"
                   (map (fn [{:keys [version notes]}]
                          (if notes
                            (release-notes->markdown notes)
                            (str "# Defold Release Summary - Version " version
                                 "\n\n_Failed to download these release notes._")))
                        slots)))))

(defn can-install-update? [updater]
  (some? (:downloaded-sha1 @(:state-atom updater))))

(defn platform-supported? [updater]
  (contains? #{Platform/X86_64Linux
               Platform/Arm64MacOS
               Platform/X86_64MacOS
               Platform/X86_64Win32}
             (:platform updater)))

(defn- create-temp-zip-file ^File []
  (let [empty-attrs (into-array FileAttribute [])
        file (.toFile (Files/createTempFile "defold-update" ".zip" empty-attrs))]
    (.deleteOnExit file)
    file))

(defn- download! [url ^File zip-file track-download-progress! cancelled-atom]
  (log/info :message "Downloading update" :url url :file (.getCanonicalPath zip-file))
  (net/download! url zip-file
                 :progress-callback (fn [current total]
                                      (track-download-progress!
                                        (progress/make (localization/message "progress.downloading-update") total current)))
                 :chunk-size (* 1024 1024)
                 :cancelled-derefable cancelled-atom
                 :read-timeout 10000
                 :connect-timeout 5000))

(def ^:private execute-permission-flag
  "9 bits in 3 triples: [rwx][rwx][rwx]
  r is read, w is write, x is execute
  1st triple is owner, 2nd is group, 3rd is others"
  2r001000000)

(defn- executable? [unix-mode]
  (not (zero? (bit-and unix-mode execute-permission-flag))))

(defn- extract! [updater ^File zip-file server-sha1 track-extract-progress! cancelled-atom]
  (let [{:keys [state-atom]} updater
        {:keys [downloaded-sha1]} @state-atom]
    (when (some? downloaded-sha1)
      (log/info :message "Removing previously downloaded update")
      (swap! state-atom dissoc :downloaded-sha1)
      (FileUtils/deleteQuietly update-dir)
      (FileUtils/deleteQuietly update-sha1-file))
    (log/info :message "Extracting update" :dir (str update-dir))
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
          (track-extract-progress! (progress/make (localization/message "progress.extracting-update") entry-count (inc i)))))
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
  extracts it to `{support-dir}/update` and creates `{support-dir}/update.sha1`
  file containing downloaded update's sha1

  Returns future that eventually will contain boolean indicating the success of
  operation"
  [updater]
  {:pre [(can-download-update? updater)]}
  (let [{:keys [state-atom platform channel]} updater
        {:keys [current-download server-sha1]} @state-atom
        archive-domain (system/defold-archive-domain)
        url (download-url archive-domain server-sha1 channel platform)
        zip-file (create-temp-zip-file)
        cancelled-atom (atom false)
        track-progress! (fn [progress]
                          (swap! state-atom assoc-in [:current-download :progress] progress))
        track-download-progress! (progress/nest-render-progress track-progress! (progress/make localization/empty-message 2 0))
        track-extract-progress! (progress/nest-render-progress track-progress! (progress/make localization/empty-message 2 1))]
    (when (some? current-download)
      (reset! (:cancelled-derefable current-download) true))
    (swap! state-atom assoc :current-download {:sha1 server-sha1
                                               :progress (progress/make localization/empty-message 2 0)
                                               :cancelled-derefable cancelled-atom})
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
                 (io/file (format "%s-%s.defbackup" target-file editor-sha1)))
      (io/copy source-file target-file))
    (move-file! source-file target-file)))

(defn- install-file! [updater source-file target-file]
  (let [{:keys [^Platform platform editor-sha1]} updater]
    (case (.getOs platform)
      ("linux" "macos") (install-unix-file! source-file target-file)
      "win32" (install-windows-file! source-file target-file editor-sha1))))

(defn- in-protected-dir? [updater ^File file]
  (let [path (.toPath file)]
    (->> updater
         :protected-dirs
         (some #(.startsWith path (.toPath ^File %)))
         boolean)))

(defn install!
  "Installs previously downloaded update"
  [updater]
  {:pre [(can-install-update? updater)]}
  (let [{:keys [install-dir state-atom]} updater
        {:keys [current-download downloaded-sha1]} @state-atom]
    (when (some? current-download)
      (reset! (:cancelled-derefable current-download) true)
      (swap! state-atom dissoc :current-download))
    (log/info :message "Installing update")
    (doseq [^File source-file (FileUtils/listFiles update-dir nil true)
            :let [relative-path (.relativize (.toPath update-dir) (.toPath source-file))
                  target-file (io/file install-dir (.toFile relative-path))]
            :when (not (in-protected-dir? updater target-file))]
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
    (apply process/start! {:dir install-dir} launcher-path *command-line-args*)
    (javafx.application.Platform/exit)))

(defn delete-backup-files!
  "Delete files left from previous update, has effect only on windows since only
  windows creates backup files"
  [updater]
  (when (= "win32" (.getOs ^Platform (:platform updater)))
    (let [{:keys [^File install-dir]} updater
          backup-files (FileUtils/listFiles
                         install-dir
                         ^"[Ljava.lang.String;" (into-array ["defbackup"])
                         true)]
      (doseq [^File file backup-files]
        (.delete file)))))

(defn- check! [updater]
  (let [{:keys [channel state-atom]} updater
        archive-domain (system/defold-archive-domain)
        url (update-url archive-domain channel)]
    (try
      (log/info :message "Checking for updates" :url url)
      (with-open [reader (io/reader url)]
        (let [update (json/read reader :key-fn keyword)
              update-sha1 (:sha1 update)]
          (swap! state-atom assoc :server-sha1 update-sha1)
          (cond
            (not (can-download-update? updater))
            (log/info :message "No update found")

            ;; Notes for this update were already fetched earlier this session, so
            ;; don't re-download them on every hourly check. The sha only changes
            ;; when a genuinely newer release appears, which re-arms the fetch.
            (= update-sha1 (:release-notes-sha @state-atom))
            (log/info :message "New version found; release notes already loaded" :sha1 update-sha1)

            :else
            (do
              (log/info :message "New version found" :sha1 update-sha1)
              (when-some [slots (fetch-release-notes! archive-domain channel)]
                ;; Partial results still display (failed versions render a
                ;; placeholder); only a complete fetch arms the sha guard, so
                ;; missing versions get retried on the next check.
                (swap! state-atom assoc :release-notes slots)
                (when (coll/every? :notes slots)
                  (swap! state-atom assoc :release-notes-sha update-sha1)))))))
      (catch IOException e
        ;; Disabled during tests to minimize log spam.
        (when-not (Boolean/getBoolean "defold.tests")
          (log/warn :message "Update check failed" :exception e))))))

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
        resources-dir (-> (system/defold-resourcespath)
                          (or "")
                          io/file
                          .getCanonicalFile)
        protected-dirs [(io/file resources-dir "packages" "jdk-25+36")]
        install-dir (.getCanonicalFile
                      (if-let [path (system/defold-resourcespath)]
                        (case os
                          "macos" (io/file path "../../")
                          ("linux" "win32") (io/file path))
                        (io/file "")))
        launcher-path (or (system/defold-launcherpath)
                          (case os
                            "win32" "./Defold.exe"
                            "linux" "./Defold"
                            "macos" "./Contents/MacOS/Defold"))
        downloaded-sha1 (when (.exists update-sha1-file)
                          (slurp update-sha1-file))
        initial-update-delay 1000
        ;; DEV-ONLY (issue-7186 validation): poll every 10s instead of hourly so a
        ;; full update cycle can be observed. Restore to 3600000 before merge.
        update-delay 10000]
    (if (or (string/blank? channel) (string/blank? sha1))
      (do
        (log/info :message "Automatic updates disabled" :channel channel :sha1 sha1)
        nil)
      (doto (make-updater channel sha1 downloaded-sha1 platform install-dir launcher-path protected-dirs)
        (start-timer! initial-update-delay update-delay)))))
