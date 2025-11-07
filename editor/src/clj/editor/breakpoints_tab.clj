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
            [cljfx.fx.anchor-pane :as fx.anchor-pane]
            [cljfx.fx.button :as fx.button]
            [cljfx.fx.check-box :as fx.check-box]
            [cljfx.fx.context-menu :as fx.context-menu]
            [cljfx.fx.h-box :as fx.h-box]
            [cljfx.fx.list-cell :as fx.list-cell]
            [cljfx.fx.list-view :as fx.list-view]
            [cljfx.fx.menu-item :as fx.menu-item]
            [cljfx.fx.separator-menu-item :as fx.separator-menu-item]
            [clojure.java.io :as io]
            [clojure.set :as set]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.code.data :as code-data]
            [editor.defold-project :as project]
            [editor.error-reporting :as error-reporting]
            [editor.fxui :as fxui]
            [editor.handler :as handler]
            [editor.localization :as localization]
            [editor.prefs :as prefs]
            [editor.protobuf :as protobuf]
            [editor.ui :as ui]
            [service.log :as log]
            [util.coll :as coll])
  (:import [javafx.scene.input MouseButton MouseEvent]))

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
                                 (filter code-data/breakpoint-region?)
                                 (filter #(contains? breakpoint-rows (code-data/breakpoint-row %)))
                                 (map (fn [region]
                                        (let [row (code-data/breakpoint-row region)
                                              breakpoint (breakpoint-map row)]
                                          (merge region (select-keys breakpoint [:active :condition]))))))
        existing-bp-rows (into #{} (comp (filter code-data/breakpoint-region?) (map code-data/breakpoint-row)) regions)
        new-bp-rows (set/difference breakpoint-rows existing-bp-rows)
        new-bp-regions (map (partial code-data/make-breakpoint-region lines) new-bp-rows)]
    (vec (sort (concat non-bp-regions existing-bp-regions new-bp-regions)))))

(defn- breakpoint-cell-view [breakpoint]
  ;; NOTE: We're getting passed in a nil breakpoint
  (when (some? breakpoint)
    (let [{:keys [resource row condition active]} breakpoint
          proj-path (:project-path resource)
          label-text (str proj-path ":" (+ 1 row) (when condition (str " [" condition "]")))]
      {:graphic
       {:fx/type fx.h-box/lifecycle
        :alignment :center-left
        :spacing 8
        :max-width ##Inf
        :on-mouse-clicked {:event-type :breakpoint-clicked
                           :clicked-breakpoint breakpoint}
        :children [{:fx/type fx.check-box/lifecycle
                    :h-box/hgrow :always
                    :text label-text
                    :selected active
                    :on-selected-changed {:event-type :toggle-active
                                          :breakpoint breakpoint}}
                   {:fx/type fx.button/lifecycle
                    :text "×"
                    :on-action {:event-type :remove
                                :breakpoint breakpoint}}]}
       :context-menu {:fx/type fx.context-menu/lifecycle
                      ;; :on-hidden {:event-type :context-menu-hidden}
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
                               :on-action {:event-type :remove-selected}}]} })))

(def ext-with-anchor-pane-props
  (fx/make-ext-with-props fx.anchor-pane/props))

(defn- breakpoints-view [parent breakpoints selected-breakpoints]
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
                               :on-selected-items-changed {:event-type :selected-items-changed}}
                       :desc
                       {:fx/type fx.list-view/lifecycle
                        :id "breakpoints-list-view"
                        :fixed-cell-size 60.0
                        :items breakpoints
                        :cell-factory {:fx/cell-type fx.list-cell/lifecycle
                                       :describe breakpoint-cell-view}}}]}})

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

(defn- handle-breakpoint-event! [project state event]
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
      (swap! state assoc :selected (:fx/event event))

      :breakpoint-clicked
      (let [^MouseEvent e (:fx/event event)
            {:keys [resource row]} (:clicked-breakpoint event)]
        (if (and (= MouseButton/PRIMARY (.getButton e))
                 (= 2 (.getClickCount e)))
          (let [open-fn (:open-resource-fn @state)]
            (open-fn resource (+ 1 row)))))

      ;; default case
      ;; The rest of the actions share a similar enough `action-scope` pattern that by deconstructing this way
      ;; we can shorten this code quite a bit
      (let [[action scope] (string/split (name (:event-type event)) #"-")
            all-breakpoints (:breakpoints @state)
            breakpoints (if (= scope "selected")
                          (:selected @state)
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

(defn create-breakpoint-tab-renderer [project breakpoints-list-view open-resource-fn]
  (let [state (atom {:breakpoints []
                     :selected []
                     :open-resource-fn (make-open-resource-fn project open-resource-fn)})
        timer (ui/->timer
               4
               "breakpoints-tab-update-timer"
               (fn [timer _ _]
                 (when-not (ui/ui-disabled?)
                   (let [breakpoints (g/node-value project :breakpoints)]
                     (swap! state assoc :breakpoints breakpoints)))))]
    (let [tab-pane (ui/parent-tab-pane (.lookup (ui/main-root) "#breakpoints-container"))]
      (ui/context! tab-pane
                   :breakpoints-tab
                   ;; TODO: Do we need the list-view?
                   {:list-view (.lookup (ui/main-root) "#breakpoints-list-view")}
                   nil))
    (fx/mount-renderer
      state
      (fx/create-renderer
        :error-handler error-reporting/report-exception!
        :middleware (comp
                      fxui/wrap-dedupe-desc
                      (fx/wrap-map-desc #(breakpoints-view breakpoints-list-view
                                                           (:breakpoints %)
                                                           (:selected- %))))
        :opts {:fx.opt/map-event-handler #(handle-breakpoint-event! project state %)}))
    (ui/timer-start! timer)))

(handler/defhandler :breakpoints-tab.toggle-breakpoint-active :breakpoints-tab
  (run [list-view tab-pane some-var]
    (g/with-auto-evaluation-context evaluation-context
      (let [selection-model (.getSelectionModel list-view)
            items (.getItems list-view)
            selected (.getSelectedItems selection-model)]
        (handle-breakpoint-action (dev/project)
                                  evaluation-context
                                  items
                                  selected
                                  toggle-breakpoints-active)))))

(handler/defhandler :breakpoints-tab.remove-breakpoint :breakpoints-tab
  (run [list-view tab-pane some-var]
    (g/with-auto-evaluation-context evaluation-context
      (let [selection-model (.getSelectionModel list-view)
            items (.getItems list-view)
            selected (.getSelectedItems selection-model)]
        (handle-breakpoint-action (dev/project)
                                  evaluation-context
                                  items
                                  selected
                                  #(vec (remove (set %2) %1)))))))

(handler/defhandler :breakpoints-tab.edit-breakpoint :breakpoints-tab
  (run []
       ()))
