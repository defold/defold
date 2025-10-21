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

(ns editor.boot-open-project
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.app-view :as app-view]
            [editor.prefs :as prefs]
            [editor.asset-browser :as asset-browser]
            [editor.build-errors-view :as build-errors-view]
            [editor.changes-view :as changes-view]
            [editor.cljfx-form-view :as cljfx-form-view]
            [editor.code.view :as code-view]
            [editor.color-dropper :as color-dropper]
            [editor.command-requests :as command-requests]
            [editor.console :as console]
            [editor.curve-view :as curve-view]
            [editor.debug-view :as debug-view]
            [editor.defold-project :as project]
            [editor.dialogs :as dialogs]
            [editor.disk :as disk]
            [editor.editor-extensions :as extensions]
            [editor.engine-profiler :as engine-profiler]
            [editor.git :as git]
            [editor.hot-reload :as hot-reload]
            [editor.html-view :as html-view]
            [editor.icons :as icons]
            [editor.localization :as localization]
            [editor.notifications :as notifications]
            [editor.notifications-view :as notifications-view]
            [editor.os :as os]
            [editor.outline-view :as outline-view]
            [editor.pipeline.bob :as bob]
            [editor.properties-view :as properties-view]
            [editor.resource :as resource]
            [editor.resource-types :as resource-types]
            [editor.scene :as scene]
            [editor.scene-visibility :as scene-visibility]
            [editor.search-results-view :as search-results-view]
            [editor.shared-editor-settings :as shared-editor-settings]
            [editor.targets :as targets]
            [editor.ui :as ui]
            [editor.ui.updater :as ui.updater]
            [editor.web-server :as web-server]
            [editor.workspace :as workspace]
            [service.log :as log]
            [service.smoke-log :as slog]
            [util.http-server :as http-server])
  (:import [java.io File]
           [javafx.scene Node Scene]
           [javafx.scene.control MenuBar SplitPane Tab TabPane TreeView]
           [javafx.scene.input DragEvent InputEvent KeyCombination KeyEvent MouseEvent]
           [javafx.scene.layout StackPane]
           [javafx.stage Stage]))

(set! *warn-on-reflection* true)

(def ^:dynamic *workspace-graph*)
(def ^:dynamic *project-graph*)
(def ^:dynamic *view-graph*)

(def the-root (atom nil))

(defn initialize-systems! [prefs]
  (code-view/initialize! prefs))

