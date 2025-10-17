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

(ns editor.app-view
  (:require [cljfx.api :as fx]
            [cljfx.fx.hyperlink :as fx.hyperlink]
            [cljfx.fx.image-view :as fx.image-view]
            [cljfx.fx.text :as fx.text]
            [cljfx.fx.text-flow :as fx.text-flow]
            [cljfx.fx.tooltip :as fx.tooltip]
            [cljfx.fx.v-box :as fx.v-box]
            [clojure.java.io :as io]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.build :as build]
            [editor.build-errors-view :as build-errors-view]
            [editor.code.data :as data :refer [CursorRange->line-number]]
            [editor.console :as console]
            [editor.debug-view :as debug-view]
            [editor.defold-project :as project]
            [editor.dialogs :as dialogs]
            [editor.disk :as disk]
            [editor.disk-availability :as disk-availability]
            [editor.editor-extensions :as extensions]
            [editor.editor-localization-bundle :as editor-localization-bundle]
            [editor.engine :as engine]
            [editor.engine.build-errors :as engine-build-errors]
            [editor.engine.native-extensions :as native-extensions]
            [editor.error-reporting :as error-reporting]
            [editor.fs :as fs]
            [editor.future :as future]
            [editor.fxui :as fxui]
            [editor.game-project :as game-project]
            [editor.git :as git]
            [editor.github :as github]
            [editor.graph-util :as gu]
            [editor.grid :as grid]
            [editor.handler :as handler]
            [editor.hot-reload :as hot-reload]
            [editor.icons :as icons]
            [editor.keymap :as keymap]
            [editor.live-update-settings :as live-update-settings]
            [editor.localization :as localization]
            [editor.lsp :as lsp]
            [editor.lua :as lua]
            [editor.menu-items :as menu-items]
            [editor.os :as os]
            [editor.pipeline :as pipeline]
            [editor.pipeline.bob :as bob]
            [editor.prefs :as prefs]
            [editor.prefs-dialog :as prefs-dialog]
            [editor.process :as process]
            [editor.progress :as progress]
            [editor.properties :as properties]
            [editor.recent-files :as recent-files]
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
            [internal.graph.types :as gt]
            [internal.util :refer [first-where]]
            [service.log :as log]
            [service.smoke-log :as slog]
            [util.coll :as coll :refer [pair]]
            [util.eduction :as e]
            [util.fn :as fn]
            [util.http-server :as http-server]
            [util.profiler :as profiler]
            [util.text-util :as text-util]
            [util.thread-util :as thread-util])
  (:import [com.defold.editor Editor]
           [com.defold.editor UIUtil]
           [com.dynamo.bob Platform]
           [com.sun.javafx.scene NodeHelper]
           [java.io File PipedInputStream PipedOutputStream]
           [java.net URL]
           [java.util Arrays Collection List]
           [java.util.concurrent ExecutionException]
           [javafx.beans.value ChangeListener]
           [javafx.collections ListChangeListener ObservableList]
           [javafx.event Event]
           [javafx.geometry HPos Orientation Pos]
           [javafx.scene Node Parent Scene]
           [javafx.scene.control Hyperlink Label MenuBar SplitPane Tab TabPane TabPane$TabClosingPolicy TabPane$TabDragPolicy Tooltip]
           [javafx.scene.image Image ImageView]
           [javafx.scene.input Clipboard ClipboardContent MouseButton MouseEvent]
           [javafx.scene.layout AnchorPane GridPane HBox Priority Region StackPane VBox]
           [javafx.scene.paint Color]
           [javafx.scene.shape Ellipse]
           [javafx.scene.text Font]
           [javafx.stage Screen Stage WindowEvent]
           [org.luaj.vm2 LuaError]))

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
            :split-id "center-split"}
   :changed-files {:index 1
                   :pane-id "changed-files-pane"
                   :split-id "assets-split"}})

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
  ^String [resource dirty]
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
    (if dirty
      (str "*" escaped-resource-name)
      escaped-resource-name)))

