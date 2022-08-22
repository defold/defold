;; Copyright 2020-2022 The Defold Foundation
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

(ns editor.app-view
  (:require [cljfx.fx.hyperlink :as fx.hyperlink]
            [cljfx.fx.image-view :as fx.image-view]
            [cljfx.fx.text :as fx.text]
            [cljfx.fx.text-flow :as fx.text-flow]
            [cljfx.fx.tooltip :as fx.tooltip]
            [cljfx.fx.v-box :as fx.v-box]
            [clojure.java.io :as io]
            [clojure.edn :as edn]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.build :as build]
            [editor.build-errors-view :as build-errors-view]
            [editor.bundle-dialog :as bundle-dialog]
            [editor.changes-view :as changes-view]
            [editor.code.data :refer [CursorRange->line-number]]
            [editor.console :as console]
            [editor.debug-view :as debug-view]
            [editor.defold-project :as project]
            [editor.dialogs :as dialogs]
            [editor.disk :as disk]
            [editor.disk-availability :as disk-availability]
            [editor.editor-extensions :as extensions]
            [editor.engine :as engine]
            [editor.engine.build-errors :as engine-build-errors]
            [editor.error-reporting :as error-reporting]
            [editor.fs :as fs]
            [editor.fxui :as fxui]
            [editor.game-project :as game-project]
            [editor.github :as github]
            [editor.graph-util :as gu]
            [editor.handler :as handler]
            [editor.hot-reload :as hot-reload]
            [editor.icons :as icons]
            [editor.keymap :as keymap]
            [editor.live-update-settings :as live-update-settings]
            [editor.lua :as lua]
            [editor.pipeline :as pipeline]
            [editor.pipeline.bob :as bob]
            [editor.placeholder-resource :as placeholder-resource]
            [editor.prefs :as prefs]
            [editor.prefs-dialog :as prefs-dialog]
            [editor.process :as process]
            [editor.progress :as progress]
            [editor.resource :as resource]
            [editor.resource-dialog :as resource-dialog]
            [editor.resource-node :as resource-node]
            [editor.scene :as scene]
            [editor.scene-cache :as scene-cache]
            [editor.scene-visibility :as scene-visibility]
            [editor.search-results-view :as search-results-view]
            [editor.shared-editor-settings :as shared-editor-settings]
            [editor.system :as system]
            [editor.targets :as targets]
            [editor.types :as types]
            [editor.ui :as ui]
            [editor.url :as url]
            [editor.view :as view]
            [editor.workspace :as workspace]
            [internal.util :refer [first-where]]
            [service.log :as log]
            [util.http-server :as http-server]
            [util.profiler :as profiler]
            [util.thread-util :as thread-util]
            [service.smoke-log :as slog])
  (:import [com.defold.editor Editor]
           [com.defold.editor UIUtil]
           [com.sun.javafx.scene NodeHelper]
           [java.io BufferedReader File IOException]
           [java.net URL]
           [java.util Collection List]
           [javafx.beans.value ChangeListener]
           [javafx.collections ListChangeListener ObservableList]
           [javafx.event Event]
           [javafx.geometry Orientation]
           [javafx.scene Parent Scene]
           [javafx.scene.control MenuBar SplitPane Tab TabPane TabPane$TabClosingPolicy TabPane$TabDragPolicy Tooltip]
           [javafx.scene.image Image ImageView]
           [javafx.scene.input Clipboard ClipboardContent]
           [javafx.scene.layout AnchorPane StackPane]
           [javafx.scene.shape Ellipse SVGPath]
           [javafx.stage Screen Stage WindowEvent]))

(set! *warn-on-reflection* true)

(def ^:private split-info-by-pane-kw
  {:left {:index 0
          :pane-id "left-pane"
          :split-id "main-split"}
   :right {:index 1
           :pane-id "right-pane"
           :split-id "workbench-split"}
   :bottom {:index 1
            :pane-id "bottom-pane"
            :split-id "center-split"}})

(defn- pane-visible? [^Scene main-scene pane-kw]
  (let [{:keys [pane-id split-id]} (split-info-by-pane-kw pane-kw)]
    (some? (.lookup main-scene (str "#" split-id " #" pane-id)))))

(defn- split-pane-length
  ^double [^SplitPane split-pane]
  (condp = (.getOrientation split-pane)
    Orientation/HORIZONTAL (.getWidth split-pane)
    Orientation/VERTICAL (.getHeight split-pane)))

(defn- set-pane-visible! [^Scene main-scene pane-kw visible?]
  (let [{:keys [index pane-id split-id]} (split-info-by-pane-kw pane-kw)
        ^SplitPane split (.lookup main-scene (str "#" split-id))
        ^Parent pane (.lookup split (str "#" pane-id))]
    (cond
      (and (nil? pane) visible?)
      (let [user-data-key (keyword "hidden-pane" pane-id)
            {:keys [pane size]} (ui/user-data split user-data-key)
            divider-index (max 0 (dec index))
            divider-position (max 0.0
                                  (min (if (zero? index)
                                         (/ size (split-pane-length split))
                                         (- 1.0 (/ size (split-pane-length split))))
                                       1.0))]
        (ui/user-data! split user-data-key nil)
        (.add (.getItems split) index pane)
        (.setDividerPosition split divider-index divider-position)
        (.layout split))

      (and (some? pane) (not visible?))
      (let [user-data-key (keyword "hidden-pane" pane-id)
            divider-index (max 0 (dec index))
            divider-position (get (.getDividerPositions split) divider-index)
            size (if (zero? index)
                   (Math/floor (* divider-position (split-pane-length split)))
                   (Math/ceil (* (- 1.0 divider-position) (split-pane-length split))))
            removing-focus-owner? (some? (when-some [focus-owner (.getFocusOwner main-scene)]
                                           (ui/closest-node-where
                                             (partial identical? pane)
                                             focus-owner)))]

        (ui/user-data! split user-data-key {:pane pane :size size})
        (.remove (.getItems split) pane)
        (.layout split)

        ;; If this action causes the focus owner to be removed from the scene,
        ;; move focus to the SplitPane. This ensures we have a valid UI context
        ;; when refreshing the menus.
        (when removing-focus-owner?
          (.requestFocus split))))
    nil))

