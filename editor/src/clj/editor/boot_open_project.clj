(ns editor.boot-open-project
  (:require [dynamo.graph :as g]
            [editor.app-view :as app-view]
            [editor.asset-browser :as asset-browser]
            [editor.atlas :as atlas]
            [editor.build-errors-view :as build-errors-view]
            [editor.camera-editor :as camera]
            [editor.changes-view :as changes-view]
            [editor.code-view :as code-view]
            [editor.collada-scene :as collada-scene]
            [editor.collection :as collection]
            [editor.collection-proxy :as collection-proxy]
            [editor.collision-object :as collision-object]
            [editor.console :as console]
            [editor.core :as core]
            [editor.cubemap :as cubemap]
            [editor.curve-view :as curve-view]
            [editor.defold-project :as project]
            [editor.dialogs :as dialogs]
            [editor.display-profiles :as display-profiles]
            [editor.factory :as factory]
            [editor.font :as font]
            [editor.form-view :as form-view]
            [editor.game-object :as game-object]
            [editor.game-project :as game-project]
            [editor.gl.shader :as shader]
            [editor.graph-view :as graph-view]
            [editor.gui :as gui]
            [editor.hot-reload :as hot-reload]
            [editor.image :as image]
            [editor.json :as json]
            [editor.label :as label]
            [editor.login :as login]
            [editor.material :as material]
            [editor.model :as model]
            [editor.outline-view :as outline-view]
            [editor.particlefx :as particlefx]
            [editor.prefs :as prefs]
            [editor.progress :as progress]
            [editor.properties-view :as properties-view]
            [editor.protobuf-types :as protobuf-types]
            [editor.resource :as resource]
            [editor.rig :as rig]
            [editor.scene :as scene]
            [editor.script :as script]
            [editor.sound :as sound]
            [editor.spine :as spine]
            [editor.sprite :as sprite]
            [editor.targets :as targets]
            [editor.text :as text]
            [editor.tile-map :as tile-map]
            [editor.tile-source :as tile-source]
            [editor.ui :as ui]
            [editor.web-profiler :as web-profiler]
            [editor.workspace :as workspace]
            [editor.sync :as sync]
            [editor.system :as system]
            [util.http-server :as http-server])
  (:import  [java.io File]
            [javafx.scene.layout VBox]
            [javafx.scene Scene]
            [javafx.stage Stage]
            [javafx.scene.control Button TextArea SplitPane TabPane Tab]))

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
    (g/transact
      (concat
        (atlas/register-resource-types workspace)
        (camera/register-resource-types workspace)
        (collada-scene/register-resource-types workspace)
        (collection/register-resource-types workspace)
        (collection-proxy/register-resource-types workspace)
        (collision-object/register-resource-types workspace)
        (cubemap/register-resource-types workspace)
        (display-profiles/register-resource-types workspace)
        (factory/register-resource-types workspace)
        (font/register-resource-types workspace)
        (game-object/register-resource-types workspace)
        (game-project/register-resource-types workspace)
        (gui/register-resource-types workspace)
        (image/register-resource-types workspace)
        (json/register-resource-types workspace)
        (label/register-resource-types workspace)
        (material/register-resource-types workspace)
        (model/register-resource-types workspace)
        (particlefx/register-resource-types workspace)
        (protobuf-types/register-resource-types workspace)
        (rig/register-resource-types workspace)
        (script/register-resource-types workspace)
        (shader/register-resource-types workspace)
        (sound/register-resource-types workspace)
        (spine/register-resource-types workspace)
        (sprite/register-resource-types workspace)
        (tile-map/register-resource-types workspace)
        (tile-source/register-resource-types workspace)))
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
                     (future
                       (ui/with-disabled-ui
                         (ui/with-progress [render-fn ui/default-render-progress!]
                           (editor.workspace/resource-sync! workspace true [] render-fn))))))))))

(defn- find-tab [^TabPane tabs id]
  (some #(and (= id (.getId ^Tab %)) %) (.getTabs tabs)))

(defn- handle-resource-changes! [changes changes-view editor-tabs]
  (ui/run-later
    (changes-view/refresh! changes-view)
    (doseq [resource (:removed changes)]
      (app-view/remove-resource-tab editor-tabs resource))))

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
          app-view             (app-view/make-app-view *view-graph* *project-graph* project stage menu-bar editor-tabs prefs)
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
          (for [label [:selected-node-ids-by-resource :selected-node-properties-by-resource :sub-selections-by-resource]]
            (g/connect project label app-view label))
          (g/connect project :_node-id app-view :project-id)
          (g/connect app-view :selected-node-ids outline-view :selection)
          (for [label [:active-resource :active-outline :open-resources]]
            (g/connect app-view label outline-view label))
          (for [view [outline-view asset-browser]]
            (g/update-property app-view :auto-pulls conj [view :tree-view]))
          (g/update-property app-view :auto-pulls conj [properties-view :pane])))
      (if (system/defold-dev?)
        (graph-view/setup-graph-view root)
        (.removeAll (.getTabs tool-tabs) (to-array (mapv #(find-tab tool-tabs %) ["graph-tab" "css-tab"]))))

      ; If project sync was in progress when we shut down the editor, re-open the sync dialog at startup.
      (let [git   (g/node-value changes-view :git)
            prefs (g/node-value changes-view :prefs)]
        (when (sync/flow-in-progress? git)
          (ui/run-later
            (sync/open-sync-dialog (sync/resume-flow git prefs))
            (workspace/resource-sync! workspace)))))

    (reset! the-root root)
    root))

(defn open-project
  [^File game-project-file prefs render-progress!]
  (let [project-path (.getPath (.getParentFile game-project-file))
        workspace    (setup-workspace project-path)
        game-project-res (workspace/resolve-workspace-resource workspace "/game.project")
        project      (project/open-project! *project-graph* workspace game-project-res render-progress! (partial login/login prefs))
        ^VBox root   (ui/run-now (load-stage workspace project prefs))]
    (workspace/update-version-on-disk! *workspace-graph*)
    (g/reset-undo! *project-graph*)))