(defn- update-quick-help-pane [^SplitPane editor-tabs-split keymap]
  (let [tab-panes (.getItems editor-tabs-split)
        is-empty (not-any? #(-> ^TabPane % .getTabs count pos?) tab-panes)
        parent (.getParent editor-tabs-split)
        quick-help-box (.lookup parent "#quick-help-box")
        ^GridPane grid-pane (.lookup parent "#quick-help-items")]

    ;; Only make quick help visible when there is no-tabs.
    (.setVisible quick-help-box is-empty)

    (when is-empty
      (let [label-font (Font. "Dejavu Sans Mono" 13)
            key-font (Font. "" 13)
            color (Color. 1.0 1.0 0.59765625 0.6)]
        (.clear (.getChildren grid-pane))
        (->> [[:file.open "Open Asset"]
              [:file.reopen-recent "Re-Open Closed File"]
              [:file.search "Search in Files"]
              [:project.build "Build and Run Project"]
              [:debugger.start "Start or Attach Debugger"]]
             (e/keep (fn [[command label]]
                       (when-let [display-text (keymap/display-text keymap command nil)]
                         (coll/pair label display-text))))
             (e/map-indexed coll/pair)
             (run! (fn [[row [label display-text]]]
                     (doto grid-pane
                       (.add (doto (Label. label)
                               (.setFont label-font)
                               (GridPane/setHalignment HPos/RIGHT)
                               (.setTextFill color))
                             0 row)
                       (.add (doto (Label. display-text)
                               (.setFont key-font)
                               (.setAlignment Pos/CENTER)
                               (.setTextFill color)
                               (-> .getStyleClass (.add "key-button")))
                             1 row)))))))))

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
  (property keymap g/Any)

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
  (output refresh-tab-panes g/Any :cached (g/fnk [^SplitPane editor-tabs-split open-views open-dirty-views keymap]
                                            (let [tab-panes (.getItems editor-tabs-split)]
                                              (update-quick-help-pane editor-tabs-split keymap)

                                              (doseq [^TabPane tab-pane tab-panes
                                                      ^Tab tab (.getTabs tab-pane)
                                                      :let [view (ui/user-data tab ::view)
                                                            resource (:resource (get open-views view))
                                                            dirty (contains? open-dirty-views view)
                                                            title (tab-title resource dirty)]]
                                                (ui/text! tab title)))))
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

(defn- context-openable-resource
  ([app-view selection]
   (when-let [resource (or (selection->single-resource selection)
                           (g/node-value app-view :active-resource))]
     (when (resource/openable-resource? resource)
       resource)))
  ([app-view selection evaluation-context]
   (when-let [resource (or (selection->single-resource selection)
                           (g/node-value app-view :active-resource evaluation-context))]
     (when (resource/openable-resource? resource)
       resource))))

(defn- context-resource
  ([app-view selection]
   (or (selection->single-resource selection)
       (g/node-value app-view :active-resource)))
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

(handler/defhandler :scene.select-move-tool :workbench
  (run [app-view] (g/transact (g/set-property app-view :active-tool :move)))
  (state [app-view] (= (g/node-value app-view :active-tool) :move)))

(handler/defhandler :scene.select-scale-tool :workbench
  (run [app-view] (g/transact (g/set-property app-view :active-tool :scale)))
  (state [app-view] (= (g/node-value app-view :active-tool) :scale)))

(handler/defhandler :scene.select-rotate-tool :workbench
  (run [app-view] (g/transact (g/set-property app-view :active-tool :rotate)))
  (state [app-view] (= (g/node-value app-view :active-tool) :rotate)))

(handler/defhandler :scene.visibility.show-settings :workbench
  (run [app-view scene-visibility]
    (when-let [btn (some-> ^TabPane (g/node-value app-view :active-tab-pane)
                           ui/selected-tab
                           .getContent
                           (.lookup "#visibility-settings-graphic")
                           .getParent)]
      (scene-visibility/show-visibility-settings! app-view btn scene-visibility)))
  (state [app-view scene-visibility]
    (when-let [btn (some-> ^TabPane (g/node-value app-view :active-tab-pane)
                           ui/selected-tab
                           .getContent
                           (.lookup "#visibility-settings-graphic")
                           .getParent)]
      ;; TODO: We have no mechanism for updating the style nor icon on
      ;; on the toolbar button. For now we piggyback on the state
      ;; update polling to set a style when the filters are active.
      (if (scene-visibility/filters-appear-active? scene-visibility)
        (ui/add-style! btn "filters-active")
        (ui/remove-style! btn "filters-active"))
      (scene-visibility/settings-visible? btn))))

(defn get-grid-settings-button
  [app-view]
  (some-> ^TabPane (g/node-value app-view :active-tab-pane)
          ui/selected-tab
          .getContent
          (.lookup "#show-grid-settings")))

(handler/defhandler :scene.grid.show-settings :workbench
  (run [app-view scene-visibility prefs]
       (when-let [btn (get-grid-settings-button app-view)]
         (grid/show-settings! app-view btn prefs)))
  (state [app-view scene-visibility]
         (when-let [btn (get-grid-settings-button app-view)]
           (scene-visibility/settings-visible? btn))))

(def ^:private eye-icon-svg-path
  (ui/load-svg-path "scene/images/eye_icon_eye_arrow.svg"))

(def ^:private perspective-icon-svg-path
  (ui/load-svg-path "scene/images/perspective_icon.svg"))

(def ^:private mode-2d-svg-path
  (ui/load-svg-path "scene/images/2d-mode.svg"))

(def ^:private grid-svg-path
  (ui/load-svg-path "scene/images/grid.svg"))

(defn- make-visibility-settings-graphic []
  (doto (StackPane.)
    (.setId "visibility-settings-graphic")
    (ui/children! [(doto (icons/make-svg-icon-graphic eye-icon-svg-path)
                     (.setId "eye-icon"))
                   (doto (Ellipse. 3.0 3.0)
                     (.setId "active-indicator"))])))

(handler/register-menu! :toolbar
  [{:id :select
    :tooltip "Select tool"
    :icon "icons/45/Icons_T_01_Select.png"
    :command :scene.select-select-tool}
   {:id :move
    :tooltip "Move tool"
    :icon "icons/45/Icons_T_02_Move.png"
    :command :scene.select-move-tool}
   {:id :rotate
    :tooltip "Rotate tool"
    :icon "icons/45/Icons_T_03_Rotate.png"
    :command :scene.select-rotate-tool}
   {:id :scale
    :tooltip "Scale tool"
    :icon "icons/45/Icons_T_04_Scale.png"
    :command :scene.select-scale-tool}
   menu-items/separator
   {:id :grid
    :tooltip "Grid"
    :graphic-fn (partial icons/make-svg-icon-graphic grid-svg-path)
    :command :scene.visibility.toggle-grid
    :more {:id :show-grid-settings
           :command :scene.grid.show-settings}}
   {:id :2d-mode
    :tooltip "2d mode"
    :graphic-fn (partial icons/make-svg-icon-graphic mode-2d-svg-path)
    :command :scene.toggle-interaction-mode}
   {:id :perspective-camera
    :tooltip "Perspective camera"
    :graphic-fn (partial icons/make-svg-icon-graphic perspective-icon-svg-path)
    :command :scene.toggle-camera-type}
   {:id :visibility-settings
    :tooltip "Visibility settings"
    :graphic-fn make-visibility-settings-graphic
    :command :scene.visibility.show-settings}])

(def ^:const prefs-window-dimensions [:window :dimensions])
(def ^:const prefs-split-positions [:window :split-positions])
(def ^:const prefs-hidden-panes [:window :hidden-panes])

(handler/defhandler :app.quit :global
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
    (prefs/set! prefs prefs-window-dimensions dims)))

(defn restore-window-dimensions [^Stage stage prefs]
  (when-let [dims (prefs/get prefs prefs-window-dimensions)]
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
  (if-some [split-positions (prefs/get prefs prefs-split-positions)]
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
    (prefs/set! prefs prefs-split-positions split-positions)))

(defn restore-split-positions! [^Scene scene prefs]
  (let [split-positions (stored-split-positions prefs)
        split-panes (existing-split-panes scene)]
    ;; The nested run-later fixes restore on Linux, by forcing an initial rendering pass. 
    (ui/run-later
      (doseq [[id positions] split-positions]
        (when-some [^SplitPane split-pane (get split-panes id)]
          (.setDividerPositions split-pane (double-array positions))
          (.layout split-pane))))))

(defn stored-hidden-panes [prefs]
  (prefs/get prefs prefs-hidden-panes))

(defn store-hidden-panes! [^Scene scene prefs]
  (let [hidden-panes (into #{}
                           (remove (partial pane-visible? scene))
                           (keys split-info-by-pane-kw))]
    (prefs/set! prefs prefs-hidden-panes hidden-panes)))

(defn restore-hidden-panes! [^Scene scene prefs]
  (let [hidden-panes (stored-hidden-panes prefs)]
    (doseq [pane-kw hidden-panes]
      (set-pane-visible! scene pane-kw false))))

(handler/defhandler :app.preferences :global
  (run [workspace prefs app-view localization]
    (prefs-dialog/open! prefs localization)
    (workspace/update-build-settings! workspace prefs)
    (let [new-keymap (keymap/from-prefs prefs)]
      (when-not (= new-keymap (g/raw-property-value (g/now) app-view :keymap))
        (g/set-property! app-view :keymap new-keymap)))
    (ui/invalidate-menubar-item! ::file)))

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

(defn- local-url [target web-server]
  (format "http://%s:%s%s" (:local-address target) (http-server/port web-server) hot-reload/url-prefix))

(def ^:private app-task-progress
  {:main (ref progress/done)
   :build (ref progress/done)
   :resource-sync (ref progress/done)
   :save-all (ref progress/done)
   :fetch-libraries (ref progress/done)
   :download-update (ref progress/done)})

(declare ^:private render-task-progress!)

(defn- cancel-task!
  [task-key]
  (dosync
    (let [progress-ref (task-key app-task-progress)]
      (render-task-progress! task-key (progress/cancel @progress-ref)))))

(def ^:private app-task-ui-priority
  "Task priority in descending order (from highest to lowest)"
  [:save-all :resource-sync :fetch-libraries :build :download-update :main])

(def ^:private render-task-progress-ui-inflight (ref false))

(def status-bar-controls-delay
  (delay
    (ui/collect-controls
      (.. (ui/main-stage) (getScene) (getRoot) (lookup "#status-bar"))
      ["progress-bar" "progress-hbox" "progress-percentage-label" "status-label" "progress-cancel-button"])))

(defn- render-task-progress-ui! []
  (let [task-progress-snapshot (ref nil)]
    (dosync
      (ref-set render-task-progress-ui-inflight false)
      (ref-set task-progress-snapshot
               (into {} (map (juxt first (comp deref second))) app-task-progress)))
    (let [[key progress] (->> app-task-ui-priority
                              (map (juxt identity @task-progress-snapshot))
                              (filter (comp (complement progress/done?) second))
                              first)
          show-progress-hbox? (boolean (and (not= key :main)
                                            progress
                                            (not (progress/done? progress))))
          localization (ui/user-data (.getScene (ui/main-stage)) :localization)
          {:keys [progress-bar progress-hbox progress-percentage-label status-label progress-cancel-button]} @status-bar-controls-delay]
      (ui/render-progress-message!
        (if key progress (@task-progress-snapshot :main))
        status-label
        localization)
      ;; The bottom right of the status bar can show either the progress-hbox
      ;; or the update-link, or both. The progress-hbox will cover
      ;; the update-link if both are visible.
      (if-not show-progress-hbox?
        (ui/visible! progress-hbox false)
        (do
          (ui/visible! progress-hbox true)
          (ui/render-progress-bar! progress progress-bar)
          (ui/render-progress-percentage! progress progress-percentage-label localization)
          (if (progress/cancellable? progress)
            (doto progress-cancel-button
              (ui/visible! true)
              (ui/managed! true)
              (ui/on-action! (fn [_] (cancel-task! key))))
            (doto progress-cancel-button
              (ui/visible! false)
              (ui/managed! false)
              (ui/on-action! identity))))))))

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

(defn begin-task-progress! [key]
  (let [progress-ref (get app-task-progress key)
        prev-progress-atom (atom nil)]
    (assert (some? progress-ref))
    (pair
      (fn render-progress! [progress]
        ;; Combined throttling and inheritance of cancel state.
        ;; The first call to render-progress! overwrites the cancel state of the
        ;; progress-ref, and then subsequent calls will inherit the cancel state
        ;; from the progress-ref. This is to ensure we see changes to the
        ;; progress-refs cancel state from the cancel-task! function.
        (let [prev-progress @prev-progress-atom
              progress (cond-> progress
                               prev-progress
                               (progress/with-inherited-cancel-state @progress-ref))]
          (when (progress/relevant-change? prev-progress progress)
            (reset! prev-progress-atom progress)
            (render-task-progress! key progress))))

      (fn task-cancelled? []
        (progress/cancelled? @progress-ref)))))

(defn render-main-task-progress! [progress]
  (render-task-progress! :main progress))

(defn- report-build-launch-progress! [message]
  (render-main-task-progress! (progress/make message)))

(defn- clear-build-launch-progress! []
  (render-main-task-progress! progress/done))

(def ^:private build-in-progress-atom (atom false))

(defn- build-in-progress? []
  @build-in-progress-atom)

(declare async-save!)

(defn- async-reload-on-app-focus? [prefs]
  (prefs/get prefs [:workflow :load-external-changes-on-app-focus]))

(defn- auto-save-on-app-unfocus? [prefs]
  (prefs/get prefs [:workflow :save-on-app-focus-lost]))

(defn- can-async-reload? []
  (and (disk-availability/available?)
       (not (build-in-progress?))))

(defn- can-async-save? []
  (and (disk-availability/available?)
       (not (bob/build-in-progress?))))

(defn async-reload!
  [app-view changes-view workspace moved-files]
  (let [render-reload-progress! (make-render-task-progress :resource-sync)]
    (disk/async-reload! render-reload-progress! workspace moved-files changes-view
                        (fn [_success]
                          (ui/user-data! (g/node-value app-view :scene) ::ui/refresh-requested? true)))))

(defn handle-application-focused! [app-view changes-view workspace prefs]
  (when (and (disk-availability/available?)
             (not (build-in-progress?)))
    (clear-build-launch-progress!))
  (when (and (async-reload-on-app-focus? prefs)
             (can-async-reload?))
    (async-reload! app-view changes-view workspace [])))

(defn handle-application-unfocused!
  [app-view changes-view project prefs]
  (when (and (auto-save-on-app-unfocus? prefs)
             (can-async-save?))
    (async-save! app-view changes-view project project/dirty-save-data)))

(defn- decorate-target [engine-descriptor target]
  (assoc target :engine-id (:id engine-descriptor)))

(defn- launch-engine! [engine-descriptor project-directory prefs debug?]
  (try
    (report-build-launch-progress! (localization/message "progress.launching-engine"))
    (let [engine (engine/install-engine! project-directory engine-descriptor)
          count (prefs/get prefs [:run :instance-count])
          pause-ms 100
          instance-index-range (if (= count 1) (range (inc 0)) (range 1 (inc count)))
          launched-targets (for [instance-index instance-index-range]
                             (let [last-instance? (or (= count 1) (= instance-index count))
                                   instance-debug? (and debug? last-instance?)
                                   launched-target (->> (engine/launch! engine project-directory prefs instance-debug? instance-index)
                                                        (decorate-target engine-descriptor)
                                                        (targets/add-launched-target! instance-index))]
                               (when (not last-instance?)
                                 (Thread/sleep pause-ms))        ;pause needed to make sure the launch order of instances is right
                               launched-target))
          last-launched-target (last launched-targets)]
      (if (= count 1)
        (targets/select-target! prefs last-launched-target)
        (targets/select-target! prefs {:id :all-launched-targets}))
      (report-build-launch-progress! (localization/message "progress.launched-engine" {"engine" (targets/target-message last-launched-target)}))
      launched-targets)
    (catch Exception e
      (targets/kill-launched-targets!)
      (report-build-launch-progress! (localization/message "progress.engine-launch-failed"))
      (throw e))))

(defn- make-launched-log-sink [launched-target on-service-url-found]
  (let [initial-output (atom "")
        version-line (atom nil)
        updated-target (atom nil)]
    (fn [line]
      (when (< (count @initial-output) 5000)
        (swap! initial-output str line "\n")
        (when-let [target-info (engine/parse-launched-target-info @initial-output)]
          (let [result-target (targets/update-launched-target! launched-target target-info)]
            (reset! updated-target result-target)))
        (when (not @version-line)
          (when-let [engine-version-line (engine/parse-engine-version-line line)]
            (reset! version-line engine-version-line))))
      ;; After the version line, wait briefly for stream readiness, then call the callback.
      (when (and @updated-target (= @version-line line))
        (future
          (let [max-wait-ms 2000
                step-ms 100
                deadline (+ (System/currentTimeMillis) max-wait-ms)]
            (loop []
              (if (or (console/current-stream? (:log-stream @updated-target))
                      (>= (System/currentTimeMillis) deadline))
                (on-service-url-found @updated-target)
                (do
                  (Thread/sleep step-ms)
                  (recur)))))))
      (when (console/current-stream? (:log-stream launched-target))
        (console/append-console-line! line)))))

(defn- reboot-engine! [target web-server debug?]
  (try
    (report-build-launch-progress!
      (localization/message "progress.rebooting-engine" {"engine" (targets/target-message target)}))
    (engine/reboot! target (local-url target web-server) debug?)
    (report-build-launch-progress!
      (localization/message "progress.rebooted-engine" {"engine" (targets/target-message target)}))
    target
    (catch Exception e
      (report-build-launch-progress! (localization/message "progress.engine-reboot-failed"))
      (throw e))))

(defn- on-launched-hook! [project process url]
  (let [hook-opts {:url url}]
    (future
      (error-reporting/catch-all!
        @(extensions/execute-hook! project :on_target_launched hook-opts :exception-policy :ignore)
        (process/on-exit! process #(extensions/execute-hook! project :on_target_terminated hook-opts :exception-policy :ignore))))))

(defn- target-cannot-swap-engine? [target]
  (and (some? target)
       (targets/controllable-target? target)
       (targets/remote-target? target)))

(defn- on-service-url-found [prefs target]
  (engine/apply-simulated-resolution! prefs target))

(defn- launch-built-project! [project engine-descriptor project-directory prefs web-server debug?]
  (let [selected-target (targets/selected-target prefs)
        launch-new-engine! (fn []
                             (targets/kill-launched-targets!)
                             (let [launched-targets (launch-engine! engine-descriptor project-directory prefs debug?)
                                   last-launched-target (last launched-targets)]
                               (doseq [launched-target launched-targets]
                                 (targets/when-url (:id launched-target)
                                                   #(on-launched-hook! project (:process launched-target) %))
                                 (let [log-stream (:log-stream launched-target)]
                                   (console/reset-console-stream! log-stream)
                                   (console/reset-remote-log-pump-thread! nil)
                                   (console/start-log-pump! log-stream (make-launched-log-sink launched-target (partial on-service-url-found prefs)))))
                               last-launched-target))]
    (try
      (cond
        (or (not selected-target) (targets/all-launched-targets? selected-target))
        (launch-new-engine!)

        (not (targets/controllable-target? selected-target))
        (do
          (assert (targets/launched-target? selected-target))
          (launch-new-engine!))

        (target-cannot-swap-engine? selected-target)
        (let [log-stream (engine/get-log-service-stream selected-target)]
          (when log-stream
            (console/set-log-service-stream log-stream))
          (reboot-engine! selected-target web-server debug?))

        :else
        (do
          (assert (and (targets/controllable-target? selected-target) (targets/launched-target? selected-target)))
          (if (= (:id engine-descriptor) (:engine-id selected-target))
            (do
              ;; We're running "the same" engine and can reuse the
              ;; running process by rebooting
              (console/reset-console-stream! (:log-stream selected-target))
              (console/reset-remote-log-pump-thread! nil)
              ;; Launched target log pump already
              ;; running to keep engine process
              ;; from halting because stdout/err is
              ;; not consumed.
              (reboot-engine! selected-target web-server debug?))
            (launch-new-engine!))))
      (catch Exception e
        (log/warn :exception e)
        (let [localization (g/with-auto-evaluation-context evaluation-context
                             (workspace/localization (project/workspace project evaluation-context) evaluation-context))]
          (dialogs/make-info-dialog
            localization
            {:title (localization/message "dialog.launch-failed.title")
             :icon :icon/triangle-error
             :header {:fx/type fx.v-box/lifecycle
                      :children [{:fx/type fxui/legacy-label
                                  :variant :header
                                  :text (localization
                                          (localization/message
                                            "dialog.launch-failed.header"
                                            {"engine" (if (some? selected-target)
                                                        (targets/target-message selected-target)
                                                        (localization/message "dialog.launch-failed.target.new-local-engine"))}))}
                                 {:fx/type fxui/legacy-label
                                  :text (localization (localization/message "dialog.launch-failed.detail"))}]}
             :content (.getMessage e)}))))))

(defn- get-cycle-detected-help-message [basis node-id]
  (let [proj-path (some-> (resource-node/owner-resource basis node-id) resource/proj-path)
        resource-path (or proj-path "'unknown'")]
    (case (g/node-type-kw basis node-id)
      :editor.collection/CollectionNode
      (localization/message "dialog.build-error.cycle.detail.collection" {"resource" resource-path})

      :editor.game-object/GameObjectNode
      (localization/message "dialog.build-error.cycle.detail.game-object" {"resource" resource-path})

      :editor.code.script/ScriptNode
      (localization/message "dialog.build-error.cycle.detail.script" {"resource" resource-path})

      (localization/message "dialog.build-error.cycle.detail.generic" {"resource" resource-path}))))

(defn- ex-root-cause [exception]
  (if-let [cause (ex-cause exception)]
    (recur cause)
    exception))

(defn- build-project!
  [project old-artifact-map opts evaluation-context]
  (let [game-project (project/get-resource-node project "/game.project" evaluation-context)
        render-progress! (or (:render-progress! opts)
                             progress/null-render-progress!)]
    (try
      (ui/with-progress [render-progress! render-progress!]
        (build/build-project! project game-project old-artifact-map opts evaluation-context))
      (catch Throwable error
        (let [error (if (instance? ExecutionException error)
                      (ex-cause error)
                      error)
              cause (ex-root-cause error)
              cause-ex-data (ex-data cause)
              ex-type (:ex-type cause-ex-data)
              basis (:basis evaluation-context)
              localization (workspace/localization (project/workspace project evaluation-context) evaluation-context)]
          (case ex-type
            :task-cancelled
            nil ; We'll just produce an error signaling that the build was cancelled below.

            :cycle-detected
            (ui/run-later
              (dialogs/make-info-dialog
                localization
                {:title (localization/message "dialog.build-error.title")
                 :icon :icon/triangle-error
                 :header (localization/message "dialog.build-error.cycle.header")
                 :content {:wrap-text true
                           :text (get-cycle-detected-help-message basis (-> cause-ex-data :endpoint gt/endpoint-node-id))}}))

            ;; Default case.
            (error-reporting/report-exception! error))
          {:error (cond
                    (= :task-cancelled ex-type)
                    {:severity :fatal
                     :message (ex-message cause)}

                    (= :cycle-detected ex-type)
                    {:severity :fatal
                     :message (localization
                                (localization/message
                                  "dialog.build-error.cycle.message"
                                  {"header" (localization/message "dialog.build-error.cycle.header")
                                   "detail" (get-cycle-detected-help-message basis (-> cause-ex-data :endpoint gt/endpoint-node-id))}))}

                    (pipeline/decorated-build-exception? error)
                    (let [{:keys [node-id owner-resource-node-id]} (ex-data error)]
                      (g/map->error {:_node-id owner-resource-node-id
                                     :severity :fatal
                                     :causes [{:_node-id node-id
                                               :severity :fatal
                                               :message (str (ex-message error)
                                                             \newline
                                                             (ex-message (ex-cause error)))}]}))

                    :else
                    {:severity :fatal
                     :message (or (ex-message error)
                                  (.getName (class error)))})})))))

(defn async-build!
  "Asynchronously build the project and notify the :result-fn with results

  Kv-args:
    :result-fn           required fn that will receive build results, a map with
                         the following keys:
                         * :artifacts, :artifact-map and :etags - results for
                           successfully built project resources
                         * :engine - engine descriptor map when asked to build
                           the engine, and it was successfully built
                         * :error - error value in case of any errors, be it
                           project resources, linting or engine build error
                         * :warning - error value in case there are non-critical
                           issues reported by the build process
    :build-engine        optional flag that indicates whether the engine should
                         be built in addition to the project
    :lint                optional flag that indicates whether to run LSP lints
                         and present the diagnostics alongside the build errors,
                         defaults to the value of \"general-lint-on-build\" pref
                         (true if not set)
    :prefs               required, preferences for linting and engine building,
                         e.g. the build server settings
    :debug               optional flag that indicates whether to also build
                         debugging tools
    :run-build-hooks     optional flag that indicates whether to run pre- and
                         post-build hooks
    :render-progress!    optional progress reporter fn
    :task-cancelled?     optional fn that will be called periodically to check
                         if the user has cancelled the build process.
    :old-artifact-map    optional old artifact map with previous build results
                         to speed up the build process"
  [project & {:keys [;; required
                     result-fn
                     prefs
                     ;; optional
                     debug build-engine run-build-hooks render-progress! task-cancelled? old-artifact-map lint]
              :or {debug false
                   build-engine true
                   run-build-hooks true
                   render-progress! progress/null-render-progress!
                   task-cancelled? fn/constantly-false
                   old-artifact-map {}}}]
  {:pre [(ifn? result-fn)
         (or (not build-engine) (some? prefs))]}
  (let [lint (if (nil? lint)
               (prefs/get prefs [:build :lint-code])
               lint)
        ;; After any pre-build hooks have completed successfully, we will start
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
          (when build-engine
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

        lint-promise (promise)

        start-lint!
        (fn start-lint! []
          (when lint (lsp/pull-workspace-diagnostics! (lsp/get-node-lsp project) lint-promise)))

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
                      (render-progress! progress/done)
                      (cancel-engine-build!)
                      (throw error)))))
              (catch Throwable error
                (reset! build-in-progress-atom false)
                (render-progress! progress/done)
                (cancel-engine-build!)
                (error-reporting/report-exception! error))))
          nil)

        finish-with-result!
        (fn finish-with-result! [project-build-results]
          (reset! build-in-progress-atom false)
          (render-progress! progress/done)
          (cancel-engine-build!)
          (result-fn project-build-results)
          nil)

        phase-7-await-lint!
        (fn phase-7-await-lint! [project-build-results]
          (if lint
            (do
              (render-progress! (progress/make-indeterminate (localization/message "progress.linting")))
              (run-on-background-thread!
                (fn await-lint-on-background-thread! []
                  (deref lint-promise))
                (fn process-lint-results-on-ui-thread! [results]
                  (if results
                    (g/with-auto-evaluation-context evaluation-context
                      (let [{errors true warnings false}
                            (->> results
                                 (eduction
                                   (mapcat
                                     (fn [[resource diagnostic-ranges]]
                                       (when-let [node-id (project/get-resource-node project resource evaluation-context)]
                                         (eduction
                                           (map (fn [{:keys [severity message] :as diagnostic-range}]
                                                  (g/map->error
                                                    {:_node-id node-id
                                                     :severity (case severity
                                                                 :error :fatal
                                                                 :warning :warning
                                                                 :information :info
                                                                 :hint :info)
                                                     :user-data {:cursor-range (data/sanitize-cursor-range diagnostic-range)}
                                                     :message message})))
                                           diagnostic-ranges)))))
                                 (group-by #(= :fatal (:severity %))))]
                        (finish-with-result!
                          (cond-> project-build-results
                                  errors
                                  (update :error (fn [existing-error]
                                                   (g/map->error {:causes (cond-> errors existing-error (conj existing-error))})))
                                  warnings
                                  (assoc :warning (g/map->error {:causes warnings}))))))
                    (finish-with-result! project-build-results)))))
            (finish-with-result! project-build-results)))

        phase-6-await-engine-build!
        (fn phase-6-await-engine-build! [project-build-results]
          (assert (nil? (:error project-build-results)))
          (let [engine-build-future @engine-build-future-atom]
            (if (nil? engine-build-future)
              (phase-7-await-lint! project-build-results)
              (do
                (render-progress! (progress/make-indeterminate (localization/message "progress.fetching-engine")))
                (run-on-background-thread!
                  (fn run-engine-build-on-background-thread! []
                    (deref engine-build-future))
                  (fn process-engine-build-results-on-ui-thread! [engine-or-exception]
                    (if (instance? Throwable engine-or-exception)
                      (phase-7-await-lint!
                        (assoc project-build-results
                          :error (g/with-auto-evaluation-context evaluation-context
                                   (engine-build-errors/exception->error-value engine-or-exception project evaluation-context))))
                      (phase-7-await-lint!
                        (assoc project-build-results :engine engine-or-exception)))))))))

        phase-5-run-post-build-hook!
        (fn phase-5-run-post-build-hook! [project-build-results]
          (render-progress! (progress/make-indeterminate (localization/message "progress.executing-post-build-hooks")))
          (let [platform (engine/current-platform)
                project-build-successful (nil? (:error project-build-results))]
            (run-on-background-thread!
              (fn run-post-build-hook-on-background-thread! []
                @(extensions/execute-hook! project
                                           :on_build_finished
                                           {:success project-build-successful :platform platform}
                                           :exception-policy :ignore))
              (fn process-post-build-hook-results-on-ui-thread! [_]
                (if project-build-successful
                  (phase-6-await-engine-build! (assoc project-build-results :project-build-successful true))
                  (finish-with-result! project-build-results))))))

        phase-4-build-project!
        (fn phase-4-build-project! []
          ;; We're about to create an evaluation-context. Make sure it is
          ;; created from the main thread, so it makes sense to update the cache
          ;; from it after the project build concludes. Note that we selectively
          ;; transfer only the cached build-targets back to the system cache.
          ;; We do this because the project build process involves most of the
          ;; cached outputs in the project graph, and the intermediate steps
          ;; risk evicting the previous build targets as the cache fills up.
          (assert (ui/on-ui-thread?))
          (let [evaluation-context (g/make-evaluation-context)]
            (render-progress! (progress/make (localization/message "progress.building-project") 1))
            (run-on-background-thread!
              (fn run-project-build-on-background-thread! []
                (let [extra-build-targets
                      (when debug
                        (debug-view/build-targets project evaluation-context))

                      opts
                      {:extra-build-targets extra-build-targets
                       :render-progress! render-progress!
                       :task-cancelled? task-cancelled?}]
                  (build-project! project old-artifact-map opts evaluation-context)))
              (fn process-project-build-results-on-ui-thread! [project-build-results]
                (project/update-system-cache-build-targets! evaluation-context)
                (project/log-cache-info! (g/cache) "Cached compiled build targets in system cache.")
                (cond
                  run-build-hooks (phase-5-run-post-build-hook! project-build-results)
                  (nil? (:error project-build-results)) (phase-6-await-engine-build! project-build-results)
                  :else (finish-with-result! project-build-results))))))

        phase-3-start-all-build-processes!
        (fn phase-3-start-all-build-processes! []
          (start-engine-build!)
          (start-lint!)
          (phase-4-build-project!))

        phase-2-run-pre-build-hook!
        (fn phase-2-run-pre-build-hook! []
          (render-progress! (progress/make-indeterminate (localization/message "progress.executing-pre-build-hooks")))
          (let [platform (engine/current-platform)]
            (run-on-background-thread!
              (fn run-pre-build-hook-on-background-thread! []
                (let [extension-error @(extensions/execute-hook! project
                                                                 :on_build_started
                                                                 {:platform platform}
                                                                 :exception-policy :as-error)]
                  ;; If there was an error in the pre-build hook, we won't proceed
                  ;; with the project build. But we still want to report the build
                  ;; failure to any post-build hooks that might need to know.
                  (when (some? extension-error)
                    (render-progress! (progress/make-indeterminate (localization/message "progress.executing-post-build-hooks")))
                    @(extensions/execute-hook! project
                                               :on_build_finished
                                               {:success false :platform platform}
                                               :exception-policy :ignore))
                  extension-error))
              (fn process-pre-build-hook-results-on-ui-thread! [extension-error]
                (if (some? extension-error)
                  (finish-with-result! {:error extension-error})
                  (phase-3-start-all-build-processes!))))))

        phase-1-await-current-reload!
        (fn phase-1-await-current-reload! []
          (run-on-background-thread!
            disk/await-current-reload
            (fn start-the-build-process-on-ui-thread! [_]
              (if run-build-hooks
                (phase-2-run-pre-build-hook!)
                (phase-3-start-all-build-processes!)))))]

    ;; Trigger phase 1. Subsequent phases will be triggered as prior phases
    ;; finish without errors. Each phase will do some work on a background
    ;; thread, then process the results on the ui thread, and potentially
    ;; trigger subsequent phases which will again get off the ui thread as
    ;; soon as they can.
    (assert (not @build-in-progress-atom))
    (reset! build-in-progress-atom true)
    (phase-1-await-current-reload!)))

(defn- handle-build-results! [workspace render-build-error! build-results]
  (let [{:keys [error warning artifact-map etags project-build-successful]} build-results
        rendered-error (cond
                         (and error warning) (g/map->error {:causes [error warning]})
                         error error
                         warning warning)]
    (when rendered-error
      (render-build-error! rendered-error))
    (when project-build-successful
      (workspace/artifact-map! workspace artifact-map)
      (workspace/etags! workspace etags)
      (workspace/save-build-cache! workspace))
    (nil? error)))

(defn- build-handler [project workspace prefs web-server build-errors-view main-stage tool-tab-pane]
  (let [project-directory (workspace/project-directory workspace)
        main-scene (.getScene ^Stage main-stage)
        render-build-error! (make-render-build-error main-scene tool-tab-pane build-errors-view)
        skip-engine (target-cannot-swap-engine? (targets/selected-target prefs))
        [render-progress! task-cancelled?] (begin-task-progress! :build)]
    (build-errors-view/clear-build-errors build-errors-view)
    (async-build! project
                  :debug true
                  :build-engine (not skip-engine)
                  :prefs prefs
                  :render-progress! render-progress!
                  :task-cancelled? task-cancelled?
                  :old-artifact-map (workspace/artifact-map workspace)
                  :result-fn (fn [{:keys [engine] :as build-results}]
                               (when (handle-build-results! workspace render-build-error! build-results)
                                 (when (or engine skip-engine)
                                   (show-console! main-scene tool-tab-pane)
                                   (launch-built-project! project engine project-directory prefs web-server false)))))))

(handler/defhandler :project.build :global
  (enabled? [] (not (build-in-progress?)))
  (run [project workspace prefs web-server build-errors-view debug-view main-stage tool-tab-pane]
    (debug-view/detach! debug-view)
    (build-handler project workspace prefs web-server build-errors-view main-stage tool-tab-pane)))

(handler/defhandler :run.set-instance-count :global
  (options [prefs user-data]
    (when-not user-data
      (mapv (fn [i]
              {:label (localization/message "command.run.set-instance-count.option.count" {"count" i})
               :command :run.set-instance-count
               :check true
               :user-data {:instance-count i}})
            (range 1 5))))
  (run [prefs user-data]
    (let [count (:instance-count user-data)]
      (prefs/set! prefs [:run :instance-count] count)))
  (state [prefs user-data]
    (= (:instance-count user-data)
       (prefs/get prefs [:run :instance-count]))))

(defn- debugging-supported?
  [project localization]
  (if (project/shared-script-state? project)
    true
    (do (dialogs/make-info-dialog
          localization
          {:title (localization/message "dialog.debugging-not-supported.title")
           :icon :icon/triangle-error
           :header (localization/message "dialog.debugging-not-supported.header")
           :content {:wrap-text true
                     :text (localization/message "dialog.debugging-not-supported.content")}})
        false)))

(defn- run-with-debugger! [workspace project prefs debug-view render-build-error! web-server]
  (let [project-directory (workspace/project-directory workspace)
        skip-engine (target-cannot-swap-engine? (targets/selected-target prefs))
        [render-progress! task-cancelled?] (begin-task-progress! :build)]
    (async-build! project
                  :debug true
                  :build-engine (not skip-engine)
                  :prefs prefs
                  :render-progress! render-progress!
                  :task-cancelled? task-cancelled?
                  :old-artifact-map (workspace/artifact-map workspace)
                  :result-fn (fn [{:keys [engine] :as build-results}]
                               (when (handle-build-results! workspace render-build-error! build-results)
                                 (when (or engine skip-engine)
                                   (when-let [target (launch-built-project! project engine project-directory prefs web-server true)]
                                     (when (nil? (debug-view/current-session debug-view))
                                       (debug-view/start-debugger! debug-view project (:address target "localhost") (:instance-index target 0))))))))))

(defn- attach-debugger! [workspace project prefs debug-view render-build-error!]
  (let [[render-progress! task-cancelled?] (begin-task-progress! :build)]
    (async-build! project
                  :debug true
                  :build-engine false
                  :run-build-hooks false
                  :lint false
                  :render-progress! render-progress!
                  :task-cancelled? task-cancelled?
                  :old-artifact-map (workspace/artifact-map workspace)
                  :prefs prefs
                  :result-fn (fn [build-results]
                               (when (handle-build-results! workspace render-build-error! build-results)
                                 (let [target (targets/selected-target prefs)]
                                   (when (targets/controllable-target? target)
                                     (debug-view/attach! debug-view project target (:artifacts build-results)))))))))

(handler/defhandler :debugger.start :global
  ;; NOTE: Shares a shortcut with :debug-view/continue.
  ;; Only one of them can be active at a time. This creates the impression that
  ;; there is a single menu item whose label changes in various states.
  (active? [debug-view evaluation-context]
           (not (debug-view/debugging? debug-view evaluation-context)))
  (enabled? [] (not (build-in-progress?)))
  (run [project workspace prefs web-server build-errors-view console-view debug-view main-stage tool-tab-pane localization]
    (when (debugging-supported? project localization)
      (let [main-scene (.getScene ^Stage main-stage)
            render-build-error! (make-render-build-error main-scene tool-tab-pane build-errors-view)]
        (build-errors-view/clear-build-errors build-errors-view)
        (if (debug-view/can-attach? prefs)
          (attach-debugger! workspace project prefs debug-view render-build-error!)
          (run-with-debugger! workspace project prefs debug-view render-build-error! web-server))))))

(def ^:private clean-build-dialog-info
  {:title (localization/message "dialog.clean-build.title")
   :icon :icon/circle-question
   :header (localization/message "dialog.clean-build.header")
   :buttons [{:text (localization/message "dialog.button.cancel")
              :cancel-button true
              :result false}
             {:text (localization/message "dialog.clean-build.button.clean")
              :default-button true
              :result true}]})

(handler/defhandler :project.clean-build :global
  (enabled? [] (not (build-in-progress?)))
  (run [project workspace prefs web-server build-errors-view debug-view main-stage tool-tab-pane localization]
    (when (dialogs/make-confirmation-dialog localization clean-build-dialog-info)
      (debug-view/detach! debug-view)
      (workspace/clear-build-cache! workspace)
      (build-handler project workspace prefs web-server build-errors-view main-stage tool-tab-pane))))

(defn- start-new-log-pipe!
  ^PipedOutputStream []
  (let [in (PipedInputStream.)]
    (console/pipe-log-stream-to-console! in)
    (PipedOutputStream. in)))

(defn- build-html5! [project prefs web-server build-errors-view changes-view main-stage tool-tab-pane bob-commands]
  (let [main-scene (.getScene ^Stage main-stage)
        render-build-error! (make-render-build-error main-scene tool-tab-pane build-errors-view)
        render-reload-progress! (make-render-task-progress :resource-sync)
        render-save-progress! (make-render-task-progress :save-all)
        [render-build-progress! build-task-cancelled?] (begin-task-progress! :build)
        bob-args (bob/build-html5-bob-options project prefs)
        out (start-new-log-pipe!)]
    (build-errors-view/clear-build-errors build-errors-view)
    (disk/async-bob-build! render-reload-progress! render-save-progress! render-build-progress! out build-task-cancelled?
                           render-build-error! bob-commands bob-args project changes-view
                           (fn [successful?]
                             (when successful?
                               (let [url (str (http-server/local-url web-server) "/html5")]
                                 (if (prefs/get prefs [:build :open-html5-build])
                                   (ui/open-url url)
                                   (console/append-console-entry! nil (format "INFO: The game is available at %s" url))))
                               (.close out))))))

(handler/defhandler :project.clean-build-html5 :global
  (run [project prefs web-server build-errors-view changes-view main-stage tool-tab-pane localization]
       (when (dialogs/make-confirmation-dialog localization clean-build-dialog-info)
         (build-html5! project prefs web-server build-errors-view changes-view main-stage tool-tab-pane
                       bob/clean-build-html5-bob-commands))))

(handler/defhandler :project.build-html5 :global
  (run [project prefs web-server build-errors-view changes-view main-stage tool-tab-pane]
       (build-html5! project prefs web-server build-errors-view changes-view main-stage tool-tab-pane
                     bob/build-html5-bob-commands)))

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
        flat-build-targets (build/resolve-node-dependencies resource-node project evaluation-context)
        updated-build-resource-proj-paths (updated-build-resource-proj-paths old-etags new-etags)]
    (into []
          (keep (fn [{build-resource :resource :as _build-target}]
                  (when (contains? updated-build-resource-proj-paths (resource/proj-path build-resource))
                    build-resource)))
          (rseq flat-build-targets))))

(defn- can-hot-reload? [debug-view prefs evaluation-context]
  (when-some [target (targets/selected-target prefs)]
    (and (or (targets/controllable-target? target) (targets/all-launched-targets? target))
         (not (debug-view/suspended? debug-view evaluation-context))
         (not (build-in-progress?)))))

(defn- hot-reload! [project prefs localization build-errors-view main-stage tool-tab-pane]
  (let [main-scene (.getScene ^Stage main-stage)
        target (targets/selected-target prefs)
        workspace (project/workspace project)
        old-etags (workspace/etags workspace)
        render-build-error! (make-render-build-error main-scene tool-tab-pane build-errors-view)
        [render-progress! task-cancelled?] (begin-task-progress! :build)]
    ;; NOTE: We must build the entire project even if we only want to reload a
    ;; subset of resources in order to maintain a functioning build cache.
    ;; If we decide to support hot reload of a subset of resources, we must
    ;; keep track of which resource versions have been loaded by the engine,
    ;; or we might miss resources that were recompiled but never reloaded.
    (build-errors-view/clear-build-errors build-errors-view)
    (async-build! project
                  :debug false
                  :build-engine false
                  :run-build-hooks false
                  :lint false
                  :render-progress! render-progress!
                  :task-cancelled? task-cancelled?
                  :old-artifact-map (workspace/artifact-map workspace)
                  :prefs prefs
                  :result-fn (fn [{:keys [error artifact-map etags]}]
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
                                       (if (targets/all-launched-targets? target)
                                         (doseq [launched-target (targets/all-launched-targets)]
                                           (engine/reload-build-resources! launched-target updated-build-resources))
                                         (engine/reload-build-resources! target updated-build-resources)))
                                     (catch Exception e
                                       (dialogs/make-info-dialog
                                         localization
                                         {:title (localization/message "dialog.hot-reload-failed.title")
                                          :icon :icon/triangle-error
                                          :header (localization/message
                                                    "dialog.hot-reload-failed.header"
                                                    {"engine" (targets/target-message (targets/selected-target prefs))})
                                          :content (.getMessage e)})))))))))

