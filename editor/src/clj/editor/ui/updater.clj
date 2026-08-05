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
            [editor.markdown :as markdown]
            [editor.ui :as ui]
            [editor.updater :as updater]
            [service.log :as log])
  (:import [javafx.application Platform]
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

(ui/defc release-notes-update-dialog
  {:compose [{:fx/type fx/ext-watcher
              :ref (:localization props)
              :key :localization-state}]}
  [{:keys [project localization-state release-notes result-fn]}]
  {:fx/type dialogs/dialog-stage
   :showing true
   :on-close-request (fn [_] (result-fn :later))
   :title (localization-state (localization/message "updater.release-notes-dialog.title"))
   :size :large
   :width 800
   :header {:fx/type fxui/legacy-label
            :variant :header
            :text (let [versions (:versions release-notes)]
                    (localization-state
                      (localization/message "updater.release-notes-dialog.header"
                                            {"count" (count versions)
                                             "version" (first versions)})))}
   :content {:fx/type markdown/view
             :content (:markdown release-notes)
             :project project
             :stylesheets [(str (io/resource "editor.css"))]
             :root-props {:style-class "md-page-root"}}
   :footer {:fx/type dialogs/dialog-buttons
            :children [{:fx/type fxui/legacy-button
                        :text (localization-state (localization/message "updater.release-notes-dialog.button.later"))
                        :cancel-button true
                        :on-action (fn [_] (result-fn :later))}
                       {:fx/type fxui/legacy-button
                        :text (localization-state (localization/message "updater.dialog.button.skip-version"))
                        :on-action (fn [_] (result-fn :skip))}
                       {:fx/type fxui/legacy-button
                        :text (localization-state (localization/message "updater.release-notes-dialog.button.update-now"))
                        :variant :primary
                        :default-button true
                        :on-action (fn [_] (result-fn :update))}]}})

(defn- show-release-notes-update-dialog!
  "Shows the release notes dialog, blocking the current thread until the user
  dismisses it. Must be called on the JavaFX application thread. Returns the
  user's choice as :skip, :later, or :update."
  [project localization release-notes]
  (fxui/show-stateless-dialog-and-await-result!
    (fn [result-fn]
      {:fx/type release-notes-update-dialog
       :result-fn result-fn
       :localization localization
       :release-notes release-notes
       :project project})))

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

      (updater/download-in-progress? updater)
      (dialogs/make-info-dialog
        localization
        {:title (localization/message "updater.download-in-progress-dialog.title")
         :icon :icon/circle-info
         :owner stage
         :header (localization/message "updater.download-in-progress-dialog.header")})

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
