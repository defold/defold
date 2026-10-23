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
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [editor.connection-properties :refer [connection-properties]]
            [editor.dialogs :as dialogs]
            [editor.localization :as localization]
            [editor.markdown-view :as markdown-view]
            [editor.notifications :as notifications]
            [editor.prefs :as prefs]
            [editor.system :as system]
            [editor.ui :as ui]
            [editor.updater :as updater]
            [service.log :as log])
  (:import [com.dynamo.bob Platform]
           [javafx.stage Stage]))

(set! *warn-on-reflection* true)

(defonce ^:private automatic-check-started (atom false))

(defn- eligible? [platform]
  (and (= "stable" (system/defold-channel))
       (updater/platform-supported? platform)))

(defn- fetch-info! []
  (let [url (format (get-in connection-properties [:beta :info-url-template])
                    (system/defold-archive-domain))
        {:keys [version sha1] :as info} (updater/fetch-json! url)]
    (when (and (updater/version-string? version)
               (string? sha1)
               (not (string/blank? sha1)))
      info)))

(defn- newer-than-running? [^String version]
  (let [running (system/defold-version)]
    (if-not running
      true
      (updater/newer-version? version running))))

(defn- check-once!
  "Checks for a beta suggestion at most once per editor session. Call off the UI
  thread. Opted-out users and ineligible installations make no requests."
  [prefs platform]
  (when (and (eligible? platform)
             (not (prefs/get prefs [:versioning :beta-suggestions-disabled]))
             (compare-and-set! automatic-check-started false true))
    (when-let [info (fetch-info!)]
      (when (and (newer-than-running? (:version info))
                 (not= (:sha1 info) (prefs/get prefs [:versioning :opened-beta-sha1])))
        info))))

(defn- release-notes [version]
  (when-let [notes (updater/fetch-version-notes! (system/defold-archive-domain) "beta" version)]
    (when (and (map? notes) (vector? (:issues notes)))
      (try
        (updater/release-notes-markdown (assoc notes :version (str version " beta")))
        (catch Exception e
          (log/warn :message "Failed to render beta release notes" :version version :exception e)
          nil)))))

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

(defn try!
  "Offers to open the beta download link in the web browser. Returns the future
  doing so, or nil if disabled or already busy."
  [^Stage stage project prefs localization]
  (when (and (enabled?) (compare-and-set! busy false true))
    (future
      (let [info (fetch-info!)
            version (:version info)
            newer (and info (newer-than-running? version))
            markdown (when newer (release-notes version))]
        (ui/run-later
          (try
            (when (.isShowing stage)
              (if newer
                (case (confirm-download! stage project localization version markdown)
                  :download
                  (let [platform (Platform/getHostPlatform)
                        extension (if (= "macos" (.getOs platform)) "dmg" "zip")]
                    (ui/open-url (updater/download-url (system/defold-archive-domain) (:sha1 info) "beta" platform extension))
                    (prefs/set! prefs [:versioning :opened-beta-sha1] (:sha1 info)))

                  :dont-suggest
                  (prefs/set! prefs [:versioning :beta-suggestions-disabled] true)

                  nil)
                (dialogs/make-info-dialog
                  localization
                  {:title (localization/message "beta.dialog.title")
                   :icon (if info :icon/circle-info :icon/triangle-error)
                   :owner stage
                   :header (localization/message (if info
                                                   "beta.no-newer-dialog.header"
                                                   "beta.check-failed-dialog.header"))})))
            (finally
              (reset! busy false))))))))

(defn suggest! [^Stage stage notifications-node project prefs localization]
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
                                     (when-let [offer (try! stage project prefs localization)]
                                       (notifications/close! notifications-node ::suggestion)
                                       offer))}
                       {:message (localization/message "beta.button.dont-suggest")
                        :on-action (fn disable-suggestions! []
                                     (prefs/set! prefs [:versioning :beta-suggestions-disabled] true)
                                     (notifications/close! notifications-node ::suggestion))}]}))))))
