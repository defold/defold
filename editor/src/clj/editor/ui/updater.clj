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

(ns editor.ui.updater
  (:require [cljfx.api :as fx]
            [clojure.java.io :as io]
            [editor.dialogs :as dialogs]
            [editor.fxui :as fxui]
            [editor.localization :as localization]
            [editor.markdown-view :as markdown-view]
            [editor.ui :as ui]
            [editor.updater :as updater]
            [service.log :as log]
            [util.coll :as coll])
  (:import [java.io File]
           [javafx.application Platform]
           [javafx.stage Stage WindowEvent]))

(defn- make-link-fn [link localization]
  (fn [updater]
    (ui/run-later
      (let [can-install (updater/can-install-update? updater)
            advertised (updater/update-advertised? updater)]
        (ui/visible! link (or can-install advertised))
        (cond
          can-install (localization/localize! link localization (localization/message "updater.button.restart-to-update"))
          advertised (localization/localize! link localization (localization/message "updater.button.update-available")))))))

(defn- install! [^Stage stage updater localization]
  (try
    (updater/install! updater)
    true
    (catch Exception e
      (log/info :message "Update failed" :exception e)
      (dialogs/make-update-failed-dialog stage localization)
      false)))

(defn install-and-restart! [stage updater localization]
  (if (install! stage updater localization)
    (updater/restart! updater)
    (Platform/exit)))

(ui/defc release-notes-dialog
  {:compose [{:fx/type fx/ext-watcher
              :ref (:localization props)
              :key :localization-state}]}
  [{:keys [project localization-state markdown title header buttons result-fn]}]
  {:fx/type dialogs/dialog-stage
   :showing true
   :on-close-request (fn on-close-request [_] (result-fn (:result (coll/first-where :cancel-button buttons))))
   :title (localization-state title)
   :size :large
   :width 800
   :header {:fx/type fxui/legacy-label
            :variant :header
            :text (localization-state header)}
   :content {:fx/type markdown-view/view
             :content markdown
             :project project
             :stylesheets [(str (io/resource "editor.css"))]
             :root-props {:style-class "md-page-root"}}
   :footer {:fx/type dialogs/dialog-buttons
            :children (mapv (fn [{:keys [text result default-button cancel-button]}]
                              (cond-> {:fx/type fxui/legacy-button
                                       :text (localization-state text)
                                       :on-action (fn on-action [_] (result-fn result))}
                                cancel-button (assoc :cancel-button true)
                                default-button (assoc :variant :primary :default-button true)))
                            buttons)}})

(defn- show-release-notes-dialog!
  "Shows release notes with a choice of `buttons`, blocking the current thread
  until the user dismisses it. Must be called on the JavaFX application thread.
  Returns the :result of the chosen button."
  [project localization markdown title header buttons]
  (fxui/show-stateless-dialog-and-await-result!
    (fn [result-fn]
      {:fx/type release-notes-dialog
       :result-fn result-fn
       :localization localization
       :markdown markdown
       :title title
       :header header
       :buttons buttons
       :project project})))

(defn- show-release-notes-update-dialog!
  "Returns the user's choice as :skip, :later, or :update."
  [project localization release-notes]
  (let [versions (:versions release-notes)]
    (show-release-notes-dialog!
      project localization (:markdown release-notes)
      (localization/message "updater.release-notes-dialog.title")
      (localization/message "updater.release-notes-dialog.header"
                            {"count" (count versions)
                             "version" (first versions)})
      [{:text (localization/message "updater.release-notes-dialog.button.later")
        :result :later
        :cancel-button true}
       {:text (localization/message "updater.dialog.button.skip-version")
        :result :skip}
       {:text (localization/message "updater.release-notes-dialog.button.update-now")
        :result :update
        :default-button true}])))

(defn- prompt-and-download! [stage project updater localization download-confirmed]
  (let [release-notes (updater/release-notes updater)
        choice (if release-notes
                 (show-release-notes-update-dialog! project localization release-notes)
                 (if download-confirmed
                   :update
                   (dialogs/make-download-update-dialog stage localization)))]
    (case choice
      :skip (updater/skip-update! updater (or (:sha1 release-notes)
                                              (updater/current-update-sha1 updater)))
      :later nil
      :update (updater/download-and-extract! updater))))

