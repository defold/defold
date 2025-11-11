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

(ns editor.breakpoints-tab
  (:require [cljfx.api :as fx]
            [cljfx.ext.list-view :as fx.ext.list-view]
            [cljfx.ext.table-view :as fx.ext.table-view]
            [cljfx.fx.button :as fx.button]
            [cljfx.fx.check-box :as fx.check-box]
            [cljfx.fx.context-menu :as fx.context-menu]
            [cljfx.fx.h-box :as fx.h-box]
            [cljfx.fx.list-cell :as fx.list-cell]
            [cljfx.fx.list-view :as fx.list-view]
            [cljfx.fx.menu-item :as fx.menu-item]
            [cljfx.fx.separator-menu-item :as fx.separator-menu-item]
            [cljfx.fx.svg-path :as fx.svg-path]
            [cljfx.fx.table-cell :as fx.table-cell]
            [cljfx.fx.table-column :as fx.table-column]
            [cljfx.fx.table-view :as fx.table-view]
            [cljfx.fx.text-field :as fx.text-field]
            [clojure.java.io :as io]
            [clojure.set :as set]
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
  (:import [javafx.scene.control ListCell ListView]
           [javafx.scene.input MouseButton MouseEvent]))

(set! *warn-on-reflection* true)

(defonce state (atom {:breakpoints []
                      :selected-indices []
                      :hovered nil
                      :edited-breakpoint nil
                      :edited-condition nil}))

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

(defn- update-script-regions-from-breakpoints [script-node breakpoints evaluation-context]
  (let [lines (g/node-value script-node :lines evaluation-context)
        regions (g/node-value script-node :regions evaluation-context)
        breakpoint-map (into {} (map (juxt :row identity)) breakpoints)
        breakpoint-rows (set (keys breakpoint-map))
        non-bp-regions (remove code-data/breakpoint-region? regions)
        existing-bp-regions (->> regions
                                 (filter #(and (code-data/breakpoint-region? %)
                                               (contains? breakpoint-rows (code-data/breakpoint-row %))))
                                 (map (fn [region]
                                        (let [row (code-data/breakpoint-row region)
                                              breakpoint (breakpoint-map row)
                                              updated (merge region (select-keys breakpoint [:active :condition]))]
                                          (if (nil? (:condition breakpoint))
                                            (dissoc updated :condition)
                                            updated)))))
        new-bp-regions (->> regions
                            (into #{} (comp (filter code-data/breakpoint-region?)
                                            (map code-data/breakpoint-row)))
                            (set/difference breakpoint-rows)
                            (map (partial code-data/make-breakpoint-region lines)))
        updated-regions (concat non-bp-regions existing-bp-regions new-bp-regions)]
    (vec (sort updated-regions))))

(defn- breakpoints-view [parent state]
  {:fx/type ext-with-anchor-pane-props
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
                        :on-mouse-pressed {:event-type :list-view-clicked}
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
                        :items (:breakpoints state)
                        :columns [{:fx/type fx.table-column/lifecycle
                                   :text "Enabled"
                                   :pref-width 80
                                   :cell-value-factory identity
                                   :cell-factory {:fx/cell-type fx.table-cell/lifecycle
                                                  :describe (fn [breakpoint]
                                                              {:graphic {:fx/type fx.check-box/lifecycle
                                                                         :selected (:active breakpoint)
                                                                         :on-selected-changed {:event-type :toggle-active
                                                                                               :breakpoint breakpoint}}})}}
                                  {:fx/type fx.table-column/lifecycle
                                   :text "Location"
                                   :pref-width 250
                                   :cell-value-factory (fn [breakpoint]
                                                         (str (-> breakpoint :resource :project-path)
                                                              ":" (inc (:row breakpoint))))}
                                  {:fx/type fx.table-column/lifecycle
                                   :text "Condition"
                                   :pref-width 200
                                   :cell-value-factory :condition
                                   :editable true
                                   :on-edit-commit (fn [e]
                                                     {:event-type :save-condition
                                                      :breakpoint (.getRowValue e)
                                                      :new-value (.getNewValue e)})}]}}]}})

