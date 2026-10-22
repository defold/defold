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

(ns editor.beta
  (:require [clojure.data.json :as json]
            [clojure.java.io :as io]
            [clojure.string :as string]
            [editor.connection-properties :refer [connection-properties]]
            [editor.dialogs :as dialogs]
            [editor.localization :as localization]
            [editor.markdown-view :as markdown-view]
            [editor.notifications :as notifications]
            [editor.prefs :as prefs]
            [editor.progress :as progress]
            [editor.system :as system]
            [editor.ui :as ui]
            [editor.util :as util]
            [service.log :as log]
            [util.coll :as coll]
            [util.net :as net]
            [util.path :as path])
  (:import [com.dynamo.bob Platform]
           [java.io ByteArrayOutputStream File]
           [java.nio.file Path]
           [javafx.stage Stage]))

(set! *warn-on-reflection* true)

(defonce ^:private automatic-check-started (atom false))

(defn- platform-supported? [platform]
  (contains? #{Platform/X86_64Linux
               Platform/Arm64MacOS
               Platform/X86_64MacOS
               Platform/X86_64Win32}
             platform))

(defn- eligible? [platform]
  (and (= "stable" (system/defold-channel))
       (platform-supported? platform)))

(defn- fetch-json! [url]
  (try
    (let [out (ByteArrayOutputStream.)]
      (net/download! url out :read-timeout 10000 :connect-timeout 5000)
      (json/read-str (.toString out "UTF-8") :key-fn keyword))
    (catch Exception e
      (log/warn :message "Failed to fetch beta resource" :url url :exception e)
      nil)))