(defn- handle-update-check! [stage project updater install-and-restart! localization ignore-skip]
  (let [can-install (updater/can-install-update? updater)
        update-exists (if ignore-skip
                        (updater/can-download-update? updater)
                        (updater/update-advertised? updater))
        can-get-new (and update-exists (updater/platform-supported? updater))]
    (cond
      (and can-install can-get-new)
      (case (dialogs/make-download-update-or-restart-dialog stage localization)
        :cancel nil
        :skip (updater/skip-update! updater (updater/current-update-sha1 updater))
        :download (prompt-and-download! stage project updater localization true)
        :restart (install-and-restart!))

      can-get-new
      (prompt-and-download! stage project updater localization false)

      (updater/download-in-progress? updater)
      (dialogs/make-info-dialog
        localization
        {:title (localization/message "updater.download-in-progress-dialog.title")
         :icon :icon/circle-info
         :owner stage
         :header (localization/message "updater.download-in-progress-dialog.header")})

      can-install
      (when (dialogs/make-confirmation-dialog
              localization
              {:title (localization/message "updater.dialog.title")
               :icon :icon/circle-question
               :header (localization/message "updater.dialog.header")
               :buttons [{:text (localization/message "updater.dialog.button.not-now")
                          :cancel-button true
                          :result false}
                         {:text (localization/message "updater.dialog.button.install-and-restart")
                          :default-button true
                          :result true}]
               :owner stage})
        (install-and-restart!))

      ;; A newer version exists, but newer releases no longer support this
      ;; platform, so there's nothing to download.
      update-exists
      (dialogs/make-platform-no-longer-supported-dialog stage localization)

      :else
      (dialogs/make-info-dialog
        localization
        {:title (localization/message "updater.up-to-date-dialog.title")
         :icon :icon/circle-info
         :owner stage
         :header (localization/message "updater.up-to-date-dialog.header")}))))

(defn check-for-updates! [stage project updater install-and-restart! localization]
  (when (updater/begin-manual-update-check! updater)
    (future
      (let [checked (updater/check! updater)]
        (ui/run-later
          (try
            (if checked
              (handle-update-check! stage project updater install-and-restart! localization true)
              (dialogs/make-info-dialog
                localization
                {:title (localization/message "updater.check-failed-dialog.title")
                 :icon :icon/triangle-error
                 :owner stage
                 :header (localization/message "updater.check-failed-dialog.header")}))
            (finally
              (updater/end-manual-update-check! updater))))))))

(defn- confirm-try-channel! [^Stage stage project localization version release-notes]
  (if release-notes
    (show-release-notes-dialog!
      project localization (:markdown release-notes)
      (localization/message "updater.try-channel-dialog.title")
      (localization/message "updater.try-channel-dialog.header" {"version" version})
      [{:text (localization/message "updater.release-notes-dialog.button.later")
        :result false
        :cancel-button true}
       {:text (localization/message "updater.try-channel-dialog.button.download")
        :result true
        :default-button true}])
    (dialogs/make-confirmation-dialog
      localization
      {:title (localization/message "updater.try-channel-dialog.title")
       :icon :icon/circle-question
       :owner stage
       :header (localization/message "updater.try-channel-dialog.header" {"version" version})
       :buttons [{:text (localization/message "updater.release-notes-dialog.button.later")
                  :cancel-button true
                  :result false}
                 {:text (localization/message "updater.try-channel-dialog.button.download")
                  :variant :primary
                  :default-button true
                  :result true}]})))

(defn- choose-channel-download-file [^Stage stage updater localization channel-info]
  (dialogs/make-save-file-dialog
    (localization (localization/message "updater.channel-save-dialog.title" {"version" (:version channel-info)}))
    [[(localization (localization/message "updater.channel-save-dialog.filter")) (str "*." (updater/channel-download-extension (:platform updater)))]]
    (io/file (updater/downloads-dir) (updater/channel-download-file-name updater channel-info))
    stage))

