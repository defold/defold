(ns editor.boot-open-project
  (:require [clojure.string :as string]
            [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.app-view :as app-view]
            [editor.asset-browser :as asset-browser]
            [editor.build-errors-view :as build-errors-view]
            [editor.changes-view :as changes-view]
            [editor.code.view :as code-view]
            [editor.console :as console]
            [editor.curve-view :as curve-view]
            [editor.debug-view :as debug-view]
            [editor.defold-project :as project]
            [editor.dialogs :as dialogs]
            [editor.error-reporting :as error-reporting]
            [editor.form-view :as form-view]
            [editor.git :as git]
            [editor.graph-view :as graph-view]
            [editor.hot-reload :as hot-reload]
            [editor.login :as login]
            [editor.html-view :as html-view]
            [editor.outline-view :as outline-view]
            [editor.pipeline.bob :as bob]
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
            [editor.search-results-view :as search-results-view]
            [editor.sync :as sync]
            [editor.system :as system]
            [editor.updater :as updater]
            [editor.util :as util]
            [service.log :as log]
            [util.http-server :as http-server])
  (:import [java.io File]
           [javafx.scene Node Scene]
           [javafx.stage Stage WindowEvent]
           [javafx.scene.layout Region VBox AnchorPane]
           [javafx.scene.control Label MenuBar Tab TabPane TreeView]))

(set! *warn-on-reflection* true)

(def ^:dynamic *workspace-graph*)
(def ^:dynamic *project-graph*)
(def ^:dynamic *view-graph*)

(def the-root (atom nil))

;; invoked to control the timing of when the namespaces load
(defn load-namespaces []
  (println "loaded namespaces"))