(handler/defhandler :run.hot-reload :global
  (enabled? [debug-view prefs evaluation-context]
    (can-hot-reload? debug-view prefs evaluation-context))
  (run [project app-view prefs localization build-errors-view selection main-stage tool-tab-pane]
    (hot-reload! project prefs localization build-errors-view main-stage tool-tab-pane)))

(handler/defhandler :window.tab.close :global
  (enabled? [app-view evaluation-context]
            (not-empty (get-active-tabs app-view evaluation-context)))
  (run [app-view]
    (let [tab-pane (g/node-value app-view :active-tab-pane)]
      (when-let [tab (ui/selected-tab tab-pane)]
        (remove-tab! tab-pane tab)))))

(handler/defhandler :window.tab.close-others :global
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

(handler/defhandler :window.tab.close-all :global
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

(handler/defhandler :window.tab.move-to-other-group :global
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

(handler/defhandler :window.tab.swap-with-other-group :global
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

(handler/defhandler :window.tab.join-groups :global
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

(defn make-about-dialog [localization]
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
    (ui/show! stage localization)))

(handler/defhandler :help.open-documentation :global
  (run [] (ui/open-url "https://www.defold.com/learn/")))

(handler/defhandler :help.open-forum :global
  (run [] (ui/open-url "https://forum.defold.com/")))

(handler/defhandler :help.open-asset-portal :global
  (run [] (ui/open-url "https://www.defold.com/assets")))

(handler/defhandler :help.report-issue :global
  (run [] (ui/open-url (github/new-issue-link))))

(handler/defhandler :help.report-suggestion :global
  (run [] (ui/open-url (github/new-suggestion-link))))

(handler/defhandler :help.open-issues :global
  (run [] (ui/open-url (github/search-issues-link))))

(handler/defhandler :help.open-logs :global
  (run [] (ui/open-file (.getAbsoluteFile (.toFile (Editor/getLogDirectory))))))

(handler/defhandler :help.open-donations :global
  (run [] (ui/open-url "https://www.defold.com/donate")))

(handler/defhandler :app.about :global
  (run [localization] (make-about-dialog localization)))

(handler/defhandler :dev.reload-css :global
  (run [] (ui/reload-root-styles!)))

(handler/defhandler :file.open-project :global
  (active? [] (and (system/defold-resourcespath) (system/defold-launcherpath)))
  (run [] (let [resources-path (system/defold-resourcespath)
                install-dir (.getCanonicalFile
                              (case (.getOs (Platform/getHostPlatform))
                                "macos" (io/file resources-path "../../")
                                ("linux" "win32") (io/file resources-path)))]
            (process/start! {:dir install-dir} (system/defold-launcherpath)))))

(handler/register-menu! ::menubar
  [{:label (localization/message "menu.file")
    :id ::file
    :children [{:label (localization/message "command.file.new")
                :id ::new
                :command :file.new}
               {:label (localization/message "command.file.open")
                :id ::open
                :command :file.open}
               {:label (localization/message "command.file.load-external-changes")
                :id ::async-reload
                :command :file.load-external-changes}
               {:label (localization/message "command.file.save-all")
                :id ::save-all
                :command :file.save-all}
               {:label (localization/message "command.file.save-and-upgrade-all")
                :id ::save-and-upgrade-all
                :command :file.save-and-upgrade-all}
               menu-items/separator
               {:label (localization/message "command.file.search")
                :command :file.search}
               {:label (localization/message "command.private.recent-files")
                :command :private/recent-files}
               menu-items/separator
               {:label (localization/message "command.window.tab.close")
                :command :window.tab.close}
               {:label (localization/message "command.window.tab.close-all")
                :command :window.tab.close-all}
               {:label (localization/message "command.window.tab.close-others")
                :command :window.tab.close-others}
               menu-items/separator
               {:label (localization/message "command.file.show-references")
                :command :file.show-references}
               {:label (localization/message "command.file.show-dependencies")
                :command :file.show-dependencies}
               menu-items/separator
               menu-items/show-overrides
               menu-items/pull-up-overrides
               menu-items/push-down-overrides
               menu-items/separator
               {:label (localization/message "command.run.hot-reload")
                :command :run.hot-reload}
               menu-items/separator
               {:label (localization/message "command.file.open-project")
                :command :file.open-project}
               {:label (localization/message "command.app.preferences")
                :command :app.preferences}
               {:label (localization/message "command.app.quit")
                :command :app.quit}]}
   {:label (localization/message "menu.edit")
    :id ::edit
    :children [{:label (localization/message "command.edit.undo")
                :icon "icons/undo.png"
                :command :edit.undo}
               {:label (localization/message "command.edit.redo")
                :icon "icons/redo.png"
                :command :edit.redo}
               menu-items/separator
               {:label (localization/message "command.edit.cut")
                :command :edit.cut}
               {:label (localization/message "command.edit.copy")
                :command :edit.copy}
               {:label (localization/message "command.edit.paste")
                :command :edit.paste}
               {:label (localization/message "command.code.select-all")
                :command :code.select-all}
               {:label (localization/message "command.edit.delete")
                :icon "icons/32/Icons_M_06_trash.png"
                :command :edit.delete}
               menu-items/separator
               {:label (localization/message "command.edit.reorder-up")
                :command :edit.reorder-up}
               {:label (localization/message "command.edit.reorder-down")
                :command :edit.reorder-down}
               (menu-items/separator-with-id ::edit-end)]}
   {:label (localization/message "menu.view")
    :id ::view
    :children [{:label (localization/message "command.window.toggle-left-pane")
                :command :window.toggle-left-pane}
               {:label (localization/message "command.window.toggle-changed-files-pane")
                :command :window.toggle-changed-files-pane}
               {:label (localization/message "command.window.toggle-bottom-pane")
                :command :window.toggle-bottom-pane}
               {:label (localization/message "command.window.toggle-right-pane")
                :command :window.toggle-right-pane}
               menu-items/separator
               {:label (localization/message "command.window.show-console")
                :command :window.show-console}
               {:label (localization/message "command.window.show-curve-editor")
                :command :window.show-curve-editor}
               {:label (localization/message "command.window.show-build-errors")
                :command :window.show-build-errors}
               {:label (localization/message "command.window.show-search-results")
                :command :window.show-search-results}
               (menu-items/separator-with-id ::view-end)]}
   {:label (localization/message "menu.help")
    :children [{:label (localization/message "command.dev.reload-css")
                :command :dev.reload-css}
               {:label (localization/message "command.help.open-logs")
                :command :help.open-logs}
               menu-items/separator
               {:label (localization/message "command.file.create-desktop-entry")
                :command :file.create-desktop-entry}
               menu-items/separator
               {:label (localization/message "command.help.open-documentation")
                :command :help.open-documentation}
               {:label (localization/message "command.help.open-forum")
                :command :help.open-forum}
               {:label (localization/message "command.help.open-asset-portal")
                :command :help.open-asset-portal}
               menu-items/separator
               {:label (localization/message "command.help.report-issue")
                :command :help.report-issue}
               {:label (localization/message "command.help.report-suggestion")
                :command :help.report-suggestion}
               {:label (localization/message "command.help.open-issues")
                :command :help.open-issues}
               menu-items/separator
               {:label (localization/message "command.help.open-donations")
                :command :help.open-donations}
               menu-items/separator
               {:label (localization/message "command.app.about")
                :command :app.about}]}])

(handler/register-menu! ::tab-menu
  [menu-items/open-as
   menu-items/separator
   {:label (localization/message "command.window.tab.close")
    :command :window.tab.close}
   {:label (localization/message "command.window.tab.close-others")
    :command :window.tab.close-others}
   {:label (localization/message "command.window.tab.close-all")
    :command :window.tab.close-all}
   menu-items/separator
   {:label (localization/message "command.window.tab.move-to-other-group")
    :command :window.tab.move-to-other-group}
   {:label (localization/message "command.window.tab.swap-with-other-group")
    :command :window.tab.swap-with-other-group}
   {:label (localization/message "command.window.tab.join-groups")
    :command :window.tab.join-groups}
   menu-items/separator
   {:label (localization/message "command.edit.copy-resource-path")
    :command :edit.copy-resource-path}
   {:label (localization/message "command.edit.copy-absolute-path")
    :command :edit.copy-absolute-path}
   {:label (localization/message "command.edit.copy-require-path")
    :command :edit.copy-require-path}
   menu-items/separator
   {:label (localization/message "command.file.show-in-assets")
    :icon "icons/32/Icons_S_14_linkarrow.png"
    :command :file.show-in-assets}
   {:label (localization/message "command.file.show-in-desktop")
    :icon "icons/32/Icons_S_14_linkarrow.png"
    :command :file.show-in-desktop}
   {:label (localization/message "command.file.show-references")
    :command :file.show-references}
   {:label (localization/message "command.file.show-dependencies")
    :command :file.show-dependencies}
   menu-items/separator
   menu-items/show-overrides
   menu-items/pull-up-overrides
   menu-items/push-down-overrides])

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
       (g/operation-label (localization/message "operation.select"))
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
           (g/operation-label (localization/message "operation.select"))
           (project/sub-select project-id active-resource-node sub-selection open-resource-nodes)))))))

