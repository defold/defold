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
            [cljfx.fx.context-menu :as fx.context-menu]
            [cljfx.fx.h-box :as fx.h-box]
            [cljfx.fx.menu-item :as fx.menu-item]
            [cljfx.fx.separator-menu-item :as fx.separator-menu-item]
            [cljfx.fx.stack-pane :as fx.stack-pane]
            [cljfx.fx.svg-path :as fx.svg-path]
            [cljfx.fx.table-cell :as fx.table-cell]
            [cljfx.fx.table-column :as fx.table-column]
            [cljfx.fx.table-row :as fx.table-row]
            [cljfx.fx.table-view :as fx.table-view]
            [cljfx.fx.text-field :as fx.text-field]
            [clojure.java.io :as io]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.code.data :as code-data]
            [editor.defold-project :as project]
            [editor.error-reporting :as error-reporting]
            [editor.fxui :as fxui]
            [editor.handler :as handler]
            [editor.prefs :as prefs]
            [editor.resource :as resource]
            [editor.ui :as ui]
            [editor.workspace :as workspace]
            [util.fn :as fn])
  (:import [javafx.scene.control TextField]
           [javafx.scene.input KeyCode KeyEvent MouseButton MouseEvent]))

(set! *warn-on-reflection* true)

(defonce state (atom {:breakpoints []
                      :selected-indices []
                      :hovered-breakpoint nil
                      :edited-breakpoint nil
                      :edited-condition nil}))

(defn- icon-button [icon-path on-action-event]
  {:fx/type fx.button/lifecycle
   :style "-fx-padding: 2; -fx-min-width: 0; -fx-pref-width: 30;"
   :graphic {:fx/type fx.stack-pane/lifecycle
             :min-width 12
             :min-height 12
             :max-width 12
             :max-height 12
             :children [{:fx/type fx.svg-path/lifecycle
                         :content icon-path
                         :scale-x 0.5
                         :scale-y 0.5
                         :style "-fx-fill: -fx-text-base-color;"}]}
   :on-action on-action-event})

(defn- condition-table-cell [state breakpoint]
  (let [condition (:condition breakpoint)
        editing? (= (:edited-breakpoint state) breakpoint)
        hovered? (= (:hovered-breakpoint state) breakpoint)
        edit-icon "M3 17.25V21h3.75L17.81 9.94l-3.75-3.75L3 17.25zM20.71 7.04c.39-.39.39-1.02 0-1.41l-2.34-2.34c-.39-.39-1.02-.39-1.41 0l-1.83 1.83 3.75 3.75 1.83-1.83z"
        close-icon "M19 6.41L17.59 5 12 10.59 6.41 5 5 6.41 10.59 12 5 17.59 6.41 19 12 13.41 17.59 19 19 17.59 13.41 12z"]
    (if editing?
      {:graphic {:fx/type fx/ext-on-instance-lifecycle
                 :on-created (fn [^TextField node]
                               (ui/run-later (.requestFocus node)))
                 :desc {:fx/type fx.text-field/lifecycle
                        :text condition
                        :on-text-changed {:event-type :condition-text-changed}
                        :on-action {:event-type :save-condition}
                        :on-key-pressed {:event-type :key-pressed}}}}
      {:graphic {:fx/type fx.stack-pane/lifecycle
                 :children [{:fx/type :label
                             :max-width ##Inf
                             :text-overrun :ellipsis
                             :text (or condition "")}
                            {:fx/type fx.h-box/lifecycle
                             :max-width ##Inf
                             :alignment :center-right
                             :children (concat
                                         (when hovered?
                                           [(icon-button edit-icon {:event-type :edit-condition
                                                                    :source :button
                                                                    :breakpoint breakpoint})])
                                         (when (and hovered? condition)
                                           [(icon-button close-icon {:event-type :remove-condition
                                                                     :breakpoint breakpoint})]))}]}
       :on-mouse-entered {:event-type :row-mouse-entered  :breakpoint breakpoint}
       :on-mouse-exited  {:event-type :row-mouse-exited   :breakpoint breakpoint}
       :on-mouse-clicked {:event-type :edit-condition
                          :source :double-click
                          :breakpoint breakpoint}})))

