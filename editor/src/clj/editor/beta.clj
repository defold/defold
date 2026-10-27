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
  thread. Opted-out users, users who have run a beta and ineligible
  installations make no requests."
  [prefs platform]
  (when (and (eligible? platform)
             (not (prefs/get prefs [:versioning :beta-suggestions-disabled]))
             (not (prefs/get prefs [:versioning :ran-beta]))
             (compare-and-set! automatic-check-started false true))
    (when-let [info (fetch-info!)]
      (when (newer-than-running? (:version info))
        info))))

(defn- fetch-release-notes! [version]
  (when-let [notes (updater/fetch-version-notes! (system/defold-archive-domain) "beta" version)]
    (when (and (map? notes) (vector? (:issues notes)))
      (try
        (updater/release-notes-markdown (assoc notes :version (str version " beta")))
        (catch Exception e
          (log/warn :message "Failed to render beta release notes" :version version :exception e)
          nil)))))

(defonce ^:private busy (atom false))

(defonce ^:private cached-offer (atom nil))

(defn try-active? []
  (eligible? (Platform/getHostPlatform)))

(defn try-enabled? []
  (and (try-active?)
       (not @busy)))

(defn- confirm-download! [stage project localization version markdown show-warning]
  (dialogs/make-confirmation-dialog
    localization
    (cond-> {:title (localization/message "beta.dialog.title")
             :icon :icon/circle-question
             :owner stage
             :header (localization/message "beta.dialog.header" {"version" version})
             :buttons [{:text (localization/message "beta.dialog.button.later")
                        :cancel-button true
                        :result :later}
                       {:text (localization/message "beta.button.do-not-suggest")
                        :result :do-not-suggest}
                       {:text (localization/message "beta.dialog.button.download")
                        :default-button true
                        :result :download}]}
      (or show-warning markdown)
      (assoc :size :large
             :width 800
             :content {:fx/type fx.v-box/lifecycle
                       :children (cond-> []
                                   show-warning
                                   (conj {:fx/type fx.v-box/lifecycle
                                          :style-class "beta-warning"
                                          :children [{:fx/type fxui/paragraph
                                                      :min-height Region/USE_PREF_SIZE
                                                      :color :warning
                                                      :text (localization (localization/message "beta.dialog.warning"))}]})

                                   markdown
                                   (conj {:fx/type markdown-view/view
                                          :v-box/vgrow :always
                                          :content markdown
                                          :project project
                                          :stylesheets [(str (io/resource "editor.css"))]
                                          :root-props {:style-class "md-page-root"}}))}))))

(defn try!
  "Offers to open the beta download link in the web browser. Returns the future
  doing so, or nil if disabled or already busy."
  [^Stage stage notifications-node project prefs localization render-progress!]
  (when (and (try-enabled?) (compare-and-set! busy false true))
    (future
      (try
        (render-progress! (progress/make-indeterminate (localization/message "progress.checking-beta")))
        (let [info (fetch-info!)
              version (:version info)
              newer (and info (newer-than-running? version))
              cached @cached-offer
              markdown (when newer
                         (or (when (= version (:version (:info cached)))
                               (:markdown cached))
                             (fetch-release-notes! version)))]
          (render-progress! progress/done)
          (ui/run-later
            (try
              (when (.isShowing stage)
                (if newer
                  (case (confirm-download! stage project localization version markdown true)
                    :download
                    (let [platform (Platform/getHostPlatform)
                          extension (if (= "macos" (.getOs platform)) "dmg" "zip")]
                      (ui/open-url (updater/download-url (system/defold-archive-domain) (:sha1 info) "beta" platform extension))
                      (notifications/close! notifications-node ::suggestion))

                    :do-not-suggest
                    (do (prefs/set! prefs [:versioning :beta-suggestions-disabled] true)
                        (notifications/close! notifications-node ::suggestion))

                    nil)
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
                (reset! busy false)))))
        (catch Throwable e
          (render-progress! progress/done)
          (reset! busy false)
          (log/warn :message "Failed to check for beta" :exception e))))))

(defn suggest! [^Stage stage notifications-node project git prefs localization render-progress!]
  (when (= "beta" (system/defold-channel))
    (prefs/set! prefs [:versioning :ran-beta] true))
  (when git
    (future
      (when-let [info (check-once! prefs (Platform/getHostPlatform))]
        (reset! cached-offer {:info info
                              :markdown (fetch-release-notes! (:version info))})
        (ui/run-later
          (when (and (.isShowing stage)
                     (not (prefs/get prefs [:versioning :beta-suggestions-disabled])))
            (notifications/show!
              notifications-node
              {:type :info
               :id ::suggestion
               :message (localization/message "beta.notification.available" {"version" (:version info)})
               :actions [{:message (localization/message "beta.notification.try-it")
                          :on-action #(try! stage notifications-node project prefs localization render-progress!)}
                         {:message (localization/message "beta.button.do-not-suggest")
                          :on-action #(prefs/set! prefs [:versioning :beta-suggestions-disabled] true)}]})))))))