(defn- make-title
  ([] (if-some [version (system/defold-version)]
        (str "Defold " version)
        "Defold"))
  ([project-title] (str project-title " - " (make-title))))

(defn- refresh-app-title! [^Stage stage project]
  (let [settings      (g/node-value project :settings)
        project-title (settings ["project" "title"])
        new-title     (make-title project-title)]
    (when (not= (.getTitle stage) new-title)
      (.setTitle stage new-title))))

(defn- refresh-menus-and-toolbars! [app-view ^Scene scene]
  (ui/user-data! scene :keymap (g/node-value app-view :keymap))
  (ui/refresh scene))

(defn- refresh-views! [app-view]
  (let [auto-pulls (g/node-value app-view :auto-pulls)]
    (doseq [[node label] auto-pulls]
      (profiler/profile "view" (:name @(g/node-type* node))
        (g/node-value node label)))))

(defn- refresh-scene-views! [app-view dt]
  (profiler/begin-frame)
  (scene-cache/process-pending-deletions! nil)
  (doseq [view-id (g/node-value app-view :scene-view-ids)]
    (try
      (scene/refresh-scene-view! view-id dt)
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

(let [TabHeaderSkin (Class/forName "javafx.scene.control.skin.TabPaneSkin$TabHeaderSkin")
      getTab (.getDeclaredMethod TabHeaderSkin "getTab" (into-array Class []))]
  (.setAccessible getTab true)
  (defn- handle-tab-pane-mouse-pressed! 
    [^TabPane tab-pane ^MouseEvent event]
    (when (= MouseButton/SECONDARY (.getButton event))
      (when-let [node (ui/closest-node-where #(instance? TabHeaderSkin %) (.getTarget event))]
        (->> (.invoke getTab node (into-array Object []))
             (.select (.getSelectionModel tab-pane)))))))

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
                  (let [remaining-tab-pane (.get tab-panes 0)
                        selected-tab (ui/selected-tab remaining-tab-pane)
                        resource-node (tab->resource-node selected-tab)
                        view-type (tab->view-type selected-tab)]
                    (.requestFocus ^TabPane remaining-tab-pane)
                    (on-selected-tab-changed! app-view app-scene selected-tab resource-node view-type)))))))))
  (.addEventFilter tab-pane MouseEvent/MOUSE_PRESSED (ui/event-handler event (handle-tab-pane-mouse-pressed! tab-pane event)))
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

