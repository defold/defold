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
            [clojure.java.io :as io]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.code.data :as code-data]
            [editor.defold-project :as project]
            [editor.editor-extensions.ui-components :as ui-components]
            [editor.error-reporting :as error-reporting]
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
           [javafx.scene Node]
           [javafx.scene.control TableView TextField]
           [javafx.scene.input KeyCode KeyEvent MouseButton MouseEvent]))

(set! *warn-on-reflection* true)

(defonce state (atom {:breakpoints []
                      :selected-indices []
                      :hovered-condition nil
                      :hovered-row nil
                      :edited-breakpoint nil
                      :edited-condition nil}))

;; NOTE: Updating the state while editing text wasn't playing nice, maybe better to keep it out of the state
(defonce condition-text (atom nil))

(def ext-with-focus-changed-handler
  (fx/make-ext-with-props
   {:on-focused-changed
    (fx.prop/make
      (fx.mutator/property-change-listener #(.focusedProperty ^TextField %))
      (fx.lifecycle/wrap-coerce
        fx.lifecycle/event-handler
        (fn [f]
          (reify ChangeListener
            (changed [_ observable-value _ v]
              (when-let [scene (.getScene ^Node (.getBean ^ReadOnlyProperty observable-value))]
                (f {:window (.getWindow scene)
                    :value v})))))))}))

(defn- icon-button [icon-path on-action-event]
  {:fx/type fx.button/lifecycle
   :style-class ["icon-button"]
   :graphic {:fx/type fx.stack-pane/lifecycle
             :min-width 12
             :min-height 12
             :max-width 12
             :max-height 12
             :alignment :center
             :children [{:fx/type fx.svg-path/lifecycle
                         :content icon-path
                         :scale-x 0.5
                         :scale-y 0.5
                         :style-class ["icon-button-graphic"]}]}
   :on-action on-action-event})

(defn- column-cell-factory [state column-id breakpoint-idx]
  ;; NOTE: We receive nil sometimes
  (when-let [breakpoint (get (:breakpoints state) breakpoint-idx)]
    (case column-id
      ::column-enabled
      {:style-class ["enabled-cell"]
       :alignment :center
       :graphic {:fx/type fx.check-box/lifecycle
                 :selected (:enabled breakpoint)
                 :on-selected-changed {:event-type :toggle-enabled
                                       :breakpoint breakpoint}}}

      ::colum-line {:text (str (inc (:row breakpoint)))}
      ::column-name {:text (get-in breakpoint [:resource :name])}
      ::column-path {:text (get-in breakpoint [:resource :project-path])}

      ::column-actions
      (let [hovered? (= (:hovered-row state) breakpoint)]
        (when hovered?
          {:alignment :center
           :graphic {:fx/type fx.h-box/lifecycle
                     :alignment :center-left
                     :children [(assoc (icon-button "M6 19c0 1.1.9 2 2 2h8c1.1 0 2-.9 2-2V7H6v12zM19 4h-3.5l-1-1h-5l-1 1H5v2h14V4z"
                                                    {:event-type :remove-breakpoint
                                                     :breakpoint breakpoint})
                                       :style-class ["icon-button" "remove-button"])]}}))

      ::column-condition
      (let [condition (:condition breakpoint)
            editing? (= (:edited-breakpoint state) breakpoint)
            hovered? (= (:hovered-condition state) breakpoint)
            edit-icon "M3 17.25V21h3.75L17.81 9.94l-3.75-3.75L3 17.25zM20.71 7.04c.39-.39.39-1.02 0-1.41l-2.34-2.34c-.39-.39-1.02-.39-1.41 0l-1.83 1.83 3.75 3.75 1.83-1.83z"
            close-icon "M19 6.41L17.59 5 12 10.59 6.41 5 5 6.41 10.59 12 5 17.59 6.41 19 12 13.41 17.59 19 19 17.59 13.41 12z"]
        (if editing?
          ;; NOTE: There's `ext-focused-by-default` which does something similar, but it's aggressive
          ;; and steals focus when modifying breakpoints from the code view
          {:graphic {:fx/type fx/ext-on-instance-lifecycle
                     :on-created (fn [^TextField node]
                                   (ui/run-later (.requestFocus node)))
                     :desc {:fx/type ext-with-focus-changed-handler
                            :props {:on-focused-changed {:event-type :condition-focus-changed}}
                            :desc {:fx/type fx.text-field/lifecycle
                                   :text condition
                                   :on-text-changed {:event-type :condition-text-changed}
                                   :on-action {:event-type :save-condition}
                                   :on-key-pressed {:event-type :condition-key-pressed}}}}}
          {:graphic {:fx/type fx.stack-pane/lifecycle
                     :children [{:fx/type :label
                                 :max-width ##Inf
                                 :text-overrun :ellipsis
                                 :text (or condition "")}
                                {:fx/type fx.h-box/lifecycle
                                 :alignment :center-right
                                 :children (concat
                                            (when (and hovered? condition)
                                              [(icon-button close-icon {:event-type :remove-condition
                                                                        :breakpoint breakpoint})])
                                            (when hovered?
                                              [(icon-button edit-icon {:event-type :edit-condition
                                                                       :source :button
                                                                       :breakpoint breakpoint})]))}]}
           :on-mouse-entered {:event-type :condition-mouse-entered  :breakpoint breakpoint}
           :on-mouse-exited  {:event-type :condition-mouse-exited   :breakpoint breakpoint}
           :on-mouse-clicked {:event-type :edit-condition
                              :source :double-click
                              :breakpoint breakpoint}})))))

(defn- breakpoints-view [{:keys [parent localization-state state]}]
  {:fx/type fxui/ext-with-anchor-pane-props
   :desc {:fx/type fxui/ext-value
          :value parent}
   :props {:stylesheets [(str (io/resource "editor.css"))]
           :children [{:fx/type fx.h-box/lifecycle
                       :id "breakpoints-tool-bar"
                       :anchor-pane/top 0
                       :anchor-pane/left 0
                       :anchor-pane/right 0
                       :spacing 10
                       :children [{:fx/type fx.button/lifecycle
                                   :id "breakpoints-enable-all"
                                   :text (localization-state (localization/message "breakpoints.button.enable-all"))
                                   :on-action {:event-type :enable-all}}
                                  {:fx/type fx.button/lifecycle
                                   :id "breakpoints-disable-all"
                                   :text (localization-state (localization/message "breakpoints.button.disable-all"))
                                   :on-action {:event-type :disable-all}}
                                  {:fx/type fx.button/lifecycle
                                   :id "breakpoints-toggle-all"
                                   :text (localization-state (localization/message "breakpoints.button.toggle-all"))
                                   :on-action {:event-type :toggle-all}}
                                  {:fx/type fx.button/lifecycle
                                   :id "breakpoints-remove-all"
                                   :text (localization-state (localization/message "breakpoints.button.remove-all"))
                                   :on-action {:event-type :remove-all}}]}
                      {:fx/type fx.ext.table-view/with-selection-props
                       :anchor-pane/top 57
                       :anchor-pane/right 0
                       :anchor-pane/bottom 0
                       :anchor-pane/left 0
                       :props {:selection-mode :multiple
                               :on-selected-indices-changed {:event-type :selected-items-changed}}
                       :desc
                       {:fx/type fx.table-view/lifecycle
                        :id "breakpoints-table-view"
                        :fixed-cell-size 35.0
                        :column-resize-policy TableView/CONSTRAINED_RESIZE_POLICY
                        :row-factory {:fx/cell-type fx.table-row/lifecycle
                                      :describe
                                      (fn [idx]
                                        (let [bp (get (:breakpoints state) idx)]
                                          {:on-mouse-entered {:event-type :row-mouse-entered  :breakpoint bp}
                                          :on-mouse-exited  {:event-type :row-mouse-exited   :breakpoint bp}
                                          :on-mouse-clicked {:event-type :breakpoint-clicked
                                                             :clicked-breakpoint bp}}))}
                        :items (range (count (:breakpoints state)))
                        :columns
                        [{:fx/type fx.table-column/lifecycle
                          :text (localization-state (localization/message "breakpoints.column.enabled"))
                          :pref-width 60
                          :min-width 60
                          :max-width 80
                          :cell-value-factory identity
                          :cell-factory {:fx/cell-type fx.table-cell/lifecycle
                                         :describe (fn/partial column-cell-factory state ::column-enabled)}}
                         {:fx/type fx.table-column/lifecycle
                          :text (localization-state (localization/message "breakpoints.column.line"))
                          :pref-width 50
                          :min-width 50
                          :max-width 100
                          :cell-value-factory identity
                          :cell-factory {:fx/cell-type fx.table-cell/lifecycle
                                         :describe (fn/partial column-cell-factory state ::colum-line)}}
                         {:fx/type fx.table-column/lifecycle
                          :text (localization-state (localization/message "breakpoints.column.name"))
                          :pref-width 200
                          :cell-value-factory identity
                          :cell-factory {:fx/cell-type fx.table-cell/lifecycle
                                         :describe (fn/partial column-cell-factory state ::column-name)}}
                         {:fx/type fx.table-column/lifecycle
                          :text (localization-state (localization/message "breakpoints.column.condition"))
                          :pref-width 250
                          :cell-value-factory identity
                          :cell-factory {:fx/cell-type fx.table-cell/lifecycle
                                         :describe (fn/partial column-cell-factory state ::column-condition)}}
                         {:fx/type fx.table-column/lifecycle
                          :text (localization-state (localization/message "breakpoints.column.path"))
                          :style-class ["path-cell"]
                          :pref-width 200
                          :cell-value-factory identity
                          :cell-factory {:fx/cell-type fx.table-cell/lifecycle
                                         :describe (fn/partial column-cell-factory state ::column-path)}}
                         {:fx/type fx.table-column/lifecycle
                          :pref-width 50
                          :reorderable false
                          :resizable false
                          :sortable false
                          :cell-value-factory identity
                          :cell-factory {:fx/cell-type fx.table-cell/lifecycle
                                         :describe (fn/partial column-cell-factory state ::column-actions)}}]}}]}})

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

(defn- update-breakpoints [breakpoints breakpoints-batch f]
  (let [bp-set (set breakpoints-batch)]
    (mapv (fn [bp]
            (if (bp-set bp)
              (f bp)
              bp))
          breakpoints)))

(defn- update-breakpoint [breakpoints breakpoint f]
  (mapv (fn [bp]
          (if (= bp breakpoint)
            (f bp)
            bp))
        breakpoints))

(defn- toggle-breakpoints-enabled [breakpoints breakpoint-batch]
  (update-breakpoints breakpoints breakpoint-batch #(assoc % :enabled (not (:enabled %)))))

(defn- toggle-breakpoint-enabled [breakpoints breakpoint]
  (toggle-breakpoints-enabled breakpoints [breakpoint]))

(defn- update-breakpoints-enabled-state [breakpoints breakpoint-batch enabled]
  (update-breakpoints breakpoints breakpoint-batch #(assoc % :enabled enabled)))

(defn- update-breakpoint-enabled-state [breakpoints breakpoint enabled]
  (update-breakpoints breakpoints [breakpoint] enabled))

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
  (filter #(= (:resource %) (:resource breakpoint)) breakpoints))

(defn- set-breakpoint-condition! [project breakpoints breakpoint condition evaluation-context]
  (let [script-node (breakpoint->script-node project breakpoint evaluation-context)
        breakpoints-in-script (get-breakpoints-in-script breakpoints breakpoint)
        updated-breakpoints (update-breakpoint-condition breakpoints-in-script breakpoint condition)
        regions (update-script-regions-from-breakpoints script-node updated-breakpoints evaluation-context)]
    (g/set-property! script-node :regions regions)))

(defn- set-regions-with-action! [project evaluation-context all-breakpoints breakpoints action-fn]
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
    (swap! state assoc :edited-breakpoint nil :breakpoints updated-breakpoints)))

(defn- handle-breakpoint-event! [project open-resource-fn event]
  (case (:event-type event)
    :toggle-enabled
    (g/with-auto-evaluation-context evaluation-context
      (let [breakpoint (:breakpoint event)
            breakpoints-in-script (get-breakpoints-in-script (:breakpoints @state) breakpoint)
            script-node (breakpoint->script-node project breakpoint evaluation-context)
            toggled-breakpoint (toggle-breakpoint-enabled breakpoints-in-script breakpoint)
            regions (update-script-regions-from-breakpoints script-node toggled-breakpoint evaluation-context)]
        (swap! state assoc :edited-breakpoint nil)
        (g/set-property! script-node :regions regions)))

    :remove-breakpoint
    (g/with-auto-evaluation-context evaluation-context
      (let [breakpoint (:breakpoint event)
            breakpoints-in-script (get-breakpoints-in-script (:breakpoints @state) breakpoint)
            script-node (breakpoint->script-node project breakpoint evaluation-context)
            remaining (filterv #(not (= breakpoint %)) breakpoints-in-script)
            regions (update-script-regions-from-breakpoints script-node remaining evaluation-context)]
        (swap! state #(assoc % :breakpoints (vec (remove #{breakpoint} (:breakpoints %)))))
        (g/set-property! script-node :regions regions)))

    :selected-items-changed
    (swap! state assoc :selected-indices (:fx/event event))

    :condition-focus-changed
    (when (not (get-in event [:fx/event :value]))
      (swap! state assoc :edited-breakpoint nil))

    :breakpoint-clicked
    (let [^MouseEvent e (:fx/event event)
          {:keys [resource row]} (:clicked-breakpoint event)]
      (when (and (= MouseButton/PRIMARY (.getButton e))
                 (= 2 (.getClickCount e)))
        (open-resource-fn resource (inc row))))

    :row-mouse-entered (swap! state assoc :hovered-row (:breakpoint event))
    :row-mouse-exited  (swap! state assoc :hovered-row nil)
    :condition-mouse-entered (swap! state assoc :hovered-condition (:breakpoint event))
    :condition-mouse-exited  (swap! state assoc :hovered-condition nil)

    :edit-condition
    (let [^MouseEvent me (:fx/event event)]
      (when (or (= (:source event) :button)
                (= 2 (.getClickCount me)))
        ;; NOTE: If we don't consume this it'll fall through to the :breakpoint-clicked condition
        (.consume me)
        (swap! state assoc :edited-breakpoint (:breakpoint event))))

    :remove-condition
    (g/with-auto-evaluation-context evaluation-context
      (let [breakpoint (:breakpoint event)]
        (set-breakpoint-condition! project (:breakpoints @state) breakpoint nil evaluation-context)
        (swap! state assoc :edited-breakpoint nil)
        (reset! condition-text nil)))

    :condition-text-changed (reset! condition-text (:fx/event event))

    :condition-key-pressed
    (when (= (.getCode ^KeyEvent (:fx/event event)) KeyCode/ESCAPE)
      (reset! condition-text nil)
      (swap! state assoc :edited-breakpoint nil))

    :save-condition
    (g/with-auto-evaluation-context evaluation-context
      (let [condition (or @condition-text (get-in @state [:edited-breakpoint :condition]))]
        (set-breakpoint-condition! project (:breakpoints @state) (:edited-breakpoint @state) condition evaluation-context)
        (swap! state assoc :edited-breakpoint nil)
        (reset! condition-text nil)))

    ;; default case
    ;; The rest of the actions share a similar enough `action-scope` pattern that by deconstructing this way
    ;; we can shorten this code quite a bit
    (g/with-auto-evaluation-context evaluation-context
      (let [[action scope] (string/split (name (:event-type event)) #"-")
            all-breakpoints (:breakpoints @state)
            breakpoints (if (= scope "selected")
                          (mapv #(get (:breakpoints @state) %) (:selected-indices @state))
                          all-breakpoints)
            handle-fn (partial set-regions-with-action! project evaluation-context all-breakpoints breakpoints)]
        (case action
          "remove" (do (handle-fn #(vec (remove (set %2) %1)))
                       (swap! state assoc :selected-indices []))
          "toggle" (handle-fn toggle-breakpoints-enabled)
          "enable" (handle-fn #(update-breakpoints-enabled-state %1 %2 true))
          "disable" (handle-fn #(update-breakpoints-enabled-state %1 %2 false)))))))

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

(defn create-breakpoint-view-renderer [project localization prefs breakpoints-container open-resource-fn]
  (restore-breakpoints! project prefs)
  (let [open-res-fn (make-open-resource-fn open-resource-fn)]
    (fx/mount-renderer
      state
      (fx/create-renderer
       :error-handler #'error-reporting/report-exception!
       :middleware (comp
                     fxui/wrap-dedupe-desc
                     (fx/wrap-map-desc
                       (fn [state]
                         {:fx/type fx/ext-watcher
                          :ref localization
                          :key :localization-state
                          :desc {:fx/type breakpoints-view
                                 :parent breakpoints-container
                                 :state state}})))
       :opts {:fx.opt/map-event-handler #(handle-breakpoint-event! project open-res-fn %)})))
  (let [tab-pane (ui/parent-tab-pane (.lookup (ui/main-root) "#breakpoints-container"))
        table (.lookup (ui/main-root) "#breakpoints-table-view")]
    (ui/context! table :breakpoints-view
                 {:project project :table table}
                 (ui/->selection-provider table)
                 {}
                 {resource/Resource (fn [idx] (-> @state :breakpoints (get idx) :resource))})
    (ui/register-context-menu table ::breakpoint-menu))
  (let [timer (ui/->timer
               5
               "breakpoints-view-update-timer"
               (fn [_ _ _]
                 (when-not (ui/ui-disabled?)
                   (let [breakpoints (g/node-value project :breakpoints)]
                     (save-breakpoints! prefs breakpoints)
                     (swap! state assoc :breakpoints breakpoints)))))]
    (ui/timer-start! timer)))

(handler/defhandler :breakpoints.toggle-selected-enabled :breakpoints-view
  (run [project]
    (g/with-auto-evaluation-context evaluation-context
      (let [breakpoints (:breakpoints @state)
            selected (map #(get breakpoints %) (:selected-indices @state))]
        (set-regions-with-action! project
                                  evaluation-context
                                  breakpoints
                                  selected
                                  toggle-breakpoints-enabled)))))

(handler/defhandler :breakpoints.remove-selected :breakpoints-view
  (run [project]
    (g/with-auto-evaluation-context evaluation-context
      (let [breakpoints (:breakpoints @state)
            selected (map #(get breakpoints %) (:selected-indices @state))]
        (set-regions-with-action! project
                                  evaluation-context
                                  breakpoints
                                  selected
                                  #(vec (remove (set %2) %1)))))))

(handler/defhandler :breakpoints.edit-selected :breakpoints-view
  (run []
    (g/with-auto-evaluation-context evaluation-context
      (let [breakpoints (:breakpoints @state)
            selected (last (:selected-indices @state))]
        (swap! state assoc :edited-breakpoint (get breakpoints selected))))))

(comment
  (let [open-resource (partial #'editor.app-view/open-resource
                               (dev/app-view) (dev/prefs) (dev/localization) (dev/workspace) (dev/project))
        breakpoints-container (.lookup (ui/main-root) "#breakpoints-container")]
    (create-breakpoint-view-renderer (dev/project) (dev/localization) (dev/prefs) breakpoints-container open-resource))

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
    (let [bps-prefs [{:proj-path "/scripts/knight.script", :row 21, :enabled true}
                     {:proj-path "/scripts/game.script", :row 20, :enabled true}]
          breakpoints (mapv #(assoc % :resource (workspace/find-resource (dev/workspace) (:proj-path %) ec)) bps-prefs)
          script-bps (collect-script-nodes-from-breakpoints (dev/project) breakpoints ec)]
      (g/transact
        (for [{:keys [script-node breakpoints]} script-bps
              :let [updated-regions (update-script-regions-from-breakpoints script-node breakpoints ec)]]
          (g/set-property script-node :regions updated-regions)))))

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
  (prefs/set! (dev/prefs) [:window :keymap] {:breakpoints.remove-selected {:add #{"Delete"}, :remove #{}},
                                             :breakpoints.toggle-selected-enabled
                                             {:add #{"Shift+F9"}, :remove #{}},
                                             :debugger.toggle-breakpoint-enabled {:add #{"Shift+F9"}, :remove #{}},
                                             :scene.select-scale-tool {:add #{"D"}, :remove #{}},
                                             :scene.set-camera-type {:add #{}, :remove #{}}})
  (reset! state {:breakpoints [] :selected-indices [] :hovered-condition nil :hovered-row nil
                 :condition-text nil :edited-breakpoint nil :edited-condition nil})
  ,)
