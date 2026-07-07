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
  (:require [clojure.java.io :as io]
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
      (let [can-install? (updater/can-install-update? updater)
            can-download? (updater/can-download-update? updater)]
        (ui/visible! link (or can-install? can-download?))
        (cond
          can-install? (localization/localize! link localization (localization/message "updater.button.restart-to-update"))
          can-download? (localization/localize! link localization (localization/message "updater.button.update-available")))))))

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

(defn- release-notes-update-dialog [{:keys [content project localization] :as props}]
  {:fx/type dialogs/dialog-stage
   :showing (fxui/dialog-showing? props)
   :on-close-request {:result false}
   :title (localization (localization/message "updater.release-notes-dialog.title"))
   :size :large
   :header {:fx/type fxui/legacy-label
            :variant :header
            :text (localization (localization/message "updater.release-notes-dialog.header"))}
   :content {:fx/type markdown/view
             :content content
             :project project
             :stylesheets [(str (io/resource "editor.css"))]
             :root-props {:style-class "md-page-root"}}
   :footer {:fx/type dialogs/dialog-buttons
            :children [{:fx/type fxui/legacy-button
                        :text (localization (localization/message "updater.release-notes-dialog.button.later"))
                        :cancel-button true
                        :on-action {:result false}}
                       {:fx/type fxui/legacy-button
                        :text (localization (localization/message "updater.release-notes-dialog.button.update-now"))
                        :on-action {:result true}}]}})

(defn- show-release-notes-update-dialog!
  "Shows the release notes dialog, blocking the current thread until the user
  dismisses it. Must be called on the JavaFX application thread. Returns true if
  the user chose to update now, false otherwise."
  [content project localization]
  (fxui/show-dialog-and-await-result!
    :event-handler (fn [state event]
                     (assoc state ::fxui/result (:result event)))
    :description {:fx/type release-notes-update-dialog
                  :content content
                  :project project
                  :localization localization}))

(defn- prompt-and-download! [stage project updater localization download-confirmed?]
  (if-let [content (updater/release-notes updater)]
    (when (show-release-notes-update-dialog! content project localization)
      (updater/download-and-extract! updater))
    (when (or download-confirmed?
              (dialogs/make-download-update-dialog stage localization))
      (updater/download-and-extract! updater))))

(defn init! [^Stage stage link project updater install-and-restart! render-progress! localization]
  (let [link-fn (make-link-fn link localization)]
    (ui/on-closing! stage
      (fn [_]
        (when (updater/can-install-update? updater)
          (install! stage updater localization))
        true))
    (localization/localize! link localization (localization/message "updater.button.update-available"))
    (ui/on-action! link
      (fn [_]
        (let [can-install? (updater/can-install-update? updater)
              can-download? (updater/can-download-update? updater)
              can-get-new? (and can-download? (updater/platform-supported? updater))]
          (cond
            (and can-install? can-get-new?)
            (case (dialogs/make-download-update-or-restart-dialog stage localization)
              :cancel nil
              :download (prompt-and-download! stage project updater localization true)
              :restart (install-and-restart!))

            can-get-new?
            (prompt-and-download! stage project updater localization false)

            can-install?
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
            can-download?
            (dialogs/make-platform-no-longer-supported-dialog stage localization)))))
    (updater/add-progress-watch updater render-progress!)
    (updater/add-state-watch updater link-fn)
    (.addEventHandler stage
                      WindowEvent/WINDOW_HIDING
                      (ui/event-handler event
                        (updater/remove-progress-watch updater render-progress!)
                        (updater/remove-state-watch updater link-fn)))))