(defn make-app-view [view-graph project ^Stage stage ^MenuBar menu-bar ^SplitPane editor-tabs-split ^TabPane tool-tab-pane prefs localization]
  (let [app-scene (.getScene stage)]
    (ui/disable-menu-alt-key-mnemonic! menu-bar)
    (.setUseSystemMenuBar menu-bar true)
    (.setTitle stage (make-title))
    (let [editor-tab-pane (TabPane.)
          keymap (keymap/from-prefs prefs)
          app-view (first (g/tx-nodes-added (g/transact (g/make-node view-graph AppView
                                                                     :stage stage
                                                                     :scene app-scene
                                                                     :editor-tabs-split editor-tabs-split
                                                                     :active-tab-pane editor-tab-pane
                                                                     :tool-tab-pane tool-tab-pane
                                                                     :active-tool :move
                                                                     :manip-space :world
                                                                     :keymap keymap))))]
      (.add (.getItems editor-tabs-split) editor-tab-pane)
      (configure-editor-tab-pane! editor-tab-pane app-scene app-view)
      (ui/observe (.focusOwnerProperty app-scene)
                  (fn [_ _ new-focus-owner]
                    (handle-focus-owner-change! app-view app-scene new-focus-owner)))

      (ui/register-menubar app-scene menu-bar ::menubar)

      (let [prev-localization-bundle (volatile! nil)
            refresh-timer (ui/->timer
                            "refresh-app-view"
                            (fn [_ elapsed dt]
                              (when-not (ui/ui-disabled?)
                                (let [refresh-requested? (ui/user-data app-scene ::ui/refresh-requested?)]
                                  (when refresh-requested?
                                    (g/with-auto-evaluation-context evaluation-context
                                      (let [localization-bundle (-> project
                                                                    (project/editor-localization-bundle evaluation-context)
                                                                    (editor-localization-bundle/bundle evaluation-context))]
                                        (when-not (identical? @prev-localization-bundle localization-bundle)
                                          (vreset! prev-localization-bundle localization-bundle)
                                          (localization/set-bundle! localization ::project localization-bundle))))
                                    (ui/user-data! app-scene ::ui/refresh-requested? false)
                                    (refresh-menus-and-toolbars! app-view app-scene)
                                    (refresh-views! app-view))
                                  (refresh-scene-views! app-view dt)
                                  (refresh-app-title! stage project)))))]
        (ui/timer-stop-on-closed! stage refresh-timer)
        (ui/timer-start! refresh-timer))
      (ui/on-closed! stage (fn [_] (dispose-scene-views! app-view)))
      app-view)))

(defn- make-info-box! [localization]
  (let [info-panel (HBox.)
        left-label (doto (Label.) (localization/localize! localization (localization/message "resource.readonly.detail")))
        right-link (doto (Hyperlink.) (localization/localize! localization (localization/message "resource.readonly.button.read-more")))
        spacer (Region.)]
    (HBox/setHgrow spacer Priority/ALWAYS)
    (.getStyleClass info-panel)
    (ui/set-style! info-panel "info-panel" true)
    (ui/set-style! left-label "info-panel-label" true)
    (ui/set-style! right-link "info-panel-link" true)
    (.setOnAction right-link (ui/event-handler _ (ui/open-url "https://defold.com/manuals/libraries/#editing-files-in-library-dependencies")))
    (.addAll  (.getChildren info-panel) (Arrays/asList (into-array Node [left-label spacer right-link])))
    info-panel))

