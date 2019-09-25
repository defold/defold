(ns editor.changes-view
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.core :as core]
            [editor.defold-project :as project]
            [editor.dialogs :as dialogs]
            [editor.diff-view :as diff-view]
            [editor.disk-availability :as disk-availability]
            [editor.error-reporting :as error-reporting]
            [editor.fxui :as fxui]
            [editor.git :as git]
            [editor.handler :as handler]
            [editor.login :as login]
            [editor.progress :as progress]
            [editor.resource :as resource]
            [editor.sync :as sync]
            [editor.ui :as ui]
            [editor.vcs-status :as vcs-status]
            [editor.workspace :as workspace]
            [service.log :as log])
  (:import [java.io File]
           [javafx.beans.value ChangeListener]
           [javafx.scene Parent]
           [javafx.scene.control SelectionMode ListView]
           [org.eclipse.jgit.api Git]))

(set! *warn-on-reflection* true)

(defn- refresh-list-view! [list-view unified-status]
  (ui/items! list-view unified-status))

(defn refresh! [changes-view render-progress!]
  (let [list-view (g/node-value changes-view :list-view)
        unconfigured-git (g/node-value changes-view :unconfigured-git)
        refresh-pending (ui/user-data list-view :refresh-pending)
        schedule-refresh (ref nil)]
    (when unconfigured-git
      (dosync
        (ref-set schedule-refresh (not @refresh-pending))
        (ref-set refresh-pending true))
      (when @schedule-refresh
        (render-progress! (progress/make-indeterminate "Refreshing file status..."))
        (future
          (try
            (dosync (ref-set refresh-pending false))
            (let [unified-status (git/unified-status unconfigured-git)]
              (ui/run-later
                (ui/with-progress [render-progress! render-progress!]
                  (refresh-list-view! list-view unified-status))))
            (catch Throwable error
              (render-progress! progress/done)
              (error-reporting/report-exception! error))))))))

(handler/register-menu! ::changes-menu
  [{:label "Open"
    :icon "icons/32/Icons_S_14_linkarrow.png"
    :command :open}
   {:label "Open As"
    :icon "icons/32/Icons_S_14_linkarrow.png"
    :command :open-as}
   {:label :separator}
   {:label "Copy Project Path"
    :command :copy-project-path}
   {:label "Copy Full Path"
    :command :copy-full-path}
   {:label "Copy Require Path"
    :command :copy-require-path}
   {:label :separator}
   {:label "Show in Asset Browser"
    :icon "icons/32/Icons_S_14_linkarrow.png"
    :command :show-in-asset-browser}
   {:label "Show in Desktop"
    :icon "icons/32/Icons_S_14_linkarrow.png"
    :command :show-in-desktop}
   {:label "Referencing Files..."
    :command :referencing-files}
   {:label "Dependencies..."
    :command :dependencies}
   {:label :separator}
   {:label "View Diff"
    :icon "icons/32/Icons_S_06_arrowup.png"
    :command :diff}
   {:label "Revert"
    :icon "icons/32/Icons_S_02_Reset.png"
    :command :revert}])

(defn- path->file [workspace ^String path]
  (File. ^File (workspace/project-path workspace) path))

