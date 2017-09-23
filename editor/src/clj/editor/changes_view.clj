(ns editor.changes-view
  (:require [dynamo.graph :as g]
            [editor.core :as core]
            [editor.defold-project :as project]
            [editor.dialogs :as dialogs]
            [editor.diff-view :as diff-view]
            [editor.git :as git]
            [editor.handler :as handler]
            [editor.login :as login]
            [editor.progress :as progress]
            [editor.resource :as resource]
            [editor.resource-watch :as resource-watch]
            [editor.ui :as ui]
            [editor.vcs-status :as vcs-status]
            [editor.workspace :as workspace]
            [internal.util :as util]
            [service.log :as log])
  (:import [javafx.scene Parent]
           [javafx.scene.control SelectionMode ListView]
           [java.io File]))

(set! *warn-on-reflection* true)

(defn- list-view-items [git-status dirty-resources]
  (sort-by vcs-status/path
           (util/distinct-by vcs-status/path
                             (concat (map (comp git/make-unsaved-change resource/path) dirty-resources)
                                     git-status))))

(defn- refresh-list-view! [list-view git-status dirty-resources]
  (let [shown-data (ui/user-data list-view ::shown-data)]
    (when-not (and (identical? (:git-status shown-data) git-status)
                   (identical? (:dirty-resources shown-data) dirty-resources))
      (ui/user-data! list-view ::shown-data {:git-status git-status
                                             :dirty-resources dirty-resources})
      (ui/items! list-view (list-view-items git-status dirty-resources)))))

(defn resource-sync-after-git-change!
  ([changes-view workspace]
   (resource-sync-after-git-change! changes-view workspace []))
  ([changes-view workspace moved-files]
   (resource-sync-after-git-change! changes-view workspace moved-files progress/null-render-progress!))
  ([changes-view workspace moved-files render-progress!]
   (let [diff (workspace/resource-sync! workspace true moved-files render-progress!)]
     ;; The call to resource-sync! will refresh the changes view if it detected changes,
     ;; but committing a file from the command line will not actually change the file
     ;; as far as resource-sync! is concerned. To ensure the changed files view reflects
     ;; the current Git state, we explicitly refresh the changes view here if the the
     ;; call to resource-sync! would not have already done so.
     (when (resource-watch/empty-diff? diff)
       (refresh! changes-view)))))

(ui/extend-menu ::changes-menu nil
                [{:label "Open"
                  :icon "icons/32/Icons_S_14_linkarrow.png"
                  :command :open}
                 {:label "Open As"
                  :icon "icons/32/Icons_S_14_linkarrow.png"
                  :command :open-as}
                 {:label "Show in Asset Browser"
                  :icon "icons/32/Icons_S_14_linkarrow.png"
                  :command :show-in-asset-browser}
                 {:label "Show in Desktop"
                  :icon "icons/32/Icons_S_14_linkarrow.png"
                  :command :show-in-desktop}
                 {:label "Referencing Files"
                  :command :referencing-files}
                 {:label "Dependencies"
                  :command :dependencies}
                 {:label "Hot Reload"
                  :command :hot-reload}
                 {:label :separator}
                 {:label "View Diff"
                  :icon "icons/32/Icons_S_06_arrowup.png"
                  :command :diff}
                 {:label "Revert"
                  :icon "icons/32/Icons_S_02_Reset.png"
                  :command :revert}])

(defn- path->file [workspace ^String path]
  (File. ^File (workspace/project-path workspace) path))

