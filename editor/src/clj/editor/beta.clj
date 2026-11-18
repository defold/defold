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
  (:require [cljfx.fx.v-box :as fx.v-box]
            [clojure.java.io :as io]
            [clojure.string :as string]
            [editor.connection-properties :refer [connection-properties]]
            [editor.dialogs :as dialogs]
            [editor.fxui :as fxui]
            [editor.localization :as localization]
            [editor.markdown-view :as markdown-view]
            [editor.notifications :as notifications]
            [editor.prefs :as prefs]
            [editor.progress :as progress]
            [editor.system :as system]
            [editor.ui :as ui]
            [editor.updater :as updater]
            [service.log :as log])
  (:import [com.dynamo.bob Platform]
           [javafx.scene.layout Region]
           [javafx.stage Stage]))

(set! *warn-on-reflection* true)

(defn make-session []
  (atom {:automatic-check-started false
         :busy false}))

(defonce session (make-session))

(defn- claim! [session key]
  (let [[old] (swap-vals! session
                          (fn [state]
                            (if (key state)
                              state
                              (assoc state key true))))]
    (not (key old))))

(defn release-source []
  {:base-url (str "https://" (system/defold-archive-domain))
   :channel (system/defold-channel)
   :version (system/defold-version)
   :platform (Platform/getHostPlatform)})

(defn download-url [{:keys [base-url ^Platform platform]} sha1]
  (format (get-in connection-properties [:beta :download-url-template])
          base-url
          sha1
          (.getPair platform)
          (if (= "macos" (.getOs platform)) "dmg" "zip")))

(defn eligible? [{:keys [channel platform]}]
  (and (= "stable" channel)
       (updater/platform-supported? platform)))

(defn- fetch-info! [{:keys [base-url]}]
  (let [url (format (get-in connection-properties [:beta :info-url-template]) base-url)
        {:keys [version sha1] :as info} (updater/fetch-json! url)]
    (when (and (updater/version-string? version)
               (string? sha1)
               (not (string/blank? sha1)))
      info)))

(defn- newer-than-running? [{running :version} ^String version]
  (or (not running)
      (updater/newer-version? version running)))

(defn- check-once!
  "Checks for a beta suggestion at most once per editor session. Call off the UI
  thread. Opted-out users, users who have run a beta and ineligible
  installations make no requests."
  [session source prefs]
  (when (and (eligible? source)
             (not (prefs/get prefs [:versioning :beta-suggestions-disabled]))
             (not (prefs/get prefs [:versioning :ran-beta]))
             (claim! session :automatic-check-started))
    (when-let [info (fetch-info! source)]
      (when (newer-than-running? source (:version info))
        info))))

(defn- fetch-release-notes! [{:keys [base-url]} version]
  (let [url (format (get-in connection-properties [:beta :release-notes-url-template]) base-url version)
        notes (updater/fetch-json! url)]
    (when (and (map? notes) (vector? (:issues notes)))
      (try
        (updater/release-notes-markdown (assoc notes :version (str version " beta")))
        (catch Exception e
          (log/warn :message "Failed to render beta release notes" :version version :exception e)
          nil)))))

(defn fetch-offer!
  "Fetches the beta metadata and, if the beta is newer than the running editor,
  its release notes. Call off the UI thread. :info is nil if the metadata is
  unavailable or malformed."
  [source]
  (let [info (fetch-info! source)
        version (:version info)
        newer (boolean (and info (newer-than-running? source version)))]
    {:info info
     :newer newer
     :markdown (when newer
                 (fetch-release-notes! source version))}))

(defn busy? [session]
  (:busy @session))

(defn- confirm-download! [stage project localization version markdown]
  (dialogs/make-confirmation-dialog
    localization
    {:title (localization/message "beta.dialog.title")
     :icon :icon/circle-question
     :owner stage
     :header (localization/message "beta.dialog.header" {"version" version})
     :buttons [{:text (localization/message "beta.dialog.button.later")
                :cancel-button true
                :result :later}
               {:text (localization/message "beta.dialog.button.download")
                :default-button true
                :result :download}]
     :size :large
     :width 800
     :content {:fx/type fx.v-box/lifecycle
               :children (cond-> [{:fx/type fx.v-box/lifecycle
                                   :style-class "beta-warning"
                                   :children [{:fx/type fxui/paragraph
                                               :min-height Region/USE_PREF_SIZE
                                               :color :warning
                                               :text (localization (localization/message "beta.dialog.warning"))}]}]
                           markdown
                           (conj {:fx/type markdown-view/view
                                  :v-box/vgrow :always
                                  :content markdown
                                  :project project
                                  :stylesheets [(str (io/resource "editor.css"))]
                                  :root-props {:style-class "md-page-root"}}))}}))

(defn try!
  "Offers to open the beta download link in the web browser. Returns the future
  doing so, or nil if disabled or already busy."
  [session source ^Stage stage notifications-node project localization render-progress!]
  (when (and (eligible? source) (claim! session :busy))
    (future
      (try
        (render-progress! (progress/make-indeterminate (localization/message "progress.checking-beta")))
        (let [{:keys [info newer markdown]} (fetch-offer! source)
              version (:version info)]
          (render-progress! progress/done)
          (ui/run-later
            (try
              (when (.isShowing stage)
                (if newer
                  (when (= :download (confirm-download! stage project localization version markdown))
                    (ui/open-url (download-url source (:sha1 info)))
                    (notifications/close! notifications-node ::suggestion))
                  (dialogs/make-info-dialog
                    localization
                    {:title (localization/message "beta.dialog.title")
                     :icon (if info :icon/circle-info :icon/triangle-error)
                     :owner stage
                     :header (localization/message
                               (if info
                                 "beta.no-newer-dialog.header"
                                 "beta.check-failed-dialog.header"))})))
              (finally
                (swap! session assoc :busy false)))))
        (catch Throwable e
          (render-progress! progress/done)
          (swap! session assoc :busy false)
          (log/warn :message "Failed to check for beta" :exception e))))))

(defn suggest! [session source ^Stage stage notifications-node project git prefs localization render-progress!]
  (when (= "beta" (:channel source))
    (prefs/set! prefs [:versioning :ran-beta] true))
  (when git
    (future
      (when-let [info (check-once! session source prefs)]
        (ui/run-later
          (when (and (.isShowing stage)
                     (not (prefs/get prefs [:versioning :beta-suggestions-disabled])))
            (notifications/show!
              notifications-node
              {:type :info
               :id ::suggestion
               :message (localization/message "beta.notification.available" {"version" (:version info)})
               :actions [{:message (localization/message "beta.notification.try-it")
                          :on-action #(try! session source stage notifications-node project localization render-progress!)}
                         {:message (localization/message "beta.button.do-not-suggest")
                          :on-action #(prefs/set! prefs [:versioning :beta-suggestions-disabled] true)}]})))))))