(defn- make-tab! [app-view prefs localization workspace project resource resource-node
                  resource-type view-type make-view-fn ^ObservableList tabs
                  open-resource opts]
  (let [parent (AnchorPane.)
        tab-content (if (resource/read-only? resource)
                      (doto (VBox.)
                        (ui/children! [(make-info-box! localization)
                                       (doto parent (VBox/setVgrow Priority/ALWAYS))]))
                      parent)
        tab (doto (Tab. (tab-title resource false))
              (.setContent tab-content)
              (.setTooltip (Tooltip. (or (resource/proj-path resource) "unknown")))
              (ui/user-data! ::view-type view-type))
        view-graph (g/make-graph! :history false :volatility 2)
        select-fn (partial select app-view)
        opts (merge opts
                    (get (:view-opts resource-type) (:id view-type))
                    {:app-view app-view
                     :select-fn select-fn
                     :open-resource-fn (partial open-resource app-view prefs localization workspace project)
                     :prefs prefs
                     :project project
                     :workspace workspace
                     :localization localization
                     :tab tab})
        view (make-view-fn view-graph parent resource-node opts)]
    (assert (g/node-instance? view/WorkbenchView view))
    (recent-files/add! prefs resource view-type)
    (g/transact
      (concat
        (view/connect-resource-node view resource-node)
        (g/connect view :view-data app-view :open-views)
        (g/connect view :view-dirty app-view :open-dirty-views)))
    (ui/user-data! tab ::view view)
    (.add tabs tab)
    (.setGraphic tab (icons/get-image-view (or (:icon resource-type) "icons/64/Icons_29-AT-Unknown.png") 16))
    (.addAll (.getStyleClass tab) ^Collection (resource/style-classes resource))
    (ui/register-tab-toolbar tab "#toolbar" :toolbar)
    (.setOnSelectionChanged tab (ui/event-handler event
                                  (when (.isSelected tab)
                                    (recent-files/add! prefs resource view-type))))
    (let [close-handler (.getOnClosed tab)]
      (.setOnClosed tab (ui/event-handler event
                          (recent-files/add! prefs resource view-type)
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

(defn- custom-code-editor-executable-path-preference
  ^String [prefs]
  (some-> prefs
          (prefs/get [:code :custom-editor])
          (string/trim)
          (not-empty)))

(defn view-types
  [resource]
  (cond->> (:view-types (resource/resource-type resource))
           (text-util/binary? resource)
           (e/filter #(not= :code (:id %)))))

(defn open-resource
  ([app-view prefs localization workspace project resource]
   (open-resource app-view prefs localization workspace project resource {}))
  ([app-view prefs localization workspace project resource opts]
   (let [resource-type  (resource/resource-type resource)
         resource-node  (or (project/get-resource-node project resource)
                            (throw (ex-info (format "No resource node found for resource '%s'" (resource/proj-path resource))
                                            {})))
         text-view-type (workspace/get-view-type workspace :text)
         view-type      (or (:selected-view-type opts)
                            (first (view-types resource))
                            text-view-type)
         view-type-id (:id view-type)
         specific-view-type-selected (some? (:selected-view-type opts))]
     (cond
       (not (resource/loaded? resource))
       (do (dialogs/make-info-dialog
             localization
             {:title (localization/message "dialog.open-resource.excluded.title")
              :icon :icon/triangle-error
              :header (localization/message "dialog.open-resource.excluded.header"
                                           {"resource" (resource/proj-path resource)})})
           false)

       (g/defective? resource-node)
       (do (dialogs/make-info-dialog
             localization
             {:title (localization/message "dialog.open-resource.unrecognized.title")
              :icon :icon/triangle-error
              :header (localization/message "dialog.open-resource.unrecognized.header"
                                           {"resource" (resource/proj-path resource)})})
           false)

       :else
       (if-let [custom-editor
                (when (:use-custom-editor opts true)
                  (let [is-code-editor-view-type (contains? #{:code :text} view-type-id)
                        default-to-custom-editor (get-in resource-type [:view-opts view-type-id :use-custom-editor] true)]
                    (when (and is-code-editor-view-type
                               (or default-to-custom-editor
                                   specific-view-type-selected))
                      (custom-code-editor-executable-path-preference prefs))))]
         (let [cursor-range (:cursor-range opts)
               arg-tmpl (string/trim (if cursor-range
                                       (prefs/get prefs [:code :open-file-at-line])
                                       (prefs/get prefs [:code :open-file])))
               arg-sub (cond-> {:file (resource/externally-available-absolute-path resource)}
                               cursor-range (assoc :line (CursorRange->line-number cursor-range)))
               args (->> (string/split arg-tmpl #" ")
                         (map #(substitute-args % arg-sub)))]
           (doto (ProcessBuilder. ^List (cons custom-editor args))
             (.directory (workspace/project-directory workspace))
             (.start))
           false)
         (if (contains? view-type :make-view-fn)
           (let [^SplitPane editor-tabs-split (g/node-value app-view :editor-tabs-split)
                 tab-panes (.getItems editor-tabs-split)
                 open-tabs (mapcat #(.getTabs ^TabPane %) tab-panes)
                 make-view-fn (:make-view-fn view-type)
                 existing-tab (some #(when (and (= (tab->resource-node %) resource-node)
                                                (= view-type (ui/user-data % ::view-type)))
                                       %)
                                    open-tabs)
                 ^Tab tab (or existing-tab
                              (let [^TabPane active-tab-pane (g/node-value app-view :active-tab-pane)
                                    active-tab-pane-tabs (.getTabs active-tab-pane)]
                                (make-tab! app-view prefs localization workspace project resource resource-node
                                           resource-type view-type make-view-fn active-tab-pane-tabs
                                           open-resource opts)))]
             (when (or (nil? existing-tab) (:select-node opts))
               (g/transact
                 (select app-view resource-node [(:select-node opts resource-node)])))
             (.select (.getSelectionModel (.getTabPane tab)) tab)
             (when-let [focus (:focus-fn view-type)]
               ;; Force layout pass since the focus function of some views
               ;; needs proper width + height (f.i. code view for
               ;; scrolling to selected line).
               (NodeHelper/layoutNodeForPrinting (.getRoot ^Scene (g/node-value app-view :scene)))
               (focus (ui/user-data tab ::view) opts))
             ;; Do an initial rendering so it shows up as fast as possible.
             (ui/run-later (refresh-scene-views! app-view 1/60)
                           (ui/run-later (slog/smoke-log "opened-resource")))
             true)
           (let [^String path (or (resource/abs-path resource)
                                  (resource/temp-path resource))
                 ^File f (File. path)]
             (ui/open-file f (fn [msg]
                               (ui/run-later
                                 (dialogs/make-info-dialog
                                   localization
                                   {:title (localization/message "dialog.open-resource.open-file-failed.title")
                                    :icon :icon/triangle-error
                                    :header (localization/message "dialog.open-resource.open-file-failed.header"
                                                                  {"resource" (.getName f)})
                                    :content {:wrap-text true
                                              :text (localization/message "dialog.open-resource.open-file-failed.detail"
                                                                          {"error" msg})}}))))
             false)))))))

(handler/defhandler :file.open-selected :global
  (active? [selection] (not-empty (selection->openable-resources selection)))
  (enabled? [selection] (some resource/exists? (selection->openable-resources selection)))
  (run [selection app-view prefs localization workspace project]
    (doseq [resource (filter resource/exists? (selection->openable-resources selection))]
      (open-resource app-view prefs localization workspace project resource))))

(handler/defhandler :file.open-as :global
  (active? [app-view selection evaluation-context] (context-openable-resource app-view selection evaluation-context))
  (enabled? [app-view selection evaluation-context] (resource/exists? (context-openable-resource app-view selection evaluation-context)))
  (run [selection app-view prefs localization workspace project user-data]
       (let [resource (context-openable-resource app-view selection)]
         (open-resource app-view prefs localization workspace project resource user-data)))
  (options [app-view prefs workspace selection user-data]
           (when-not user-data
             (let [[resource active-view-type-id]
                   (g/with-auto-evaluation-context evaluation-context
                     (if-let [selected-resource (selection->single-resource selection)]
                       (pair selected-resource nil)
                       (let [active-resource (g/node-value app-view :active-resource evaluation-context)
                             active-view-type-id (:id (:view-type (g/node-value app-view :active-view-info evaluation-context)))]
                         (pair active-resource active-view-type-id))))

                   is-custom-code-editor-configured (some? (custom-code-editor-executable-path-preference prefs))

                   make-option
                   (fn make-option [label user-data]
                     {:label label
                      :command :file.open-as
                      :user-data user-data})

                   view-type->option
                   (fn view-type->option [{:keys [label] :as view-type}]
                     (make-option (or label (localization/message "command.file.open-as.option.associated-application"))
                                  {:selected-view-type view-type}))]

               (into []
                     (if is-custom-code-editor-configured
                       (mapcat (fn [{:keys [id label] :as view-type}]
                                 (case id
                                   (:code :text)
                                   [(make-option (localization/message "command.file.open-as.option.in-custom-editor" {"view" label})
                                                 {:selected-view-type view-type})
                                    (make-option (localization/message "command.file.open-as.option.in-defold-editor" {"view" label})
                                                 {:selected-view-type view-type
                                                  :use-custom-editor false})]
                                   [(view-type->option view-type)])))
                       (map view-type->option))
                     (cond->> (view-types resource)

                              active-view-type-id
                              (e/filter #(not= active-view-type-id (:id %)))))))))

(handler/defhandler :private/recent-files :global
  (enabled? [prefs workspace evaluation-context]
    (recent-files/exist? prefs workspace evaluation-context))
  (active? [] true)
  (options [prefs workspace app-view]
    (g/with-auto-evaluation-context evaluation-context
      (-> [{:label (localization/message "command.file.reopen-recent")
            :command :file.reopen-recent}]
          (cond-> (recent-files/exist? prefs workspace evaluation-context)
                  (->
                    (conj menu-items/separator)
                    (into
                      (map (fn [[resource view-type :as resource+view-type]]
                             {:label (-> "command.private.recent-files.option.entry"
                                         (localization/message
                                           {"path" (resource/proj-path resource)
                                            "view" (:label view-type)})
                                         (localization/transform string/replace "_" "__"))
                              :command :private/open-selected-recent-file
                              :user-data resource+view-type}))
                      (recent-files/some-recent prefs workspace evaluation-context))
                    (conj menu-items/separator)))
          (conj {:label (localization/message "command.private.recent-files.option.more")
                 :command :file.open-recent})))))

(handler/defhandler :private/open-selected-recent-file :global
  (run [prefs localization app-view workspace project user-data]
    (let [[resource view-type] user-data]
      (open-resource app-view prefs localization workspace project resource {:selected-view-type view-type
                                                                             :use-custom-editor false}))))

(handler/defhandler :file.open-recent :global
  (active? [prefs workspace evaluation-context]
    (recent-files/exist? prefs workspace evaluation-context))
  (run [prefs localization app-view workspace project]
    (g/with-auto-evaluation-context evaluation-context
      (doseq [[resource view-type] (recent-files/select prefs workspace evaluation-context)]
        (open-resource app-view prefs localization workspace project resource {:selected-view-type view-type
                                                                               :use-custom-editor false})))))

(handler/defhandler :file.reopen-recent :global
  (enabled? [prefs workspace evaluation-context app-view]
    (recent-files/exist-closed? prefs workspace app-view evaluation-context))
  (run [prefs localization app-view workspace project]
    (g/with-auto-evaluation-context evaluation-context
      (let [[resource view-type] (recent-files/last-closed prefs workspace app-view evaluation-context)]
        (open-resource app-view prefs localization workspace project resource {:selected-view-type view-type
                                                                               :use-custom-editor false})))))

(defn- async-save!
  ([app-view changes-view project save-data-fn]
   (async-save! app-view changes-view project save-data-fn nil))
  ([app-view changes-view project save-data-fn callback!]
   {:pre [(g/node-id? app-view)
          (g/node-id? changes-view)
          (g/node-id? project)
          (ifn? save-data-fn)
          (or (nil? callback!) (ifn? callback!))]}
   (let [render-reload-progress! (make-render-task-progress :resource-sync)
         render-save-progress! (make-render-task-progress :save-all)]
     (disk/async-save! render-reload-progress! render-save-progress! save-data-fn project changes-view
                       (fn [successful?]
                         (when successful?
                           (ui/user-data! (g/node-value app-view :scene) ::ui/refresh-requested? true))
                         (when callback!
                           (callback! successful? render-reload-progress! render-save-progress!)))))))

(defn- make-version-control-info-dialog-content
  ([localization]
   (make-version-control-info-dialog-content localization nil))
  ([localization preamble]
   {:fx/type fx.text-flow/lifecycle
    :style-class "dialog-content-padding"
    :children [{:fx/type fx.text/lifecycle
                :text (let [intro-text (localization (localization/message "dialog.save-and-upgrade.version-control.info.before-manual"))]
                        (if preamble
                          (str (localization preamble) "\n\n" intro-text)
                          intro-text))}
               {:fx/type fx.hyperlink/lifecycle
                :text (localization (localization/message "dialog.save-and-upgrade.version-control.info.manual-link"))
                :on-action (fn [_]
                             (ui/open-url "https://www.defold.com/manuals/version-control/"))}
               {:fx/type fx.text/lifecycle
                :text (localization (localization/message "dialog.save-and-upgrade.version-control.info.after-manual"))}]}))

(handler/defhandler :file.save-all :global
  (enabled? [] (not (bob/build-in-progress?)))
  (run [app-view changes-view project]
       (async-save! app-view changes-view project project/dirty-save-data)))

(handler/defhandler :file.save-and-upgrade-all :global
  (enabled? [] (not (bob/build-in-progress?)))
  (run [app-view changes-view project workspace localization]
       (let [git (g/node-value changes-view :git)]
         (when (and

                 ;; Check if the project is under version control. If not,
                 ;; advise against performing the file format upgrade, and show
                 ;; a dialog on how to set up version control for the project.
                 ;; The user can opt to proceed with the upgrade anyway.
                 (or (some? git)
                     (dialogs/make-confirmation-dialog
                       localization
                       {:title (localization/message "dialog.save-and-upgrade.title.not-safe")
                        :size :default
                        :icon :icon/triangle-error
                        :header (localization/message "dialog.save-and-upgrade.version-control.header")
                        :content (make-version-control-info-dialog-content
                                   localization
                                   (localization/message "dialog.save-and-upgrade.version-control.preamble"))
                        :buttons [{:text (localization/message "dialog.save-and-upgrade.button.abort")
                                   :cancel-button true
                                   :default-button true
                                   :result false}
                                  {:text (localization/message "dialog.save-and-upgrade.button.proceed-anyway")
                                   :variant :danger
                                   :result true}]}))

                 ;; Check if there are uncommitted changes. If so, show a dialog
                 ;; advising against performing the file format upgrade, and
                 ;; instead ask the user to commit their changes before
                 ;; retrying. The user can opt to proceed with the upgrade
                 ;; anyway.
                 (or (nil? git)
                     (not (git/has-local-changes? git))
                     (dialogs/make-confirmation-dialog
                       localization
                       {:title (localization/message "dialog.save-and-upgrade.title.not-safe")
                        :size :default
                        :icon :icon/triangle-error
                        :header (localization/message "dialog.save-and-upgrade.uncommitted.header")
                        :content {:fx/type fxui/legacy-label
                                  :style-class "dialog-content-padding"
                                  :text (localization (localization/message "dialog.save-and-upgrade.uncommitted.content"))}
                        :buttons [{:text (localization/message "dialog.save-and-upgrade.button.abort")
                                   :cancel-button true
                                   :default-button true
                                   :result false}
                                  {:text (localization/message "dialog.save-and-upgrade.button.proceed-anyway")
                                   :variant :danger
                                   :result true}]})))

           ;; We've deemed it safe to proceed with the file format upgrade, or
           ;; the user has chosen to ignore our warnings. Show one last
           ;; confirmation dialog before proceeding.
           (let [workspace-has-non-editable-directories (workspace/has-non-editable-directories? workspace)
                 buttons (cond-> [{:text (localization/message "dialog.button.cancel")
                                   :cancel-button true
                                   :default-button true
                                   :result nil}
                                  {:text (localization/message (if workspace-has-non-editable-directories
                                                                 "dialog.save-and-upgrade.button.upgrade-editable-files"
                                                                 "dialog.save-and-upgrade.button.upgrade-project-files"))
                                   :variant :danger
                                   :result :upgrade-editable-files}]

                                 workspace-has-non-editable-directories
                                 (conj {:text (localization/message "dialog.save-and-upgrade.button.upgrade-all-files")
                                        :variant :danger
                                        :result :upgrade-all-files}))
                 result (dialogs/make-confirmation-dialog
                          localization
                          {:title (localization/message "dialog.save-and-upgrade.confirm.title")
                           :size :large
                           :icon :icon/circle-question
                           :header (localization/message "dialog.save-and-upgrade.confirm.header")
                           :content {:fx/type fxui/legacy-label
                                     :style-class "dialog-content-padding"
                                     :text (localization (localization/message "dialog.save-and-upgrade.confirm.content"))}
                           :buttons buttons})
                 save-data-fn (case result
                                :upgrade-editable-files (partial project/upgraded-file-formats-save-data false)
                                :upgrade-all-files (partial project/upgraded-file-formats-save-data true)
                                nil)]

             (when save-data-fn
               ;; The user has opted to proceed with the file format upgrade.
               (project/clear-cached-save-data! project)
               (async-save! app-view changes-view project save-data-fn)))))))

(handler/defhandler :file.load-external-changes :global
  (active? [prefs] (not (async-reload-on-app-focus? prefs)))
  (enabled? [] (can-async-reload?))
  (run [app-view changes-view workspace] (async-reload! app-view changes-view workspace [])))

(handler/defhandler :file.show-in-desktop :global
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

(handler/defhandler :file.show-references :global
  (active? [app-view selection evaluation-context]
           (context-openable-resource app-view selection evaluation-context))
  (enabled? [app-view selection evaluation-context]
            (when-let [r (context-openable-resource app-view selection evaluation-context)]
              (and (resource/abs-path r)
                   (resource/exists? r))))
  (run [selection app-view prefs localization workspace project]
       (when-let [r (context-openable-resource app-view selection)]
         (let [selected-resources (resource-dialog/make
                                    workspace project
                                    {:title (localization/message "dialog.referencing-files.title")
                                     :selection :multiple
                                     :ok-label (localization/message "dialog.referencing-files.button.ok")
                                     :filter (format "refs:%s" (resource/proj-path r))})]
           (run! #(open-resource app-view prefs localization workspace project %)
                 (e/filter resource/openable-resource? selected-resources))))))

(handler/defhandler :file.show-dependencies :global
  (active? [app-view selection evaluation-context]
           (context-openable-resource app-view selection evaluation-context))
  (enabled? [app-view selection evaluation-context]
            (when-let [r (context-openable-resource app-view selection evaluation-context)]
              (and (resource/abs-path r)
                   (resource/exists? r)
                   (resource/loaded? r))))
  (run [selection app-view prefs localization workspace project]
       (when-let [r (context-openable-resource app-view selection)]
         (let [selected-resources (resource-dialog/make
                                    workspace project
                                    {:title (localization/message "dialog.dependencies.title")
                                     :selection :multiple
                                     :ok-label (localization/message "dialog.dependencies.button.ok")
                                     :filter (format "deps:%s" (resource/proj-path r))})]
           (run! #(open-resource app-view prefs localization workspace project %)
                 (e/filter resource/openable-resource? selected-resources))))))

(defn show-override-inspector!
  "Show override inspector view and focus on its tab

  Args:
    app-view               app view node id
    search-results-view    node id of a search result view
    node-id                root node id whose overrides are inspected
    properties             either :all or a coll of property keywords to include
                           in the override inspector output
    localization           the Localization instance"
  [app-view search-results-view node-id properties localization]
  (g/with-auto-evaluation-context evaluation-context
    (let [scene (g/node-value app-view :scene evaluation-context)
          tool-tab-pane (g/node-value app-view :tool-tab-pane evaluation-context)]
      (show-search-results! scene tool-tab-pane)
      (search-results-view/show-override-inspector! search-results-view node-id properties localization))))

(defn- select-possibly-overridable-resource-node [selection project evaluation-context]
  ;; TODO: This will return the outline-selected resource-node when used from an
  ;; editor tab context. Shouldn't we use the resource-node associated with the
  ;; editor tab in that scenario?
  (or (handler/selection->node-id selection)
      (when-let [resource (handler/adapt-single selection resource/Resource)]
        (when (resource/overridable? resource)
          (project/get-resource-node project resource evaluation-context)))))

(handler/defhandler :edit.show-overrides :global
  (enabled? [selection project evaluation-context]
    (let [node-id (select-possibly-overridable-resource-node selection project evaluation-context)]
      (and node-id (pos? (count (g/overrides (:basis evaluation-context) node-id))))))
  (run [selection search-results-view project app-view localization]
    (show-override-inspector!
      app-view
      search-results-view
      (g/with-auto-evaluation-context evaluation-context
        (select-possibly-overridable-resource-node selection project evaluation-context))
      :all
      localization)))

(handler/defhandler :edit.pull-up-overrides :global
  (enabled? [selection user-data evaluation-context]
    (if user-data
      (properties/can-transfer-overrides? (:transfer-overrides-plan user-data))
      (if-let [node-id (handler/selection->node-id selection)]
        (not (coll/empty? (g/overridden-properties node-id evaluation-context)))
        false))
    (or (some? user-data)
        (if-let [node-id (handler/selection->node-id selection)]
          (not (coll/empty? (g/overridden-properties node-id evaluation-context)))
          false)))
  (options [selection user-data]
    (when (nil? user-data)
      (when-let [node-id (handler/selection->node-id selection)]
        (g/with-auto-evaluation-context evaluation-context
          (when-let [source-prop-infos-by-prop-kw (properties/transferred-properties node-id :all evaluation-context)]
            (mapv (fn [transfer-overrides-plan]
                    {:label (properties/transfer-overrides-description transfer-overrides-plan evaluation-context)
                     :command :edit.pull-up-overrides
                     :user-data {:transfer-overrides-plan transfer-overrides-plan}})
                  (properties/pull-up-overrides-plan-alternatives node-id source-prop-infos-by-prop-kw evaluation-context)))))))
  (run [user-data]
    (properties/transfer-overrides! (:transfer-overrides-plan user-data))))

(handler/defhandler :edit.push-down-overrides :global
  (enabled? [selection user-data evaluation-context]
    (if user-data
      (properties/can-transfer-overrides? (:transfer-overrides-plan user-data))
      (if-let [node-id (handler/selection->node-id selection)]
        (let [basis (:basis evaluation-context)]
          (and (not (coll/empty? (g/overrides basis node-id)))
               (not (coll/empty? (g/overridden-properties node-id evaluation-context)))))
        false)))
  (options [selection user-data]
    (when (nil? user-data)
      (when-let [node-id (handler/selection->node-id selection)]
        (g/with-auto-evaluation-context evaluation-context
          (when-let [source-prop-infos-by-prop-kw (properties/transferred-properties node-id :all evaluation-context)]
            (mapv (fn [transfer-overrides-plan]
                    {:label (properties/transfer-overrides-description transfer-overrides-plan evaluation-context)
                     :command :edit.push-down-overrides
                     :user-data {:transfer-overrides-plan transfer-overrides-plan}})
                  (properties/push-down-overrides-plan-alternatives node-id source-prop-infos-by-prop-kw evaluation-context)))))))
  (run [user-data]
    (properties/transfer-overrides! (:transfer-overrides-plan user-data))))

(handler/defhandler :window.toggle-left-pane :global
  (run [^Stage main-stage]
       (let [main-scene (.getScene main-stage)]
         (set-pane-visible! main-scene :left (not (pane-visible? main-scene :left))))))

(handler/defhandler :window.toggle-right-pane :global
  (run [^Stage main-stage]
       (let [main-scene (.getScene main-stage)]
         (set-pane-visible! main-scene :right (not (pane-visible? main-scene :right))))))

(handler/defhandler :window.toggle-bottom-pane :global
  (run [^Stage main-stage]
       (let [main-scene (.getScene main-stage)]
         (set-pane-visible! main-scene :bottom (not (pane-visible? main-scene :bottom))))))

(handler/defhandler :window.toggle-changed-files-pane :global
  (enabled? [^Stage main-stage]
            (pane-visible? (.getScene main-stage) :left))
  (run [^Stage main-stage]
       (let [main-scene (.getScene main-stage)]
         (set-pane-visible! main-scene :changed-files (not (pane-visible? main-scene :changed-files))))))

(handler/defhandler :window.show-console :global
  (run [^Stage main-stage tool-tab-pane] (show-console! (.getScene main-stage) tool-tab-pane)))

(handler/defhandler :window.show-curve-editor :global
  (run [^Stage main-stage tool-tab-pane] (show-curve-editor! (.getScene main-stage) tool-tab-pane)))

(handler/defhandler :window.show-build-errors :global
  (run [^Stage main-stage tool-tab-pane] (show-build-errors! (.getScene main-stage) tool-tab-pane)))

(handler/defhandler :window.show-search-results :global
  (run [^Stage main-stage tool-tab-pane] (show-search-results! (.getScene main-stage) tool-tab-pane)))

(defn- put-on-clipboard!
  [s]
  (doto (Clipboard/getSystemClipboard)
    (.setContent (doto (ClipboardContent.)
                   (.putString s)))))

(handler/defhandler :edit.copy-resource-path :global
  (active? [app-view selection evaluation-context]
           (context-resource app-view selection evaluation-context))
  (enabled? [app-view selection evaluation-context]
            (when-let [r (context-resource app-view selection evaluation-context)]
              (and (resource/proj-path r)
                   (resource/exists? r))))
  (run [selection app-view]
    (when-let [r (context-resource app-view selection)]
      (put-on-clipboard! (resource/proj-path r)))))

(handler/defhandler :edit.copy-absolute-path :global
  (active? [app-view selection evaluation-context]
           (context-resource app-view selection evaluation-context))
  (enabled? [app-view selection evaluation-context]
            (when-let [r (context-resource app-view selection evaluation-context)]
              (and (resource/abs-path r)
                   (resource/exists? r))))
  (run [selection app-view]
    (when-let [r (context-resource app-view selection)]
      (put-on-clipboard! (resource/abs-path r)))))

(handler/defhandler :edit.copy-require-path :global
  (active? [app-view selection evaluation-context]
           (when-let [r (context-resource app-view selection evaluation-context)]
             (= "lua" (resource/type-ext r))))
  (enabled? [app-view selection evaluation-context]
            (when-let [r (context-resource app-view selection evaluation-context)]
              (and (resource/proj-path r)
                   (resource/exists? r))))
  (run [selection app-view]
       (when-let [r (context-resource app-view selection)]
         (put-on-clipboard! (lua/path->lua-module (resource/proj-path r))))))

(defn- gen-tooltip [workspace project app-view resource]
  (when (resource/loaded? resource)
    (let [resource-type (resource/resource-type resource)
          view-type (or (first (:view-types resource-type))
                        (workspace/get-view-type workspace :text))]
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
                             (g/delete-graph! view-graph)))))}))))

(def ^:private open-assets-term-prefs-key [:open-assets :term])

(defn- query-and-open! [workspace project app-view prefs localization term]
  (let [prev-filter-term (prefs/get prefs open-assets-term-prefs-key)
        filter-term-atom (atom prev-filter-term)
        selected-resources (resource-dialog/make workspace project
                                                 (cond-> {:title (localization/message "dialog.open-assets.title")
                                                          :accept-fn resource/editor-openable-resource?
                                                          :selection :multiple
                                                          :ok-label (localization/message "dialog.open-assets.button.ok")
                                                          :filter-atom filter-term-atom
                                                          :tooltip-gen (partial gen-tooltip workspace project app-view)}
                                                         (some? term)
                                                         (assoc :filter term)))
        filter-term @filter-term-atom]
    (when (not= prev-filter-term filter-term)
      (prefs/set! prefs open-assets-term-prefs-key filter-term))
    (doseq [resource selected-resources]
      (open-resource app-view prefs localization workspace project resource))))

(handler/defhandler :private/select-items :global
  (run [user-data localization]
    (dialogs/make-select-list-dialog (:items user-data) localization (:options user-data))))

(defn- get-view-text-selection [{:keys [view-id view-type]}]
  (when-let [text-selection-fn (:text-selection-fn view-type)]
    (text-selection-fn view-id)))

(defn file-open-user-data->openable-resources
  ([workspace x]
   (g/with-auto-evaluation-context evaluation-context
     (file-open-user-data->openable-resources workspace x evaluation-context)))
  ([workspace x evaluation-context]
   (cond
     (string? x)
     (when-let [resource (workspace/find-resource workspace x evaluation-context)]
       (recur workspace resource evaluation-context))

     (resource/resource? x)
     (when (and (resource/openable? x)
                (resource/exists? x))
       [x])

     (sequential? x)
     (e/mapcat #(file-open-user-data->openable-resources workspace % evaluation-context) x)

     :else
     (throw (IllegalArgumentException. (str "Didn't expect file.open argument to be " x))))))

(handler/defhandler :file.open :global
  (run [workspace project app-view prefs user-data localization]
    (if user-data
      (run! #(open-resource app-view prefs localization workspace project %) (file-open-user-data->openable-resources workspace user-data))
      (let [term (get-view-text-selection (g/node-value app-view :active-view-info))]
        (query-and-open! workspace project app-view prefs localization term)))))

(handler/defhandler :file.search :global
  (run [project app-view prefs localization search-results-view main-stage tool-tab-pane]
    (when-let [term (get-view-text-selection (g/node-value app-view :active-view-info))]
      (search-results-view/set-search-term! prefs term))
    (let [main-scene (.getScene ^Stage main-stage)
          show-search-results-tab! (partial show-search-results! main-scene tool-tab-pane)]
      (search-results-view/show-search-in-files-dialog! search-results-view project prefs localization show-search-results-tab!))))

(handler/defhandler :project.bundle :global
  (options [user-data _context]
    (when-not user-data
      (let [contexts [_context]]
        (into []
              (keep
                (fn [{:keys [command]}]
                  (when command
                    (when-let [handler+context (handler/active command contexts true)]
                      {:command :project.bundle
                       :label (handler/label handler+context)
                       :user-data {:command command
                                   :handler+context handler+context}}))))
              (handler/realize-menu :editor.bundle/menu)))))
  (run [prefs user-data]
    (let [{:keys [command handler+context]} user-data]
      (prefs/set! prefs [:bundle :last-bundle-command] (if (handler/synthetic-command? command) nil command))
      (when (handler/enabled? handler+context)
        (handler/run handler+context)))))

(handler/defhandler :project.rebundle :global
  (enabled? [evaluation-context prefs]
    (keyword? (prefs/get prefs [:bundle :last-bundle-command])))
  (run [prefs _context]
    (let [command (prefs/get prefs [:bundle :last-bundle-command])]
      (when-let [handler+context (handler/active command [_context] false)]
        (handler/run handler+context)))))

(defn reload-extensions! [app-view project kind workspace changes-view build-errors-view prefs localization web-server]
  (extensions/reload!
    project kind
    :prefs prefs
    :reload-resources! (fn reload-resources! []
                         (let [f (future/make)]
                           (disk/async-reload! (make-render-task-progress :resource-sync)
                                               workspace
                                               []
                                               changes-view
                                               (fn [success]
                                                 (if success
                                                   (future/complete! f nil)
                                                   (future/fail! f (RuntimeException. "Reload failed")))))
                           f))
    :display-output! (fn display-output! [type string]
                       (let [[console-type prefix] (case type
                                                     :err [:extension-error "ERROR:EXT: "]
                                                     :out [:extension-output ""])]
                         (doseq [line (string/split-lines string)]
                           (console/append-console-entry! console-type (str prefix line)))))
    :save! (fn save! []
             (let [f (future/make)
                   render-reload-progress! (make-render-task-progress :resource-sync)
                   render-save-progress! (make-render-task-progress :save-all)]
               (disk/async-save! render-reload-progress! render-save-progress! project/dirty-save-data project changes-view
                                 (fn [successful?]
                                   (if successful?
                                     (do (ui/user-data! (g/node-value app-view :scene) ::ui/refresh-requested? true)
                                         (future/complete! f nil))
                                     (future/fail! f (Exception. "Save failed")))))
               f))
    :open-resource! (fn open-resource! [resource]
                      (let [f (future/make)]
                        (ui/run-later
                          (try
                            (open-resource app-view prefs localization workspace project resource)
                            (catch Throwable e (error-reporting/report-exception! e)))
                          (future/complete! f nil))
                        f))
    :invoke-bob! (fn invoke-bob! [options commands evaluation-context]
                   (let [f (future/make)]
                     (fx/on-fx-thread
                       (let [options (cond-> options
                                             (not (contains? options "build-server"))
                                             (assoc "build-server" (native-extensions/get-build-server-url prefs project evaluation-context))
                                             (not (contains? options "build-server-header"))
                                             (assoc "build-server-header" (native-extensions/get-build-server-headers prefs)))
                             main-scene (g/node-value app-view :scene evaluation-context)
                             tool-tab-pane (g/node-value app-view :tool-tab-pane evaluation-context)
                             render-build-error! (make-render-build-error main-scene tool-tab-pane build-errors-view)
                             render-reload-progress! (make-render-task-progress :resource-sync)
                             render-save-progress! (make-render-task-progress :save-all)
                             [render-build-progress! build-task-cancelled?] (begin-task-progress! :build)
                             out (start-new-log-pipe!)]
                         (build-errors-view/clear-build-errors build-errors-view)
                         (disk/async-bob-build! render-reload-progress!
                                                render-save-progress!
                                                render-build-progress!
                                                out
                                                build-task-cancelled?
                                                render-build-error!
                                                commands
                                                options
                                                project
                                                changes-view
                                                (fn [successful]
                                                  (if successful
                                                    (future/complete! f nil)
                                                    (future/fail! f (LuaError. "Bob invocation failed")))
                                                  (.close out)))))
                     f))
    :web-server web-server)
  (ui/invalidate-menubar-item! ::project/bundle))

(defn- fetch-libraries [app-view workspace project changes-view build-errors-view prefs localization web-server]
  (let [library-uris (project/project-dependencies project)
        hosts (into #{} (map url/strip-path) library-uris)]
    (if-let [first-unreachable-host (first-where (complement url/reachable?) hosts)]
      (dialogs/make-info-dialog
        localization
        {:title (localization/message "dialog.fetch-libraries.host-unreachable.title")
         :icon :icon/triangle-error
         :size :large
         :header (localization/message "dialog.fetch-libraries.host-unreachable.header")
         :content (localization/message "dialog.fetch-libraries.host-unreachable.content" {"host" first-unreachable-host})})
      (future
        (error-reporting/catch-all!
          (ui/with-progress [render-fetch-progress! (make-render-task-progress :fetch-libraries)]
            (when (workspace/dependencies-reachable? library-uris)
              (let [lib-states (workspace/fetch-and-validate-libraries workspace library-uris render-fetch-progress!)
                    render-install-progress! (make-render-task-progress :resource-sync)]
                (render-install-progress! (progress/make (localization/message "progress.installing-updated-libraries")))
                (ui/run-later
                  (workspace/install-validated-libraries! workspace lib-states)
                  (disk/async-reload! render-install-progress! workspace [] changes-view
                                      (fn [success]
                                        (when success
                                          (reload-extensions! app-view project :library workspace changes-view build-errors-view prefs localization web-server)
                                          (project/update-fetch-libraries-notification! project)))))))))))))

(handler/defhandler :private/add-dependency :global
  (enabled? [] (disk-availability/available?))
  (run [selection app-view workspace project changes-view user-data build-errors-view prefs localization web-server]
    (let [game-project (project/get-resource-node project "/game.project")
          dependencies (game-project/get-setting game-project ["project" "dependencies"])
          dependency-uri (.toURI (URL. (:dep-url user-data)))]
      (when (not-any? (partial = dependency-uri) dependencies)
        (game-project/set-setting! game-project ["project" "dependencies"]
                                   (conj (vec dependencies) dependency-uri))
        (fetch-libraries app-view workspace project changes-view build-errors-view prefs localization web-server)))))

(handler/defhandler :project.fetch-libraries :global
  (enabled? [] (disk-availability/available?))
  (run [app-view workspace project changes-view build-errors-view prefs localization web-server]
    (fetch-libraries app-view workspace project changes-view build-errors-view prefs localization web-server)))

(handler/defhandler :project.reload-editor-scripts :global
  (enabled? [] (disk-availability/available?))
  (run [app-view project workspace changes-view build-errors-view prefs localization web-server]
    (reload-extensions! app-view project :all workspace changes-view build-errors-view prefs localization web-server)))

(defn- ensure-exists-and-open-for-editing! [proj-path app-view changes-view prefs localization project]
  (let [workspace (project/workspace project)
        resource (workspace/resolve-workspace-resource workspace proj-path)]
    (if (resource/exists? resource)
      (open-resource app-view prefs localization workspace project resource)
      (let [render-reload-progress! (make-render-task-progress :resource-sync)]
        (fs/touch-file! (io/as-file resource))
        (disk/async-reload! render-reload-progress! workspace [] changes-view
                            (fn [successful?]
                              (when successful?
                                (when-some [created-resource (workspace/find-resource workspace proj-path)]
                                  (open-resource app-view prefs localization workspace project created-resource)))))))))

(handler/defhandler :file.open-liveupdate-settings :global
  (enabled? [] (disk-availability/available?))
  (run [app-view changes-view prefs localization workspace project]
       (let [live-update-settings-proj-path (live-update-settings/get-live-update-settings-path project)]
         (ensure-exists-and-open-for-editing! live-update-settings-proj-path app-view changes-view prefs localization project))))

(handler/defhandler :file.open-shared-editor-settings :global
  (enabled? [] (disk-availability/available?))
  (run [app-view changes-view prefs localization workspace project]
       (ensure-exists-and-open-for-editing! shared-editor-settings/project-shared-editor-settings-proj-path app-view changes-view prefs localization project)))

(defn- get-linux-desktop-entry [launcher-path install-dir]
  (str "[Desktop Entry]\n"
       "Name=Defold\n"
       "Comment=An out of the box, turn-key solution for multi-platform game development\n"
       "Terminal=false\n"
       "Type=Application\n"
       "StartupWMClass=com.defold.editor.Start\n"
       "Categories=Games;Development;Editor;\n"
       "StartupNotify=true\n"
       "Exec=" launcher-path "\n"
       "Icon=" install-dir "/logo_blue.png\n"))

(def ^:private xdg-desktop-menu-path
  (delay
    (when (os/is-linux?)
      (try
        (process/exec! "which" "xdg-desktop-menu")
        (catch Throwable _)))))

(handler/defhandler :file.create-desktop-entry :global
  (active? [] (some? @xdg-desktop-menu-path))
  (enabled? [] (and (system/defold-resourcespath) (system/defold-launcherpath)))
  (run [localization]
    (let [xdg-desktop-menu @xdg-desktop-menu-path
          install-dir (.getCanonicalFile (io/file (system/defold-resourcespath)))
          launcher-path (.getCanonicalFile (io/file (system/defold-launcherpath)))
          desktop-entry (get-linux-desktop-entry launcher-path install-dir)
          desktop-entry-file (io/file install-dir "defold-editor.desktop")]
      (try
        (spit desktop-entry-file desktop-entry)
        (process/exec! xdg-desktop-menu "install" "--mode" "user" (str desktop-entry-file))
        (fs/delete! desktop-entry-file)
        (dialogs/make-info-dialog
          localization
          {:title (localization/message "dialog.desktop-entry.created.title")
           :header (localization/message "dialog.desktop-entry.created.header")
           :icon :icon/circle-happy
           :content {:wrap-text true
                     :text (localization/message "dialog.desktop-entry.created.content")}})
        (catch Exception e
          (dialogs/make-info-dialog
            localization
            {:title (localization/message "dialog.desktop-entry.creation-failed.title")
             :header (localization/message "dialog.desktop-entry.creation-failed.header")
             :icon :icon/triangle-error
             :content {:wrap-text true
                       :text (localization/message "dialog.desktop-entry.creation-failed.content" {"error" (.getMessage e)})}}))))))
