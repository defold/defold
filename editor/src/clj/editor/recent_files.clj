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
  (:import [javafx.scene.control TabPane]))

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