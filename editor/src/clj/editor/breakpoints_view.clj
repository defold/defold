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

(ns editor.breakpoints-view
  (:require [cljfx.api :as fx]
            [cljfx.ext.table-view :as fx.ext.table-view]
            [cljfx.fx.button :as fx.button]
            [cljfx.fx.check-box :as fx.check-box]
            [cljfx.fx.h-box :as fx.h-box]
            [cljfx.fx.stack-pane :as fx.stack-pane]
            [cljfx.fx.svg-path :as fx.svg-path]
            [cljfx.fx.table-cell :as fx.table-cell]
            [cljfx.fx.table-column :as fx.table-column]
            [cljfx.fx.table-row :as fx.table-row]
            [cljfx.fx.table-view :as fx.table-view]
            [cljfx.fx.text-field :as fx.text-field]
            [cljfx.lifecycle :as fx.lifecycle]
            [cljfx.mutator :as fx.mutator]
            [cljfx.prop :as fx.prop]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.code.data :as code-data]
            [editor.defold-project :as project]
            [editor.fxui :as fxui]
            [editor.handler :as handler]
            [editor.localization :as localization]
            [editor.menu-items :as menu-items]
            [editor.prefs :as prefs]
            [editor.resource :as resource]
            [editor.ui :as ui]
            [editor.workspace :as workspace]
            [util.fn :as fn])
  (:import [javafx.beans.property ReadOnlyProperty]
           [javafx.beans.value ChangeListener]
           [javafx.scene Node Parent]
           [javafx.scene.control TableView TextField]
           [javafx.scene.input KeyCode KeyEvent MouseButton MouseEvent]
           [javafx.scene.layout AnchorPane]))

(set! *warn-on-reflection* true)

(def ^:private toolbar-height 57)

(def ^:private edit-bp-icon-path "M3 17.25V21h3.75L17.81 9.94l-3.75-3.75L3 17.25zM20.71 7.04c.39-.39.39-1.02 0-1.41l-2.34-2.34c-.39-.39-1.02-.39-1.41 0l-1.83 1.83 3.75 3.75 1.83-1.83z")
(def ^:private delete-bp-icon-path "M12 0C5.37 0 0 5.37 0 12s5.37 12 12 12 12-5.37 12-12S18.63 0 12 0zm0 20c-4.41 0-8-3.59-8-8s3.59-8 8-8 8 3.59 8 8-3.59 8-8 8zm6.36-13.64L15.41 12l2.95 2.95-2.95 2.95L12 15.41l-3.41 3.41-2.95-2.95L8.59 12 5.64 9.05l2.95-2.95L12 8.59l3.41-3.41z")
(def ^:private remove-condition-icon-path "M19 6.41L17.59 5 12 10.59 6.41 5 5 6.41 10.59 12 5 17.59 6.41 19 12 13.41 17.59 19 19 17.59 13.41 12z")

(defn- update-breakpoints [breakpoints breakpoints-batch f]
  (let [bp-set (set breakpoints-batch)]
    (mapv (fn [bp]
            (if (bp-set bp)
              (f bp)
              bp))
          breakpoints)))

