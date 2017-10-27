(ns editor.changes-view
  (:require [dynamo.graph :as g]
            [editor.app-view :as app-view]
            [editor.core :as core]
            [editor.defold-project :as project]
            [editor.diff-view :as diff-view]
            [editor.git :as git]
            [editor.handler :as handler]
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
            (not (empty? selection)))
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