(defn initialize-project []
  (when (nil? @the-root)
    (g/initialize! {})
    (alter-var-root #'*workspace-graph* (fn [_] (g/last-graph-added)))
    (alter-var-root #'*project-graph*   (fn [_] (g/make-graph! :history true  :volatility 1)))
    (alter-var-root #'*view-graph*      (fn [_] (g/make-graph! :history false :volatility 2)))))

(defn setup-workspace [project-path build-settings]
  (let [workspace (workspace/make-workspace *workspace-graph* project-path build-settings)]
    (g/transact
      (concat
        (text/register-view-types workspace)
        (code-view/register-view-types workspace)
        (scene/register-view-types workspace)
        (form-view/register-view-types workspace)
        (html-view/register-view-types workspace)))
    (resource-types/register-resource-types! workspace)
    (workspace/resource-sync! workspace)
    workspace))

(defn- handle-application-focused! [workspace changes-view]
  (when-not (sync/sync-dialog-open?)
    (let [project-path (workspace/project-path workspace)
          dependencies (workspace/dependencies workspace)
          snapshot-cache (workspace/snapshot-cache workspace)]
      (future
        (error-reporting/catch-all!
          (let [snapshot-info (workspace/make-snapshot-info workspace project-path dependencies snapshot-cache)]
            (ui/run-later
              (workspace/update-snapshot-cache! workspace (:snapshot-cache snapshot-info))
              (let [render-fn (progress/throttle-render-progress (app-view/make-render-task-progress :resource-sync))
                    new-snapshot (:snapshot snapshot-info)
                    new-map (:map snapshot-info)]
                (ui/with-progress [render-fn render-fn]
                  (->> (workspace/resource-sync! workspace [] render-fn new-snapshot new-map)
                       (changes-view/refresh-after-resource-sync! changes-view)))))))))))

(defn- find-tab [^TabPane tabs id]
  (some #(and (= id (.getId ^Tab %)) %) (.getTabs tabs)))

(defn- handle-resource-changes! [changes changes-view editor-tabs]
  (ui/run-later
    (changes-view/refresh! changes-view)))

(defn- install-pending-update-check-timer! [^Stage stage ^Label label update-context]
  (let [update-visibility! (fn [] (ui/visible! label (let [update (updater/pending-update update-context)]
                                                       (and (some? update) (not= update (system/defold-editor-sha1))))))
        tick-fn (fn [_ _] (update-visibility!))
        timer (ui/->timer 0.1 "pending-update-check" tick-fn)]
    (update-visibility!)
    (.addEventHandler stage WindowEvent/WINDOW_SHOWN (ui/event-handler event (ui/timer-start! timer)))
    (.addEventHandler stage WindowEvent/WINDOW_HIDING (ui/event-handler event (ui/timer-stop! timer)))))

(defn- init-pending-update-indicator! [^Stage stage ^Label label project update-context]
  (.setOnMouseClicked label
                      (ui/event-handler event
                                        (when (dialogs/make-pending-update-dialog stage)
                                          (when (updater/install-pending-update! update-context (io/file (system/defold-resourcespath)))
                                            ;; Save the project and block until complete. Before saving, perform a
                                            ;; resource sync to ensure we do not overwrite external changes.
                                            (workspace/resource-sync! (project/workspace project))
                                            (project/save-all! project)
                                            (updater/restart!)))))
  (install-pending-update-check-timer! stage label update-context))

(defn- update-status-pane-contents! [^AnchorPane status-pane content-prospects]
  (let [content (first (.getChildren status-pane))
        new-content (first (filter ui/visible? content-prospects))]
    (when (not= content new-content)
      (ui/children! status-pane (when new-content [new-content]))
      (when new-content
        (ui/fill-control new-content)))))

(defn- init-status-pane-timer! [^Stage stage ^AnchorPane status-pane content-prospects]
  (let [timer (ui/->timer 5 "update-status-pane" (fn [_ _] (update-status-pane-contents! status-pane content-prospects)))]
    (update-status-pane-contents! status-pane content-prospects)
    (.addEventHandler stage WindowEvent/WINDOW_SHOWN (ui/event-handler event (ui/timer-start! timer)))
    (.addEventHandler stage WindowEvent/WINDOW_HIDING (ui/event-handler event (ui/timer-stop! timer)))))

(defn- show-tracked-internal-files-warning! []
  (dialogs/make-alert-dialog (str "It looks like internal files such as downloaded dependencies or build output were placed under source control.\n"
                                  "This can happen if a commit was made when the .gitignore file was not properly configured.\n"
                                  "\n"
                                  "To fix this, make a commit where you delete the .internal and build directories, then reopen the project.")))

(defn load-stage [workspace project prefs update-context newly-created?]
  (let [^VBox root (ui/load-fxml "editor.fxml")
        stage      (ui/make-stage)
        scene      (Scene. root)
        status-pane (.lookup root "#status-pane")
        update-available-label (doto (Label. "Update Available")
                                 (ui/visible! false)
                                 (ui/add-style! "link-label"))]

    (when update-context
      (init-pending-update-indicator! stage update-available-label project update-context))

    (init-status-pane-timer! stage status-pane
                             [app-view/progress-hbox
                              update-available-label])

    (ui/set-main-stage stage)
    (.setScene stage scene)

    (app-view/restore-window-dimensions stage prefs)
    
    (ui/show! stage)
    (targets/start)

    (let [^MenuBar menu-bar    (.lookup root "#menu-bar")
          ^Node menu-bar-space (.lookup root "#menu-bar-space")
          ^TabPane editor-tabs (.lookup root "#editor-tabs")
          ^TabPane tool-tabs   (.lookup root "#tool-tabs")
          ^TreeView outline    (.lookup root "#outline")
          ^TreeView assets     (.lookup root "#assets")
          console-tab          (first (.getTabs tool-tabs))
          console-grid-pane    (.lookup root "#console-grid-pane")
          workbench            (.lookup root "#workbench")
          app-view             (app-view/make-app-view *view-graph* workspace project stage menu-bar editor-tabs tool-tabs)
          outline-view         (outline-view/make-outline-view *view-graph* *project-graph* outline app-view)
          properties-view      (properties-view/make-properties-view workspace project app-view *view-graph* (.lookup root "#properties"))
          asset-browser        (asset-browser/make-asset-browser *view-graph* workspace assets prefs)
          web-server           (-> (http-server/->server 0 {"/profiler" web-profiler/handler
                                                            hot-reload/url-prefix (partial hot-reload/build-handler workspace project)
                                                            hot-reload/verify-etags-url-prefix (partial hot-reload/verify-etags-handler workspace project)
                                                            bob/html5-url-prefix (partial bob/html5-handler project)})
                                   http-server/start!)
          open-resource        (partial app-view/open-resource app-view prefs workspace project)
          console-view         (console/make-console! *view-graph* console-tab console-grid-pane)
          build-errors-view    (build-errors-view/make-build-errors-view (.lookup root "#build-errors-tree")
                                                                         (fn [resource selected-node-ids opts]
                                                                           (when (open-resource resource opts)
                                                                             (app-view/select! app-view selected-node-ids))))
          search-results-view  (search-results-view/make-search-results-view! *view-graph*
                                                                              (.lookup root "#search-results-container")
                                                                              open-resource)
          changes-view         (changes-view/make-changes-view *view-graph* workspace prefs
                                                               (.lookup root "#changes-container"))
          curve-view           (curve-view/make-view! app-view *view-graph*
                                                      (.lookup root "#curve-editor-container")
                                                      (.lookup root "#curve-editor-list")
                                                      (.lookup root "#curve-editor-view")
                                                      {:tab (find-tab tool-tabs "curve-editor-tab")})
          debug-view           (debug-view/make-view! app-view *view-graph*
                                                      project
                                                      root
                                                      open-resource)]
      (ui/add-application-focused-callback! :main-stage handle-application-focused! workspace changes-view)

      ;; The menu-bar-space element should only be present if the menu-bar element is not.
      (let [collapse-menu-bar? (and (util/is-mac-os?)
                                     (.isUseSystemMenuBar menu-bar))]
        (.setVisible menu-bar-space collapse-menu-bar?)
        (.setManaged menu-bar-space collapse-menu-bar?))

      (workspace/add-resource-listener! workspace (reify resource/ResourceListener
                                                    (handle-changes [_ changes _]
                                                      (handle-resource-changes! changes changes-view editor-tabs))))

      (ui/run-later
        (app-view/restore-split-positions! stage prefs))

      (ui/on-closing! stage (fn [_]
                              (let [result (or (empty? (project/dirty-save-data project))
                                             (dialogs/make-confirm-dialog "Unsaved changes exists, are you sure you want to quit?"))]
                                (when result
                                  (app-view/store-window-dimensions stage prefs)
                                  (app-view/store-split-positions! stage prefs))
                                result)))

      (ui/on-closed! stage (fn [_]
                             (ui/remove-application-focused-callback! :main-stage)
                             (g/transact (g/delete-node project))))

      (ui/restyle-tabs! tool-tabs)
      (let [context-env {:app-view            app-view
                         :project             project
                         :project-graph       (project/graph project)
                         :prefs               prefs
                         :workspace           (g/node-value project :workspace)
                         :outline-view        outline-view
                         :web-server          web-server
                         :build-errors-view   build-errors-view
                         :console-view        console-view
                         :search-results-view search-results-view
                         :changes-view        changes-view
                         :main-stage          stage
                         :asset-browser       asset-browser
                         :debug-view          debug-view}
            dynamics {:active-resource [:app-view :active-resource]}]
        (ui/context! root :global context-env (ui/->selection-provider assets) dynamics)
        (ui/context! workbench :workbench context-env (app-view/->selection-provider app-view) dynamics))
      (g/transact
        (concat
          (for [label [:selected-node-ids-by-resource-node :selected-node-properties-by-resource-node :sub-selections-by-resource-node]]
            (g/connect project label app-view label))
          (g/connect project :_node-id app-view :project-id)
          (g/connect app-view :selected-node-ids outline-view :selection)
          (g/connect app-view :active-resource asset-browser :active-resource)
          (for [label [:active-resource-node :active-outline :open-resource-nodes]]
            (g/connect app-view label outline-view label))
          (let [auto-pulls [[properties-view :pane]
                            [app-view :refresh-tab-pane]
                            [outline-view :tree-view]
                            [asset-browser :tree-view]
                            [curve-view :update-list-view]
                            [debug-view :update-available-controls]
                            [debug-view :update-call-stack]]]
            (g/update-property app-view :auto-pulls into auto-pulls))))
      (if (system/defold-dev?)
        (graph-view/setup-graph-view root)
        (.removeAll (.getTabs tool-tabs) (to-array (mapv #(find-tab tool-tabs %) ["graph-tab" "css-tab"]))))

      ;; If sync was in progress when we shut down the editor we offer to resume the sync process.
      (let [git   (g/node-value changes-view :git)
            prefs (g/node-value changes-view :prefs)]
        (if (sync/flow-in-progress? git)
          (ui/run-later
            (loop []
              (if-not (dialogs/make-confirm-dialog (str "The editor was shut down while synchronizing with the server.\n"
                                                        "Resume syncing or cancel and revert to the pre-sync state?")
                                                   {:title "Resume Sync?"
                                                    :ok-label "Resume Sync"
                                                    :cancel-label "Cancel Sync"
                                                    :pref-width Region/USE_COMPUTED_SIZE})
                ;; User chose to cancel sync.
                (do (sync/interactive-cancel! (partial sync/cancel-flow-in-progress! git))
                    (changes-view/resource-sync-after-git-change! changes-view workspace))

                ;; User chose to resume sync.
                (if-not (login/login prefs)
                  (recur) ;; Ask again. If the user refuses to log in, they must choose "Cancel Sync".
                  (let [creds (git/credentials prefs)
                        flow (sync/resume-flow git creds)]
                    (sync/open-sync-dialog flow)
                    (changes-view/resource-sync-after-git-change! changes-view workspace))))))

          ;; A sync was not in progress.
          (do
            ;; If the project was just created, we automatically open the readme resource.
            (when newly-created?
              (ui/run-later
                (when-some [readme-resource (workspace/find-resource workspace "/README.md")]
                  (open-resource readme-resource))))

            ;; Ensure .gitignore is configured to ignore build output and metadata files.
            (let [gitignore-was-modified? (git/ensure-gitignore-configured! git)
                  internal-files-are-tracked? (git/internal-files-are-tracked? git)]
              (if gitignore-was-modified?
                (do (changes-view/refresh! changes-view)
                    (ui/run-later
                      (dialogs/make-message-box "Updated .gitignore File"
                                                (str "The .gitignore file was automatically updated to ignore build output and metadata files.\n"
                                                     "You should include it along with your changes the next time you synchronize."))
                      (when internal-files-are-tracked?
                        (show-tracked-internal-files-warning!))))
                (when internal-files-are-tracked?
                  (ui/run-later
                    (show-tracked-internal-files-warning!)))))))))

    (reset! the-root root)
    root))

(defn- show-missing-dependencies-alert! [dependencies]
  (dialogs/make-alert-dialog (string/join "\n" (concat ["The following dependencies are missing:"]
                                                       (map #(str "\u00A0\u00A0\u2022\u00A0" %) ; "  * " (NO-BREAK SPACE, NO-BREAK SPACE, BULLET, NO-BREAK SPACE)
                                                            (sort-by str dependencies))
                                                       [""
                                                        "The project might not work without them. To download, connect to the internet and choose Fetch Libraries from the Project menu."]))))

(defn open-project
  [^File game-project-file prefs render-progress! update-context newly-created?]
  (let [project-path (.getPath (.getParentFile game-project-file))
        build-settings (workspace/make-build-settings prefs)
        workspace    (setup-workspace project-path build-settings)
        game-project-res (workspace/resolve-workspace-resource workspace "/game.project")
        project      (project/open-project! *project-graph* workspace game-project-res render-progress! (partial login/login prefs))]
    (ui/run-now
      (load-stage workspace project prefs update-context newly-created?)
      (when-let [missing-dependencies (not-empty (workspace/missing-dependencies workspace))]
        (show-missing-dependencies-alert! missing-dependencies)))
    (g/reset-undo! *project-graph*)
    (log/info :message "project loaded")))
