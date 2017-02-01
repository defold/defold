(ns editor.boot-open-project
  (:require [dynamo.graph :as g]
            [editor.app-view :as app-view]
            [editor.asset-browser :as asset-browser]
            [editor.build-errors-view :as build-errors-view]
            [editor.changes-view :as changes-view]
            [editor.code-view :as code-view]
            [editor.console :as console]
            [editor.curve-view :as curve-view]
            [editor.defold-project :as project]
            [editor.dialogs :as dialogs]
            [editor.form-view :as form-view]
            [editor.git :as git]
            [editor.graph-view :as graph-view]
            [editor.hot-reload :as hot-reload]
            [editor.login :as login]
            [editor.outline-view :as outline-view]
            [editor.prefs :as prefs]
            [editor.progress :as progress]
            [editor.properties-view :as properties-view]
            [editor.resource :as resource]
            [editor.resource-types :as resource-types]
            [editor.scene :as scene]
            [editor.targets :as targets]
            [editor.text :as text]
            [editor.ui :as ui]
            [editor.web-profiler :as web-profiler]
            [editor.workspace :as workspace]
            [editor.sync :as sync]
            [editor.system :as system]
            [util.http-server :as http-server])
  (:import  [com.defold.editor EditorApplication]
            [java.io File]
            [javafx.scene.layout Region VBox]
            [javafx.scene Scene]
            [javafx.scene.control TabPane Tab]))

(set! *warn-on-reflection* true)

(def ^:dynamic *workspace-graph*)
(def ^:dynamic *project-graph*)
(def ^:dynamic *view-graph*)

(def the-root (atom nil))

;; inovked to control the timing of when the namepsaces load
(defn load-namespaces []
  (println "loaded namespaces"))

