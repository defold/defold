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
  (:require [dynamo.graph :as g]
            [editor.app-view :as app-view]
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
            [editor.fxui :as fxui]
            [editor.git :as git]
            [editor.graph-view :as graph-view]
            [editor.hot-reload :as hot-reload]
            [editor.html-view :as html-view]
            [editor.icons :as icons]
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
            [editor.system :as system]
            [editor.targets :as targets]
            [editor.ui :as ui]
            [editor.ui.updater :as ui.updater]
            [editor.web-profiler :as web-profiler]
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

;; invoked to control the timing of when the namespaces load
(defn load-namespaces []
  (println "loaded namespaces"))

(defn initialize-systems! [prefs]
  (code-view/initialize! prefs))

(defn initialize-project! [system-config]
  (when (nil? @the-root)
    (g/initialize! (assoc system-config :cache-retain? project/cache-retain?))
    (alter-var-root #'*workspace-graph* (fn [_] (g/last-graph-added)))
    (alter-var-root #'*project-graph*   (fn [_] (g/make-graph! :history true  :volatility 1)))
    (alter-var-root #'*view-graph*      (fn [_] (g/make-graph! :history false :volatility 2)))))

(defn- setup-workspace! [project-path build-settings workspace-config]
  (let [workspace (workspace/make-workspace *workspace-graph* project-path build-settings workspace-config)]
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

(defn- init-pending-update-indicator! [^Stage stage link project changes-view updater]
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
                                     (ui.updater/install-and-restart! stage updater)
                                     (do (ui/enable-ui!)
                                         (changes-view/refresh! changes-view))))))]
    (ui.updater/init! stage link updater install-and-restart! render-download-progress!)))

(defn- show-tracked-internal-files-warning! []
  (dialogs/make-info-dialog
    {:title "Internal Files Under Version Control"
     :size :large
     :icon :icon/triangle-error
     :header "Internal files were placed under version control"
     :content {:pref-row-count 6
               :wrap-text true
               :text (str "It looks like internal files such as downloaded dependencies or build output were placed under version control.\n"
                          "This can happen if a commit was made when the .gitignore file was not properly configured.\n"
                          "\n"
                          "To fix this, make a commit where you delete the .internal and build directories, then reopen the project.")}}))

(def ^:private interaction-event-types
  #{DragEvent/DRAG_DONE
    KeyEvent/KEY_PRESSED
    KeyEvent/KEY_RELEASED
    MouseEvent/MOUSE_PRESSED
    MouseEvent/MOUSE_RELEASED})

(defn- load-stage! [workspace project prefs updater newly-created?]
  (let [^StackPane root (ui/load-fxml "editor.fxml")
        stage (ui/make-stage)
        scene (Scene. root)]

    (ui/set-main-stage stage)
    (.setScene stage scene)

    (.setFullScreenExitKeyCombination stage KeyCombination/NO_MATCH)
    (app-view/restore-window-dimensions stage prefs)
    
    (ui/show! stage)
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
          app-view             (app-view/make-app-view *view-graph* project stage menu-bar editor-tabs-split tool-tabs prefs)
          outline-view         (outline-view/make-outline-view *view-graph* project outline app-view)
          asset-browser        (asset-browser/make-asset-browser *view-graph* workspace assets prefs)
          open-resource        (partial #'app-view/open-resource app-view prefs workspace project)
          console-view         (console/make-console! *view-graph* workspace console-tab console-grid-pane open-resource prefs)
          color-dropper-view   (color-dropper/make-color-dropper! *view-graph*)
          _                    (notifications-view/init! (g/node-value workspace :notifications) notifications)
          build-errors-view    (build-errors-view/make-build-errors-view (.lookup root "#build-errors-tree")
                                                                         (fn [resource selected-node-ids opts]
                                                                           (when (open-resource resource opts)
                                                                             (app-view/select! app-view selected-node-ids))))
          search-results-view  (search-results-view/make-search-results-view! *view-graph*
                                                                              (.lookup root "#search-results-container")
                                                                              open-resource)
          properties-view      (properties-view/make-properties-view workspace project app-view search-results-view *view-graph* color-dropper-view prefs (.lookup root "#properties"))
          changes-view         (changes-view/make-changes-view *view-graph* workspace prefs (.lookup root "#changes-container")
                                                               (fn [changes-view moved-files]
                                                                 (app-view/async-reload! app-view changes-view workspace moved-files)))
          curve-view           (curve-view/make-view! app-view *view-graph*
                                                      (.lookup root "#curve-editor-container")
                                                      (.lookup root "#curve-editor-list")
                                                      (.lookup root "#curve-editor-view")
                                                      {:tab (find-tab tool-tabs "curve-editor-tab")})
          debug-view           (debug-view/make-view! app-view *view-graph*
                                                      project
                                                      root
                                                      open-resource
                                                      (partial app-view/debugger-state-changed! scene tool-tabs))
          web-server (http-server/start!
                       (web-server/make-dynamic-handler
                         (into []
                               cat
                               [(engine-profiler/routes)
                                (web-profiler/routes)
                                (console/routes console-view)
                                (hot-reload/routes workspace)
                                (bob/routes project)
                                (command-requests/router root (app-view/make-render-task-progress :resource-sync))])))]
      (.addEventFilter ^StackPane (.lookup root "#overlay") MouseEvent/ANY ui/ignore-event-filter)
      (ui/add-application-focused-callback! :main-stage app-view/handle-application-focused! app-view changes-view workspace prefs)
      (app-view/reload-extensions! app-view project :all workspace changes-view build-errors-view prefs web-server)

      (when updater
        (let [update-link (.lookup root "#update-link")]
          (init-pending-update-indicator! stage update-link project changes-view updater)))

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
                              (let [result (or (empty? (project/dirty-save-data project))
                                               (dialogs/make-confirmation-dialog
                                                 {:title "Quit Defold?"
                                                  :icon :icon/circle-question
                                                  :size :large
                                                  :header "Unsaved changes exist, are you sure you want to quit?"
                                                  :buttons [{:text "Cancel and Keep Working"
                                                             :default-button true
                                                             :cancel-button true
                                                             :result false}
                                                            {:text "Quit Without Saving"
                                                             :variant :danger
                                                             :result true}]}))]
                                (when result
                                  (app-view/store-window-dimensions stage prefs)
                                  (app-view/store-split-positions! scene prefs)
                                  (app-view/store-hidden-panes! scene prefs))
                                result)))

      (ui/on-closed! stage (fn [_]
                             (http-server/stop! web-server)
                             (ui/remove-application-focused-callback! :main-stage)

                             ;; TODO: This takes a long time in large projects.
                             ;; Disabled for now since we don't really need to
                             ;; delete the project node until we support project
                             ;; switching.
                             #_(g/transact (g/delete-node project))))

      (let [context-env {:app-view            app-view
                         :project             project
                         :project-graph       (project/graph project)
                         :prefs               prefs
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
      (if (system/defold-dev?)
        (graph-view/setup-graph-view root)
        (.removeAll (.getTabs tool-tabs) (to-array (mapv #(find-tab tool-tabs %) ["graph-tab" "css-tab"]))))

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
                    {:title "Updated .gitignore File"
                     :icon :icon/circle-info
                     :header "Updated .gitignore file"
                     :content {:fx/type fxui/legacy-label
                               :style-class "dialog-content-padding"
                               :text (str "The .gitignore file was automatically updated to ignore build output and metadata files.\n"
                                          "You should include it along with your changes the next time you synchronize.")}})
                  (when internal-files-are-tracked?
                    (show-tracked-internal-files-warning!))))
            (when internal-files-are-tracked?
              (ui/run-later
                (show-tracked-internal-files-warning!)))))))

    (reset! the-root root)
    (ui/run-later (slog/smoke-log "stage-loaded"))
    root))

(defn open-project!
  [^File game-project-file prefs render-progress! updater newly-created?]
  (let [project-path (.getPath (.getParentFile (.getAbsoluteFile game-project-file)))
        build-settings (workspace/make-build-settings prefs)
        workspace-config (shared-editor-settings/load-project-workspace-config project-path)
        workspace (setup-workspace! project-path build-settings workspace-config)
        game-project-res (workspace/resolve-workspace-resource workspace "/game.project")
        extensions (extensions/make *project-graph*)
        project (project/open-project! *project-graph* extensions workspace game-project-res render-progress!)]
    (ui/run-now
      (icons/initialize! workspace)
      (load-stage! workspace project prefs updater newly-created?))
    (g/reset-undo! *project-graph*)
    (log/info :message "project loaded")))