(defn initialize-project! [system-config]
  (when (nil? @the-root)
    (g/initialize! (assoc system-config :cache-retain? project/cache-retain?))
    (alter-var-root #'*workspace-graph* (fn [_] (g/last-graph-added)))
    (alter-var-root #'*project-graph*   (fn [_] (g/make-graph! :history true  :volatility 1)))
    (alter-var-root #'*view-graph*      (fn [_] (g/make-graph! :history false :volatility 2)))))

(defn- setup-workspace! [project-path build-settings workspace-config localization]
  (let [workspace (workspace/make-workspace *workspace-graph* project-path build-settings workspace-config localization)]
    (g/transact
      (concat
        (code-view/register-view-types workspace)
        (scene/register-view-types workspace)
        (cljfx-form-view/register-view-types workspace)
        (html-view/register-view-types workspace)))

    (workspace/clean-editor-plugins! workspace)
    (resource-types/register-resource-types! workspace)
    (workspace/resource-sync! workspace)
    (workspace/load-build-cache! workspace)
    workspace))

(defn- find-tab [^TabPane tabs id]
  (some #(and (= id (.getId ^Tab %)) %) (.getTabs tabs)))

(defn- handle-resource-changes! [app-scene tab-panes open-views changes-view]
  (ui/user-data! app-scene ::ui/refresh-requested? true)
  (app-view/remove-invalid-tabs! tab-panes open-views)
  (changes-view/refresh! changes-view))

(defn- persist-window-state!
  [^Stage stage ^Scene scene prefs]
  (app-view/store-window-dimensions stage prefs)
  (app-view/store-split-positions! scene prefs)
  (app-view/store-hidden-panes! scene prefs))

(defn- init-pending-update-indicator! [^Stage stage link project changes-view updater localization]
  (let [render-reload-progress! (app-view/make-render-task-progress :resource-sync)
        render-save-progress! (app-view/make-render-task-progress :save-all)
        render-download-progress! (app-view/make-render-task-progress :download-update)
        install-and-restart! (fn install-and-restart! []
                               (ui/disable-ui!)
                               (disk/async-save!
                                 render-reload-progress!
                                 render-save-progress!
                                 project/dirty-save-data
                                 project
                                 nil ; Use nil for changes-view to skip refresh.
                                 (fn [successful?]
                                   (if successful?
                                     (ui.updater/install-and-restart! stage updater localization)
                                     (do (ui/enable-ui!)
                                         (changes-view/refresh! changes-view))))))]
    (ui.updater/init! stage link updater install-and-restart! render-download-progress! localization)))

(defn- show-tracked-internal-files-warning! [localization]
  (dialogs/make-info-dialog
    localization
    {:title (localization/message "dialog.tracked-internal-files.title")
     :size :large
     :icon :icon/triangle-error
     :header (localization/message "dialog.tracked-internal-files.header")
     :content {:pref-row-count 6
               :wrap-text true
               :text (localization/message "dialog.tracked-internal-files.content")}}))

(def ^:private interaction-event-types
  #{DragEvent/DRAG_DONE
    KeyEvent/KEY_PRESSED
    KeyEvent/KEY_RELEASED
    MouseEvent/MOUSE_PRESSED
    MouseEvent/MOUSE_RELEASED})

(defn- load-stage! [workspace project prefs localization project-path cli-options updater newly-created?]
  (let [^StackPane root (ui/load-fxml "editor.fxml")
        stage (ui/make-stage)
        scene (Scene. root)]

    (ui/set-main-stage stage)
    (.setScene stage scene)

    (.setFullScreenExitKeyCombination stage KeyCombination/NO_MATCH)
    (app-view/restore-window-dimensions stage prefs)
    
    (ui/show! stage localization)
    (targets/start)

    (let [^MenuBar menu-bar    (.lookup root "#menu-bar")
          ^Node menu-bar-space (.lookup root "#menu-bar-space")
          editor-tabs-split    (.lookup root "#editor-tabs-split")
          ^TabPane tool-tabs   (.lookup root "#tool-tabs")
          ^TreeView outline    (.lookup root "#outline")
          ^TreeView assets     (.lookup root "#assets")
          console-tab          (first (.getTabs tool-tabs))
          console-grid-pane    (.lookup root "#console-grid-pane")
          workbench            (.lookup root "#workbench")
          notifications        (.lookup root "#notifications")
          scene-visibility     (scene-visibility/make-scene-visibility-node! *view-graph*)
          app-view             (app-view/make-app-view *view-graph* project stage menu-bar editor-tabs-split tool-tabs prefs localization)
          outline-view         (outline-view/make-outline-view *view-graph* project outline app-view localization)
          asset-browser        (asset-browser/make-asset-browser *view-graph* workspace assets prefs localization)
          open-resource        (partial #'app-view/open-resource app-view prefs localization workspace project)
          console-view         (console/make-console! *view-graph* workspace console-tab console-grid-pane open-resource prefs localization)
          color-dropper-view   (color-dropper/make-color-dropper! *view-graph*)
          _                    (notifications-view/init! (g/node-value workspace :notifications) notifications localization)
          build-errors-view    (build-errors-view/make-build-errors-view (.lookup root "#build-errors-tree")
                                                                         (fn [resource selected-node-ids opts]
                                                                           (when (open-resource resource opts)
                                                                             (app-view/select! app-view selected-node-ids))))
          search-results-view  (search-results-view/make-search-results-view! *view-graph*
                                                                              (.lookup root "#search-results-container")
                                                                              open-resource)
          properties-view      (properties-view/make-properties-view workspace project app-view search-results-view *view-graph* color-dropper-view prefs (.lookup root "#properties"))
          changes-view         (changes-view/make-changes-view *view-graph* workspace prefs localization (.lookup root "#changes-container")
                                                               (fn [changes-view moved-files]
                                                                 (app-view/async-reload! app-view changes-view workspace moved-files)))
          curve-tab            (find-tab tool-tabs "curve-editor-tab")
          curve-view           (curve-view/make-view! app-view *view-graph*
                                                      (.lookup root "#curve-editor-container")
                                                      (.lookup root "#curve-editor-list")
                                                      (.lookup root "#curve-editor-view")
                                                      localization
                                                      {:tab curve-tab})
          debug-view           (debug-view/make-view! app-view *view-graph*
                                                      project
                                                      root
                                                      open-resource
                                                      (partial app-view/debugger-state-changed! scene tool-tabs)
                                                      localization)
          server-handler (web-server/make-dynamic-handler
                           (into []
                                 cat
                                 [(engine-profiler/routes)
                                  (console/routes console-view)
                                  (hot-reload/routes workspace)
                                  (bob/routes project)
                                  (command-requests/router root (app-view/make-render-task-progress :resource-sync))]))
          server-port (:port cli-options)
          web-server (try
                       (http-server/start! server-handler :port server-port)
                       (catch Exception e
                         (let [server (http-server/start! server-handler)]
                           (notifications/show!
                             (workspace/notifications workspace)
                             {:type :warning
                              :message (localization/message
                                         "notification.web-server.port-fallback.warning"
                                         {"requested" server-port
                                          "reason" (.getMessage e)
                                          "fallback" (http-server/port server)})})
                           server)))
          port-file-content (str (http-server/port web-server))
          port-file (doto (io/file project-path ".internal" "editor.port")
                      (io/make-parents)
                      (spit port-file-content))]
      (localization/localize! (.lookup root "#assets-pane") localization (localization/message "pane.assets"))
      (localization/localize! (.lookup root "#changed-files-titled-pane") localization (localization/message "pane.changed-files"))
      (localization/localize! (.lookup root "#outline-pane") localization (localization/message "pane.outline"))
      (localization/localize! (.lookup root "#properties-pane") localization (localization/message "pane.properties"))
      (localization/localize! (.lookup root "#status-label") localization (localization/message "progress.ready"))
      (localization/localize! console-tab localization (localization/message "pane.console"))
      (localization/localize! curve-tab localization (localization/message "pane.curve-editor"))
      (localization/localize! (find-tab tool-tabs "build-errors-tab") localization (localization/message "pane.build-errors"))
      (localization/localize! (find-tab tool-tabs "search-results-tab") localization (localization/message "pane.search-results"))

      (.addShutdownHook
        (Runtime/getRuntime)
        (Thread.
          (fn []
            ;; Content might change if another editor is open in the same project
            ;; In that case, we let the other instance to clean up the file
            (when (and (.exists port-file) (= port-file-content (slurp port-file)))
              (.delete port-file)))))
      (.addEventFilter ^StackPane (.lookup root "#overlay") MouseEvent/ANY ui/ignore-event-filter)
      (ui/add-application-focused-callback! :main-stage app-view/handle-application-focused! app-view changes-view workspace prefs)
      (ui/add-application-unfocused-callback! :main-stage-unfocused app-view/handle-application-unfocused! app-view changes-view project prefs)
      (app-view/reload-extensions! app-view project :all workspace changes-view build-errors-view prefs localization web-server)

      (when updater
        (let [update-link (.lookup root "#update-link")]
          (init-pending-update-indicator! stage update-link project changes-view updater localization)))

      ;; The menu-bar-space element should only be present if the menu-bar element is not.
      (let [collapse-menu-bar? (and (os/is-mac-os?)
                                    (.isUseSystemMenuBar menu-bar))]
        (.setVisible menu-bar-space collapse-menu-bar?)
        (.setManaged menu-bar-space collapse-menu-bar?))

      (workspace/add-resource-listener! workspace 0
                                        (reify resource/ResourceListener
                                          (handle-changes [_ _ _]
                                            (let [open-views (g/node-value app-view :open-views)
                                                  panes (.getItems ^SplitPane editor-tabs-split)]
                                              (handle-resource-changes! scene panes open-views changes-view)))))

      (.addEventFilter scene
                       InputEvent/ANY
                       (ui/event-handler e
                         (when (contains? interaction-event-types (.getEventType ^InputEvent e))
                           (ui/user-data! scene ::ui/refresh-requested? true))))

      (ui/observe (.focusedProperty stage)
                  (fn [_ _ focused]
                    (when focused
                      (ui/user-data! scene ::ui/refresh-requested? true))))

      (ui/user-data! scene ::ui/refresh-requested? true)

      (ui/run-later
        (app-view/restore-split-positions! scene prefs)
        (app-view/restore-hidden-panes! scene prefs))

      (ui/on-closing! stage (fn [_]
                              (let [dirty-save-data (project/dirty-save-data project)
                                    auto-save-on-quit? (prefs/get prefs [:workflow :save-on-app-focus-lost])]
                                (if (and auto-save-on-quit? (seq dirty-save-data))
                                  (do
                                    ;; Auto-save is enabled: save changes and then close without prompting.
                                    (let [render-reload-progress! (app-view/make-render-task-progress :resource-sync)
                                          render-save-progress! (app-view/make-render-task-progress :save-all)]
                                      (ui/disable-ui!)
                                      (disk/async-save!
                                        render-reload-progress!
                                        render-save-progress!
                                        project/dirty-save-data
                                        project
                                        changes-view
                                        (fn [successful?]
                                          (if successful?
                                            (do
                                              (persist-window-state! stage scene prefs)
                                              (ui/close! stage))
                                            (ui/enable-ui!)))))
                                    false)
                                  (let [result (or (empty? dirty-save-data)
                                                   (dialogs/make-confirmation-dialog
                                                     localization
                                                     {:title (localization/message "dialog.quit-defold.title")
                                                      :icon :icon/circle-question
                                                      :size :large
                                                      :header (localization/message "dialog.quit-defold.header")
                                                      :buttons [{:text (localization/message "dialog.quit-defold.button.cancel")
                                                                 :default-button true
                                                                 :cancel-button true
                                                                 :result false}
                                                                {:text (localization/message "dialog.quit-defold.button.quit")
                                                                 :variant :danger
                                                                 :result true}]}))]
                                    (when result
                                      (persist-window-state! stage scene prefs))
                                    result)))))

      (ui/on-closed! stage (fn [_]
                             (http-server/stop! web-server)
                             (ui/remove-application-focused-callback! :main-stage)
                             (ui/remove-application-unfocused-callback! :main-stage-unfocused)

                             ;; TODO: This takes a long time in large projects.
                             ;; Disabled for now since we don't really need to
                             ;; delete the project node until we support project
                             ;; switching.
                             #_(g/transact (g/delete-node project))))

      (let [context-env {:app-view            app-view
                         :project             project
                         :project-graph       (project/graph project)
                         :prefs               prefs
                         :localization        localization
                         :workspace           (g/node-value project :workspace)
                         :outline-view        outline-view
                         :web-server          web-server
                         :build-errors-view   build-errors-view
                         :console-view        console-view
                         :scene-visibility    scene-visibility
                         :search-results-view search-results-view
                         :changes-view        changes-view
                         :main-stage          stage
                         :asset-browser       asset-browser
                         :debug-view          debug-view
                         :tool-tab-pane       tool-tabs}
            dynamics {:active-resource [:app-view :active-resource]}]
        (ui/context! root :global context-env (ui/->selection-provider assets) dynamics)
        (ui/context! workbench :workbench context-env (app-view/->selection-provider app-view) dynamics))
      (g/transact
        (concat
          (for [label [:selected-node-ids-by-resource-node :selected-node-properties-by-resource-node :sub-selections-by-resource-node]]
            (g/connect project label app-view label))
          (g/connect project :_node-id app-view :project-id)
          (g/connect app-view :selected-node-ids outline-view :selection)
          (g/connect app-view :hidden-node-outline-key-paths outline-view :hidden-node-outline-key-paths)
          (g/connect app-view :active-resource asset-browser :active-resource)
          (g/connect app-view :active-resource-node+type scene-visibility :active-resource-node+type)
          (g/connect app-view :active-scene scene-visibility :active-scene)
          (g/connect outline-view :tree-selection scene-visibility :outline-selection)
          (g/connect scene-visibility :hidden-renderable-tags app-view :hidden-renderable-tags)
          (g/connect scene-visibility :outline-name-paths outline-view :outline-name-paths)
          (g/connect scene-visibility :hidden-node-outline-key-paths app-view :hidden-node-outline-key-paths)
          (for [label [:active-resource-node :active-outline :open-resource-nodes]]
            (g/connect app-view label outline-view label))
          (let [auto-pulls [[properties-view :pane]
                            [app-view :refresh-tab-panes]
                            [outline-view :tree-view]
                            [asset-browser :tree-view]
                            [curve-view :update-list-view]
                            [debug-view :update-available-controls]
                            [debug-view :update-call-stack]]]
            (g/update-property app-view :auto-pulls into auto-pulls))))

      ;; If sync was in progress when we shut down the editor we offer to resume the sync process.
      (let [git (g/node-value changes-view :git)]
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
                  (dialogs/make-info-dialog
                    localization
                    {:title (localization/message "dialog.gitignore-updated.title")
                     :icon :icon/circle-info
                     :header (localization/message "dialog.gitignore-updated.header")
                     :content {:wrap-text true
                               :text (localization/message "dialog.gitignore-updated.content")}})
                  (when internal-files-are-tracked?
                    (show-tracked-internal-files-warning! localization))))
            (when internal-files-are-tracked?
              (ui/run-later
                (show-tracked-internal-files-warning! localization)))))))

    (reset! the-root root)
    (ui/run-later (slog/smoke-log "stage-loaded"))
    root))

(defn open-project!
  [^File game-project-file prefs localization cli-options render-progress! updater newly-created?]
  (let [project-path (.getPath (.getParentFile (.getAbsoluteFile game-project-file)))
        build-settings (workspace/make-build-settings prefs)
        workspace-config (shared-editor-settings/load-project-workspace-config project-path localization)
        workspace (setup-workspace! project-path build-settings workspace-config localization)
        game-project-res (workspace/resolve-workspace-resource workspace "/game.project")
        extensions (extensions/make *project-graph*)
        project (project/open-project! *project-graph* extensions workspace game-project-res render-progress!)]
    (ui/run-now
      (icons/initialize! workspace)
      (load-stage! workspace project prefs localization project-path cli-options updater newly-created?))
    (g/reset-undo! *project-graph*)
    (log/info :message "project loaded")))
