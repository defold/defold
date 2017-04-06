(ns editor.updater
  (:require [editor.defold-project :as project]
            [editor.ui :as ui]
            [editor.workspace :as workspace]
            [service.log :as log])
  (:import [com.defold.editor EditorApplication Updater$PendingUpdate]
           [java.io IOException]
           [javafx.animation AnimationTimer]
           [javafx.scene Scene Parent]
           [javafx.stage Stage]))

(set! *warn-on-reflection* true)

(defn show-pending-update-dialog!
  [^Stage owner]
  (let [root ^Parent (ui/load-fxml "update-alert.fxml")
        stage (ui/make-dialog-stage owner)
        scene (Scene. root)
        result (atom false)]
    (ui/title! stage "Update Available")
    (ui/with-controls root [ok cancel]
      (ui/on-action! ok (fn on-ok! [_] (reset! result true) (.close stage)))
      (ui/on-action! cancel (fn on-cancel! [_] (.close stage))))
    (.setScene stage scene)
    (ui/show-and-wait! stage)
    @result))

(defn- ask-update-and-restart!
  [^Stage owner ^Updater$PendingUpdate pending-update project]
  (when (show-pending-update-dialog! owner)
    (log/info :message "update installation requested")

    ;; If we have a project, save it before restarting the editor.
    (when (some? project)
      ;; If a build or save was in progress, wait for it to complete.
      (while (project/ongoing-build-save?)
        (Thread/sleep 300))

      ;; Save the project and block until complete. Before saving, perform a
      ;; resource sync to ensure we do not overwrite external changes.
      (workspace/resource-sync! (project/workspace project))
      (let [save-future (project/save-all! project nil)]
        (when (future? save-future)
          (deref save-future))))

    ;; Install update and restart the editor.
    (try
      (.install pending-update)
      (log/info :message "update installed - restarting")
      (System/exit 17)
      (catch IOException e
        (log/debug :message "update installation failed" :exception e)))))

(defn install-pending-update-check!
  [^Stage stage project]
  (let [tick-fn (fn [^AnimationTimer timer _dt]
                  (when-let [pending-update (EditorApplication/getPendingUpdate)]
                    (.stop timer)
                    (ui/run-later
                      (ask-update-and-restart! stage pending-update project))))
        timer (ui/->timer 1 "pending-update-check" tick-fn)]
    (.setOnShown stage (ui/event-handler event (ui/timer-start! timer)))
    (.setOnHiding stage (ui/event-handler event (ui/timer-stop! timer)))))
