(ns editor.ui.updater
  (:require [editor.ui :as ui]
            [editor.updater :as updater]
            [editor.dialogs :as dialogs])
  (:import [javafx.stage Stage WindowEvent]))

(defn- make-link-fn [link]
  (fn [updater]
    (ui/run-later
      (let [can-install? (updater/can-install-update? updater)
            can-download? (updater/can-download-update? updater)]
        (ui/visible! link (or can-install? can-download?))
        (cond
          can-install? (ui/text! link "Restart to Update")
          can-download? (ui/text! link "Update Available"))))))

(defn init! [^Stage stage link updater on-restart! render-progress!]
  (let [link-fn (make-link-fn link)]
    (ui/on-action! link
      (fn [_]
        (let [can-install? (updater/can-install-update? updater)
              can-download? (updater/can-download-update? updater)]
          (cond
            (and can-install? can-download?)
            (case (dialogs/make-download-update-or-restart-dialog stage)
              :cancel nil
              :download (updater/download-and-extract! updater)
              :restart (on-restart!))

            can-download?
            (when (dialogs/make-download-update-dialog stage)
              (updater/download-and-extract! updater))

            can-install?
            (on-restart!)))))
    (updater/add-progress-watch updater render-progress!)
    (updater/add-state-watch updater link-fn)
    (.addEventHandler stage
                      WindowEvent/WINDOW_HIDING
                      (ui/event-handler event
                        (updater/remove-progress-watch updater render-progress!)
                        (updater/remove-state-watch updater link-fn)))))