(defn initialize-project []
  (when (nil? @the-root)
    (g/initialize! {})
    (alter-var-root #'*workspace-graph* (fn [_] (g/last-graph-added)))
    (alter-var-root #'*project-graph*   (fn [_] (g/make-graph! :history true  :volatility 1)))
    (alter-var-root #'*view-graph*      (fn [_] (g/make-graph! :history false :volatility 2)))))

(defn setup-workspace [project-path]
  (let [workspace (workspace/make-workspace *workspace-graph* project-path)]
    (g/transact
      (concat
        (text/register-view-types workspace)
        (code-view/register-view-types workspace)
        (scene/register-view-types workspace)
        (form-view/register-view-types workspace)))
    (resource-types/register-resource-types! workspace)
    (workspace/resource-sync! workspace)
    workspace))


;; Slight hack to work around the fact that we have not yet found a
;; reliable way of detecting when the application loses focus.

;; If no application-owned windows have had focus within this
;; threshold, we consider the application to have lost focus.
(def application-unfocused-threshold-ms 500)

(defn- watch-focus-state! [workspace]
  (add-watch dialogs/focus-state :main-stage
             (fn [key ref old new]
               (when (and old
                          (not (:focused? old))
                          (:focused? new))
                 (let [unfocused-ms (- (:t new) (:t old))]
                   (when (< application-unfocused-threshold-ms unfocused-ms)
                     (ui/default-render-progress-now! (progress/make "Reloading modified resources"))
                     (ui/->future 0.01
                                  #(try
                                     (ui/with-progress [render-fn ui/default-render-progress!]
                                       (editor.workspace/resource-sync! workspace true [] render-fn))
                                     (finally
                                       (ui/default-render-progress-now! progress/done))))))))))

(defn- find-tab [^TabPane tabs id]
  (some #(and (= id (.getId ^Tab %)) %) (.getTabs tabs)))

(defn- handle-resource-changes! [changes changes-view editor-tabs]
  (ui/run-later
    (changes-view/refresh! changes-view)))

(defn load-stage [workspace project prefs]
  (let [^VBox root (ui/load-fxml "editor.fxml")
        stage      (ui/make-stage)
        scene      (Scene. root)]
    (ui/observe (.focusedProperty stage)
                (fn [property old-val new-val]
                  (dialogs/record-focus-change! new-val)))
    (watch-focus-state! workspace)

    (ui/set-main-stage stage)
    (.setScene stage scene)

    (app-view/restore-window-dimensions stage prefs)

    (ui/init-progress!)
    (ui/show! stage)
    (targets/start)

    (let [^MenuBar menu-bar    (.lookup root "#menu-bar")
          ^TabPane editor-tabs (.lookup root "#editor-tabs")
          ^TabPane tool-tabs   (.lookup root "#tool-tabs")
          ^TreeView outline    (.lookup root "#outline")
          ^TreeView assets     (.lookup root "#assets")
          console              (.lookup root "#console")
          prev-console         (.lookup root "#prev-console")
          next-console         (.lookup root "#next-console")
          clear-console        (.lookup root "#clear-console")
          search-console       (.lookup root "#search-console")
          workbench            (.lookup root "#workbench")
          app-view             (app-view/make-app-view *view-graph* workspace project stage menu-bar editor-tabs prefs)
          outline-view         (outline-view/make-outline-view *view-graph* *project-graph* outline app-view)
          properties-view      (properties-view/make-properties-view workspace project app-view *view-graph* (.lookup root "#properties"))
          asset-browser        (asset-browser/make-asset-browser *view-graph* workspace assets)
          web-server           (-> (http-server/->server 0 {"/profiler" web-profiler/handler
                                                            hot-reload/url-prefix (partial hot-reload/build-handler project)})
                                   http-server/start!)
          build-errors-view    (build-errors-view/make-build-errors-view (.lookup root "#build-errors-tree")
                                                                         (fn [resource node-id opts]
                                                                           (app-view/open-resource app-view
                                                                                                   (g/node-value project :workspace)
                                                                                                   project
                                                                                                   resource
                                                                                                   opts)
                                                                           (app-view/select! app-view node-id)))
          changes-view         (changes-view/make-changes-view *view-graph* workspace prefs
                                                               (.lookup root "#changes-container"))
          curve-view           (curve-view/make-view! app-view *view-graph*
                                                      (.lookup root "#curve-editor-container")
                                                      (.lookup root "#curve-editor-list")
                                                      (.lookup root "#curve-editor-view")
                                                      {:tab (find-tab tool-tabs "curve-editor-tab")})]
      (workspace/add-resource-listener! workspace (reify resource/ResourceListener
                                                    (handle-changes [_ changes _]
                                                      (handle-resource-changes! changes changes-view editor-tabs))))

      (app-view/restore-split-positions! stage prefs)

      (ui/on-closing! stage (fn [_]
                              (or (not (workspace/version-on-disk-outdated? workspace))
                                  (dialogs/make-confirm-dialog "Unsaved changes exists, are you sure you want to quit?"))))

      (ui/on-closed! stage (fn [_]
                             (app-view/store-window-dimensions stage prefs)
                             (app-view/store-split-positions! stage prefs)
                             (g/transact (g/delete-node project))))

      (console/setup-console! {:text   console
                               :search search-console
                               :clear  clear-console
                               :next   next-console
                               :prev   prev-console})

      (ui/restyle-tabs! tool-tabs)
      (let [context-env {:app-view          app-view
                         :project           project
                         :project-graph     (project/graph project)
                         :prefs             prefs
                         :workspace         (g/node-value project :workspace)
                         :outline-view      outline-view
                         :web-server        web-server
                         :build-errors-view build-errors-view
                         :changes-view      changes-view
                         :main-stage        stage
                         :asset-browser     asset-browser}
            dynamics {:active-resource [:app-view :active-resource]}]
        (ui/context! root :global context-env (ui/->selection-provider assets) dynamics)
        (ui/context! workbench :workbench context-env (app-view/->selection-provider app-view) dynamics))
      (g/transact
        (concat
          (for [label [:selected-node-ids-by-resource-node :selected-node-properties-by-resource-node :sub-selections-by-resource-node]]
            (g/connect project label app-view label))
          (g/connect project :_node-id app-view :project-id)
          (g/connect app-view :selected-node-ids outline-view :selection)
          (for [label [:active-resource-node :active-outline :open-resource-nodes]]
            (g/connect app-view label outline-view label))
          (let [auto-pulls [[properties-view :pane]
                            [app-view :refresh-tab-pane]
                            [outline-view :tree-view]
                            [asset-browser :tree-view]]]
            (g/update-property app-view :auto-pulls into auto-pulls))))
      (if (system/defold-dev?)
        (graph-view/setup-graph-view root)
        (.removeAll (.getTabs tool-tabs) (to-array (mapv #(find-tab tool-tabs %) ["graph-tab" "css-tab"]))))

      ; If sync was in progress when we shut down the editor we offer to resume the sync process.
      (let [git   (g/node-value changes-view :git)
            prefs (g/node-value changes-view :prefs)]
        (when (sync/flow-in-progress? git)
          (ui/run-later
            (loop []
              (if-not (dialogs/make-confirm-dialog (str "The editor was shut down while synchronizing with the server.\n"
                                                        "Resume syncing or cancel and revert to the pre-sync state?")
                                                   {:title "Resume Sync?"
                                                    :ok-label "Resume Sync"
                                                    :cancel-label "Cancel Sync"
                                                    :pref-width Region/USE_COMPUTED_SIZE})
                (sync/cancel-flow-in-progress! git)
                (if-not (login/login prefs)
                  (recur) ; Ask again. If the user refuses to log in, they must choose "Cancel Sync".
                  (let [creds (git/credentials prefs)
                        flow (sync/resume-flow git creds)]
                    (sync/open-sync-dialog flow)
                    (workspace/resource-sync! workspace)))))))))

    (reset! the-root root)
    root))

(defn- make-blocking-save-action
  "Returns a function that when called will save the specified project and block
  until the operation completes. Used as a pre-update action to ensure the
  project is saved before installing updates and restarting the editor."
  [project]
  (fn []
    ; If a build or save was in progress, wait for it to complete.
    (while (project/ongoing-build-save?)
      (Thread/sleep 300))

    ; Save the project and block until complete.
    (let [save-future (project/save-all! project nil)]
      (when (future? save-future)
        (deref save-future)))))

(defn open-project
  [^File game-project-file prefs render-progress!]
  (let [project-path (.getPath (.getParentFile game-project-file))
        workspace    (setup-workspace project-path)
        game-project-res (workspace/resolve-workspace-resource workspace "/game.project")
        project      (project/open-project! *project-graph* workspace game-project-res render-progress! (partial login/login prefs))
        ^VBox root   (ui/run-now (load-stage workspace project prefs))]
    (workspace/update-version-on-disk! *workspace-graph*)
    (g/reset-undo! *project-graph*)
    (EditorApplication/registerPreUpdateAction (make-blocking-save-action project))))
