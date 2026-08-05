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
            [editor.prefs :as prefs]
            [editor.process :as process]
            [editor.progress :as progress]
            [editor.system :as system]
            [editor.util :as util]
            [service.log :as log]
            [util.coll :as coll]
            [util.eduction :as e]
            [util.net :as net])
  (:import [com.defold.editor Editor]
           [com.dynamo.bob Platform]
           [java.io ByteArrayOutputStream File]
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

(defn- release-notes-manifest-url [archive-domain channel]
  (format (get-in connection-properties [:updater :release-notes-manifest-url-template]) archive-domain channel))

(defn- release-notes-version-url [archive-domain channel version]
  (format (get-in connection-properties [:updater :release-notes-version-url-template]) archive-domain channel version))

(defn release-notes-markdown
  ^String [release-notes]
  (let [issue-types ["BREAKING CHANGE" "NEW" "FIX"]
        issue->closed-issues (fn [{:keys [closed_issues repository]}]
                               (coll/join-to-string
                                 ", "
                                 (e/map (fn [issue-number]
                                          (let [issue-text (if (= "defold" repository)
                                                             (str "#" issue-number)
                                                             (str repository "#" issue-number))
                                                issue-url (format "https://github.com/defold/%s/issues/%s"
                                                                  repository
                                                                  issue-number)]
                                            (format "<a href=\"%s\">%s</a>" issue-url issue-text)))
                                        closed_issues)))
        issue->summary (fn [{:keys [title type]}]
                         ;; the summary wraps in the disclosure header; links and
                         ;; author go in the body
                         (format "<strong>%s</strong>: %s" type title))
        issue->body (fn [{:keys [author body pr_number url] :as issue}]
                      (let [closed (issue->closed-issues issue)
                            meta (str (when (pos? (count closed)) (str "Closes " closed ". "))
                                      (format "PR <a href=\"%s\">#%s</a> by %s." url pr_number author))]
                        (str meta
                             (when-not (string/blank? body) (str "\n\n" body)))))
        issue->markdown (fn [issue]
                          (str "<details>\n<summary>" (issue->summary issue) "</summary>\n\n"
                               (issue->body issue) "\n\n"
                               "</details>\n\n"))
        section-markdown (fn [issues]
                           (reduce (fn [markdown issue-type]
                                     (reduce (fn [markdown issue]
                                               (if (and (= issue-type (:type issue))
                                                        (not (:duplicate issue)))
                                                 (str markdown (issue->markdown issue))
                                                 markdown))
                                             markdown
                                             issues))
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
    (reduce (fn [markdown [heading issues]]
              (let [section (section-markdown issues)]
                (if (pos? (count section))
                  (str markdown "\n## " heading "\n" section)
                  markdown)))
            (str "# Defold " version "\n"
                 "\nFor full release notes, please visit our forum at " (:external-link release-notes) "\n")
            [["Engine" engine] ["Editor" editor] ["Other" other]])))

(def ^:private release-notes-range-limit
  "Most releases to show in the update dialog when someone is several versions
  behind. Each one links to its full notes online, so nothing older is lost."
  5)

(defn- version-string? [s]
  (boolean (and (string? s) (re-matches #"\d+(\.\d+)*" s))))

(defn- newer-version? [^String a ^String b]
  (pos? (.compare util/natural-order a b)))

(def ^:private descending-version-order (.reversed util/natural-order))

(defn- versions-to-fetch
  "Picks which versions to show from the manifest: the running editor's own
  version and anything newer, newest first, capped at `release-notes-range-limit`.
  Bad version strings are skipped. If we don't know the current version (dev
  builds), everything counts, so you get the most recent few."
  [manifest-versions current-version]
  (let [current (when (version-string? current-version) current-version)]
    (into []
          (take release-notes-range-limit)
          (sort descending-version-order
                (e/filter #(and (version-string? %)
                                (or (nil? current) (not (newer-version? current %))))
                          manifest-versions)))))

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
  (fetch-json! (release-notes-manifest-url archive-domain channel)))

(defn- fetch-version-notes! [archive-domain channel version]
  (fetch-json! (release-notes-version-url archive-domain channel version)))

(defn fetch-release-notes!
  "Downloads the notes to show for the available update. Reads the channel
  manifest, works out which versions to show (newest first, capped at
  `release-notes-range-limit`), and fetches each one's notes in parallel.
  Returns a newest-first vector of {:version <string> :notes <map-or-nil>}, one
  per version, where :notes is nil if that file didn't download. Returns nil if
  even the manifest can't be fetched."
  [archive-domain channel]
  (when-some [manifest (fetch-manifest! archive-domain channel)]
    (let [versions (versions-to-fetch (filter string? manifest) (system/defold-version))]
      (coll/pmapv (fn [version]
                    {:version version
                     :notes (fetch-version-notes! archive-domain channel version)})
                  versions))))

(def ^:private ^File support-dir
  (.getCanonicalFile (.toFile (Editor/getSupportPath))))

(def ^:private ^File updates-dir
  (io/file support-dir "updates"))

(defn- channel-update-paths [channel]
  (let [channel-dir (io/file updates-dir channel)]
    {:update-dir (io/file channel-dir "bundle")
     :update-sha1-file (io/file channel-dir "sha1")}))

(defn- make-updater [channel editor-sha1 downloaded-sha1 prefs platform install-dir launcher-path protected-dirs]
  (let [{:keys [update-dir update-sha1-file]} (channel-update-paths channel)]
    {:channel channel
     :platform platform
     :install-dir install-dir
     :launcher-path launcher-path
     :editor-sha1 editor-sha1
     :prefs prefs
     :protected-dirs protected-dirs
     :update-dir update-dir
     :update-sha1-file update-sha1-file
     :state-atom (atom {:downloaded-sha1 downloaded-sha1
                        :skipped-sha1 (get (prefs/get prefs [:versioning :skipped-update-sha1s]) channel)
                        :server-sha1 editor-sha1})}))

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
  `can-download-update?`, `update-advertised?` or `can-install-update?` may
  change.
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

(defn current-update-sha1 [updater]
  (:server-sha1 @(:state-atom updater)))

(defn manual-update-check-in-progress? [updater]
  (true? (:manual-update-check-in-progress @(:state-atom updater))))

(defn begin-manual-update-check! [updater]
  (let [[old-state] (swap-vals! (:state-atom updater) assoc :manual-update-check-in-progress true)]
    (not (:manual-update-check-in-progress old-state))))

(defn end-manual-update-check! [updater]
  (swap! (:state-atom updater) dissoc :manual-update-check-in-progress))

(defn can-download-update? [updater]
  (let [{:keys [state-atom editor-sha1]} updater
        {:keys [downloaded-sha1 server-sha1 current-download installed-sha1]} @state-atom]
    (not= server-sha1
          (or installed-sha1
              (:sha1 current-download)
              downloaded-sha1
              editor-sha1))))

(defn update-advertised? [updater]
  (let [{:keys [server-sha1 skipped-sha1]} @(:state-atom updater)]
    (and (can-download-update? updater)
         (not= skipped-sha1 server-sha1))))

(defn skip-update! [updater sha1]
  (prefs/update! (:prefs updater) [:versioning :skipped-update-sha1s] assoc (:channel updater) sha1)
  (swap! (:state-atom updater) assoc :skipped-sha1 sha1))

(defn- bundled-release-notes [version]
  (when-let [url (io/resource (str "release-notes/" version ".json"))]
    (try
      (with-open [reader (io/reader url)]
        (json/read reader :key-fn keyword))
      (catch Exception _
        nil))))

(defn- entry-markdown [current-version {:keys [version notes]}]
  (cond
    (not notes)
    (str "# Defold " version "\n\n_Failed to download these release notes._")

    (= version current-version)
    (let [bundled (bundled-release-notes version)
          diffed (if-not bundled
                   notes
                   (let [seen (into #{} (map :url) (:issues bundled))]
                     (update notes :issues (fn [issues] (into [] (remove (comp seen :url)) issues)))))]
      (if (coll/empty? (:issues diffed))
        (str "# Defold " version "\n\n_No new release notes since your current build._")
        (release-notes-markdown diffed)))

    :else
    (release-notes-markdown notes)))

(defn release-notes
  "Returns {:markdown <string> :sha1 <the update these notes describe>
  :versions <newest-first version strings>}, or nil if there's nothing to show
  for the current update."
  [updater]
  (let [state @(:state-atom updater)
        entries (:release-notes state)
        server-sha1 (:server-sha1 state)]
    (when (and (= (:release-notes-sha state) server-sha1)
               (not (coll/empty? entries)))
      (let [current-version (system/defold-version)]
        {:markdown (coll/join-to-string "\n\n---\n\n" (e/map #(entry-markdown current-version %) entries))
         :sha1 server-sha1
         :versions (mapv :version entries)}))))

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
  (let [{:keys [state-atom update-dir update-sha1-file]} updater
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
  extracts it to `{support-dir}/updates/{channel}/bundle` and creates
  `{support-dir}/updates/{channel}/sha1` file containing downloaded update's sha1

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
  (let [{:keys [install-dir state-atom ^File update-dir update-sha1-file]} updater
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

(defn check!
  "Checks for updates immediately. Returns true if the check completed (even
  if no update was found), or false if it failed, e.g. a network error or a
  malformed response."
  [updater]
  (let [{:keys [channel state-atom]} updater
        archive-domain (system/defold-archive-domain)
        url (update-url archive-domain channel)]
    (try
      (log/info :message "Checking for updates" :url url)
      (with-open [reader (io/reader url)]
        (let [update (json/read reader :key-fn keyword)
              update-sha1 (:sha1 update)
              state (swap! state-atom assoc :server-sha1 update-sha1)]
          (cond
            (not (can-download-update? updater))
            (log/info :message "No update found")

            ;; Complete notes for this update are already loaded, so don't
            ;; re-download them on every hourly check.
            (and (= update-sha1 (:release-notes-sha state))
                 (:release-notes-complete? state))
            (log/info :message "New version found; release notes already loaded" :sha1 update-sha1)

            :else
            (do
              (log/info :message "New version found" :sha1 update-sha1)
              (when-some [entries (fetch-release-notes! archive-domain channel)]
                ;; Remember which update these notes are for, so a failed fetch
                ;; for a newer one can't keep showing stale notes. "Complete"
                ;; means we got notes for every version; an empty or partial
                ;; result stays incomplete so the next check retries it.
                (swap! state-atom assoc
                       :release-notes entries
                       :release-notes-sha update-sha1
                       :release-notes-complete? (and (not (coll/empty? entries))
                                                     (coll/every? :notes entries)))))))
        true)
      (catch Exception e
        ;; Disabled during tests to minimize log spam.
        (when-not (Boolean/getBoolean "defold.tests")
          (log/warn :message "Update check failed" :exception e))
        false))))

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
  [prefs]
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
        initial-update-delay 1000
        update-delay 3600000]
    (if (or (string/blank? channel) (string/blank? sha1))
      (do
        (log/info :message "Automatic updates disabled" :channel channel :sha1 sha1)
        nil)
      (let [^File update-sha1-file (:update-sha1-file (channel-update-paths channel))
            downloaded-sha1 (when (.exists update-sha1-file)
                              (slurp update-sha1-file))]
        (doto (make-updater channel sha1 downloaded-sha1 prefs platform install-dir launcher-path protected-dirs)
          (start-timer! initial-update-delay update-delay))))))