(defn fetch-info!
  "Returns beta metadata, or nil if the request fails or the metadata is invalid."
  []
  (let [url (format (get-in connection-properties [:beta :info-url-template])
                    (system/defold-archive-domain))
        {:keys [version sha1] :as info} (fetch-json! url)]
    (when (and (string? version)
               (re-matches #"\d+(\.\d+)*" version)
               (string? sha1)
               (not (string/blank? sha1)))
      info)))

(defn newer-than-running? [^String version]
  (let [running (system/defold-version)]
    (or (nil? running)
        (pos? (.compare util/natural-order version running)))))

(defn check-once!
  "Checks for a beta suggestion at most once per editor session. Call off the UI
  thread. Opted-out users and ineligible installations make no requests."
  [prefs platform]
  (when (and (eligible? platform)
             (not (prefs/get prefs [:versioning :beta-suggestions-disabled]))
             (compare-and-set! automatic-check-started false true))
    (when-let [info (fetch-info!)]
      (when (and (newer-than-running? (:version info))
                 (not= (:sha1 info) (prefs/get prefs [:versioning :downloaded-beta-sha1])))
        info))))

(defn release-notes
  "Returns Markdown for the beta release, or nil when its notes are unavailable."
  [version]
  (let [url (format (get-in connection-properties [:beta :release-notes-url-template])
                    (system/defold-archive-domain) version)]
    (when-let [notes (fetch-json! url)]
      (when (and (map? notes) (vector? (:issues notes)))
        (str "# Defold " version " beta\n\n"
             (when-let [link (:external-link notes)]
               (str "[Full release notes](" link ")\n\n"))
             (coll/join-to-string
               "\n"
               (into []
                     (comp
                       (remove :duplicate)
                       (map (fn issue-markdown [{:keys [type title body url pr_number author]}]
                              (str "<details>\n<summary><strong>" type "</strong>: " title "</summary>\n\n"
                                   "PR [#" pr_number "](" url ") by " author ".\n\n"
                                   body "\n\n</details>\n"))))
                     (:issues notes))))))))

(defn- downloads-dir
  ^File []
  (let [home-dir (io/file (System/getProperty "user.home"))
        downloads-dir (io/file home-dir "Downloads")]
    (if (.isDirectory downloads-dir) downloads-dir home-dir)))

(defn- download-extension [^Platform platform]
  (if (= "macos" (.getOs platform)) "dmg" "zip"))

(defn download-file-name [^Platform platform version]
  (str "Defold-" version "-beta-" (.getPair platform) "." (download-extension platform)))

(defn download!
  "Downloads a beta archive to the chosen file. Reports progress and returns the
  file on success, or nil on failure. Failed downloads preserve existing files."
  [{:keys [sha1]} ^Platform platform ^File file render-progress!]
  (let [url (format (get-in connection-properties [:beta :download-url-template])
                    (system/defold-archive-domain) sha1 (.getPair platform) (download-extension platform))]
    (try
      (render-progress! (progress/make-indeterminate (localization/message "progress.downloading-beta")))
      (path/atomic-replace!
        file
        (fn download-to-temp-path! [^Path temp-path]
          (net/download! url (.toFile temp-path)
                         :read-timeout 10000
                         :connect-timeout 5000
                         :chunk-size (* 1024 1024)
                         :progress-callback
                         (fn report-progress! [current total]
                           (render-progress!
                             (progress/make (localization/message "progress.downloading-beta") total current))))))
      file
      (catch Exception e
        (log/warn :message "Beta download failed" :url url :exception e)
        nil)
      (finally
        (render-progress! progress/done)))))

(defonce ^:private busy (atom false))

(defn enabled? []
  (and (eligible? (Platform/getHostPlatform))
       (not @busy)))

(defn- confirm-download! [stage project localization version markdown]
  (dialogs/make-confirmation-dialog
    localization
    (cond-> {:title (localization/message "beta.dialog.title")
             :icon :icon/circle-question
             :owner stage
             :header (localization/message "beta.dialog.header" {"version" version})
             :buttons [{:text (localization/message "beta.dialog.button.later")
                        :cancel-button true
                        :result :later}
                       {:text (localization/message "beta.button.dont-suggest")
                        :result :dont-suggest}
                       {:text (localization/message "beta.dialog.button.download")
                        :default-button true
                        :result :download}]}
      markdown
      (assoc :size :large
             :width 800
             :content {:fx/type markdown-view/view
                       :content markdown
                       :project project
                       :stylesheets [(str (io/resource "editor.css"))]
                       :root-props {:style-class "md-page-root"}}))))

(defn- choose-download-file [stage localization platform version]
  (dialogs/make-save-file-dialog
    (localization (localization/message "beta.save-dialog.title" {"version" version}))
    [[(localization (localization/message "beta.save-dialog.filter"))
      (str "*." (download-extension platform))]]
    (io/file (downloads-dir) (download-file-name platform version))
    stage))

(defn- show-download-result! [stage localization version ^File file]
  (if-not file
    (dialogs/make-info-dialog
      localization
      {:title (localization/message "beta.dialog.title")
       :icon :icon/triangle-error
       :owner stage
       :header (localization/message "beta.download-failed-dialog.header" {"version" version})})
    (when (dialogs/make-confirmation-dialog
            localization
            {:title (localization/message "beta.dialog.title")
             :icon :icon/circle-check
             :owner stage
             :header (localization/message "beta.download-done-dialog.header" {"version" version})
             :content {:text (localization/message "beta.download-done-dialog.detail" {"path" (.getPath file)})
                       :wrap-text true}
             :buttons [{:text (localization/message "dialog.button.close")
                        :cancel-button true
                        :result false}
                       {:text (localization/message "beta.download-done-dialog.button.show")
                        :default-button true
                        :result true}]})
      (ui/open-file (.getParentFile file)))))

(defn try!
  "Offers a beta download independently of updater checks, downloads and prefs."
  [^Stage stage project prefs localization render-progress!]
  (when (and (enabled?) (compare-and-set! busy false true))
    (future
      (try
        (let [info (fetch-info!)
              version (:version info)]
          (if (and info (newer-than-running? version))
            (let [markdown (release-notes version)
                  choice (ui/run-now
                           (when (.isShowing stage)
                             (confirm-download! stage project localization version markdown)))]
              (case choice
                :download
                (let [platform (Platform/getHostPlatform)]
                  (when-let [target (ui/run-now
                                      (when (.isShowing stage)
                                        (choose-download-file stage localization platform version)))]
                    (let [file (download! info platform target render-progress!)]
                      (when file
                        (prefs/set! prefs [:versioning :downloaded-beta-sha1] (:sha1 info)))
                      (ui/run-now
                        (when (.isShowing stage)
                          (show-download-result! stage localization version file))))))

                :dont-suggest
                (prefs/set! prefs [:versioning :beta-suggestions-disabled] true)

                nil))
            (ui/run-now
              (when (.isShowing stage)
                (dialogs/make-info-dialog
                  localization
                  {:title (localization/message "beta.dialog.title")
                   :icon (if info :icon/circle-info :icon/triangle-error)
                   :owner stage
                   :header (localization/message (if info
                                                   "beta.no-newer-dialog.header"
                                                   "beta.check-failed-dialog.header"))})))))
        (catch Exception e
          (log/warn :message "Failed to offer beta download" :exception e))
        (finally
          (reset! busy false))))))

(defn suggest!
  "Checks once per session and shows a nonmodal beta notification unless opted
  out. Does not depend on the updater being present or idle."
  [^Stage stage notifications-node project prefs localization render-progress!]
  (future
    (when-let [info (check-once! prefs (Platform/getHostPlatform))]
      (ui/run-later
        (when (and (.isShowing stage)
                   (not (prefs/get prefs [:versioning :beta-suggestions-disabled])))
          (notifications/show!
            notifications-node
            {:type :info
             :id ::suggestion
             :message (localization/message "beta.notification.available" {"version" (:version info)})
             :actions [{:message (localization/message "beta.notification.try-it")
                        :on-action (fn try-beta! []
                                     (when-let [download (try! stage project prefs localization render-progress!)]
                                       (notifications/close! notifications-node ::suggestion)
                                       download))}
                       {:message (localization/message "beta.button.dont-suggest")
                        :on-action (fn disable-suggestions! []
                                     (prefs/set! prefs [:versioning :beta-suggestions-disabled] true)
                                     (notifications/close! notifications-node ::suggestion))}]}))))))