(defn- icon-button [icon-path on-action-event]
  {:fx/type fx.button/lifecycle
   :style "-fx-padding: 2; -fx-min-width: 0; -fx-pref-width: 30;"
   :graphic {:fx/type fx.svg-path/lifecycle
             :content icon-path
             :scale-x 0.5
             :scale-y 0.5
             :style "-fx-fill: -fx-text-base-color;"}
   :on-action on-action-event})

(defn- breakpoint-cell-view [state breakpoint-idx]
  (when-let [breakpoint (get (:breakpoints state) breakpoint-idx)]
    (let [{:keys [resource row condition active]} breakpoint
          proj-path (:project-path resource)
          label-text (str proj-path ":" (+ 1 row) (when condition (str " [" condition "]")))
          hovered? (= (:hovered state) breakpoint-idx)
          editing? (= (:edited-breakpoint state) breakpoint-idx)
          ;; SVG paths
          edit-icon "M3 17.25V21h3.75L17.81 9.94l-3.75-3.75L3 17.25zM20.71 7.04c.39-.39.39-1.02 0-1.41l-2.34-2.34c-.39-.39-1.02-.39-1.41 0l-1.83 1.83 3.75 3.75 1.83-1.83z"
          close-icon "M19 6.41L17.59 5 12 10.59 6.41 5 5 6.41 10.59 12 5 17.59 6.41 19 12 13.41 17.59 19 19 17.59 13.41 12z"
          save-icon "M17 3H5c-1.11 0-2 .9-2 2v14c0 1.1.89 2 2 2h14c1.1 0 2-.9 2-2V7l-4-4zm-5 16c-1.66 0-3-1.34-3-3s1.34-3 3-3 3 1.34 3 3-1.34 3-3 3zm3-10H5V5h10v4z"]
      {:graphic
       {:fx/type fx.h-box/lifecycle
        :alignment :center-left
        :spacing 8
        :max-width ##Inf
        :on-mouse-clicked {:event-type :breakpoint-clicked       :clicked-breakpoint breakpoint}
        :on-mouse-entered {:event-type :breakpoint-mouse-entered :breakpoint-idx breakpoint-idx}
        :on-mouse-exited  {:event-type :breakpoint-mouse-exited  :breakpoint-idx breakpoint-idx}
        :children (concat [{:fx/type fx.check-box/lifecycle
                            :h-box/hgrow :always
                            :text label-text
                            :selected active
                            :on-selected-changed {:event-type :toggle-active
                                                  :breakpoint breakpoint}}]
                          [{:fx/type :region
                            :h-box/hgrow :always}]
                          (when (or editing? hovered?)
                            (if editing?
                              [{:fx/type fx.text-field/lifecycle
                                :text (or condition "")
                                :prompt-text "condition"
                                :pref-width 150
                                :focus-traversable true
                                :on-text-changed {:event-type :condition-text-changed}
                                :on-action {:event-type :save-condition
                                            :breakpoint breakpoint
                                            :breakpoint-idx breakpoint-idx}}
                               (icon-button save-icon {:event-type :save-condition
                                                       :breakpoint breakpoint})]
                              (concat
                               (when condition
                                 [{:fx/type :label
                                   :text condition
                                   :style "-fx-text-fill: -fx-text-base-color; -fx-opacity: 0.6;"}])
                               [(icon-button edit-icon {:event-type :edit-condition
                                                        :breakpoint-idx breakpoint-idx})
                                (icon-button close-icon {:event-type :remove
                                                         :breakpoint breakpoint})]))))}})))