(defn- select-tool-tab! [tab-id ^Scene main-scene ^TabPane tool-tab-pane]
  (let [tabs (.getTabs tool-tab-pane)
        tab-index (first (keep-indexed (fn [i ^Tab tab] (when (= tab-id (.getId tab)) i)) tabs))]
    (set-pane-visible! main-scene :bottom true)
    (if (some? tab-index)
      (.select (.getSelectionModel tool-tab-pane) ^long tab-index)
      (throw (ex-info (str "Tab id not found: " tab-id)
                      {:tab-id tab-id
                       :tab-ids (mapv #(.getId ^Tab %) tabs)})))))

(def show-console! (partial select-tool-tab! "console-tab"))
(def show-curve-editor! (partial select-tool-tab! "curve-editor-tab"))
(def show-build-errors! (partial select-tool-tab! "build-errors-tab"))
(def show-search-results! (partial select-tool-tab! "search-results-tab"))

(defn show-asset-browser! [main-scene]
  (set-pane-visible! main-scene :left true))

(defn- show-debugger! [main-scene tool-tab-pane]
  ;; In addition to the controls in the console pane,
  ;; the right pane is used to display locals.
  (set-pane-visible! main-scene :right true)
  (show-console! main-scene tool-tab-pane))

(defn debugger-state-changed! [main-scene tool-tab-pane attention?]
  (ui/invalidate-menubar-item! ::debug-view/debug)
  (ui/user-data! main-scene ::ui/refresh-requested? true)
  (when attention?
    (show-debugger! main-scene tool-tab-pane)))

(defn- fire-tab-closed-event! [^Tab tab]
  ;; TODO: Workaround as there's currently no API to close tabs programatically with identical semantics to close manually
  ;; See http://stackoverflow.com/questions/17047000/javafx-closing-a-tab-in-tabpane-dynamically
  (Event/fireEvent tab (Event. Tab/CLOSED_EVENT)))

(defn- remove-tab! [^TabPane tab-pane ^Tab tab]
  (fire-tab-closed-event! tab)
  (.remove (.getTabs tab-pane) tab))

(defn remove-invalid-tabs! [tab-panes open-views]
  (let [invalid-tab? (fn [tab] (nil? (get open-views (ui/user-data tab ::view))))
        closed-tabs-by-tab-pane (into []
                                      (keep (fn [^TabPane tab-pane]
                                              (when-some [closed-tabs (not-empty (filterv invalid-tab? (.getTabs tab-pane)))]
                                                [tab-pane closed-tabs])))
                                      tab-panes)]
    ;; We must remove all invalid tabs from a TabPane in one go to ensure
    ;; the selected tab change event does not trigger onto an invalid tab.
    (when (seq closed-tabs-by-tab-pane)
      (doseq [[^TabPane tab-pane ^Collection closed-tabs] closed-tabs-by-tab-pane]
        (doseq [tab closed-tabs]
          (fire-tab-closed-event! tab))
        (.removeAll (.getTabs tab-pane) closed-tabs)))))

(defn- tab-title
  ^String [resource is-dirty]
  ;; Lone underscores are treated as mnemonic letter signifiers in the overflow
  ;; dropdown menu, and we cannot disable mnemonic parsing for it since the
  ;; control is internal. We also cannot replace them with double underscores to
  ;; escape them, as they will show up in the Tab labels and there appears to be
  ;; no way to enable mnemonic parsing for them. Attempts were made to call
  ;; setMnemonicParsing on the parent Labelled as the Tab graphic was added to
  ;; the DOM, but this only worked on macOS. As a workaround, we instead replace
  ;; underscores with the a unicode character that looks somewhat similar.
  (let [resource-name (resource/resource-name resource)
        escaped-resource-name (string/replace resource-name "_" "\u02CD")]
    (if is-dirty
      (str "*" escaped-resource-name)
      escaped-resource-name)))

(g/defnode AppView
  (property stage Stage)
  (property scene Scene)
  (property editor-tabs-split SplitPane)
  (property active-tab-pane TabPane)
  (property active-tab Tab)
  (property tool-tab-pane TabPane)
  (property auto-pulls g/Any)
  (property active-tool g/Keyword)
  (property manip-space g/Keyword)
  (property keymap-config g/Any)

  (input open-views g/Any :array)
  (input open-dirty-views g/Any :array)
  (input scene-view-ids g/Any :array)
  (input hidden-renderable-tags types/RenderableTags)
  (input hidden-node-outline-key-paths types/NodeOutlineKeyPaths)
  (input active-outline g/Any)
  (input active-scene g/Any)
  (input project-id g/NodeID)
  (input selected-node-ids-by-resource-node g/Any)
  (input selected-node-properties-by-resource-node g/Any)
  (input sub-selections-by-resource-node g/Any)
  (input debugger-execution-locations g/Any)

  (output open-views g/Any :cached (g/fnk [open-views] (into {} open-views)))
  (output open-dirty-views g/Any :cached (g/fnk [open-dirty-views] (into #{} (keep #(when (second %) (first %))) open-dirty-views)))
  (output hidden-renderable-tags types/RenderableTags (gu/passthrough hidden-renderable-tags))
  (output hidden-node-outline-key-paths types/NodeOutlineKeyPaths (gu/passthrough hidden-node-outline-key-paths))
  (output active-outline g/Any (gu/passthrough active-outline))
  (output active-scene g/Any (gu/passthrough active-scene))
  (output active-view g/NodeID (g/fnk [^Tab active-tab]
                                   (when active-tab
                                     (ui/user-data active-tab ::view))))
  (output active-view-info g/Any (g/fnk [^Tab active-tab]
                                        (when active-tab
                                          {:view-id (ui/user-data active-tab ::view)
                                           :view-type (ui/user-data active-tab ::view-type)})))

  (output active-resource-node g/NodeID :cached (g/fnk [active-view open-views] (:resource-node (get open-views active-view))))
  (output active-resource-node+type g/Any :cached
          (g/fnk [active-view open-views]
            (when-let [{:keys [resource-node resource-node-type]} (get open-views active-view)]
              [resource-node resource-node-type])))
  (output active-resource resource/Resource :cached (g/fnk [active-view open-views] (:resource (get open-views active-view))))
  (output open-resource-nodes g/Any :cached (g/fnk [open-views] (->> open-views vals (map :resource-node))))
  (output selected-node-ids g/Any (g/fnk [selected-node-ids-by-resource-node active-resource-node]
                                    (get selected-node-ids-by-resource-node active-resource-node)))
  (output selected-node-properties g/Any (g/fnk [selected-node-properties-by-resource-node active-resource-node]
                                           (get selected-node-properties-by-resource-node active-resource-node)))
  (output sub-selection g/Any (g/fnk [sub-selections-by-resource-node active-resource-node]
                                (get sub-selections-by-resource-node active-resource-node)))
  (output refresh-tab-panes g/Any :cached (g/fnk [^SplitPane editor-tabs-split open-views open-dirty-views]
                                            (let [tab-panes (.getItems editor-tabs-split)]
                                              (doseq [^TabPane tab-pane tab-panes
                                                      ^Tab tab (.getTabs tab-pane)
                                                      :let [view (ui/user-data tab ::view)
                                                            resource (:resource (get open-views view))
                                                            is-dirty (contains? open-dirty-views view)
                                                            title (tab-title resource is-dirty)]]
                                                (ui/text! tab title)))))
  (output keymap g/Any :cached (g/fnk [keymap-config]
                                 (keymap/make-keymap keymap-config {:valid-command? (set (handler/available-commands))})))
  (output debugger-execution-locations g/Any (gu/passthrough debugger-execution-locations)))

(defn- selection->openable-resources [selection]
  (when-let [resources (handler/adapt-every selection resource/Resource)]
    (filterv resource/openable-resource? resources)))

(defn- selection->single-openable-resource [selection]
  (when-let [r (handler/adapt-single selection resource/Resource)]
    (when (resource/openable-resource? r)
      r)))

(defn- selection->single-resource [selection]
  (handler/adapt-single selection resource/Resource))

(defn- context-resource-file
  ([app-view selection]
   (g/with-auto-evaluation-context evaluation-context
     (context-resource-file app-view selection evaluation-context)))
  ([app-view selection evaluation-context]
   (or (selection->single-openable-resource selection)
       (g/node-value app-view :active-resource evaluation-context))))

(defn- context-resource
  ([app-view selection]
   (g/with-auto-evaluation-context evaluation-context
     (context-resource app-view selection evaluation-context)))
  ([app-view selection evaluation-context]
   (or (selection->single-resource selection)
       (g/node-value app-view :active-resource evaluation-context))))

(defn- disconnect-sources [target-node target-label]
  (for [[source-node source-label] (g/sources-of target-node target-label)]
    (g/disconnect source-node source-label target-node target-label)))

(defn- replace-connection [source-node source-label target-node target-label]
  (concat
    (disconnect-sources target-node target-label)
    (if (and source-node (contains? (-> source-node g/node-type* g/output-labels) source-label))
      (g/connect source-node source-label target-node target-label)
      [])))

(defn- on-selected-tab-changed! [app-view app-scene tab resource-node view-type]
  (g/transact
    (concat
      (replace-connection resource-node :node-outline app-view :active-outline)
      (if (= :scene view-type)
        (replace-connection resource-node :scene app-view :active-scene)
        (disconnect-sources app-view :active-scene))
      (g/set-property app-view :active-tab tab)))
  (ui/user-data! app-scene ::ui/refresh-requested? true))

(handler/defhandler :move-tool :workbench
  (enabled? [app-view] true)
  (run [app-view] (g/transact (g/set-property app-view :active-tool :move)))
  (state [app-view] (= (g/node-value app-view :active-tool) :move)))

(handler/defhandler :scale-tool :workbench
  (enabled? [app-view] true)
  (run [app-view] (g/transact (g/set-property app-view :active-tool :scale)))
  (state [app-view]  (= (g/node-value app-view :active-tool) :scale)))

(handler/defhandler :rotate-tool :workbench
  (enabled? [app-view] true)
  (run [app-view] (g/transact (g/set-property app-view :active-tool :rotate)))
  (state [app-view]  (= (g/node-value app-view :active-tool) :rotate)))

(handler/defhandler :show-visibility-settings :workbench
  (run [app-view scene-visibility]
    (when-let [btn (some-> ^TabPane (g/node-value app-view :active-tab-pane)
                           (ui/selected-tab)
                           (.. getContent (lookup "#show-visibility-settings")))]
      (scene-visibility/show-visibility-settings! btn scene-visibility)))
  (state [app-view scene-visibility]
    (when-let [btn (some-> ^TabPane (g/node-value app-view :active-tab-pane)
                           (ui/selected-tab)
                           (.. getContent (lookup "#show-visibility-settings")))]
      ;; TODO: We have no mechanism for updating the style nor icon on
      ;; on the toolbar button. For now we piggyback on the state
      ;; update polling to set a style when the filters are active.
      (if (scene-visibility/filters-appear-active? scene-visibility)
        (ui/add-style! btn "filters-active")
        (ui/remove-style! btn "filters-active"))
      (scene-visibility/settings-visible? btn))))

(def ^:private eye-icon-svg-path
  (ui/load-svg-path "scene/images/eye_icon_eye_arrow.svg"))

(def ^:private perspective-icon-svg-path
  (ui/load-svg-path "scene/images/perspective_icon.svg"))

(defn make-svg-icon-graphic
  ^SVGPath [^SVGPath icon-template]
  (doto (SVGPath.)
    (.setContent (.getContent icon-template))))

(defn- make-visibility-settings-graphic []
  (doto (StackPane.)
    (ui/children! [(doto (make-svg-icon-graphic eye-icon-svg-path)
                     (.setId "eye-icon"))
                   (doto (Ellipse. 3.0 3.0)
                     (.setId "active-indicator"))])))

(handler/register-menu! :toolbar
  [{:id :select
    :icon "icons/45/Icons_T_01_Select.png"
    :command :select-tool}
   {:id :move
    :icon "icons/45/Icons_T_02_Move.png"
    :command :move-tool}
   {:id :rotate
    :icon "icons/45/Icons_T_03_Rotate.png"
    :command :rotate-tool}
   {:id :scale
    :icon "icons/45/Icons_T_04_Scale.png"
    :command :scale-tool}
   {:id :perspective-camera
    :graphic-fn (partial make-svg-icon-graphic perspective-icon-svg-path)
    :command :toggle-perspective-camera}
   {:id :visibility-settings
    :graphic-fn make-visibility-settings-graphic
    :command :show-visibility-settings}])

(def ^:const prefs-window-dimensions "window-dimensions")
(def ^:const prefs-split-positions "split-positions")
(def ^:const prefs-hidden-panes "hidden-panes")

(handler/defhandler :quit :global
  (enabled? [] true)
  (run []
    (let [^Stage main-stage (ui/main-stage)]
      (.fireEvent main-stage (WindowEvent. main-stage WindowEvent/WINDOW_CLOSE_REQUEST)))))

(defn store-window-dimensions [^Stage stage prefs]
  (let [dims    {:x           (.getX stage)
                 :y           (.getY stage)
                 :width       (.getWidth stage)
                 :height      (.getHeight stage)
                 :maximized   (.isMaximized stage)
                 :full-screen (.isFullScreen stage)}]
    (prefs/set-prefs prefs prefs-window-dimensions dims)))

(defn restore-window-dimensions [^Stage stage prefs]
  (when-let [dims (prefs/get-prefs prefs prefs-window-dimensions nil)]
    (let [{:keys [x y width height maximized full-screen]} dims
          maximized (and maximized (not system/mac?))] ; Maximized is not really a thing on macOS, and if set, cannot become false.
      (when (and (number? x) (number? y) (number? width) (number? height))
        (when-let [_ (seq (Screen/getScreensForRectangle x y width height))]
          (doto stage
            (.setX x)
            (.setY y))
          ;; Maximized and setWidth/setHeight in combination triggers a bug on macOS where the window becomes invisible
          (when (and (not maximized) (not full-screen))
            (doto stage
              (.setWidth width)
              (.setHeight height)))))
      (when maximized
        (.setMaximized stage true))
      (when full-screen
        (.setFullScreen stage true)))))

(def ^:private legacy-split-ids ["main-split"
                                 "center-split"
                                 "right-split"
                                 "assets-split"])

(def ^:private split-ids ["main-split"
                          "workbench-split"
                          "center-split"
                          "right-split"
                          "assets-split"])

(defn- existing-split-panes [^Scene scene]
  (into {}
        (keep (fn [split-id]
                (when-some [control (.lookup scene (str "#" split-id))]
                  [(keyword split-id) control])))
        split-ids))

(defn- stored-split-positions [prefs]
  (if-some [split-positions (prefs/get-prefs prefs prefs-split-positions nil)]
    (if (vector? split-positions) ; Legacy preference format
      (zipmap (map keyword legacy-split-ids)
              split-positions)
      split-positions)
    {}))

(defn store-split-positions! [^Scene scene prefs]
  (let [split-positions (into (stored-split-positions prefs)
                              (map (fn [[id ^SplitPane sp]]
                                     [id (.getDividerPositions sp)]))
                              (existing-split-panes scene))]
    (prefs/set-prefs prefs prefs-split-positions split-positions)))

(defn restore-split-positions! [^Scene scene prefs]
  (let [split-positions (stored-split-positions prefs)
        split-panes (existing-split-panes scene)]
    (doseq [[id positions] split-positions]
      (when-some [^SplitPane split-pane (get split-panes id)]
        (.setDividerPositions split-pane (double-array positions))
        (.layout split-pane)))))

(defn stored-hidden-panes [prefs]
  (prefs/get-prefs prefs prefs-hidden-panes #{}))

(defn store-hidden-panes! [^Scene scene prefs]
  (let [hidden-panes (into #{}
                           (remove (partial pane-visible? scene))
                           (keys split-info-by-pane-kw))]
    (prefs/set-prefs prefs prefs-hidden-panes hidden-panes)))

(defn restore-hidden-panes! [^Scene scene prefs]
  (let [hidden-panes (stored-hidden-panes prefs)]
    (doseq [pane-kw hidden-panes]
      (set-pane-visible! scene pane-kw false))))

(handler/defhandler :preferences :global
  (run [workspace prefs]
    (prefs-dialog/open-prefs prefs)
    (workspace/update-build-settings! workspace prefs)))

(defn- collect-resources [{:keys [children] :as resource}]
  (if (empty? children)
    #{resource}
    (set (concat [resource] (mapcat collect-resources children)))))

(defn- get-active-tabs [app-view evaluation-context]
  (let [tab-pane ^TabPane (g/node-value app-view :active-tab-pane evaluation-context)]
    (.getTabs tab-pane)))

(defn- make-render-build-error [main-scene tool-tab-pane build-errors-view]
  (fn [error-value]
    (build-errors-view/update-build-errors build-errors-view error-value)
    (show-build-errors! main-scene tool-tab-pane)))

(def ^:private remote-log-pump-thread (atom nil))
(def ^:private console-stream (atom nil))

(defn- reset-remote-log-pump-thread! [^Thread new]
  (when-let [old ^Thread @remote-log-pump-thread]
    (.interrupt old))
  (reset! remote-log-pump-thread new))

(defn- start-log-pump! [log-stream sink-fn]
  (doto (Thread. (fn []
                   (try
                     (let [this (Thread/currentThread)]
                       (with-open [buffered-reader ^BufferedReader (io/reader log-stream :encoding "UTF-8")]
                         (loop []
                           (when-not (.isInterrupted this)
                             (when-let [line (.readLine buffered-reader)] ; line of text or nil if eof reached
                               (sink-fn line)
                               (recur))))))
                     (catch IOException _
                       ;; Losing the log connection is ok and even expected
                       nil)
                     (catch InterruptedException _
                       ;; Losing the log connection is ok and even expected
                       nil))))
    (.start)))

(defn- local-url [target web-server]
  (format "http://%s:%s%s" (:local-address target) (http-server/port web-server) hot-reload/url-prefix))


(def ^:private app-task-progress
  {:main (ref progress/done)
   :build (ref progress/done)
   :resource-sync (ref progress/done)
   :save-all (ref progress/done)
   :fetch-libraries (ref progress/done)
   :download-update (ref progress/done)})

(defn- cancel-task!
  [task-key]
  (dosync
    (let [progress-ref (task-key app-task-progress)]
      (ref-set progress-ref (progress/cancel! @progress-ref)))))

(def ^:private app-task-ui-priority
  "Task priority in descending order (from highest to lowest)"
  [:save-all :resource-sync :fetch-libraries :build :download-update :main])

(def ^:private render-task-progress-ui-inflight (ref false))

(defn- render-task-progress-ui! []
  (let [task-progress-snapshot (ref nil)]
    (dosync
      (ref-set render-task-progress-ui-inflight false)
      (ref-set task-progress-snapshot
               (into {} (map (juxt first (comp deref second))) app-task-progress)))
    (let [status-bar (.. (ui/main-stage) (getScene) (getRoot) (lookup "#status-bar"))
          [key progress] (->> app-task-ui-priority
                              (map (juxt identity @task-progress-snapshot))
                              (filter (comp (complement progress/done?) second))
                              first)
          show-progress-hbox? (boolean (and (not= key :main)
                                            progress
                                            (not (progress/done? progress))))]
      (ui/with-controls status-bar [progress-bar progress-hbox progress-percentage-label status-label progress-cancel-button]
        (ui/render-progress-message!
          (if key progress (@task-progress-snapshot :main))
          status-label)
        ;; The bottom right of the status bar can show either the progress-hbox
        ;; or the update-link, or both. The progress-hbox will cover
        ;; the update-link if both are visible.
        (if-not show-progress-hbox?
          (ui/visible! progress-hbox false)
          (do
            (ui/visible! progress-hbox true)
            (ui/render-progress-bar! progress progress-bar)
            (ui/render-progress-percentage! progress progress-percentage-label)
            (if (progress/cancellable? progress)
              (doto progress-cancel-button
                (ui/visible! true)
                (ui/managed! true)
                (ui/on-action! (fn [_] (cancel-task! key))))
              (doto progress-cancel-button
                (ui/visible! false)
                (ui/managed! false)
                (ui/on-action! identity)))))))))

(defn- render-task-progress! [key progress]
  (let [schedule-render-task-progress-ui (ref false)]
    (dosync
      (ref-set (get app-task-progress key) progress)
      (ref-set schedule-render-task-progress-ui (not @render-task-progress-ui-inflight))
      (ref-set render-task-progress-ui-inflight true))
    (when @schedule-render-task-progress-ui
      (ui/run-later (render-task-progress-ui!)))))

(defn make-render-task-progress [key]
  (assert (contains? app-task-progress key))
  (progress/throttle-render-progress
    (fn [progress] (render-task-progress! key progress))))

(defn make-task-cancelled-query [keyword]
  (fn [] (progress/cancelled? @(keyword app-task-progress))))

(defn render-main-task-progress! [progress]
  (render-task-progress! :main progress))

(defn- report-build-launch-progress! [message]
  (render-main-task-progress! (progress/make message)))

(defn clear-build-launch-progress! []
  (render-main-task-progress! progress/done))

(defn- decorate-target [engine-descriptor target]
  (assoc target :engine-id (:id engine-descriptor)))

(defn- launch-engine! [engine-descriptor project-directory prefs debug?]
  (try
    (report-build-launch-progress! "Launching engine...")
    (let [engine (engine/install-engine! project-directory engine-descriptor)
          launched-target (->> (engine/launch! engine project-directory prefs debug?)
                               (decorate-target engine-descriptor)
                               (targets/add-launched-target!)
                               (targets/select-target! prefs))]
      (report-build-launch-progress! (format "Launched %s" (targets/target-message-label launched-target)))
      launched-target)
    (catch Exception e
      (targets/kill-launched-targets!)
      (report-build-launch-progress! "Launch failed")
      (throw e))))

(defn- reset-console-stream! [stream]
  (reset! console-stream stream)
  (console/clear-console!))

(defn- make-remote-log-sink [log-stream]
  (fn [line]
    (when (= @console-stream log-stream)
      (console/append-console-line! line))))

(defn- make-launched-log-sink [launched-target]
  (let [initial-output (atom "")]
    (fn [line]
      (when (< (count @initial-output) 5000)
        (swap! initial-output str line "\n")
        (when-let [target-info (engine/parse-launched-target-info @initial-output)]
          (targets/update-launched-target! launched-target target-info)))
      (when (= @console-stream (:log-stream launched-target))
        (console/append-console-line! line)))))

(defn- reboot-engine! [target web-server debug?]
  (try
    (report-build-launch-progress! (format "Rebooting %s..." (targets/target-message-label target)))
    (engine/reboot! target (local-url target web-server) debug?)
    (report-build-launch-progress! (format "Rebooted %s" (targets/target-message-label target)))
    target
    (catch Exception e
      (report-build-launch-progress! "Reboot failed")
      (throw e))))

(def ^:private build-in-progress-atom (atom false))

(defn build-in-progress? []
  @build-in-progress-atom)

(defn- on-launched-hook! [project process url]
  (let [hook-options {:exception-policy :ignore :opts {:url url}}]
    (future
      (error-reporting/catch-all!
        (extensions/execute-hook! project :on-target-launched hook-options)
        (process/watchdog! process #(extensions/execute-hook! project :on-target-terminated hook-options))))))

(defn- target-cannot-swap-engine? [target]
  (and (some? target)
       (targets/controllable-target? target)
       (targets/remote-target? target)))

(defn- launch-built-project! [project engine-descriptor project-directory prefs web-server debug?]
  (let [selected-target (targets/selected-target prefs)
        launch-new-engine! (fn []
                             (targets/kill-launched-targets!)
                             (let [launched-target (launch-engine! engine-descriptor project-directory prefs debug?)
                                   log-stream      (:log-stream launched-target)]
                               (targets/when-url (:id launched-target)
                                                 #(on-launched-hook! project (:process launched-target) %))
                               (reset-console-stream! log-stream)
                               (reset-remote-log-pump-thread! nil)
                               (start-log-pump! log-stream (make-launched-log-sink launched-target))
                               launched-target))]
    (try
      (cond
        (not selected-target)
        (launch-new-engine!)

        (not (targets/controllable-target? selected-target))
        (do
          (assert (targets/launched-target? selected-target))
          (launch-new-engine!))

        (target-cannot-swap-engine? selected-target)
        (let [log-stream (engine/get-log-service-stream selected-target)]
          (reset-console-stream! log-stream)
          (reset-remote-log-pump-thread! (start-log-pump! log-stream (make-remote-log-sink log-stream)))
          (reboot-engine! selected-target web-server debug?))

        :else
        (do
          (assert (and (targets/controllable-target? selected-target) (targets/launched-target? selected-target)))
          (if (= (:id engine-descriptor) (:engine-id selected-target))
            (do
              ;; We're running "the same" engine and can reuse the
              ;; running process by rebooting
              (reset-console-stream! (:log-stream selected-target))
              (reset-remote-log-pump-thread! nil)
              ;; Launched target log pump already
              ;; running to keep engine process
              ;; from halting because stdout/err is
              ;; not consumed.
              (reboot-engine! selected-target web-server debug?))
            (launch-new-engine!))))
      (catch Exception e
        (log/warn :exception e)
        (dialogs/make-info-dialog
          {:title "Launch Failed"
           :icon :icon/triangle-error
           :header {:fx/type fx.v-box/lifecycle
                    :children [{:fx/type fxui/label
                                :variant :header
                                :text (format "Launching %s failed"
                                              (if (some? selected-target)
                                                (targets/target-message-label selected-target)
                                                "New Local Engine"))}
                               {:fx/type fxui/label
                                :text "If the engine is already running, shut down the process manually and retry"}]}
           :content (.getMessage e)})))))

(defn- build-project!
  [project evaluation-context extra-build-targets old-artifact-map render-progress!]
  (let [game-project (project/get-resource-node project "/game.project" evaluation-context)
        render-progress! (progress/throttle-render-progress render-progress!)]
    (try
      (ui/with-progress [render-progress! render-progress!]
        (build/build-project! project game-project evaluation-context extra-build-targets old-artifact-map render-progress!))
      (catch Throwable error
        (error-reporting/report-exception! error)
        nil))))

(defn async-build! [project prefs {:keys [debug? engine?] :or {debug? false engine? true}} old-artifact-map render-build-progress! result-fn]
  (let [;; After any pre-build hooks have completed successfully, we will start
        ;; the engine build on a separate background thread so the build servers
        ;; can work while we build the project. We will await the results of the
        ;; engine build in the final phase.
        engine-build-future-atom (atom nil)

        cancel-engine-build!
        (fn cancel-engine-build! []
          (when-some [engine-build-future (thread-util/preset! engine-build-future-atom nil)]
            (future-cancel engine-build-future)
            nil))

        start-engine-build!
        (fn start-engine-build! []
          (assert (ui/on-ui-thread?))
          (cancel-engine-build!)
          (when engine?
            (let [evaluation-context (g/make-evaluation-context)
                  platform (engine/current-platform)]
              (reset! engine-build-future-atom
                      (future
                        (try
                          (let [engine (engine/get-engine project evaluation-context prefs platform)]
                            (ui/run-later
                              ;; This potentially saves us from having to
                              ;; re-calculate native extension file hashes the
                              ;; next time we build the project.
                              (g/update-cache-from-evaluation-context! evaluation-context))
                            engine)
                          (catch Throwable error
                            error))))
              nil)))

        run-on-background-thread!
        (fn run-on-background-thread! [background-thread-fn ui-thread-fn]
          (future
            (try
              (let [return-value (background-thread-fn)]
                (ui/run-later
                  (try
                    (ui-thread-fn return-value)
                    (catch Throwable error
                      (reset! build-in-progress-atom false)
                      (render-build-progress! progress/done)
                      (cancel-engine-build!)
                      (throw error)))))
              (catch Throwable error
                (reset! build-in-progress-atom false)
                (render-build-progress! progress/done)
                (cancel-engine-build!)
                (error-reporting/report-exception! error))))
          nil)

        finish-with-result!
        (fn finish-with-result! [project-build-results engine build-engine-exception]
          (reset! build-in-progress-atom false)
          (render-build-progress! progress/done)
          (cancel-engine-build!)
          (when (some? result-fn)
            (result-fn project-build-results engine build-engine-exception)
            nil))

        phase-5-await-engine-build!
        (fn phase-5-await-engine-build! [project-build-results]
          (assert (nil? (:error project-build-results)))
          (let [engine-build-future @engine-build-future-atom]
            (if (nil? engine-build-future)
              (finish-with-result! project-build-results nil nil)
              (do
                (render-build-progress! (progress/make-indeterminate "Fetching engine..."))
                (run-on-background-thread!
                  (fn run-engine-build-on-background-thread! []
                    (deref engine-build-future))
                  (fn process-engine-build-results-on-ui-thread! [engine-or-exception]
                    (if (instance? Throwable engine-or-exception)
                      (finish-with-result! project-build-results nil engine-or-exception)
                      (finish-with-result! project-build-results engine-or-exception nil))))))))

        phase-4-run-post-build-hook!
        (fn phase-4-run-post-build-hook! [project-build-results]
          (render-build-progress! (progress/make-indeterminate "Executing post-build hooks..."))
          (let [platform (engine/current-platform)
                project-build-successful? (nil? (:error project-build-results))]
            (run-on-background-thread!
              (fn run-post-build-hook-on-background-thread! []
                (extensions/execute-hook! project
                                          :on-build-finished
                                          {:exception-policy :ignore
                                           :opts {:success project-build-successful?
                                                  :platform platform}}))
              (fn process-post-build-hook-results-on-ui-thread! [_]
                (if project-build-successful?
                  (phase-5-await-engine-build! project-build-results)
                  (finish-with-result! project-build-results nil nil))))))

        phase-3-build-project!
        (fn phase-3-build-project! []
          ;; We're about to create an evaluation-context. Make sure it is
          ;; created from the main thread, so it makes sense to update the cache
          ;; from it after the project build concludes. Note that we selectively
          ;; transfer only the cached build-targets back to the system cache.
          ;; We do this because the project build process involves most of the
          ;; cached outputs in the project graph, and the intermediate steps
          ;; risk evicting the previous build targets as the cache fills up.
          (assert (ui/on-ui-thread?))
          (let [evaluation-context (g/make-evaluation-context)]
            (render-build-progress! (progress/make "Building project..." 1))
            (run-on-background-thread!
              (fn run-project-build-on-background-thread! []
                (let [extra-build-targets
                      (when debug?
                        (debug-view/build-targets project evaluation-context))]
                  (build-project! project evaluation-context extra-build-targets old-artifact-map render-build-progress!)))
              (fn process-project-build-results-on-ui-thread! [project-build-results]
                (project/update-system-cache-build-targets! evaluation-context)
                (phase-4-run-post-build-hook! project-build-results)))))

        phase-2-start-engine-build!
        (fn phase-2-start-engine-build! []
          (start-engine-build!)
          (phase-3-build-project!))

        phase-1-run-pre-build-hook!
        (fn phase-1-run-pre-build-hook! []
          (render-build-progress! (progress/make-indeterminate "Executing pre-build hooks..."))
          (let [platform (engine/current-platform)]
            (run-on-background-thread!
              (fn run-pre-build-hook-on-background-thread! []
                (let [extension-error
                      (extensions/execute-hook!
                        project
                        :on-build-started
                        {:exception-policy :as-error
                         :opts {:platform platform}})]
                  ;; If there was an error in the pre-build hook, we won't proceed
                  ;; with the project build. But we still want to report the build
                  ;; failure to any post-build hooks that might need to know.
                  (when (some? extension-error)
                    (render-build-progress! (progress/make-indeterminate "Executing post-build hooks..."))
                    (extensions/execute-hook!
                      project
                      :on-build-finished
                      {:exception-policy :ignore
                       :opts {:success false
                              :platform platform}}))
                  extension-error))
              (fn process-pre-build-hook-results-on-ui-thread! [extension-error]
                (if (some? extension-error)
                  (finish-with-result! {:error extension-error} nil nil)
                  (phase-2-start-engine-build!))))))]
    ;; Trigger phase 1. Subsequent phases will be triggered as prior phases
    ;; finish without errors. Each phase will do some work on a background
    ;; thread, then process the results on the ui thread, and potentially
    ;; trigger subsequent phases which will again get off the ui thread as
    ;; soon as they can.
    (assert (not @build-in-progress-atom))
    (reset! build-in-progress-atom true)
    (phase-1-run-pre-build-hook!)))

(defn- handle-build-results! [workspace render-build-error! build-results]
  (let [{:keys [error artifact-map etags]} build-results]
    (if (some? error)
      (do
        (render-build-error! error)
        nil)
      (do
        (workspace/artifact-map! workspace artifact-map)
        (workspace/etags! workspace etags)
        (workspace/save-build-cache! workspace)
        build-results))))

(defn- build-handler [project workspace prefs web-server build-errors-view main-stage tool-tab-pane]
  (let [project-directory (io/file (workspace/project-path workspace))
        main-scene (.getScene ^Stage main-stage)
        render-build-error! (make-render-build-error main-scene tool-tab-pane build-errors-view)
        skip-engine (target-cannot-swap-engine? (targets/selected-target prefs))]
    (build-errors-view/clear-build-errors build-errors-view)
    (async-build! project prefs {:debug? false :engine? (not skip-engine)} (workspace/artifact-map workspace)
                  (make-render-task-progress :build)
                  (fn [build-results engine-descriptor build-engine-exception]
                    (when (handle-build-results! workspace render-build-error! build-results)
                      (when (or engine-descriptor skip-engine)
                        (show-console! main-scene tool-tab-pane)
                        (launch-built-project! project engine-descriptor project-directory prefs web-server false))
                      (when build-engine-exception
                        (log/warn :exception build-engine-exception)
                        (g/with-auto-evaluation-context evaluation-context
                          (engine-build-errors/handle-build-error! render-build-error! project evaluation-context build-engine-exception))))))))

(handler/defhandler :build :global
  (enabled? [] (not (build-in-progress?)))
  (run [project workspace prefs web-server build-errors-view debug-view main-stage tool-tab-pane]
    (debug-view/detach! debug-view)
    (build-handler project workspace prefs web-server build-errors-view main-stage tool-tab-pane)))

(defn- debugging-supported?
  [project]
  (if (project/shared-script-state? project)
    true
    (do (dialogs/make-info-dialog
          {:title "Debugging Not Supported"
           :icon :icon/triangle-error
           :header "This project cannot be used with the debugger"
           :content {:fx/type fxui/label
                     :style-class "dialog-content-padding"
                     :text "It is configured to disable shared script state.

If you do not specifically require different script states, consider changing the script.shared_state property in game.project."}})
        false)))

(defn- run-with-debugger! [workspace project prefs debug-view render-build-error! web-server]
  (let [project-directory (io/file (workspace/project-path workspace))
        skip-engine (target-cannot-swap-engine? (targets/selected-target prefs))]
    (async-build! project prefs {:debug? true :engine? (not skip-engine)} (workspace/artifact-map workspace)
                  (make-render-task-progress :build)
                  (fn [build-results engine-descriptor build-engine-exception]
                    (when (handle-build-results! workspace render-build-error! build-results)
                      (when (or engine-descriptor skip-engine)
                        (when-let [target (launch-built-project! project engine-descriptor project-directory prefs web-server true)]
                          (when (nil? (debug-view/current-session debug-view))
                            (debug-view/start-debugger! debug-view project (:address target "localhost")))))
                      (when build-engine-exception
                        (log/warn :exception build-engine-exception)
                        (g/with-auto-evaluation-context evaluation-context
                          (engine-build-errors/handle-build-error! render-build-error! project evaluation-context build-engine-exception))))))))

(defn- attach-debugger! [workspace project prefs debug-view render-build-error!]
  (async-build! project prefs {:debug? true :engine? false} (workspace/artifact-map workspace)
                (make-render-task-progress :build)
                (fn [build-results _ _]
                  (when (handle-build-results! workspace render-build-error! build-results)
                    (let [target (targets/selected-target prefs)]
                      (when (targets/controllable-target? target)
                        (debug-view/attach! debug-view project target (:artifacts build-results))))))))

(handler/defhandler :start-debugger :global
  ;; NOTE: Shares a shortcut with :debug-view/continue.
  ;; Only one of them can be active at a time. This creates the impression that
  ;; there is a single menu item whose label changes in various states.
  (active? [debug-view evaluation-context]
           (not (debug-view/debugging? debug-view evaluation-context)))
  (enabled? [] (not (build-in-progress?)))
  (run [project workspace prefs web-server build-errors-view console-view debug-view main-stage tool-tab-pane]
    (when (debugging-supported? project)
      (let [main-scene (.getScene ^Stage main-stage)
            render-build-error! (make-render-build-error main-scene tool-tab-pane build-errors-view)]
        (build-errors-view/clear-build-errors build-errors-view)
        (if (debug-view/can-attach? prefs)
          (attach-debugger! workspace project prefs debug-view render-build-error!)
          (run-with-debugger! workspace project prefs debug-view render-build-error! web-server))))))

(handler/defhandler :rebuild :global
  (enabled? [] (not (build-in-progress?)))
  (run [project workspace prefs web-server build-errors-view debug-view main-stage tool-tab-pane]
    (debug-view/detach! debug-view)
    (workspace/clear-build-cache! workspace)
    (build-handler project workspace prefs web-server build-errors-view main-stage tool-tab-pane)))

(handler/defhandler :build-html5 :global
  (run [project prefs web-server build-errors-view changes-view main-stage tool-tab-pane]
    (let [main-scene (.getScene ^Stage main-stage)
          render-build-error! (make-render-build-error main-scene tool-tab-pane build-errors-view)
          render-reload-progress! (make-render-task-progress :resource-sync)
          render-save-progress! (make-render-task-progress :save-all)
          render-build-progress! (make-render-task-progress :build)
          task-cancelled? (make-task-cancelled-query :build)
          bob-args (bob/build-html5-bob-args project prefs)]
      (build-errors-view/clear-build-errors build-errors-view)
      (disk/async-bob-build! render-reload-progress! render-save-progress! render-build-progress! task-cancelled?
                             render-build-error! bob/build-html5-bob-commands bob-args project changes-view
                             (fn [successful?]
                               (when successful?
                                 (ui/open-url (format "http://localhost:%d%s/index.html" (http-server/port web-server) bob/html5-url-prefix))))))))

(defn- updated-build-resource-proj-paths [old-etags new-etags]
  ;; We only want to return resources that were present in the old etags since
  ;; we don't want to ask the engine to reload something it has not seen yet.
  ;; It is presumed that the engine will follow any newly-introduced references
  ;; and load the resources. We might ask the engine to reload these resources
  ;; the next time they are modified.
  (into #{}
        (keep (fn [[proj-path old-etag]]
                (when-some [new-etag (new-etags proj-path)]
                  (when (not= old-etag new-etag)
                    proj-path))))
        old-etags))

(defn- updated-build-resources [evaluation-context project old-etags new-etags proj-path-or-resource]
  (let [resource-node (project/get-resource-node project proj-path-or-resource evaluation-context)
        build-targets (g/node-value resource-node :build-targets evaluation-context)
        updated-build-resource-proj-paths (updated-build-resource-proj-paths old-etags new-etags)]
    (into []
          (keep (fn [{build-resource :resource :as _build-target}]
                  (when (contains? updated-build-resource-proj-paths (resource/proj-path build-resource))
                    build-resource)))
          (rseq (pipeline/flatten-build-targets build-targets)))))

(defn- can-hot-reload? [debug-view prefs evaluation-context]
  (when-some [target (targets/selected-target prefs)]
    (and (targets/controllable-target? target)
         (not (debug-view/suspended? debug-view evaluation-context))
         (not (build-in-progress?)))))

(defn- hot-reload! [project prefs build-errors-view main-stage tool-tab-pane]
  (let [main-scene (.getScene ^Stage main-stage)
        target (targets/selected-target prefs)
        workspace (project/workspace project)
        old-artifact-map (workspace/artifact-map workspace)
        old-etags (workspace/etags workspace)
        render-build-progress! (make-render-task-progress :build)
        render-build-error! (make-render-build-error main-scene tool-tab-pane build-errors-view)
        opts {:debug? false :engine? false}]
    ;; NOTE: We must build the entire project even if we only want to reload a
    ;; subset of resources in order to maintain a functioning build cache.
    ;; If we decide to support hot reload of a subset of resources, we must
    ;; keep track of which resource versions have been loaded by the engine,
    ;; or we might miss resources that were recompiled but never reloaded.
    (build-errors-view/clear-build-errors build-errors-view)
    (async-build! project prefs opts old-artifact-map render-build-progress!
                  (fn [{:keys [error artifact-map etags]} _ _]
                    (if (some? error)
                      (render-build-error! error)
                      (do
                        (workspace/artifact-map! workspace artifact-map)
                        (workspace/etags! workspace etags)
                        (workspace/save-build-cache! workspace)
                        (try
                          (when-some [updated-build-resources
                                      (not-empty
                                        (g/with-auto-evaluation-context evaluation-context
                                          (updated-build-resources evaluation-context project old-etags etags "/game.project")))]
                            (engine/reload-build-resources! target updated-build-resources))
                          (catch Exception e
                            (dialogs/make-info-dialog
                              {:title "Hot Reload Failed"
                               :icon :icon/triangle-error
                               :header (format "Failed to reload resources on '%s'"
                                               (targets/target-message-label (targets/selected-target prefs)))
                               :content (.getMessage e)})))))))))

(handler/defhandler :hot-reload :global
  (enabled? [debug-view prefs evaluation-context]
            (can-hot-reload? debug-view prefs evaluation-context))
  (run [project app-view prefs build-errors-view selection main-stage tool-tab-pane]
       (hot-reload! project prefs build-errors-view main-stage tool-tab-pane)))

(handler/defhandler :close :global
  (enabled? [app-view evaluation-context]
            (not-empty (get-active-tabs app-view evaluation-context)))
  (run [app-view]
    (let [tab-pane (g/node-value app-view :active-tab-pane)]
      (when-let [tab (ui/selected-tab tab-pane)]
        (remove-tab! tab-pane tab)))))

(handler/defhandler :close-other :global
  (enabled? [app-view evaluation-context]
            (not-empty (next (get-active-tabs app-view evaluation-context))))
  (run [app-view]
    (let [tab-pane ^TabPane (g/node-value app-view :active-tab-pane)]
      (when-let [selected-tab (ui/selected-tab tab-pane)]
        ;; Plain doseq over .getTabs will use the iterable interface
        ;; and we get a ConcurrentModificationException since we
        ;; remove from the list while iterating. Instead put the tabs
        ;; in a (non-lazy) vec before iterating.
        (doseq [tab (vec (.getTabs tab-pane))]
          (when (not= tab selected-tab)
            (remove-tab! tab-pane tab)))))))

(handler/defhandler :close-all :global
  (enabled? [app-view evaluation-context]
            (not-empty (get-active-tabs app-view evaluation-context)))
  (run [app-view]
    (let [tab-pane ^TabPane (g/node-value app-view :active-tab-pane)]
      (doseq [tab (vec (.getTabs tab-pane))]
        (remove-tab! tab-pane tab)))))

(defn- editor-tab-pane
  "Returns the editor TabPane that is above the Node in the scene hierarchy, or
  nil if the Node does not reside under an editor TabPane."
  ^TabPane [node]
  (when-some [parent-tab-pane (ui/parent-tab-pane node)]
    (when (= "editor-tabs-split" (some-> (ui/tab-pane-parent parent-tab-pane) (.getId)))
      parent-tab-pane)))

(declare ^:private configure-editor-tab-pane!)

(defn- find-other-tab-pane
  ^TabPane [^SplitPane editor-tabs-split ^TabPane current-tab-pane]
  (first-where #(not (identical? current-tab-pane %))
               (.getItems editor-tabs-split)))

(defn- add-other-tab-pane!
  ^TabPane [^SplitPane editor-tabs-split app-view]
  (let [tab-panes (.getItems editor-tabs-split)
        app-stage ^Stage (g/node-value app-view :stage)
        app-scene (.getScene app-stage)
        new-tab-pane (TabPane.)]
    (assert (= 1 (count tab-panes)))
    (.add tab-panes new-tab-pane)
    (configure-editor-tab-pane! new-tab-pane app-scene app-view)
    new-tab-pane))

(defn- open-tab-count
  ^long [app-view evaluation-context]
  (let [editor-tabs-split ^SplitPane (g/node-value app-view :editor-tabs-split evaluation-context)]
    (loop [tab-panes (.getItems editor-tabs-split)
           tab-count 0]
      (if-some [^TabPane tab-pane (first tab-panes)]
        (recur (next tab-panes)
               (+ tab-count (.size (.getTabs tab-pane))))
        tab-count))))

(defn- open-tab-pane-count
  ^long [app-view evaluation-context]
  (let [editor-tabs-split ^SplitPane (g/node-value app-view :editor-tabs-split evaluation-context)]
    (.size (.getItems editor-tabs-split))))

(handler/defhandler :move-tab :global
  (enabled? [app-view evaluation-context]
            (< 1 (open-tab-count app-view evaluation-context)))
  (run [app-view user-data]
       (let [editor-tabs-split ^SplitPane (g/node-value app-view :editor-tabs-split)
             source-tab-pane ^TabPane (g/node-value app-view :active-tab-pane)
             selected-tab (ui/selected-tab source-tab-pane)
             dest-tab-pane (or (find-other-tab-pane editor-tabs-split source-tab-pane)
                               (add-other-tab-pane! editor-tabs-split app-view))]
         (.remove (.getTabs source-tab-pane) selected-tab)
         (.add (.getTabs dest-tab-pane) selected-tab)
         (.select (.getSelectionModel dest-tab-pane) selected-tab)
         (.requestFocus dest-tab-pane))))

(handler/defhandler :swap-tabs :global
  (enabled? [app-view evaluation-context]
            (< 1 (open-tab-pane-count app-view evaluation-context)))
  (run [app-view user-data]
       (let [editor-tabs-split ^SplitPane (g/node-value app-view :editor-tabs-split)
             active-tab-pane ^TabPane (g/node-value app-view :active-tab-pane)
             other-tab-pane (find-other-tab-pane editor-tabs-split active-tab-pane)
             active-tab-pane-selection (.getSelectionModel active-tab-pane)
             other-tab-pane-selection (.getSelectionModel other-tab-pane)
             active-tab-index (.getSelectedIndex active-tab-pane-selection)
             other-tab-index (.getSelectedIndex other-tab-pane-selection)
             active-tabs (.getTabs active-tab-pane)
             other-tabs (.getTabs other-tab-pane)
             active-tab (.get active-tabs active-tab-index)
             other-tab (.get other-tabs other-tab-index)]
         ;; Fix for DEFEDIT-1673:
         ;; We need to swap in a dummy tab here so that a tab is never in both
         ;; TabPanes at once, since the tab lists are observed internally. If we
         ;; do not, the tabs will lose track of their parent TabPane.
         (.set other-tabs other-tab-index (Tab.))
         (.set active-tabs active-tab-index other-tab)
         (.set other-tabs other-tab-index active-tab)
         (.select active-tab-pane-selection other-tab)
         (.select other-tab-pane-selection active-tab)
         (.requestFocus other-tab-pane))))

(handler/defhandler :join-tab-panes :global
  (enabled? [app-view evaluation-context]
            (< 1 (open-tab-pane-count app-view evaluation-context)))
  (run [app-view user-data]
       (let [editor-tabs-split ^SplitPane (g/node-value app-view :editor-tabs-split)
             active-tab-pane ^TabPane (g/node-value app-view :active-tab-pane)
             selected-tab (ui/selected-tab active-tab-pane)
             tab-panes (.getItems editor-tabs-split)
             first-tab-pane ^TabPane (.get tab-panes 0)
             second-tab-pane ^TabPane (.get tab-panes 1)
             first-tabs (.getTabs first-tab-pane)
             second-tabs (.getTabs second-tab-pane)
             moved-tabs (vec second-tabs)]
         (.clear second-tabs)
         (.addAll first-tabs ^Collection moved-tabs)
         (.select (.getSelectionModel first-tab-pane) selected-tab)
         (.requestFocus first-tab-pane))))

(defn make-about-dialog []
  (let [root ^Parent (ui/load-fxml "about.fxml")
        stage (ui/make-dialog-stage)
        scene (Scene. root)
        controls (ui/collect-controls root ["version" "channel" "editor-sha1" "engine-sha1" "time", "sponsor-push"])]
    (ui/text! (:version controls) (System/getProperty "defold.version" "No version"))
    (ui/text! (:channel controls) (or (system/defold-channel) "No channel"))
    (ui/text! (:editor-sha1 controls) (or (system/defold-editor-sha1) "No editor sha1"))
    (ui/text! (:engine-sha1 controls) (or (system/defold-engine-sha1) "No engine sha1"))
    (ui/text! (:time controls) (or (system/defold-build-time) "No build time"))
    (UIUtil/stringToTextFlowNodes (:sponsor-push controls) "[Defold Foundation Development Fund](https://www.defold.com/community-donations)")
    (ui/title! stage "About")
    (.setScene stage scene)
    (ui/show! stage)))

(handler/defhandler :documentation :global
  (run [] (ui/open-url "https://www.defold.com/learn/")))

(handler/defhandler :support-forum :global
  (run [] (ui/open-url "https://forum.defold.com/")))

(handler/defhandler :asset-portal :global
  (run [] (ui/open-url "https://www.defold.com/assets")))

(handler/defhandler :report-issue :global
  (run [] (ui/open-url (github/new-issue-link))))

(handler/defhandler :report-suggestion :global
  (run [] (ui/open-url (github/new-suggestion-link))))

(handler/defhandler :search-issues :global
  (run [] (ui/open-url (github/search-issues-link))))

(handler/defhandler :show-logs :global
  (run [] (ui/open-file (.getAbsoluteFile (.toFile (Editor/getLogDirectory))))))

(handler/defhandler :donate :global
  (run [] (ui/open-url "https://www.defold.com/donate")))

(handler/defhandler :about :global
  (run [] (make-about-dialog)))

(handler/defhandler :reload-stylesheet :global
  (run [] (ui/reload-root-styles!)))

(handler/register-menu! ::menubar
  [{:label "File"
    :id ::file
    :children [{:label "New..."
                :id ::new
                :command :new-file}
               {:label "Open"
                :id ::open
                :command :open}
               {:label "Synchronize..."
                :id ::synchronize
                :command :synchronize}
               {:label "Save All"
                :id ::save-all
                :command :save-all}
               {:label :separator}
               {:label "Open Assets..."
                :command :open-asset}
               {:label "Search in Files..."
                :command :search-in-files}
               {:label :separator}
               {:label "Close"
                :command :close}
               {:label "Close All"
                :command :close-all}
               {:label "Close Others"
                :command :close-other}
               {:label :separator}
               {:label "Referencing Files..."
                :command :referencing-files}
               {:label "Dependencies..."
                :command :dependencies}
               {:label "Hot Reload"
                :command :hot-reload}
               {:label :separator}
               {:label "Preferences..."
                :command :preferences}
               {:label "Quit"
                :command :quit}]}
   {:label "Edit"
    :id ::edit
    :children [{:label "Undo"
                :icon "icons/undo.png"
                :command :undo}
               {:label "Redo"
                :icon "icons/redo.png"
                :command :redo}
               {:label :separator}
               {:label "Cut"
                :command :cut}
               {:label "Copy"
                :command :copy}
               {:label "Paste"
                :command :paste}
               {:label "Select All"
                :command :select-all}
               {:label "Delete"
                :icon "icons/32/Icons_M_06_trash.png"
                :command :delete}
               {:label :separator}
               {:label "Move Up"
                :command :move-up}
               {:label "Move Down"
                :command :move-down}
               {:label :separator
                :id ::edit-end}]}
   {:label "View"
    :id ::view
    :children [{:label "Toggle Assets Pane"
                :command :toggle-pane-left}
               {:label "Toggle Tools Pane"
                :command :toggle-pane-bottom}
               {:label "Toggle Properties Pane"
                :command :toggle-pane-right}
               {:label :separator}
               {:label "Show Console"
                :command :show-console}
               {:label "Show Curve Editor"
                :command :show-curve-editor}
               {:label "Show Build Errors"
                :command :show-build-errors}
               {:label "Show Search Results"
                :command :show-search-results}
               {:label :separator
                :id ::view-end}]}
   {:label "Help"
    :children [{:label "Profiler"
                :children [{:label "Measure"
                            :command :profile}
                           {:label "Measure and Show"
                            :command :profile-show}]}
               {:label "Reload Stylesheet"
                :command :reload-stylesheet}
               {:label "Show Logs"
                :command :show-logs}
               {:label :separator}
               {:label "Documentation"
                :command :documentation}
               {:label "Support Forum"
                :command :support-forum}
               {:label "Find Assets"
                :command :asset-portal}
               {:label :separator}
               {:label "Report Issue"
                :command :report-issue}
               {:label "Report Suggestion"
                :command :report-suggestion}
               {:label "Search Issues"
                :command :search-issues}
               {:label :separator}
               {:label "Development Fund"
                :command :donate}
               {:label :separator}
               {:label "About"
                :command :about}]}])

(handler/register-menu! ::tab-menu
  [{:label "Close"
    :command :close}
   {:label "Close Others"
    :command :close-other}
   {:label "Close All"
    :command :close-all}
   {:label :separator}
   {:label "Move to Other Tab Pane"
    :command :move-tab}
   {:label "Swap With Other Tab Pane"
    :command :swap-tabs}
   {:label "Join Tab Panes"
    :command :join-tab-panes}
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
    :command :dependencies}])

(defrecord SelectionProvider [app-view]
  handler/SelectionProvider
  (selection [_] (g/node-value app-view :selected-node-ids))
  (succeeding-selection [_] [])
  (alt-selection [_] []))

(defn ->selection-provider [app-view] (SelectionProvider. app-view))

(defn select
  ([app-view node-ids]
   (select app-view (g/node-value app-view :active-resource-node) node-ids))
  ([app-view resource-node node-ids]
   (g/with-auto-evaluation-context evaluation-context
     (let [project-id (g/node-value app-view :project-id evaluation-context)
           open-resource-nodes (g/node-value app-view :open-resource-nodes evaluation-context)]
       (project/select project-id resource-node node-ids open-resource-nodes)))))

(defn select!
  ([app-view node-ids]
   (select! app-view node-ids (gensym)))
  ([app-view node-ids op-seq]
   (g/transact
     (concat
       (g/operation-sequence op-seq)
       (g/operation-label "Select")
       (select app-view node-ids)))))

(defn sub-select!
  ([app-view sub-selection]
   (sub-select! app-view sub-selection (gensym)))
  ([app-view sub-selection op-seq]
   (g/with-auto-evaluation-context evaluation-context
     (let [project-id (g/node-value app-view :project-id evaluation-context)
           active-resource-node (g/node-value app-view :active-resource-node evaluation-context)
           open-resource-nodes (g/node-value app-view :open-resource-nodes evaluation-context)]
       (g/transact
         (concat
           (g/operation-sequence op-seq)
           (g/operation-label "Select")
           (project/sub-select project-id active-resource-node sub-selection open-resource-nodes)))))))

(defn- make-title
  ([] "Defold Editor 2.0")
  ([project-title] (str project-title " - " (make-title))))

(defn- refresh-app-title! [^Stage stage project]
  (let [settings      (g/node-value project :settings)
        project-title (settings ["project" "title"])
        new-title     (make-title project-title)]
    (when (not= (.getTitle stage) new-title)
      (.setTitle stage new-title))))

(defn- refresh-menus-and-toolbars! [app-view ^Scene scene]
  (let [keymap (g/node-value app-view :keymap)
        command->shortcut (keymap/command->shortcut keymap)]
    (ui/user-data! scene :command->shortcut command->shortcut)
    (ui/refresh scene)))

(defn- refresh-views! [app-view]
  (let [auto-pulls (g/node-value app-view :auto-pulls)]
    (doseq [[node label] auto-pulls]
      (profiler/profile "view" (:name @(g/node-type* node))
        (g/node-value node label)))))

(defn- refresh-scene-views! [app-view]
  (profiler/begin-frame)
  (doseq [view-id (g/node-value app-view :scene-view-ids)]
    (try
      (scene/refresh-scene-view! view-id)
      (catch Throwable error
        (error-reporting/report-exception! error))))
  (scene-cache/prune-context! nil))

(defn- dispose-scene-views! [app-view]
  (doseq [view-id (g/node-value app-view :scene-view-ids)]
    (try
      (scene/dispose-scene-view! view-id)
      (catch Throwable error
        (error-reporting/report-exception! error))))
  (scene-cache/drop-context! nil))

(defn- tab->resource-node [^Tab tab]
  (some-> tab
    (ui/user-data ::view)
    (g/node-value :view-data)
    second
    :resource-node))

(defn- tab->view-type [^Tab tab]
  (some-> tab (ui/user-data ::view-type) :id))

(defn- configure-editor-tab-pane! [^TabPane tab-pane ^Scene app-scene app-view]
  (.setTabClosingPolicy tab-pane TabPane$TabClosingPolicy/ALL_TABS)
  (.setTabDragPolicy tab-pane TabPane$TabDragPolicy/REORDER)
  (-> tab-pane
      (.getSelectionModel)
      (.selectedItemProperty)
      (.addListener
        (reify ChangeListener
          (changed [_this _observable _old-val new-val]
            (on-selected-tab-changed! app-view app-scene new-val (tab->resource-node new-val) (tab->view-type new-val))))))
  (-> tab-pane
      (.getTabs)
      (.addListener
        (reify ListChangeListener
          (onChanged [_this _change]
            ;; Check if we've ended up with an empty TabPane.
            ;; Unless we are the only one left, we should get rid of it to make room for the other TabPane.
            (when (empty? (.getTabs tab-pane))
              (let [editor-tabs-split ^SplitPane (ui/tab-pane-parent tab-pane)
                    tab-panes (.getItems editor-tabs-split)]
                (when (< 1 (count tab-panes))
                  (.remove tab-panes tab-pane)
                  (.requestFocus ^TabPane (.get tab-panes 0)))))))))
  (ui/register-tab-pane-context-menu tab-pane ::tab-menu))

(defn- handle-focus-owner-change! [app-view app-scene new-focus-owner]
  (let [old-editor-tab-pane (g/node-value app-view :active-tab-pane)
        new-editor-tab-pane (editor-tab-pane new-focus-owner)]
    (when (and (some? new-editor-tab-pane)
               (not (identical? old-editor-tab-pane new-editor-tab-pane)))
      (let [selected-tab (ui/selected-tab new-editor-tab-pane)
            resource-node (tab->resource-node selected-tab)
            view-type (tab->view-type selected-tab)]
        (ui/add-style! old-editor-tab-pane "inactive")
        (ui/remove-style! new-editor-tab-pane "inactive")
        (g/set-property! app-view :active-tab-pane new-editor-tab-pane)
        (on-selected-tab-changed! app-view app-scene selected-tab resource-node view-type)))))

(defn open-custom-keymap
  [path]
  (try (and (not= path "")
            (some-> path
                    slurp
                    edn/read-string))
       (catch Exception e
         (dialogs/make-info-dialog
          {:title "Couldn't load custom keymap config"
           :icon :icon/triangle-error
           :header {:fx/type fx.v-box/lifecycle
                    :children [{:fx/type fxui/label
                                :text (str "The keymap from " path " couldn't be opened.")}]}
           :content (.getMessage e)})
         (log/error :exception e)
         nil)))

(defn make-app-view [view-graph project ^Stage stage ^MenuBar menu-bar ^SplitPane editor-tabs-split ^TabPane tool-tab-pane prefs]
  (let [app-scene (.getScene stage)]
    (ui/disable-menu-alt-key-mnemonic! menu-bar)
    (.setUseSystemMenuBar menu-bar true)
    (.setTitle stage (make-title))
    (let [editor-tab-pane (TabPane.)
          keymap (or (open-custom-keymap (prefs/get-prefs prefs "custom-keymap-path" ""))
                     keymap/default-host-key-bindings)
          app-view (first (g/tx-nodes-added (g/transact (g/make-node view-graph AppView
                                                                     :stage stage
                                                                     :scene app-scene
                                                                     :editor-tabs-split editor-tabs-split
                                                                     :active-tab-pane editor-tab-pane
                                                                     :tool-tab-pane tool-tab-pane
                                                                     :active-tool :move
                                                                     :manip-space :world
                                                                     :keymap-config keymap))))]
      (.add (.getItems editor-tabs-split) editor-tab-pane)
      (configure-editor-tab-pane! editor-tab-pane app-scene app-view)
      (ui/observe (.focusOwnerProperty app-scene)
                  (fn [_ _ new-focus-owner]
                    (handle-focus-owner-change! app-view app-scene new-focus-owner)))

      (ui/register-menubar app-scene menu-bar ::menubar)

      (keymap/install-key-bindings! (.getScene stage) (g/node-value app-view :keymap))

      (let [refresh-timer (ui/->timer
                            "refresh-app-view"
                            (fn [_ _]
                              (when-not (ui/ui-disabled?)
                                (let [refresh-requested? (ui/user-data app-scene ::ui/refresh-requested?)]
                                  (when refresh-requested?
                                    (ui/user-data! app-scene ::ui/refresh-requested? false)
                                    (refresh-menus-and-toolbars! app-view app-scene)
                                    (refresh-views! app-view))
                                  (refresh-scene-views! app-view)
                                  (refresh-app-title! stage project)))))]
        (ui/timer-stop-on-closed! stage refresh-timer)
        (ui/timer-start! refresh-timer))
      (ui/on-closed! stage (fn [_] (dispose-scene-views! app-view)))
      app-view)))

(defn- make-tab! [app-view prefs workspace project resource resource-node
                  resource-type view-type make-view-fn ^ObservableList tabs opts]
  (let [parent     (AnchorPane.)
        tab        (doto (Tab. (tab-title resource false))
                     (.setContent parent)
                     (.setTooltip (Tooltip. (or (resource/proj-path resource) "unknown")))
                     (ui/user-data! ::view-type view-type))
        view-graph (g/make-graph! :history false :volatility 2)
        select-fn  (partial select app-view)
        opts       (merge opts
                          (get (:view-opts resource-type) (:id view-type))
                          {:app-view  app-view
                           :select-fn select-fn
                           :prefs     prefs
                           :project   project
                           :workspace workspace
                           :tab       tab})
        view       (make-view-fn view-graph parent resource-node opts)]
    (assert (g/node-instance? view/WorkbenchView view))
    (g/transact
      (concat
        (view/connect-resource-node view resource-node)
        (g/connect view :view-data app-view :open-views)
        (g/connect view :view-dirty? app-view :open-dirty-views)))
    (ui/user-data! tab ::view view)
    (.add tabs tab)
    (g/transact
      (select app-view resource-node [resource-node]))
    (.setGraphic tab (icons/get-image-view (or (:icon resource-type) "icons/64/Icons_29-AT-Unknown.png") 16))
    (.addAll (.getStyleClass tab) ^Collection (resource/style-classes resource))
    (ui/register-tab-toolbar tab "#toolbar" :toolbar)
    (let [close-handler (.getOnClosed tab)]
      (.setOnClosed tab (ui/event-handler event
                          ;; The menu refresh can occur after the view graph is
                          ;; deleted but before the tab controls lose input
                          ;; focus, causing handlers to evaluate against deleted
                          ;; graph nodes. Using run-later here prevents this.
                          (ui/run-later
                            (doto tab
                              (ui/user-data! ::view-type nil)
                              (ui/user-data! ::view nil))
                            (g/delete-graph! view-graph))
                          (when close-handler
                            (.handle close-handler event)))))
    tab))

(defn- substitute-args [tmpl args]
  (reduce (fn [tmpl [key val]]
            (string/replace tmpl (format "{%s}" (name key)) (str val)))
    tmpl args))

(defn open-resource
  ([app-view prefs workspace project resource]
   (open-resource app-view prefs workspace project resource {}))
  ([app-view prefs workspace project resource opts]
   (let [resource-type  (resource/resource-type resource)
         resource-node  (or (project/get-resource-node project resource)
                            (throw (ex-info (format "No resource node found for resource '%s'" (resource/proj-path resource))
                                            {})))
         text-view-type (workspace/get-view-type workspace :text)
         view-type      (or (:selected-view-type opts)
                            (if (nil? resource-type)
                              (placeholder-resource/view-type workspace)
                              (first (:view-types resource-type)))
                            text-view-type)]
     (if (resource-node/defective? resource-node)
       (do (dialogs/make-info-dialog
             {:title "Unable to Open Resource"
              :icon :icon/triangle-error
              :header (format "Unable to open '%s', since it contains unrecognizable data. Could the project be missing a required extension?" (resource/proj-path resource))})
           false)
       (if-let [custom-editor (and (#{:code :text} (:id view-type))
                                   (let [ed-pref (some->
                                                   (prefs/get-prefs prefs "code-custom-editor" "")
                                                   string/trim)]
                                     (and (not (string/blank? ed-pref)) ed-pref)))]
         (let [cursor-range (:cursor-range opts)
               arg-tmpl (string/trim (if cursor-range (prefs/get-prefs prefs "code-open-file-at-line" "{file}:{line}") (prefs/get-prefs prefs "code-open-file" "{file}")))
               arg-sub (cond-> {:file (resource/abs-path resource)}
                               cursor-range (assoc :line (CursorRange->line-number cursor-range)))
               args (->> (string/split arg-tmpl #" ")
                         (map #(substitute-args % arg-sub)))]
           (doto (ProcessBuilder. ^List (cons custom-editor args))
             (.directory (workspace/project-path workspace))
             (.start))
           false)
         (if (contains? view-type :make-view-fn)
           (let [^SplitPane editor-tabs-split (g/node-value app-view :editor-tabs-split)
                 tab-panes (.getItems editor-tabs-split)
                 open-tabs (mapcat #(.getTabs ^TabPane %) tab-panes)
                 view-type (if (g/node-value resource-node :editable?) view-type text-view-type)
                 make-view-fn (:make-view-fn view-type)
                 ^Tab tab (or (some #(when (and (= (tab->resource-node %) resource-node)
                                                (= view-type (ui/user-data % ::view-type)))
                                       %)
                                    open-tabs)
                              (let [^TabPane active-tab-pane (g/node-value app-view :active-tab-pane)
                                    active-tab-pane-tabs (.getTabs active-tab-pane)]
                                (make-tab! app-view prefs workspace project resource resource-node
                                           resource-type view-type make-view-fn active-tab-pane-tabs opts)))]
             (.select (.getSelectionModel (.getTabPane tab)) tab)
             (when-let [focus (:focus-fn view-type)]
               ;; Force layout pass since the focus function of some views
               ;; needs proper width + height (f.i. code view for
               ;; scrolling to selected line).
               (NodeHelper/layoutNodeForPrinting (.getRoot ^Scene (g/node-value app-view :scene)))
               (focus (ui/user-data tab ::view) opts))
             ;; Do an initial rendering so it shows up as fast as possible.
             (ui/run-later (refresh-scene-views! app-view)
                           (ui/run-later (slog/smoke-log "opened-resource")))
             true)
           (let [^String path (or (resource/abs-path resource)
                                  (resource/temp-path resource))
                 ^File f (File. path)]
             (ui/open-file f (fn [msg]
                               (ui/run-later
                                 (dialogs/make-info-dialog
                                   {:title "Could Not Open File"
                                    :icon :icon/triangle-error
                                    :header (format "Could not open '%s'" (.getName f))
                                    :content (str "This can happen if the file type is not mapped to an application in your OS.\n\nUnderlying error from the OS:\n" msg)}))))
             false)))))))

(handler/defhandler :open :global
  (active? [selection user-data] (:resources user-data (not-empty (selection->openable-resources selection))))
  (enabled? [selection user-data] (some resource/exists? (:resources user-data (selection->openable-resources selection))))
  (run [selection app-view prefs workspace project user-data]
       (doseq [resource (filter resource/exists? (:resources user-data (selection->openable-resources selection)))]
         (open-resource app-view prefs workspace project resource))))

(handler/defhandler :open-as :global
  (active? [selection] (selection->single-openable-resource selection))
  (enabled? [selection user-data] (resource/exists? (selection->single-openable-resource selection)))
  (run [selection app-view prefs workspace project user-data]
       (let [resource (selection->single-openable-resource selection)]
         (open-resource app-view prefs workspace project resource (when-let [view-type (:selected-view-type user-data)]
                                                                    {:selected-view-type view-type}))))
  (options [workspace selection user-data]
           (when-not user-data
             (let [resource (selection->single-openable-resource selection)
                   resource-type (resource/resource-type resource)]
               (map (fn [vt]
                      {:label     (or (:label vt) "External Editor")
                       :command   :open-as
                       :user-data {:selected-view-type vt}})
                    (:view-types resource-type))))))

(handler/defhandler :synchronize :global
  (enabled? [] (disk-availability/available?))
  (run [changes-view project workspace app-view]
       (let [render-reload-progress! (make-render-task-progress :resource-sync)
             render-save-progress! (make-render-task-progress :save-all)]
         (if (changes-view/project-is-git-repo? changes-view)

           ;; The project is a Git repo.
           ;; Check if there are locked files below the project folder before proceeding.
           ;; If so, we abort the sync and notify the user, since this could cause problems.
           (when (changes-view/ensure-no-locked-files! changes-view)
             (disk/async-save! render-reload-progress! render-save-progress! project changes-view
                               (fn [successful?]
                                 (when successful?
                                   (ui/user-data! (g/node-value app-view :scene) ::ui/refresh-requested? true)
                                   (when (changes-view/regular-sync! changes-view)
                                     (disk/async-reload! render-reload-progress! workspace [] changes-view))))))

           ;; The project is not a Git repo.
           ;; Show a dialog with info about how to set this up.
           (dialogs/make-info-dialog
             {:title "Version Control"
              :size :default
              :icon :icon/git
              :header "This project does not use Version Control"
              :content {:fx/type fx.text-flow/lifecycle
                        :style-class "dialog-content-padding"
                        :children [{:fx/type fx.text/lifecycle
                                    :text (str "A project under Version Control "
                                               "keeps a history of changes and "
                                               "enables you to collaborate with "
                                               "others by pushing changes to a "
                                               "server.\n\nYou can read about "
                                               "how to configure Version Control "
                                               "in the ")}
                                   {:fx/type fx.hyperlink/lifecycle
                                    :text "Defold Manual"
                                    :on-action (fn [_]
                                                 (ui/open-url "https://www.defold.com/manuals/version-control/"))}
                                   {:fx/type fx.text/lifecycle
                                    :text "."}]}})))))

(handler/defhandler :save-all :global
  (enabled? [] (not (bob/build-in-progress?)))
  (run [app-view changes-view project]
       (let [render-reload-progress! (make-render-task-progress :resource-sync)
             render-save-progress! (make-render-task-progress :save-all)]
         (disk/async-save! render-reload-progress! render-save-progress! project changes-view
                           (fn [successful?]
                             (when successful?
                               (ui/user-data! (g/node-value app-view :scene) ::ui/refresh-requested? true)))))))

(handler/defhandler :show-in-desktop :global
  (active? [app-view selection evaluation-context]
           (context-resource app-view selection evaluation-context))
  (enabled? [app-view selection evaluation-context]
            (when-let [r (context-resource app-view selection evaluation-context)]
              (and (resource/abs-path r)
                   (or (resource/exists? r)
                       (empty? (resource/path r))))))
  (run [app-view selection] (when-let [r (context-resource app-view selection)]
                              (let [f (File. (resource/abs-path r))]
                                (ui/open-file (fs/to-folder f))))))

(handler/defhandler :referencing-files :global
  (active? [app-view selection evaluation-context]
           (context-resource-file app-view selection evaluation-context))
  (enabled? [app-view selection evaluation-context]
            (when-let [r (context-resource-file app-view selection evaluation-context)]
              (and (resource/abs-path r)
                   (resource/exists? r))))
  (run [selection app-view prefs workspace project] (when-let [r (context-resource-file app-view selection)]
                                                      (doseq [resource (resource-dialog/make workspace project {:title "Referencing Files" :selection :multiple :ok-label "Open" :filter (format "refs:%s" (resource/proj-path r))})]
                                                        (open-resource app-view prefs workspace project resource)))))

(handler/defhandler :dependencies :global
  (active? [app-view selection evaluation-context]
           (context-resource-file app-view selection evaluation-context))
  (enabled? [app-view selection evaluation-context]
            (when-let [r (context-resource-file app-view selection evaluation-context)]
              (and (resource/abs-path r)
                   (resource/exists? r))))
  (run [selection app-view prefs workspace project] (when-let [r (context-resource-file app-view selection)]
                                                      (doseq [resource (resource-dialog/make workspace project {:title "Dependencies" :selection :multiple :ok-label "Open" :filter (format "deps:%s" (resource/proj-path r))})]
                                                        (open-resource app-view prefs workspace project resource)))))

(handler/defhandler :toggle-pane-left :global
  (run [^Stage main-stage]
       (let [main-scene (.getScene main-stage)]
         (set-pane-visible! main-scene :left (not (pane-visible? main-scene :left))))))

(handler/defhandler :toggle-pane-right :global
  (run [^Stage main-stage]
       (let [main-scene (.getScene main-stage)]
         (set-pane-visible! main-scene :right (not (pane-visible? main-scene :right))))))

(handler/defhandler :toggle-pane-bottom :global
  (run [^Stage main-stage]
       (let [main-scene (.getScene main-stage)]
         (set-pane-visible! main-scene :bottom (not (pane-visible? main-scene :bottom))))))

(handler/defhandler :show-console :global
  (run [^Stage main-stage tool-tab-pane] (show-console! (.getScene main-stage) tool-tab-pane)))

(handler/defhandler :show-curve-editor :global
  (run [^Stage main-stage tool-tab-pane] (show-curve-editor! (.getScene main-stage) tool-tab-pane)))

(handler/defhandler :show-build-errors :global
  (run [^Stage main-stage tool-tab-pane] (show-build-errors! (.getScene main-stage) tool-tab-pane)))

(handler/defhandler :show-search-results :global
  (run [^Stage main-stage tool-tab-pane] (show-search-results! (.getScene main-stage) tool-tab-pane)))

(defn- put-on-clipboard!
  [s]
  (doto (Clipboard/getSystemClipboard)
    (.setContent (doto (ClipboardContent.)
                   (.putString s)))))

(handler/defhandler :copy-project-path :global
  (active? [app-view selection evaluation-context]
           (context-resource-file app-view selection evaluation-context))
  (enabled? [app-view selection evaluation-context]
            (when-let [r (context-resource-file app-view selection evaluation-context)]
              (and (resource/proj-path r)
                   (resource/exists? r))))
  (run [selection app-view]
    (when-let [r (context-resource-file app-view selection)]
      (put-on-clipboard! (resource/proj-path r)))))

(handler/defhandler :copy-full-path :global
  (active? [app-view selection evaluation-context]
           (context-resource-file app-view selection evaluation-context))
  (enabled? [app-view selection evaluation-context]
            (when-let [r (context-resource-file app-view selection evaluation-context)]
              (and (resource/abs-path r)
                   (resource/exists? r))))
  (run [selection app-view]
    (when-let [r (context-resource-file app-view selection)]
      (put-on-clipboard! (resource/abs-path r)))))

(handler/defhandler :copy-require-path :global
  (active? [app-view selection evaluation-context]
           (when-let [r (context-resource-file app-view selection evaluation-context)]
             (= "lua" (resource/type-ext r))))
  (enabled? [app-view selection evaluation-context]
            (when-let [r (context-resource-file app-view selection evaluation-context)]
              (and (resource/proj-path r)
                   (resource/exists? r))))
  (run [selection app-view]
     (when-let [r (context-resource-file app-view selection)]
       (put-on-clipboard! (lua/path->lua-module (resource/proj-path r))))))


(defn- gen-tooltip [workspace project app-view resource]
  (let [resource-type (resource/resource-type resource)
        view-type (or (first (:view-types resource-type)) (workspace/get-view-type workspace :text))]
    (when-let [make-preview-fn (:make-preview-fn view-type)]
      {:fx/type fx.tooltip/lifecycle
       :graphic {:fx/type fx.image-view/lifecycle
                 :scale-y -1}
       :on-showing (fn [^Event e]
                     (let [^Tooltip tooltip (.getSource e)
                           image-view ^ImageView (.getGraphic tooltip)]
                       (when-not (.getImage image-view)
                         (let [resource-node (project/get-resource-node project resource)
                               view-graph (g/make-graph! :history false :volatility 2)
                               select-fn (partial select app-view)
                               opts (assoc ((:id view-type) (:view-opts resource-type))
                                      :app-view app-view
                                      :select-fn select-fn
                                      :project project
                                      :workspace workspace)
                               preview (make-preview-fn view-graph resource-node opts 256 256)]
                           (.setImage image-view ^Image (g/node-value preview :image))
                           (when-some [dispose-preview-fn (:dispose-preview-fn view-type)]
                             (dispose-preview-fn preview))
                           (g/delete-graph! view-graph)))))})))

(def ^:private open-assets-term-prefs-key "open-assets-term")

(defn- query-and-open! [workspace project app-view prefs term]
  (let [prev-filter-term (prefs/get-prefs prefs open-assets-term-prefs-key nil)
        filter-term-atom (atom prev-filter-term)
        selected-resources (resource-dialog/make workspace project
                                                 (cond-> {:title "Open Assets"
                                                          :accept-fn resource/editable-resource?
                                                          :selection :multiple
                                                          :ok-label "Open"
                                                          :filter-atom filter-term-atom
                                                          :tooltip-gen (partial gen-tooltip workspace project app-view)}
                                                         (some? term)
                                                         (assoc :filter term)))
        filter-term @filter-term-atom]
    (when (not= prev-filter-term filter-term)
      (prefs/set-prefs prefs open-assets-term-prefs-key filter-term))
    (doseq [resource selected-resources]
      (open-resource app-view prefs workspace project resource))))

(handler/defhandler :select-items :global
  (run [user-data] (dialogs/make-select-list-dialog (:items user-data) (:options user-data))))

(defn- get-view-text-selection [{:keys [view-id view-type]}]
  (when-let [text-selection-fn (:text-selection-fn view-type)]
    (text-selection-fn view-id)))

(handler/defhandler :open-asset :global
  (run [workspace project app-view prefs]
    (let [term (get-view-text-selection (g/node-value app-view :active-view-info))]
      (query-and-open! workspace project app-view prefs term))))

(handler/defhandler :search-in-files :global
  (run [project app-view prefs search-results-view main-stage tool-tab-pane]
    (when-let [term (get-view-text-selection (g/node-value app-view :active-view-info))]
      (search-results-view/set-search-term! prefs term))
    (let [main-scene (.getScene ^Stage main-stage)
          show-search-results-tab! (partial show-search-results! main-scene tool-tab-pane)]
      (search-results-view/show-search-in-files-dialog! search-results-view project prefs show-search-results-tab!))))

(defn- bundle! [main-stage tool-tab-pane changes-view build-errors-view project prefs platform bundle-options]
  (g/user-data! project :last-bundle-options (assoc bundle-options :platform-key platform))
  (let [main-scene (.getScene ^Stage main-stage)
        output-directory ^File (:output-directory bundle-options)
        render-build-error! (make-render-build-error main-scene tool-tab-pane build-errors-view)
        render-reload-progress! (make-render-task-progress :resource-sync)
        render-save-progress! (make-render-task-progress :save-all)
        render-build-progress! (make-render-task-progress :build)
        task-cancelled? (make-task-cancelled-query :build)
        bob-args (bob/bundle-bob-args prefs platform bundle-options)]
    (when-not (.exists output-directory)
      (fs/create-directories! output-directory))
    (build-errors-view/clear-build-errors build-errors-view)
    (disk/async-bob-build! render-reload-progress! render-save-progress! render-build-progress! task-cancelled?
                           render-build-error! bob/bundle-bob-commands bob-args project changes-view
                           (fn [successful?]
                             (when successful?
                               (if (some-> output-directory .isDirectory)
                                 (ui/open-file output-directory)
                                 (dialogs/make-info-dialog
                                   {:title "Bundle Failed"
                                    :icon :icon/triangle-error
                                    :size :large
                                    :header "Failed to bundle project, please fix build errors and try again"})))))))

(handler/defhandler :bundle :global
  (run [user-data workspace project prefs app-view changes-view build-errors-view main-stage tool-tab-pane]
    (let [owner-window (g/node-value app-view :stage)
          platform (:platform user-data)
          bundle! (partial bundle! main-stage tool-tab-pane changes-view build-errors-view project prefs platform)]
      (bundle-dialog/show-bundle-dialog! workspace platform prefs owner-window bundle!))))

(handler/defhandler :rebundle :global
  (enabled? [project] (some? (g/user-data project :last-bundle-options)))
  (run [workspace project prefs app-view changes-view build-errors-view main-stage tool-tab-pane]
    (let [last-bundle-options (g/user-data project :last-bundle-options)
          platform (:platform-key last-bundle-options)]
      (bundle! main-stage tool-tab-pane changes-view build-errors-view project prefs platform last-bundle-options))))

(def ^:private editor-extensions-allowed-commands-prefs-key
  "editor-extensions/allowed-commands")

(defn make-extensions-ui [workspace changes-view prefs]
  (reify extensions/UI
    (reload-resources! [_]
      (let [success-promise (promise)]
        (disk/async-reload! (make-render-task-progress :resource-sync)
                            workspace
                            []
                            changes-view
                            success-promise)
        (when-not @success-promise
          (throw (ex-info "Reload failed" {})))))
    (can-execute? [_ [cmd-name :as command]]
      (let [allowed-commands (prefs/get-prefs prefs editor-extensions-allowed-commands-prefs-key #{})]
        (if (allowed-commands cmd-name)
          true
          (let [allow (ui/run-now
                        (dialogs/make-confirmation-dialog
                          {:title "Allow executing shell command?"
                           :icon {:fx/type fxui/icon
                                  :type :icon/triangle-error
                                  :fill "#fa6731"}
                           :header "Extension wants to execute a shell command"
                           :content {:fx/type fxui/label
                                     :style-class "dialog-content-padding"
                                     :text (string/join " " command)}
                           :buttons [{:text "Abort Command"
                                      :cancel-button true
                                      :default-button true
                                      :result false}
                                     {:text "Allow"
                                      :variant :danger
                                      :result true}]}))]
            (when allow
              (prefs/set-prefs prefs editor-extensions-allowed-commands-prefs-key (conj allowed-commands cmd-name)))
            allow))))
    (display-output! [_ type string]
      (let [[console-type prefix] (case type
                                    :err [:extension-err "ERROR:EXT: "]
                                    :out [:extension-out ""])]
        (doseq [line (string/split-lines string)]
          (console/append-console-entry! console-type (str prefix line)))))
    (on-transact-thread [_ f]
      (ui/run-now (f)))))

(defn- fetch-libraries [workspace project changes-view prefs]
  (let [library-uris (project/project-dependencies project)
        hosts (into #{} (map url/strip-path) library-uris)]
    (if-let [first-unreachable-host (first-where (complement url/reachable?) hosts)]
      (dialogs/make-info-dialog
        {:title "Fetch Failed"
         :icon :icon/triangle-error
         :size :large
         :header "Fetch was aborted because a host could not be reached"
         :content (str "Unreachable host: " first-unreachable-host
                       "\n\nPlease verify internet connection and try again.")})
      (future
        (error-reporting/catch-all!
          (ui/with-progress [render-fetch-progress! (make-render-task-progress :fetch-libraries)]
            (when (workspace/dependencies-reachable? library-uris)
              (let [lib-states (workspace/fetch-and-validate-libraries workspace library-uris render-fetch-progress!)
                    render-install-progress! (make-render-task-progress :resource-sync)]
                (render-install-progress! (progress/make "Installing updated libraries..."))
                (ui/run-later
                  (workspace/install-validated-libraries! workspace library-uris lib-states)
                  (disk/async-reload! render-install-progress! workspace [] changes-view
                                      (fn [success]
                                        (when success
                                          (extensions/reload! project :library (make-extensions-ui workspace changes-view prefs))))))))))))))

(handler/defhandler :add-dependency :global
  (enabled? [] (disk-availability/available?))
  (run [selection app-view prefs workspace project changes-view user-data]
       (let [game-project (project/get-resource-node project "/game.project")
             dependencies (game-project/get-setting game-project ["project" "dependencies"])
             dependency-uri (.toURI (URL. (:dep-url user-data)))]
         (when (not-any? (partial = dependency-uri) dependencies)
           (game-project/set-setting! game-project ["project" "dependencies"]
                                      (conj (vec dependencies) dependency-uri))
           (fetch-libraries workspace project changes-view prefs)))))

(handler/defhandler :fetch-libraries :global
  (enabled? [] (disk-availability/available?))
  (run [workspace project changes-view prefs]
       (fetch-libraries workspace project changes-view prefs)))

(handler/defhandler :reload-extensions :global
  (enabled? [] (disk-availability/available?))
  (run [project workspace changes-view prefs]
       (extensions/reload! project :all (make-extensions-ui workspace changes-view prefs))))

(defn- ensure-exists-and-open-for-editing! [proj-path app-view changes-view prefs project]
  (let [workspace (project/workspace project)
        resource (workspace/resolve-workspace-resource workspace proj-path)]
    (if (resource/exists? resource)
      (open-resource app-view prefs workspace project resource)
      (let [render-reload-progress! (make-render-task-progress :resource-sync)]
        (fs/touch-file! (io/as-file resource))
        (disk/async-reload! render-reload-progress! workspace [] changes-view
                            (fn [successful?]
                              (when successful?
                                (when-some [created-resource (workspace/find-resource workspace proj-path)]
                                  (open-resource app-view prefs workspace project created-resource)))))))))

(handler/defhandler :live-update-settings :global
  (enabled? [] (disk-availability/available?))
  (run [app-view changes-view prefs workspace project]
       (let [live-update-settings-proj-path (live-update-settings/get-live-update-settings-path project)]
         (ensure-exists-and-open-for-editing! live-update-settings-proj-path app-view changes-view prefs project))))

(handler/defhandler :shared-editor-settings :global
  (enabled? [] (disk-availability/available?))
  (run [app-view changes-view prefs workspace project]
       (ensure-exists-and-open-for-editing! shared-editor-settings/project-shared-editor-settings-proj-path app-view changes-view prefs project)))