(defn- toggle-breakpoints-enabled [breakpoints breakpoint-batch]
  (update-breakpoints breakpoints breakpoint-batch #(assoc % :enabled (not (:enabled %)))))

(defn- toggle-breakpoint-enabled [breakpoints breakpoint]
  (toggle-breakpoints-enabled breakpoints [breakpoint]))

(defn- update-breakpoints-enabled-state [breakpoints breakpoint-batch enabled]
  (update-breakpoints breakpoints breakpoint-batch #(assoc % :enabled enabled)))

(defn- update-breakpoint-condition [breakpoints breakpoint condition]
  (update-breakpoints breakpoints [breakpoint] #(if (not (string/blank? condition))
                                                  (assoc % :condition condition)
                                                  (dissoc % :condition))))

(defn- collect-script-nodes-from-breakpoints [project breakpoints evaluation-context]
  (for [[resource bps] (group-by :resource breakpoints)
        :let [script-node (project/get-resource-node project resource evaluation-context)]]
    {:script-node script-node :resource resource :breakpoints bps}))

(defn- breakpoint->script-node [project breakpoint evaluation-context]
  (project/get-resource-node project (:resource breakpoint) evaluation-context))

(defn- breakpoint->region [lines breakpoint]
  (let [region (code-data/make-breakpoint-region lines (:row breakpoint))]
    (merge region (select-keys breakpoint [:condition :enabled]))))

(defn- update-script-regions-from-breakpoints [script-node breakpoints evaluation-context]
  (let [lines (g/node-value script-node :lines evaluation-context)
        regions (g/node-value script-node :regions evaluation-context)
        non-bp-regions (remove code-data/breakpoint-region? regions)
        bp-regions (map (partial breakpoint->region lines) breakpoints)]
    (vec (sort (concat non-bp-regions bp-regions)))))

(defn- get-breakpoints-in-script [breakpoints breakpoint]
  (filterv #(= (:resource %) (:resource breakpoint)) breakpoints))

(defn- set-breakpoint-condition! [project breakpoints breakpoint condition evaluation-context]
  (let [script-node (breakpoint->script-node project breakpoint evaluation-context)
        breakpoints-in-script (get-breakpoints-in-script breakpoints breakpoint)
        updated-breakpoints (update-breakpoint-condition breakpoints-in-script breakpoint condition)
        regions (update-script-regions-from-breakpoints script-node updated-breakpoints evaluation-context)]
    (g/set-property! script-node :regions regions)))

(defn- set-regions-with-action! [swap-state project evaluation-context all-breakpoints breakpoints action-fn]
  (let [affected-scripts (collect-script-nodes-from-breakpoints project breakpoints evaluation-context)
        updated-breakpoints (action-fn all-breakpoints breakpoints)
        updated-by-resource (group-by :resource updated-breakpoints)
        txs (mapcat (fn [{:keys [script-node resource]}]
                      (let [bps-for-script (get updated-by-resource resource [])
                            new-regions (update-script-regions-from-breakpoints script-node bps-for-script evaluation-context)]
                        (g/set-property script-node :regions new-regions)))
                    affected-scripts)]
    (g/transact txs) ;; Ignore the results
    ;; All actions should disable editing
    (swap-state assoc :edited-breakpoint nil :breakpoints updated-breakpoints)))

(defn- make-open-resource-fn [open-resource-fn]
  (fn [resource line]
    (when resource
      (open-resource-fn resource {:cursor-range (code-data/line-number->CursorRange line)}))
    nil))

(defn- save-breakpoints! [prefs breakpoints]
  (let [bps-prefs (mapv #(dissoc (assoc % :proj-path (resource/proj-path (:resource %))) :resource) breakpoints)]
    (prefs/set! prefs [:code :breakpoints] bps-prefs)))

(defn- restore-breakpoints! [project prefs]
  (g/with-auto-evaluation-context evaluation-context
    (let [breakpoints (keep #(when-some [resource (workspace/find-resource (project/workspace project)
                                                                           (:proj-path %)
                                                                           evaluation-context)]
                               (assoc % :resource resource))
                            (prefs/get prefs [:code :breakpoints]))
          script-bps (collect-script-nodes-from-breakpoints project breakpoints evaluation-context)]
      (g/transact
        (for [{:keys [script-node breakpoints]} script-bps
              :let [updated-regions (update-script-regions-from-breakpoints script-node breakpoints evaluation-context)]]
          (g/set-property script-node :regions updated-regions))))))

(handler/register-menu! ::breakpoint-menu
  [{:label (localization/message "command.file.show-in-assets")
    :icon "icons/32/Icons_S_14_linkarrow.png"
    :command :file.show-in-assets}
   menu-items/separator
   {:label (localization/message "breakpoints.context-menu.edit-selected")
    :command :breakpoints.edit-selected}
   {:label (localization/message "breakpoints.context-menu.toggle-selected-enabled")
    :command :breakpoints.toggle-selected-enabled}
   menu-items/separator
   {:label (localization/message "breakpoints.context-menu.remove-selected")
    :command :breakpoints.remove-selected}])

(defn- breakpoints-toolbar-view [project localization-state state swap-state]
  (let [breakpoints (:breakpoints state)
        action-all-fn (fn [action-fn _]
                        (g/with-auto-evaluation-context evaluation-context
                          (set-regions-with-action! swap-state project evaluation-context breakpoints breakpoints action-fn)))]
    {:fx/type fx.h-box/lifecycle
     :id "breakpoints-tool-bar"
     :anchor-pane/top 0
     :anchor-pane/left 0
     :anchor-pane/right 0
     :pref-height toolbar-height
     :alignment :center-left
     :spacing 10
     :children [{:fx/type fx.button/lifecycle
                 :id "breakpoints-enable-all"
                 :text (localization-state (localization/message "breakpoints.button.enable-all"))
                 :on-action (fn/partial action-all-fn #(update-breakpoints-enabled-state %1 %2 true))}
                {:fx/type fx.button/lifecycle
                 :id "breakpoints-disable-all"
                 :text (localization-state (localization/message "breakpoints.button.disable-all"))
                 :on-action (fn/partial action-all-fn #(update-breakpoints-enabled-state %1 %2 false))}
                {:fx/type fx.button/lifecycle
                 :id "breakpoints-toggle-all"
                 :text (localization-state (localization/message "breakpoints.button.toggle-all"))
                 :on-action (fn/partial action-all-fn toggle-breakpoints-enabled)}
                {:fx/type fx.button/lifecycle
                 :id "breakpoints-remove-all"
                 :text (localization-state (localization/message "breakpoints.button.remove-all"))
                 :on-action (fn/partial action-all-fn #(vec (remove (set %2) %1)))}]}))

(defn- icon-button [icon-path classes on-action-event]
  {:fx/type fx.button/lifecycle
   :style-class (concat ["icon-button"] classes)
   :graphic {:fx/type fx.stack-pane/lifecycle
             :min-width 20
             :min-height 20
             :alignment :center
             :children [{:fx/type fx.svg-path/lifecycle
                         :content icon-path
                         :scale-x 0.6
                         :scale-y 0.6
                         :style-class ["icon-button-graphic"]}]}
   :on-action on-action-event})

(defn- table-row-factory [open-resource-fn breakpoints idx]
  (when-let [bp (get breakpoints idx)]
    {:on-mouse-clicked
     (fn [event]
       (let [^MouseEvent e event
             {:keys [resource row]} bp]
         (when (and (= MouseButton/PRIMARY (.getButton e))
                    (= 2 (.getClickCount e)))
           (open-resource-fn resource (inc row)))))}))

(defn- column-enabled-cell-factory [project breakpoints idx]
  (when-let [breakpoint (get breakpoints idx)]
    {:style-class ["enabled-cell"]
     :alignment :center
     :graphic
     {:fx/type fx.check-box/lifecycle
      :selected (:enabled breakpoint)
      :on-selected-changed
      (fn [enabled?]
        ;; TODO: I think we can somewhat generalize this?
        (g/with-auto-evaluation-context evaluation-context
          (let [breakpoints-in-script (get-breakpoints-in-script breakpoints breakpoint)
                script-node (breakpoint->script-node project breakpoint evaluation-context)
                toggled-breakpoint (toggle-breakpoint-enabled breakpoints-in-script breakpoint)
                regions (update-script-regions-from-breakpoints script-node toggled-breakpoint evaluation-context)]
            ;; (swap-state assoc :edited-breakpoint nil)
            (g/set-property! script-node :regions regions))))}}))

(defn- column-line-cell-factory [breakpoints idx]
  (when-let [breakpoint (get breakpoints idx)]
    {:text (str (inc (:row breakpoint)))}))

(defn- column-name-cell-factory [breakpoints idx]
  (when-let [breakpoint (get breakpoints idx)]
    {:text (:name (:resource breakpoint))}))

(defn- column-path-cell-factory [breakpoints idx]
  (when-let [breakpoint (get breakpoints idx)]
    {:text (:project-path (:resource breakpoint))}))

(defn- condition-value-field [{:keys [state swap-state value on-value-changed on-focused-changed] :as props}]
  (let [current (:value state value)]
    {:fx/type fx.text-field/lifecycle
     :text current
     :on-focused-changed on-focused-changed
     :on-text-changed (fn [new-text]
                        (swap-state assoc :value new-text))
     :on-key-pressed (fn [^KeyEvent e]
                       (condp = (.getCode e)
                         KeyCode/ENTER
                         (do (.consume e)
                             (when on-value-changed
                               (on-value-changed current)))

                         KeyCode/ESCAPE
                         (do (.consume e)
                             (when on-value-changed
                               (on-value-changed nil)))

                         nil))}))

(defn- column-condition-cell-factory [state swap-state project breakpoints idx]
  (when-let [breakpoint (get breakpoints idx)]
    (let [condition (:condition breakpoint)
          editing? (= (:edited-breakpoint state) breakpoint)]
      (if editing?
        {:graphic
         {:fx/type fx/ext-on-instance-lifecycle
          :on-created (fn [^TextField node]
                        (ui/run-later (.requestFocus node)))
          :desc {:fx/type fx/ext-state
                 :initial-state {:value condition}
                 :desc {:fx/type condition-value-field
                        :value condition
                        :on-focused-changed (fn [focused]
                                              (when-not focused (swap-state assoc :edited-breakpoint nil)))
                        :on-value-changed
                        (fn [new-condition]
                          (g/with-auto-evaluation-context evaluation-context
                            (when new-condition
                              (set-breakpoint-condition! project breakpoints breakpoint new-condition evaluation-context))
                            (swap-state assoc :edited-breakpoint nil)))}}}}
        {:graphic {:fx/type fx.stack-pane/lifecycle
                   :children
                   [{:fx/type :label
                     :stack-pane/alignment :center-left
                     :text-overrun :ellipsis
                     :text (or condition "")}
                    {:fx/type fx.h-box/lifecycle
                     :stack-pane/alignment :center-right
                     :fill-height false
                     :max-width :use-pref-size
                     :max-height :use-pref-size
                     :spacing 5
                     :children
                     (concat
                       (when condition
                         [(icon-button remove-condition-icon-path []
                                       (fn [_]
                                         (g/with-auto-evaluation-context evaluation-context
                                           (set-breakpoint-condition! project breakpoints breakpoint nil evaluation-context)
                                           (swap-state assoc :edited-breakpoint nil))))])
                       [(icon-button edit-bp-icon-path [] (fn [_] (swap-state assoc :edited-breakpoint breakpoint)))])}]}
         :on-mouse-clicked (fn [event]
                             (let [^MouseEvent me event]
                               (when (= 2 (.getClickCount me))
                                 ;; NOTE: If we don't consume this it'll fall through to the row double click
                                 (.consume me)
                                 (swap-state assoc :edited-breakpoint breakpoint))))}))))

(defn- column-remove-btn-cell-factory [swap-state project breakpoints idx]
  (when-let [breakpoint (get breakpoints idx)]
    {:alignment :center
     :graphic
     {:fx/type fx.h-box/lifecycle
      :alignment :center-left
      :children
      ;; TODO: Organize this a bit better
      [(icon-button
         delete-bp-icon-path
         ["remove-button"]
         (fn [event]
           (g/with-auto-evaluation-context evaluation-context
             (let [breakpoints-in-script (get-breakpoints-in-script breakpoints breakpoint)
                   script-node (breakpoint->script-node project breakpoint evaluation-context)
                   remaining (filterv #(not (= breakpoint %)) breakpoints-in-script)
                   regions (update-script-regions-from-breakpoints script-node remaining evaluation-context)]
               (swap-state #(assoc % :breakpoints (vec (remove #{breakpoint} (:breakpoints %)))))
               (g/set-property! script-node :regions regions)))))]}}))

(defn- ->breakpoints-selection-provider [table-view breakpoints]
  (reify handler/SelectionProvider
    (selection [this] (mapv #(get breakpoints %) (ui/selection table-view)))
    (succeeding-selection [this] [])
    (alt-selection [this] [])))

(def ^:private prop-table-context-menu
  (fx.prop/make (fx.mutator/setter ui/register-context-menu) fx.lifecycle/scalar))

(def ^:private prop-property-context
  (fx.prop/make
    (fx.mutator/setter
      (fn [table-view [project breakpoints swap-state]]
        (let [selection-provider (->breakpoints-selection-provider table-view breakpoints)]
          (ui/context! table-view :breakpoints-view
                       {:project project
                        :table-view table-view
                        :swap-state swap-state
                        :breakpoints breakpoints
                        :selection-provider selection-provider}
                       selection-provider
                       {}
                       {resource/Resource #(:resource %)}))))
    fx.lifecycle/scalar))

(defn- breakpoints-table-view [project open-resource-fn localization-state state swap-state]
  (let [{:keys [breakpoints selected-indices]} state]
    {:fx/type fx.ext.table-view/with-selection-props
     :anchor-pane/top toolbar-height
     :anchor-pane/right 0
     :anchor-pane/bottom 0
     :anchor-pane/left 0
     :props {:selection-mode :multiple
             :on-selected-indices-changed #(swap-state assoc :selected-indices %)}
     :desc
     {:fx/type fx.table-view/lifecycle
      :id "breakpoints-table-view"
      prop-property-context [project breakpoints swap-state]
      prop-table-context-menu ::breakpoint-menu
      :fixed-cell-size 33.0
      :column-resize-policy TableView/CONSTRAINED_RESIZE_POLICY
      :row-factory {:fx/cell-type fx.table-row/lifecycle
                    :describe (fn/partial table-row-factory open-resource-fn breakpoints)}
      :items (range (count breakpoints))
      :columns
      [{:fx/type fx.table-column/lifecycle
        :text (localization-state (localization/message "breakpoints.column.enabled"))
        :pref-width 60
        :min-width 60
        :max-width 80
        :cell-value-factory identity
        :cell-factory {:fx/cell-type fx.table-cell/lifecycle
                       :describe (fn/partial column-enabled-cell-factory project breakpoints)}}
       {:fx/type fx.table-column/lifecycle
        :text (localization-state (localization/message "breakpoints.column.line"))
        :pref-width 50
        :min-width 50
        :max-width 100
        :cell-value-factory identity
        :cell-factory {:fx/cell-type fx.table-cell/lifecycle
                       :describe (fn/partial column-line-cell-factory breakpoints)}}
       {:fx/type fx.table-column/lifecycle
        :text (localization-state (localization/message "breakpoints.column.name"))
        :pref-width 200
        :cell-value-factory identity
        :cell-factory {:fx/cell-type fx.table-cell/lifecycle
                       :describe (fn/partial column-name-cell-factory breakpoints)}}
       {:fx/type fx.table-column/lifecycle
        :text (localization-state (localization/message "breakpoints.column.condition"))
        :pref-width 250
        :cell-value-factory identity
        :cell-factory {:fx/cell-type fx.table-cell/lifecycle
                       :style-class ["condition-cell"]
                       :describe (fn/partial column-condition-cell-factory state swap-state project breakpoints)}}
       {:fx/type fx.table-column/lifecycle
        :text (localization-state (localization/message "breakpoints.column.path"))
        :style-class ["path-cell"]
        :pref-width 200
        :cell-value-factory identity
        :cell-factory {:fx/cell-type fx.table-cell/lifecycle
                       :describe (fn/partial column-path-cell-factory breakpoints)}}
       {:fx/type fx.table-column/lifecycle
        :pref-width 50
        :reorderable false
        :resizable false
        :sortable false
        :cell-value-factory identity
        :cell-factory {:fx/cell-type fx.table-cell/lifecycle
                       :describe (fn/partial column-remove-btn-cell-factory swap-state project breakpoints)}}]}}))

(fxui/defc breakpoints-view
  {:compose [{:fx/type fx/ext-watcher
              :ref (:localization (:context props))
              :key :localization-state}
             {:fx/type fx/ext-state
              :initial-state {:breakpoints (:breakpoints (:context props))
                              :selected-indices []
                              :edited-breakpoint nil
                              :edited-condition nil}}]}
  [{:keys [context localization-state parent state swap-state]}]
  (let [project (:project context)
        open-resource-fn (:open-resource-fn context)]
    {:fx/type fxui/ext-with-anchor-pane-props
     :desc {:fx/type fxui/ext-value :value parent}
     :props {:children [(breakpoints-toolbar-view project localization-state state swap-state)
                        (breakpoints-table-view project open-resource-fn localization-state state swap-state)]}}))

(g/defnk produce-breakpoints-ui [parent-view open-resource-fn breakpoints workspace project prefs localization]
  (save-breakpoints! prefs (g/node-value project :breakpoints))
  (let [context {:workspace workspace
                 :project project
                 :prefs prefs
                 :breakpoints breakpoints
                 :open-resource-fn open-resource-fn
                 :localization localization}]
    (fxui/advance-ui-user-data-component!
      parent-view ::breakpoints
      {:fx/type breakpoints-view
       :context context
       :parent parent-view})))

(g/defnode BreakpointsView
  (property parent-view Parent)
  (property prefs g/Any)
  (property open-resource-fn g/Any)

  (input workspace g/Any)
  (input localization g/Any)
  (input project g/Any)
  (input breakpoints g/Any)

  (output anchor-pane AnchorPane :cached produce-breakpoints-ui))

(defn make-breakpoints-view [workspace project open-resource-fn app-view view-graph prefs ^Node parent]
  (restore-breakpoints! project prefs)
  (first
    (g/tx-nodes-added
      (g/transact
        (let [open-res-fn (make-open-resource-fn open-resource-fn)]
          (g/make-nodes view-graph [view [BreakpointsView :parent-view parent :prefs prefs :open-resource-fn open-res-fn]]
            (g/connect workspace :_node-id view :workspace)
            (g/connect workspace :localization view :localization)
            (g/connect project :breakpoints view :breakpoints)
            (g/connect project :_node-id view :project)))))))

(handler/defhandler :breakpoints.toggle-selected-enabled :breakpoints-view
  (run [project swap-state breakpoints selection]
    (g/with-auto-evaluation-context evaluation-context
      (set-regions-with-action! swap-state project evaluation-context breakpoints selection toggle-breakpoints-enabled))))

(handler/defhandler :breakpoints.remove-selected :breakpoints-view
  (run [project swap-state breakpoints selection]
    (g/with-auto-evaluation-context evaluation-context
      (set-regions-with-action! swap-state project evaluation-context breakpoints selection #(vec (remove (set %2) %1))))))

(handler/defhandler :breakpoints.edit-selected :breakpoints-view
  (enabled? [selection] (= (count selection) 1))
  (run [project swap-state breakpoints selection]
    (g/with-auto-evaluation-context evaluation-context
      (swap-state assoc :edited-breakpoint (first selection)))))

(comment
  (g/node-value @the-view :breakpoints)
  (handler/selection (ui/->selection-provider (.lookup (ui/main-root) "#breakpoints-table-view")))
  (g/node-value (dev/app-view) :auto-pulls)
  (g/node-value (dev/project) :breakpoints)
  (def the-view (atom nil))
  (let [bp-container (.lookup (ui/main-root) "#breakpoints-container")
        open-resource (partial #'editor.app-view/open-resource
                               (dev/app-view) (dev/prefs) (dev/localization) (dev/workspace) (dev/project))
        bp-view (make-breakpoints-view (dev/workspace) (dev/project) open-resource (dev/app-view)
                                       editor.boot-open-project/*view-graph*
                                       (dev/prefs) bp-container)
        auto-pulls [[bp-view :anchor-pane]]]
    (reset! the-view bp-view)
    (g/transact (concat (g/update-property (dev/app-view) :auto-pulls into auto-pulls))))

  (let [tab-pane (ui/parent-tab-pane (.lookup (ui/main-root) "#breakpoints-container"))
        table (.lookup (ui/main-root) "#breakpoints-table-view")]
    (ui/context! table :breakpoints-view
                 {:project (dev/project) :table table}
                 (ui/->selection-provider table)
                 {}
                 {resource/Resource (fn [idx] (-> @state :breakpoints (get idx) :resource))}))

  (let [table (.lookup (ui/main-root) "#breakpoints-table-view")]
    (ui/register-context-menu table ::breakpoint-menu true))

  (g/with-auto-evaluation-context ec
    (let [script (project/get-resource-node (dev/project)
                                            (workspace/find-resource (dev/workspace) "/scripts/game.script" ec)
                                            ec)]
      (g/node-value script :regions)))

  (defn save-breakpoints! [_ _] nil)
  (prefs/get (dev/prefs) [:code :breakpoints])
  (save-breakpoints! (dev/prefs) (g/node-value (dev/project) :breakpoints))
  (restore-breakpoints! (dev/project) (dev/prefs))

  (ui/run-command (.lookup (ui/main-root) "#breakpoints-table-view") :breakpoints-view.edit-breakpoint)

  (prefs/get (dev/prefs) [:window :keymap])

  (defn print-scene-graph
    ([node]
     (print-scene-graph node 0))
    ([node depth]
     (let [indent (apply str (repeat depth "  "))
           node-type (-> node .getClass .getSimpleName)
           node-id (.getId node)
           node-style-class (when (instance? javafx.scene.Node node)
                              (.getStyleClass node))]
       (println (str indent node-type
                     (when node-id (str " [id=" node-id "]"))
                     (when (seq node-style-class) (str " " node-style-class))))

       (cond
         (instance? javafx.scene.Parent node)
         (doseq [child (.getChildrenUnmodifiable node)]
           (print-scene-graph child (inc depth)))

         (instance? javafx.scene.control.Control node)
         (when-let [skin (.getSkin node)]
           (when (instance? javafx.scene.Parent (.getNode skin))
             (print-scene-graph (.getNode skin) (inc depth))))))))
  (print-scene-graph (.lookup (ui/main-root) "#breakpoints-container"))
  (ui/run-now (ui/reload-root-styles!))
  :-)