(defn- breakpoints-view [parent state]
  {:fx/type ext-with-anchor-pane-props
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
                      {:fx/type fx.ext.list-view/with-selection-props
                       :anchor-pane/top 57
                       :anchor-pane/right 0
                       :anchor-pane/bottom 0
                       :anchor-pane/left 0
                       :props {:selection-mode :multiple
                               ;; TODO: Need to reconsider how we handle selections when we delete with
                               ;; the dedicated breakpoints remove button
                               ;; :selected-indices (:selected-indices state)
                               :on-selected-indices-changed {:event-type :selected-items-changed}}
                       :desc
                       {:fx/type fx.list-view/lifecycle
                        :id "breakpoints-list-view"
                        :on-mouse-pressed {:event-type :list-view-clicked}
                        :fixed-cell-size 60.0
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
                        :items (range (count (:breakpoints state)))
                        :cell-factory {:fx/cell-type fx.list-cell/lifecycle
                                       :describe (fn/partial breakpoint-cell-view state)}}}]}})

(defn- handle-breakpoint-action [project evaluation-context all-breakpoints breakpoints action-fn]
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

(defn- handle-breakpoint-event! [root project open-resource-fn event]
  ;; TODO: No all events need this, probably better to split between those that do and don't
  (g/with-auto-evaluation-context evaluation-context
    (case (:event-type event)
      :toggle-active
      (let [breakpoint (:breakpoint event)
            breakpoints-in-script (filter #(= (:resource %) (:resource breakpoint)) (:breakpoints @state))
            script-node (breakpoint->script-node project breakpoint evaluation-context)
            toggled-breakpoint (toggle-breakpoint-active breakpoints-in-script breakpoint)
            regions (update-script-regions-from-breakpoints script-node toggled-breakpoint evaluation-context)]
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

      ;; We clicked empty space, not an item
      :list-view-clicked
      (let [^MouseEvent e (:fx/event event)
            ^ListView target (.getTarget e)]
        (when (and (instance? ListCell target)
                   (.isEmpty ^ListCell target))
          (.clearSelection (.getSelectionModel (.getSource e)))))

      :breakpoint-clicked
      (let [^MouseEvent e (:fx/event event)
            {:keys [resource row]} (:clicked-breakpoint event)]
        (if (and (= MouseButton/PRIMARY (.getButton e))
                 (= 2 (.getClickCount e)))
          (open-resource-fn resource (+ 1 row))))

      :breakpoint-mouse-entered (swap! state assoc :hovered (:breakpoint-idx event))
      :breakpoint-mouse-exited  (swap! state assoc :hovered nil)

      :edit-condition (swap! state assoc :edited-breakpoint (:breakpoint-idx event))
      :condition-text-changed (swap! state assoc :edited-condition (:fx/event event))

      :save-condition
      (let [condition-text (:edited-condition @state)
            breakpoint (get (:breakpoints @state) (:edited-breakpoint @state))
            breakpoints-in-script (filter #(= (:resource %) (:resource breakpoint)) (:breakpoints @state))
            updated-breakpoints (update-breakpoint-condition breakpoints-in-script breakpoint condition-text)
            script-node (breakpoint->script-node project breakpoint evaluation-context)
            regions (update-script-regions-from-breakpoints script-node updated-breakpoints evaluation-context)]
        (swap! state assoc :edited-breakpoint nil)
        (g/set-property! script-node :regions regions))

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
  [project open-resource-fn]
  (fn [resource line]
    (when resource
      (open-resource-fn resource {:cursor-range (code-data/line-number->CursorRange line)}))
    nil))

(defn save-breakpoints [prefs breakpoints]
  (let [bps-prefs (mapv #(dissoc (assoc % :proj-path (resource/proj-path (:resource %))) :resource) breakpoints)]
    (prefs/set! prefs [:code :breakpoints] bps-prefs)))

(defn create-breakpoint-tab-renderer [root workspace project prefs breakpoint-container open-resource-fn]
  (let [tab-pane (ui/parent-tab-pane (.lookup (ui/main-root) "#breakpoints-container"))]
    (ui/context! tab-pane
                 :breakpoints-tab
                 {:project project :list-view (.lookup (ui/main-root) "#breakpoints-list-view")}
                 nil))
  ;; Restore breakpoints
  (g/with-auto-evaluation-context evaluation-context
    (let [bps-prefs (prefs/get prefs [:code :breakpoints])
          breakpoints (mapv #(assoc % :resource (workspace/find-resource workspace (:proj-path %) evaluation-context)) bps-prefs)
          script-bps (collect-script-nodes-from-breakpoints project breakpoints evaluation-context)]
      (g/transact
        (for [{:keys [script-node breakpoints]} script-bps
              :let [updated-regions (update-script-regions-from-breakpoints script-node breakpoints evaluation-context)]]
          (g/set-property script-node :regions updated-regions)))))
  (fx/mount-renderer
    state
    (fx/create-renderer
     :error-handler #'error-reporting/report-exception!
     :middleware (comp
                   fxui/wrap-dedupe-desc
                   (fx/wrap-map-desc #(breakpoints-view breakpoint-container %)))
     :opts {:fx.opt/map-event-handler #(handle-breakpoint-event! root project open-resource-fn %)}))
  (let [timer (ui/->timer
               4
               "breakpoints-tab-update-timer"
               (fn [timer _ _]
                 (when-not (ui/ui-disabled?)
                   (let [breakpoints (g/node-value project :breakpoints)]
                     (save-breakpoints prefs breakpoints)
                     (swap! state assoc :breakpoints breakpoints)))))]
    (ui/timer-start! timer)))

(handler/defhandler :breakpoints-tab.toggle-breakpoint-active :breakpoints-tab
  (run [project list-view]
    (g/with-auto-evaluation-context evaluation-context
      (let [breakpoints (:breakpoints @state)
            selected (map #(get breakpoints %) (:selected-indices @state))]
        (handle-breakpoint-action project
                                  evaluation-context
                                  breakpoints
                                  selected
                                  toggle-breakpoints-active)))))

(handler/defhandler :breakpoints-tab.remove-breakpoint :breakpoints-tab
  (run [project list-view]
    (g/with-auto-evaluation-context evaluation-context
      (let [breakpoints (:breakpoints @state)
            selected (map #(get breakpoints %) (:selected-indices @state))]
        (handle-breakpoint-action project
                                  evaluation-context
                                  breakpoints
                                  selected
                                  #(vec (remove (set %2) %1)))))))

(handler/defhandler :breakpoints-tab.edit-breakpoint :breakpoints-tab
  (run []
       ()))

(comment
  (let [list-view (.lookup (ui/main-root) "#breakpoints-list-view")
        items (.getItems list-view)]
    (select-breakpoints-by-bp! list-view (take 2 items)))
  (let [open-resource (partial #'editor.app-view/open-resource
                               (dev/app-view) (dev/prefs) (dev/localization) (dev/workspace) (dev/project))
        breakpoints-container (.lookup (ui/main-root) "#breakpoints-container")]
    (create-breakpoint-tab-renderer (ui/main-root) (dev/workspace) (dev/project) (dev/prefs) breakpoints-container open-resource))
  (prefs/get (dev/prefs) [:code :breakpoints])
  (g/with-auto-evaluation-context ec
    (let [bps-prefs [{:proj-path "/scripts/knight.script", :row 21, :active true}
                     {:proj-path "/scripts/game.script", :row 20, :active true}]
          breakpoints (mapv #(assoc % :resource (workspace/find-resource (dev/workspace) (:proj-path %) ec)) bps-prefs)
          script-bps (collect-script-nodes-from-breakpoints (dev/project) breakpoints ec)]
      (g/transact
        (for [{:keys [script-node breakpoints]} script-bps
              :let [updated-regions (update-script-regions-from-breakpoints script-node breakpoints ec)]]
          (g/set-property script-node :regions updated-regions)))))
  ,)