(handler/defhandler :revert :changes-view
  (enabled? [selection]
            (and (disk-availability/available?)
                 (pos? (count selection))))
  (run [async-reload! selection git changes-view workspace]
    (when (dialogs/make-confirmation-dialog
            {:title "Revert Changes?"
             :size :large
             :icon :icon/circle-question
             :header "Are you sure you want to revert changes on selected files?"
             :buttons [{:text "Cancel"
                        :cancel-button true
                        :default-button true
                        :result false}
                       {:text "Revert Changes"
                        :variant :danger
                        :result true}]})
      (let [moved-files (mapv #(vector (path->file workspace (:new-path %)) (path->file workspace (:old-path %))) (filter #(= (:change-type %) :rename) selection))]
        (git/revert git (mapv (fn [status] (or (:new-path status) (:old-path status))) selection))
        (async-reload! workspace changes-view moved-files)))))

(handler/defhandler :diff :changes-view
  (enabled? [selection]
            (git/selection-diffable? selection))
  (run [selection ^Git git]
       (diff-view/present-diff-data (git/selection-diff-data git selection))))

(defn project-is-git-repo? [changes-view]
  (some? (g/node-value changes-view :unconfigured-git)))

(defn first-sync! [changes-view dashboard-client project]
  (let [workspace (project/workspace project)
        proj-path (.getPath (workspace/project-path workspace))
        project-title (project/project-title project)
        prefs (g/node-value changes-view :prefs)]
    (-> (sync/begin-first-flow! proj-path project-title prefs dashboard-client
                                (fn [git]
                                  (ui/run-later
                                    (g/set-property! changes-view :unconfigured-git git))))
        (sync/open-sync-dialog))))

(defn regular-sync! [changes-view dashboard-client]
  (if-not (login/sign-in! dashboard-client :synchronize)
    false
    (let [git (g/node-value changes-view :git)
          prefs (g/node-value changes-view :prefs)
          creds (git/credentials prefs)
          flow (sync/begin-flow! git creds)]
      (sync/open-sync-dialog flow)
      true)))

(defn ensure-no-locked-files! [changes-view]
  (let [git (g/node-value changes-view :unconfigured-git)]
    (loop []
      (if-some [locked-files (not-empty (git/locked-files git))]
        ;; Found locked files below the project. Notify user and offer to retry.
        (if (dialogs/make-confirmation-dialog
              {:title "Not Safe to Sync"
               :icon :icon/circle-question
               :header "There are locked files, retry?"
               :content {:fx/type fxui/label
                         :style-class "dialog-content-padding"
                         :text (git/locked-files-error-message locked-files)}
               :buttons [{:text "Cancel"
                          :cancel-button true
                          :result false}
                         {:text "Retry"
                          :default-button true
                          :result true}]})
          (recur)
          false)

        ;; Found no locked files.
        true))))

(g/defnode ChangesView
  (inherits core/Scope)
  (property list-view g/Any)
  (property unconfigured-git g/Any)
  (output git g/Any (g/fnk [unconfigured-git prefs]
                      (when unconfigured-git
                        (doto unconfigured-git
                          (git/ensure-user-configured! prefs)))))
  (property prefs g/Any))

(defn- status->resource [workspace status]
  (workspace/resolve-workspace-resource workspace (format "/%s" (or (:new-path status)
                                                                    (:old-path status)))))

(defn- try-open-git
  ^Git [workspace]
  (let [repo-path (io/as-file (g/node-value workspace :root))
        git (git/try-open repo-path)
        head-commit (some-> git .getRepository (git/get-commit "HEAD"))]
    (if (some? head-commit)
      git
      (when (some? git)
        (.close git)))))

(defn make-changes-view [view-graph workspace prefs async-reload! ^Parent parent]
  (assert (fn? async-reload!))
  (let [^ListView list-view     (.lookup parent "#changes")
        diff-button             (.lookup parent "#changes-diff")
        revert-button           (.lookup parent "#changes-revert")
        git                     (try-open-git workspace)
        view-id                 (g/make-node! view-graph ChangesView :list-view list-view :unconfigured-git git :prefs prefs)
        disk-available-listener (reify ChangeListener
                                  (changed [_this _observable _old _new]
                                    (ui/refresh-bound-action-enabled! revert-button)))]
    ; TODO: try/catch to protect against project without git setup
    ; Show warning/error etc?
    (try
      (ui/user-data! list-view :refresh-pending (ref false))
      (.setSelectionMode (.getSelectionModel list-view) SelectionMode/MULTIPLE)
      (ui/context! parent :changes-view {:async-reload! async-reload! :changes-view view-id :workspace workspace} (ui/->selection-provider list-view)
        {:git [:changes-view :unconfigured-git]}
        {resource/Resource (fn [status] (status->resource workspace status))})
      (ui/register-context-menu list-view ::changes-menu)
      (ui/cell-factory! list-view vcs-status/render)
      (ui/bind-action! diff-button :diff)
      (ui/bind-action! revert-button :revert)
      (ui/disable! diff-button true)
      (ui/disable! revert-button true)
      (ui/bind-double-click! list-view :open)
      (when git (refresh-list-view! list-view (git/unified-status git)))
      (.addListener disk-availability/available-property disk-available-listener)
      (ui/on-closed! (.. parent getScene getWindow)
                     (fn [_]
                       (.removeListener disk-availability/available-property disk-available-listener)))
      (ui/observe-selection list-view
                            (fn [_ _]
                              (ui/refresh-bound-action-enabled! diff-button)
                              (ui/refresh-bound-action-enabled! revert-button)))
      view-id
      (catch Exception e
        (log/error :exception e)))))