(defn- breakpoints-view [parent state]
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
                                   :text "Enable All"
                                   :on-action {:event-type :enable-all}}
                                  {:fx/type fx.button/lifecycle
                                   :id "breakpoints-disable-all"
                                   :text "Disable All"
                                   :on-action {:event-type :disable-all}}
                                  {:fx/type fx.button/lifecycle
                                   :id "breakpoints-toggle-all"
                                   :text "Toggle All"
                                   :on-action {:event-type :toggle-all}}
                                  {:fx/type fx.button/lifecycle
                                   :id "breakpoints-remove-all"
                                   :text "Remove All"
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
                        :context-menu {:fx/type fx.context-menu/lifecycle
                                       :consume-auto-hiding-events false
                                       :items [{:fx/type fx.menu-item/lifecycle
                                                :text "Enable Selected"
                                                :on-action {:event-type :enable-selected}}
                                               {:fx/type fx.menu-item/lifecycle
                                                :text "Disable Selected"
                                                :on-action {:event-type :disable-selected}}
                                               {:fx/type fx.menu-item/lifecycle
                                                :text "Toggle Selected"
                                                :on-action {:event-type :toggle-selected}}
                                               {:fx/type fx.separator-menu-item/lifecycle}
                                               {:fx/type fx.menu-item/lifecycle
                                                :text "Remove Selected"
                                                :on-action {:event-type :remove-selected}}]}
                        :row-factory {:fx/cell-type fx.table-row/lifecycle
                                      :describe (fn [bp]
                                                  {:on-mouse-clicked {:event-type :breakpoint-clicked
                                                                      :clicked-breakpoint bp}})}
                        :items (:breakpoints state)
                        :columns [{:fx/type fx.table-column/lifecycle
                                   :text "Enabled"
                                   :pref-width 60
                                   :cell-value-factory identity
                                   :cell-factory {:fx/cell-type fx.table-cell/lifecycle
                                                  :describe (fn [breakpoint]
                                                              ;; NOTE: When we delete a breakpoint, we receive a nil
                                                              (if (nil? breakpoint)
                                                                {:text ""}
                                                                {:style-class ["enabled-cell"]
                                                                 :alignment :center
                                                                 :graphic {:fx/type fx.check-box/lifecycle
                                                                           :selected (:active breakpoint)
                                                                           :on-selected-changed {:event-type :toggle-active
                                                                                                 :breakpoint breakpoint}}}))}}
                                  {:fx/type fx.table-column/lifecycle
                                   :text "File"
                                   :pref-width 250
                                   :cell-value-factory (fn [breakpoint]
                                                         (-> breakpoint :resource :project-path))}
                                  {:fx/type fx.table-column/lifecycle
                                   :text "Line"
                                   :pref-width 50
                                   :cell-value-factory identity
                                   :cell-factory {:fx/cell-type fx.table-cell/lifecycle
                                                        :describe (fn [breakpoint]
                                                                    ;; NOTE: When we delete a breakpoint, we receive a nil
                                                                    (if (nil? breakpoint)
                                                                      {:text ""}
                                                                      {:text (str (inc (:row breakpoint)))}))}}
                                  {:fx/type fx.table-column/lifecycle
                                   :text "Condition"
                                   :pref-width 200
                                   :cell-value-factory identity
                                   :cell-factory {:fx/cell-type fx.table-cell/lifecycle
                                                  :describe (fn/partial condition-table-cell state)}}]}}]}})

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

