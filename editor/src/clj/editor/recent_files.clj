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

(ns editor.recent-files
  (:require [cljfx.fx.label :as fx.label]
            [cljfx.fx.region :as fx.region]
            [dynamo.graph :as g]
            [editor.dialogs :as dialogs]
            [editor.fxui :as fxui]
            [editor.localization :as localization]
            [editor.prefs :as prefs]
            [editor.resource :as resource]
            [editor.resource-dialog :as resource-dialog]
            [editor.ui :as ui]
            [editor.ui.fuzzy-choices :as fuzzy-choices]
            [editor.workspace :as workspace])
  (:import [javafx.scene.control SplitPane Tab TabPane]))

(set! *warn-on-reflection* true)

(def ^:private history-size 32)

(defn- conj-history-item [items x]
  (let [new-items (into [] (remove #(= x %)) items)
        drop-count (- (count new-items)
                      (dec history-size))]
    (-> new-items
        (cond->> (pos? drop-count) (into [] (drop drop-count)))
        (conj x))))

(defn add! [prefs resource view-type]
  {:pre [(resource/openable? resource)
         (some? (:id view-type))]}
  (let [item [(resource/proj-path resource) (:id view-type)]
        k [:workflow :recent-files]]
    (prefs/set! prefs k (conj-history-item (prefs/get prefs k) item))))

(defn- project-path+view-type-id->resource+view-type [workspace evaluation-context [project-path view-type-id]]
  (when-let [res (workspace/find-resource workspace project-path evaluation-context)]
    (when (resource/openable? res)
      (when-let [view-type (workspace/get-view-type workspace view-type-id evaluation-context)]
        [res view-type]))))

(defn- ordered-resource+view-types [prefs workspace evaluation-context]
  (-> (prefs/get prefs [:workflow :recent-files])
      rseq
      (->> (keep #(project-path+view-type-id->resource+view-type workspace evaluation-context %)))))

(defn exist? [prefs workspace evaluation-context]
  (some? (seq (ordered-resource+view-types prefs workspace evaluation-context))))

(defn select [prefs workspace evaluation-context]
  (let [items (ordered-resource+view-types prefs workspace evaluation-context)]
    (dialogs/make-select-list-dialog
      items
      (workspace/localization workspace evaluation-context)
      {:title (localization/message "dialog.recent-files.title")
       :ok-label (localization/message "dialog.recent-files.button.ok")
       :cell-fn (fn [[resource view-type :as item] localization]
                  {:style-class (into ["list-cell"] (resource/style-classes resource))
                   :graphic {:fx/type resource-dialog/matched-list-item-view
                             :icon (workspace/resource-icon resource)
                             :text (resource/proj-path resource)
                             :matching-indices (:matching-indices (meta item))
                             :children [{:fx/type fx.region/lifecycle
                                         :h-box/hgrow :always}
                                        {:fx/type fxui/ext-localize
                                         :localization localization
                                         :message (:label view-type)
                                         :desc
                                         {:fx/type fx.label/lifecycle
                                          :style {:-fx-text-fill :-df-text-dark}}}]}})
       :selection :multiple
       :filter-fn #(fuzzy-choices/filter-options (comp resource/proj-path first) (comp resource/proj-path first) %1 %2)})))

(defn- open-resource+view-types [app-view evaluation-context]
  (into #{}
        (map (fn [tab]
               (let [view-type (ui/user-data tab :editor.app-view/view-type)
                     view (ui/user-data tab :editor.app-view/view)]
                 [(-> view
                      (g/node-value :resource-node evaluation-context)
                      (g/node-value :resource evaluation-context))
                  view-type])))
        (.getTabs ^TabPane (g/node-value app-view :active-tab-pane evaluation-context))))

(defn exist-closed? [prefs workspace app-view evaluation-context]
  (->> (ordered-resource+view-types prefs workspace evaluation-context)
       (remove (open-resource+view-types app-view evaluation-context))
       seq
       some?))

(defn last-closed [prefs workspace app-view evaluation-context]
  (->> (ordered-resource+view-types prefs workspace evaluation-context)
       (remove (open-resource+view-types app-view evaluation-context))
       first))

(defn some-recent [prefs workspace evaluation-context]
  (->> (ordered-resource+view-types prefs workspace evaluation-context)
       (take 10)))

(defn- tab->resource [^Tab tab]
  {:pre [(some? tab)]
   :post [#(resource/resource? %)]}
  (-> tab
      (ui/user-data :editor.app-view/view)
      (g/node-value :view-data)
      second
      :resource))

(defn- tab->prefs-data [tab]
  (let [tab-resource (tab->resource tab)]
    [(resource/proj-path tab-resource)
     (-> tab-resource
         resource/resource-type
         :view-types
         first
         :id)]))

(defn- collect-open-tabs [app-view]
  (let [editor-tabs-split ^SplitPane (g/node-value app-view :editor-tabs-split)]
    (mapv (fn [^TabPane tab-pane]
            (mapv tab->prefs-data (.getTabs tab-pane)))
          (.getItems editor-tabs-split))))

(defn- collect-tab-selections [app-view]
  (g/with-auto-evaluation-context evaluation-context
    (let [editor-tabs-split ^SplitPane (g/node-value app-view :editor-tabs-split evaluation-context)
          active-tab-pane (g/node-value app-view :active-tab-pane evaluation-context)
          tab-panes (.getItems editor-tabs-split)]
      {:selected-pane (.indexOf tab-panes active-tab-pane)
       :tab-selection-by-pane (mapv (fn [^TabPane pane]
                                  (-> pane .getSelectionModel .getSelectedIndex))
                                tab-panes)})))

(defn save-open-tabs [prefs app-view]
  (prefs/set! prefs [:workflow :open-tabs] (collect-open-tabs app-view)))

(defn save-tab-selections [prefs app-view]
  (prefs/set! prefs [:workflow :last-selected-tabs] (collect-tab-selections app-view)))

(comment
  (defn save-open-tabs [prefs app-view] nil)
  (defn save-tab-selections [prefs app-view] nil)
  (prefs/set! (dev/prefs) [:workflow :open-tabs] [[["/main/main.collection" :collection] ["/scripts/knight.script" :code]]
                                                  [["/scripts/utils_blah.lua" :code]["/scripts/utils.lua" :code]]])
  (prefs/set! (dev/prefs) [:workflow :open-tabs] [[["/scripts/utils.lua" :code]["/scripts/knight.script" :code]]])
  (prefs/set! (dev/prefs) [:workflow :last-selected-tabs] {:selected-pane 1, :tab-selection-by-pane [0 0]})
  (prefs/get (dev/prefs) [:workflow :open-tabs])
  (prefs/get (dev/prefs) [:workflow :last-selected-tabs])
  (prefs/get (dev/prefs) [:window])
  (prefs/set! (dev/prefs) [:workflow :open-tabs] (collect-open-tabs (dev/app-view)))
  ,)
