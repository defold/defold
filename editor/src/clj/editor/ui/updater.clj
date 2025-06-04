;; Copyright 2020-2025 The Defold Foundation
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
  (:require [editor.dialogs :as dialogs]
            [editor.ui :as ui]
            [editor.updater :as updater]
            [service.log :as log])
  (:import [javafx.application Platform]
           [javafx.stage Stage WindowEvent]))

(defn- make-link-fn [link]
  (fn [updater]
    (ui/run-later
      (let [can-install? (updater/can-install-update? updater)
            can-download? (updater/can-download-update? updater)]
        (ui/visible! link (or can-install? can-download?))
        (cond
          can-install? (ui/text! link "Restart to Update")
          can-download? (ui/text! link "Update Available"))))))

(defn- install! [^Stage stage updater]
  (try
    (updater/install! updater)
    true
    (catch Exception e
      (log/info :message "Update failed" :exception e)
      (dialogs/make-update-failed-dialog stage)
      false)))

(defn install-and-restart! [stage updater]
  (if (install! stage updater)
    (updater/restart! updater)
    (Platform/exit)))

(defn init! [^Stage stage link updater install-and-restart! render-progress!]
  (let [link-fn (make-link-fn link)]
    (ui/on-closing! stage
      (fn [_]
        (when (updater/can-install-update? updater)
          (install! stage updater))
        true))
    (ui/on-action! link
      (fn [_]
        (let [can-install? (updater/can-install-update? updater)
              can-download? (updater/can-download-update? updater)]
          (cond
            (and can-install? can-download?)
            (case (dialogs/make-download-update-or-restart-dialog stage)
              :cancel nil
              :download (updater/download-and-extract! updater)
              :restart (install-and-restart!))

            (and can-download? (not (updater/platform-supported? updater)))
            (dialogs/make-platform-no-longer-supported-dialog stage)

            can-download?
            (when (dialogs/make-download-update-dialog stage)
              (updater/download-and-extract! updater))

            can-install?
            (when (dialogs/make-confirmation-dialog
                    {:title "Install Update and Restart?"
                     :icon :icon/circle-question
                     :header "Install downloaded update and restart?"
                     :buttons [{:text "Not Now"
                                :cancel-button true
                                :result false}
                               {:text "Install and Restart"
                                :default-button true
                                :result true}]
                     :owner stage})
              (install-and-restart!))))))
    (updater/add-progress-watch updater render-progress!)
    (updater/add-state-watch updater link-fn)
    (.addEventHandler stage
                      WindowEvent/WINDOW_HIDING
                      (ui/event-handler event
                        (updater/remove-progress-watch updater render-progress!)
                        (updater/remove-state-watch updater link-fn)))))