(defn- start-channel-download! [updater localization channel-info file]
  (let [version (:version channel-info)
        download (updater/download-channel! updater channel-info file)]
    (future
      (let [^File file @download]
        (ui/run-later
          (if file
            (when (dialogs/make-confirmation-dialog
                    localization
                    {:title (localization/message "updater.try-channel-dialog.title")
                     :icon :icon/circle-check
                     :owner (ui/main-stage)
                     :header (localization/message "updater.channel-download-done-dialog.header" {"version" version})
                     :content {:text (localization/message "updater.channel-download-done-dialog.detail" {"path" (.getPath file)})
                               :wrap-text true}
                     :buttons [{:text (localization/message "dialog.button.close")
                                :cancel-button true
                                :result false}
                               {:text (localization/message "updater.channel-download-done-dialog.button.show")
                                :variant :primary
                                :default-button true
                                :result true}]})
              (ui/open-file (.getParentFile file)))
            (dialogs/make-info-dialog
              localization
              {:title (localization/message "updater.try-channel-dialog.title")
               :icon :icon/triangle-error
               :owner (ui/main-stage)
               :header (localization/message "updater.channel-download-failed-dialog.header" {"version" version})})))))))

(defn try-channel!
  "Offers to download the beta editor to a location the user picks, for them
  to install. The current installation stays as it is."
  [stage project updater localization]
  (when (and (not (updater/channel-download-in-progress? updater))
             (updater/begin-manual-update-check! updater))
    (future
      (let [channel "beta"
            info (updater/fetch-channel-info! channel)
            newer (and info (updater/newer-than-running? (:version info)))
            release-notes (when newer (updater/channel-release-notes channel (:version info)))]
        (ui/run-later
          (try
            (cond
              newer
              (when (confirm-try-channel! stage project localization (:version info) release-notes)
                (let [channel-info (assoc info :channel channel)]
                  (when-let [file (choose-channel-download-file stage updater localization channel-info)]
                    (start-channel-download! updater localization channel-info file))))

              info
              (dialogs/make-info-dialog
                localization
                {:title (localization/message "updater.try-channel-dialog.title")
                 :icon :icon/circle-info
                 :owner stage
                 :header (localization/message "updater.no-newer-channel-dialog.header")})

              :else
              (dialogs/make-info-dialog
                localization
                {:title (localization/message "updater.check-failed-dialog.title")
                 :icon :icon/triangle-error
                 :owner stage
                 :header (localization/message "updater.check-failed-dialog.header")}))
            (finally
              (updater/end-manual-update-check! updater))))))))

(defn init-channel-offer!
  "Makes `link` suggest a channel with a newer version whenever there is one.
  Clicking it offers to download that channel's editor."
  [^Stage stage link project updater localization]
  (ui/on-action! link
    (fn on-channel-offer-link-action [_]
      (try-channel! stage project updater localization)))
  (let [link-fn (fn update-channel-offer-link [updater]
                  (ui/run-later
                    (let [offer (updater/channel-offer updater)]
                      (ui/visible! link (some? offer))
                      (ui/managed! link (some? offer))
                      (when offer
                        (localization/localize! link localization
                                                (localization/message "updater.button.channel-offer"
                                                                      {"channel" (:channel offer)
                                                                       "version" (:version offer)}))))))]
    (updater/add-state-watch updater link-fn)
    (.addEventHandler stage
                      WindowEvent/WINDOW_HIDING
                      (ui/event-handler _
                        (updater/remove-state-watch updater link-fn)))))

(defn init! [^Stage stage link project updater install-and-restart! render-progress! localization]
  (let [link-fn (make-link-fn link localization)]
    (ui/user-data! stage ::install-and-restart! install-and-restart!)
    (ui/on-closing! stage
      (fn [_]
        (when (updater/can-install-update? updater)
          (install! stage updater localization))
        true))
    (localization/localize! link localization (localization/message "updater.button.update-available"))
    (ui/on-action! link
      (fn [_] (handle-update-check! stage project updater install-and-restart! localization false)))
    (updater/add-progress-watch updater render-progress!)
    (updater/add-state-watch updater link-fn)
    (.addEventHandler stage
                      WindowEvent/WINDOW_HIDING
                      (ui/event-handler _
                        (updater/remove-progress-watch updater render-progress!)
                        (updater/remove-state-watch updater link-fn)))))