(defn- revert-changes! [workspace git changes]
  (let [{git-changes false unsaved-changes true} (group-by #(= :unsaved (:change-type %)) changes)]
    (when (some? unsaved-changes)
      (workspace/invalidate-resources! workspace (map vcs-status/path unsaved-changes)))
    (when (some? git-changes)
      (git/revert git (map vcs-status/path git-changes)))))

(handler/defhandler :revert :changes-view
  (enabled? [selection]
            (git/selection-revertible? selection))
  (run [selection app-view workspace]
    (let [git (g/node-value app-view :git)
          moved-files (mapv #(vector (path->file workspace (:new-path %)) (path->file workspace (:old-path %))) (filter #(= (:change-type %) :rename) selection))]
      (revert-changes! workspace git selection)
      (app-view/resource-sync-after-git-change! app-view workspace moved-files))))

(defn- status->resource [workspace status]
  (workspace/resolve-workspace-resource workspace (format "/%s" (or (:new-path status)
                                                                    (:old-path status)))))

(defn- unsaved-diff-data [workspace app-view status]
  (let [project (g/node-value app-view :project-id)
        resource (status->resource workspace status)
        resource-node (project/get-resource-node project resource)
        old (slurp resource)
        new (:content (g/node-value resource-node :save-data))]
    {:binary? false
     :new new
     :new-path (:new-path status)
     :old old
     :old-path (:old-path status)}))

(defn- diff-data [workspace app-view git status]
  (if (= :unsaved (:change-type status))
    (unsaved-diff-data workspace app-view status)
    (git/diff-data git status)))

(handler/defhandler :diff :changes-view
  (enabled? [selection]
            (git/selection-diffable? selection))
  (run [selection app-view workspace]
       (let [git (g/node-value app-view :git)]
         (diff-view/present-diff-data (diff-data workspace app-view git (first selection))))))

(handler/defhandler :synchronize :global
  (enabled? [changes-view]
            (g/node-value changes-view :git))
  (run [changes-view workspace project]
    (let [git   (g/node-value changes-view :git)
          prefs (g/node-value changes-view :prefs)]
      ;; Check if there are locked files below the project folder before proceeding.
      ;; If so, we abort the sync and notify the user, since this could cause problems.
      (loop []
        (if-some [locked-files (not-empty (git/locked-files git))]
          ;; Found locked files below the project. Notify user and offer to retry.
          (when (dialogs/make-confirm-dialog (git/locked-files-error-message locked-files)
                                             {:title "Not Safe to Sync"
                                              :ok-label "Retry"
                                              :cancel-label "Cancel"})
            (recur))

          ;; Found no locked files.
          ;; Save the project before we initiate the sync process. We need to do this because
          ;; the unsaved files may also have changed on the server, and we'll need to merge.
          (do (project/save-all! project)
              (when (login/login prefs)
                (let [creds (git/credentials prefs)
                      flow (sync/begin-flow! git creds)]
                  (sync/open-sync-dialog flow)
                  (resource-sync-after-git-change! changes-view workspace)))))))))

(ui/extend-menu ::menubar :editor.app-view/open
                [{:label "Synchronize..."
                  :id ::synchronize
                  :command :synchronize}])

(g/defnode ChangesView
  (inherits core/Scope)
  (property app-view g/NodeID)
  (property list-view g/Any)
  (input git g/Any)
  (input git-status g/Any)
  (input dirty-resources g/Any)
  (output refresh-changes-view g/Any (g/fnk [dirty-resources git-status list-view]
                                       (refresh-list-view! list-view git-status dirty-resources))))

(defn- setup-changes-view [view-graph list-view app-view]
  (first
    (g/tx-nodes-added
      (g/transact
        (g/make-nodes view-graph
                      [view-id [ChangesView :app-view app-view :list-view list-view]]
                      (g/connect app-view :git view-id :git)
                      (g/connect app-view :git-status view-id :git-status)
                      (g/connect app-view :dirty-resources view-id :dirty-resources))))))

(defn make-changes-view [view-graph workspace app-view ^Parent parent]
  (let [^ListView list-view (.lookup parent "#changes")
        diff-button         (.lookup parent "#changes-diff")
        revert-button       (.lookup parent "#changes-revert")
        view-id             (setup-changes-view view-graph list-view app-view)]
    (try
      (.setSelectionMode (.getSelectionModel list-view) SelectionMode/MULTIPLE)
      (ui/context! parent :changes-view {:app-view app-view :changes-view view-id :workspace workspace} (ui/->selection-provider list-view) {}
                   {resource/Resource (fn [status] (status->resource workspace status))})
      (ui/register-context-menu list-view ::changes-menu)
      (ui/cell-factory! list-view vcs-status/render)
      (ui/bind-action! diff-button :diff)
      (ui/bind-action! revert-button :revert)
      (ui/disable! diff-button true)
      (ui/disable! revert-button true)
      (ui/bind-double-click! list-view :open)
      (ui/observe-selection list-view
                            (fn [_ _]
                              (ui/refresh-bound-action-enabled! diff-button)
                              (ui/refresh-bound-action-enabled! revert-button)))
      view-id
      (catch Exception e
        (log/error :exception e)))))