(defn- toggle-breakpoints-active [breakpoints breakpoint-batch]
  (update-breakpoints breakpoints breakpoint-batch #(assoc % :active (not (:active %)))))

(defn- toggle-breakpoint-active [breakpoints breakpoint]
  (toggle-breakpoints-active breakpoints [breakpoint]))

(defn- update-breakpoints-active-state [breakpoints breakpoint-batch active]
  (update-breakpoints breakpoints breakpoint-batch #(assoc % :active active)))

(defn- update-breakpoint-active-state [breakpoints breakpoint active]
  (update-breakpoints breakpoints [breakpoint] active))

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
    (merge region (select-keys breakpoint [:condition :active]))))

(defn- update-script-regions-from-breakpoints [script-node breakpoints evaluation-context]
  (let [lines (g/node-value script-node :lines evaluation-context)
        regions (g/node-value script-node :regions evaluation-context)
        non-bp-regions (remove code-data/breakpoint-region? regions)
        bp-regions (map (partial breakpoint->region lines) breakpoints)]
    (vec (sort (concat non-bp-regions bp-regions)))))

(defn- handle-breakpoint-action [project evaluation-context all-breakpoints breakpoints action-fn]
  ;; Any of these actions should disable editing
  (swap! state assoc :edited-breakpoint nil)
  (let [affected-scripts (collect-script-nodes-from-breakpoints project breakpoints evaluation-context)
        all-by-resource (group-by :resource all-breakpoints)
        breakpoints-by-script (map (fn [{:keys [script-node resource breakpoints]}]
                                     {:script script-node
                                      :affected-bps breakpoints
                                      :all-bps (get all-by-resource resource [])})
                                   affected-scripts)
        txs (mapcat (fn [{:keys [script affected-bps all-bps]}]
                      (let [updated-bps (action-fn all-bps affected-bps)
                            new-regions (update-script-regions-from-breakpoints script updated-bps evaluation-context)]
                        (g/set-property script :regions new-regions)))
                    breakpoints-by-script)]
    (g/transact txs)))

(defonce condition-text (atom nil))

(defn- handle-breakpoint-event! [project open-resource-fn event]
  ;; TODO: No all events need this, probably better to split between those that do and don't
  (g/with-auto-evaluation-context evaluation-context
    (case (:event-type event)
      :toggle-active
      (let [breakpoint (:breakpoint event)
            breakpoints-in-script (filter #(= (:resource %) (:resource breakpoint)) (:breakpoints @state))
            script-node (breakpoint->script-node project breakpoint evaluation-context)
            toggled-breakpoint (toggle-breakpoint-active breakpoints-in-script breakpoint)
            regions (update-script-regions-from-breakpoints script-node toggled-breakpoint evaluation-context)]
        (swap! state assoc :edited-breakpoint nil)
        (g/set-property! script-node :regions regions))

      :remove
      (let [breakpoint (:breakpoint event)
            breakpoints-in-script (filter #(= (:resource %) (:resource breakpoint)) (:breakpoints @state))
            script-node (breakpoint->script-node project breakpoint evaluation-context)
            remaining (filterv #(not (= breakpoint %)) breakpoints-in-script)
            regions (update-script-regions-from-breakpoints script-node remaining evaluation-context)]
        (g/set-property! script-node :regions regions))

      :selected-items-changed
      (swap! state assoc :selected-indices (:fx/event event))

      :breakpoint-clicked
      (let [^MouseEvent e (:fx/event event)
            {:keys [resource row]} (:clicked-breakpoint event)]
        (swap! state assoc :edited-breakpoint nil)
        ;; TODO: Do we need to check primary?
        (when (and (= MouseButton/PRIMARY (.getButton e))
                 (= 2 (.getClickCount e)))
          (open-resource-fn resource (inc row))))

      :row-mouse-entered
      (swap! state assoc :hovered-breakpoint (:breakpoint event))

      :row-mouse-exited
      (swap! state assoc :hovered-breakpoint nil)

      :edit-condition
      (let [^MouseEvent me (:fx/event event)]
        (when (or (= (:source event) :button)
                  (= 2 (.getClickCount me)))
          (.consume me)
          (swap! state assoc :edited-breakpoint (:breakpoint event))))

      :remove-condition
      ;; TODO: Maybe generalize this since it's the same below for save-condition
      (let [breakpoint (:breakpoint event)
            breakpoints-in-script (filter #(= (:resource %) (:resource breakpoint)) (:breakpoints @state))
            updated-breakpoints (update-breakpoint-condition breakpoints-in-script breakpoint nil)
            script-node (breakpoint->script-node project breakpoint evaluation-context)
            regions (update-script-regions-from-breakpoints script-node updated-breakpoints evaluation-context)]
        (swap! state assoc :edited-breakpoint nil)
        (reset! condition-text nil)
        (g/set-property! script-node :regions regions))

      :condition-text-changed (reset! condition-text (:fx/event event))

      :key-pressed
      (when (= (.getCode ^KeyEvent (:fx/event event)) KeyCode/ESCAPE)
        (reset! condition-text nil)
        (swap! state assoc :edited-breakpoint nil))

      ;; TODO: When I hit enter on an empty string it doesn't just save with empty
      :save-condition
      (when (not (string/blank? @condition-text))
        (let [breakpoint (:edited-breakpoint @state)
              breakpoints-in-script (filter #(= (:resource %) (:resource breakpoint)) (:breakpoints @state))
              updated-breakpoints (update-breakpoint-condition breakpoints-in-script breakpoint @condition-text)
              script-node (breakpoint->script-node project breakpoint evaluation-context)
              regions (update-script-regions-from-breakpoints script-node updated-breakpoints evaluation-context)]
          (swap! state assoc :edited-breakpoint nil)
          (reset! condition-text nil)
          (g/set-property! script-node :regions regions)))

      ;; default case
      ;; The rest of the actions share a similar enough `action-scope` pattern that by deconstructing this way
      ;; we can shorten this code quite a bit
      (let [[action scope] (string/split (name (:event-type event)) #"-")
            all-breakpoints (:breakpoints @state)
            breakpoints (if (= scope "selected")
                               (mapv #(get (:breakpoints @state) %) (:selected-indices @state))
                               all-breakpoints)
            handle-fn (partial handle-breakpoint-action project evaluation-context all-breakpoints breakpoints)]
        (case action
          "remove" (handle-fn #(vec (remove (set %2) %1)))
          "toggle" (handle-fn toggle-breakpoints-active)
          "enable" (handle-fn #(update-breakpoints-active-state %1 %2 true))
          "disable" (handle-fn #(update-breakpoints-active-state %1 %2 false)))))))

(defn- make-open-resource-fn
  [open-resource-fn]
  (fn [resource line]
    (when resource
      (open-resource-fn resource {:cursor-range (code-data/line-number->CursorRange line)}))
    nil))

(defn- save-breakpoints [prefs breakpoints]
  (let [bps-prefs (mapv #(dissoc (assoc % :proj-path (resource/proj-path (:resource %))) :resource) breakpoints)]
    (prefs/set! prefs [:code :breakpoints] bps-prefs)))

(defn- restore-breakpoints [project prefs]
  (g/with-auto-evaluation-context evaluation-context
    (let [bps-prefs (prefs/get prefs [:code :breakpoints])
          workspace (project/workspace project)
          breakpoints (mapv #(assoc % :resource (workspace/find-resource workspace (:proj-path %) evaluation-context))
                            bps-prefs)
          script-bps (collect-script-nodes-from-breakpoints project breakpoints evaluation-context)]
      (g/transact
        (for [{:keys [script-node breakpoints]} script-bps
              :let [updated-regions (update-script-regions-from-breakpoints script-node breakpoints evaluation-context)]]
          (g/set-property script-node :regions updated-regions))))))

(defn create-breakpoint-view-renderer [project prefs breakpoint-container open-resource-fn]
  (let [tab-pane (ui/parent-tab-pane (.lookup (ui/main-root) "#breakpoints-container"))]
    (ui/context! tab-pane :breakpoints-view {:project project} nil))
  (restore-breakpoints project prefs)
  (let [open-res-fn (make-open-resource-fn open-resource-fn)]
    (fx/mount-renderer
      state
      (fx/create-renderer
       :error-handler #'error-reporting/report-exception!
       :middleware (comp
                     fxui/wrap-dedupe-desc
                     (fx/wrap-map-desc #(breakpoints-view breakpoint-container %)))
       :opts {:fx.opt/map-event-handler #(handle-breakpoint-event! project open-res-fn %)})))
  (let [timer (ui/->timer
               4
               "breakpoints-view-update-timer"
               (fn [_ _ _]
                 (when-not (ui/ui-disabled?)
                   (let [breakpoints (g/node-value project :breakpoints)]
                     (save-breakpoints prefs breakpoints)
                     (swap! state assoc :breakpoints breakpoints)))))]
    (ui/timer-start! timer)))

(handler/defhandler :breakpoints-view.toggle-breakpoint-active :breakpoints-view
  (run [project]
    (g/with-auto-evaluation-context evaluation-context
      (let [breakpoints (:breakpoints @state)
            selected (map #(get breakpoints %) (:selected-indices @state))]
        (handle-breakpoint-action project
                                  evaluation-context
                                  breakpoints
                                  selected
                                  toggle-breakpoints-active)))))

(handler/defhandler :breakpoints-view.remove-breakpoint :breakpoints-view
  (run [project]
    (g/with-auto-evaluation-context evaluation-context
      (let [breakpoints (:breakpoints @state)
            selected (map #(get breakpoints %) (:selected-indices @state))]
        (handle-breakpoint-action project
                                  evaluation-context
                                  breakpoints
                                  selected
                                  #(vec (remove (set %2) %1)))))))

(handler/defhandler :breakpoints-view.edit-breakpoint :breakpoints-view
  (run []
       ()))

(comment
  (let [open-resource (partial #'editor.app-view/open-resource
                               (dev/app-view) (dev/prefs) (dev/localization) (dev/workspace) (dev/project))
        breakpoints-container (.lookup (ui/main-root) "#breakpoints-container")]
    (create-breakpoint-view-renderer (dev/project) (dev/prefs) breakpoints-container open-resource))

  (g/with-auto-evaluation-context ec
    (let [bps-prefs [{:proj-path "/scripts/knight.script", :row 21, :active true}
                     {:proj-path "/scripts/game.script", :row 20, :active true}]
          breakpoints (mapv #(assoc % :resource (workspace/find-resource (dev/workspace) (:proj-path %) ec)) bps-prefs)
          script-bps (collect-script-nodes-from-breakpoints (dev/project) breakpoints ec)]
      (g/transact
        (for [{:keys [script-node breakpoints]} script-bps
              :let [updated-regions (update-script-regions-from-breakpoints script-node breakpoints ec)]]
          (g/set-property script-node :regions updated-regions)))))

  (defn save-breakpoints [_ _] nil)
  (prefs/get (dev/prefs) [:code :breakpoints])
  (save-breakpoints (dev/prefs) (g/node-value (dev/project) :breakpoints))
  (restore-breakpoints (dev/project) (dev/prefs))

  (ui/run-command (.lookup (ui/main-root) "#breakpoints-table-view") :breakpoints-view.edit-breakpoint)

  ,)
