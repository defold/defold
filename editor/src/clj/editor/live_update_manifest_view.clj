;; Copyright 2020-2026 The Defold Foundation
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

(ns editor.live-update-manifest-view
  (:require [cljfx.api :as fx]
            [cljfx.ext.tree-table-view :as fx.ext.tree-table-view]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.app-view :as app-view]
            [editor.fxui :as fxui]
            [editor.live-update-manifest :as manifest]
            [editor.live-update-preview :as preview]
            [editor.localization :as localization]
            [editor.prefs :as prefs]
            [editor.progress :as progress]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.ui :as ui]
            [editor.view :as view]
            [editor.workspace :as workspace]
            [util.coll :as coll]
            [util.debug-util :as debug-util])
  (:import [clojure.lang PersistentQueue]
           [com.dynamo.bob.archive.liveupdate PackagePreview$Result PackageResolver$Contribution PackageResolver$Diagnostic PackageResolver$Package PackageResolver$Plan PackageResolver$Resource PackageResolver$RuleId]
           [java.io File]
           [javafx.scene Parent]
           [javafx.scene.control Tab TreeItem TreeTableView]
           [javafx.scene.input Clipboard ClipboardContent]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(g/defnode ManifestView
  (inherits view/WorkbenchView)
  (input manifest-resource resource/Resource)
  (input packages g/Any)
  (input diagnostics g/Any)
  (input resource-map g/Any)
  (input generated-paths g/Any)
  (input resource-snapshot g/Any)
  (input save-data g/Any)
  (input selected-nodes g/Any)
  (output fingerprint g/Any :cached
          (g/fnk [resource-snapshot save-data generated-paths] [resource-snapshot save-data generated-paths]))
  (output model g/Any :cached
          (g/fnk [resource-node manifest-resource packages diagnostics resource-map generated-paths save-data selected-nodes fingerprint]
            {:resource-node resource-node
             :resource manifest-resource
             :packages packages
             :diagnostics diagnostics
             :resource-map resource-map
             :generated-paths generated-paths
             :save-data save-data
             :selection (get selected-nodes resource-node)
             :fingerprint fingerprint})))

(defn- selection-package [packages selected-node-ids]
  (or (coll/some (fn [{:keys [node-id rules value]}]
                   (when (or (contains? selected-node-ids node-id)
                             (coll/any? #(contains? selected-node-ids (:node-id %)) rules))
                     (:name value)))
                 packages)
      "base"))

(defn- base-dependency-paths [^PackageResolver$Plan plan]
  ;; Startup paths supply the roots and eager dependencies. Continue through
  ;; deferred edges for content that is also included in the base archive.
  (let [dependencies (coll/pair-map-by identity
                                     (fn [path]
                                       (when-let [^PackageResolver$Resource resource (get (.resources plan) path)]
                                         (filterv #(contains? (.base plan) %) (coll/keys (.dependencies resource)))))
                                     (.base plan))
        referenced (coll/into-> (coll/vals dependencies) #{} cat)
        roots (coll/into-> (.base plan) {} (remove referenced) (map #(vector % [%])))
        startup-paths (coll/into-> (.locks plan) roots (map (fn [[path route]] [path (vec route)])))]
    (loop [paths startup-paths
           pending (into PersistentQueue/EMPTY (coll/sorted-key-set startup-paths))
           remaining (into (sorted-set) (.base plan))]
      (if-let [path (peek pending)]
        (let [children (filterv #(not (contains? paths %)) (dependencies path))]
          (recur (reduce #(assoc %1 %2 (conj (paths path) %2)) paths children)
                 (into (pop pending) (sort children))
                 (disj remaining path)))
        (if-let [root (first remaining)]
          ;; Keep disconnected cycles visible when an ancestor is packaged.
          (recur (assoc paths root [root]) (conj pending root) remaining)
          paths)))))

(defn- resource-rows [^PackagePreview$Result result package-name packages]
  (if-not result
    []
    (let [plan (.plan result)
          ^PackageResolver$Package package (get (.packages plan) package-name)
          memberships (reduce (fn [memberships [name paths]]
                                (reduce #(update %1 %2 (fnil conj []) name) memberships paths))
                              {}
                              (coll/into-> (sort-by key (.packages plan)) [["base" (.base plan)]]
                                (map (fn [[name ^PackageResolver$Package package]] [name (.resources package)]))))
          paths (case package-name
                  nil (coll/sorted-key-set memberships)
                  "base" (.base plan)
                  (when package (.resources package)))
          base-paths (when (or (nil? package-name) (= "base" package-name)) (base-dependency-paths plan))
          rule-map (coll/into-> packages {}
                     (mapcat (fn [{:keys [rules value]}]
                               (mapv (fn [index rule] [(PackageResolver$RuleId. (:name value) index) rule])
                                     (range (count rules)) rules))))]
      (mapv (fn [path]
              (let [contributions (if package-name
                                    (when package (get (.provenance package) path))
                                    (coll/into-> (get memberships path) []
                                      (mapcat (fn [name]
                                                (when-let [^PackageResolver$Package package (get (.packages plan) name)]
                                                  (get (.provenance package) path))))))
                    rules (coll/into-> contributions [] (mapcat #(.rules ^PackageResolver$Contribution %)) (distinct))
                    source (or (get (.primarySources plan) path)
                               (when-let [^PackageResolver$Contribution contribution (first contributions)] (.source contribution)))]
                {:path path
                 :size (get (.compiledSizes result) path 0)
                 :packages (get memberships path)
                 :source source
                 :rule-nodes (coll/into-> rules [] (keep #(get-in rule-map [% :node-id])))
                 :added-by (coll/join-to-string ", " (coll/into-> rules [] (keep #(get-in rule-map [% :value :name]))))
                 :dependency-paths (if (coll/not-empty contributions)
                                     (mapv #(vec (.dependencyPath ^PackageResolver$Contribution %)) contributions)
                                     [(get base-paths path [path])])
                 :trace (if (coll/not-empty contributions)
                          (coll/join-to-string "\n" (mapv #(coll/join-to-string " → " (.dependencyPath ^PackageResolver$Contribution %)) contributions))
                          (coll/join-to-string " → " (get base-paths path)))}))
            (sort paths)))))

(defn- resource-tree [rows]
  (let [path->row (coll/pair-map-by :path identity rows)
        tree (reduce (fn [tree path]
                       (update-in tree (filterv #(contains? path->row %) path) #(or % {})))
                     {}
                     (eduction (mapcat :dependency-paths) rows))]
    (letfn [(children [tree]
              (mapv (fn [[path descendants]]
                      (assoc (path->row path) :children (children descendants)))
                    (sort-by key tree)))]
      (children tree))))

(defn- resource-tree-item [row]
  {:fx/type :tree-item
   :fx/key (:path row)
   :expanded true
   :value row
   :children (mapv resource-tree-item (:children row))})

(defn- selected-package [model ui-state]
  (if (= (set (:selection model)) (:highlighted ui-state))
    (:package ui-state "base")
    (selection-package (:packages model) (set (:selection model)))))

(def ^:private preview-progress-owner (atom nil))

(defn- make-update-progress! []
  (let [state (volatile! nil)]
    (fn [job ^double elapsed-time]
      (assert (ui/on-ui-thread?))
      (when-not (identical? job (:job @state))
        ;; Several manifest views can be preparing previews. Only the owner
        ;; may finish the shared status-bar task or read its cancellation state.
        (when-let [render-progress! (:render-progress! @state)]
          (when (compare-and-set! preview-progress-owner (:job @state) nil)
            (render-progress! progress/done)))
        (vreset! state (when job {:job job :started-at elapsed-time})))
      (when job
        ;; Reuse the view's frame timer, as search results do, to keep quick
        ;; updates silent without starting another timer for every edit.
        (when (and (not (:render-progress! @state))
                   (< 0.2 (- elapsed-time (double (:started-at @state))))
                   (compare-and-set! preview-progress-owner nil job))
          (let [[render-progress! task-cancelled?] (app-view/begin-task-progress! :live-update-preview)]
            (vswap! state assoc :render-progress! render-progress! :task-cancelled? task-cancelled?)
            (render-progress! (progress/make-cancellable-indeterminate
                               (localization/message "liveupdate.preview-progress")))))
        (when-let [task-cancelled? (:task-cancelled? @state)]
          (when (task-cancelled?)
            (reset! (:cancelled job) true)))))))

(defn- update-preview! [controller model options]
  (let [fingerprint [(:fingerprint model) options]]
    (when (not= fingerprint (:fingerprint @controller))
      ;; Capture resource types before starting background serialization. No
      ;; evaluation context or mutable graph state escapes into the builder.
      (let [save-data (mapv (fn [{:keys [resource save-value]}]
                             [(resource/proj-path resource) save-value (resource/resource-type resource)])
                           (:save-data model))
            resources (manifest/source-resources (:resource-map model) (:generated-paths model))]
        (preview/update! controller fingerprint
                         (fn [cancelled?]
                           (let [contents (coll/into-> save-data {}
                                            (keep (fn [[path save-value resource-type]]
                                                    (when-let [content (resource-node/save-content save-value resource-type)]
                                                      [path content]))))]
                             {:compiled (preview/build! controller resources contents (resource/proj-path (:resource model)) options cancelled?)
                              :packages (:packages model)})))))))

(defn- make-update-preview! [controller]
  (let [pending (volatile! nil)]
    (fn [model options elapsed-time]
      (let [fingerprint [(:fingerprint model) options]]
        (when (not= fingerprint (:fingerprint @pending))
          (preview/invalidate! controller)
          (vreset! pending {:fingerprint fingerprint :since elapsed-time}))
        (when (<= 0.2 (- (double elapsed-time) (double (:since @pending))))
          (update-preview! controller model options))))))

(defn- handle-event! [{:keys [view-node app-view ui-state]} {:keys [event/type fx/event package path]}]
  (let [model (g/node-value view-node :model)
        root (:resource-node model)]
    (case type
      :filter (swap! ui-state assoc :filter event :row nil)
      :copy-path (.setContent (Clipboard/getSystemClipboard) (doto (ClipboardContent.) (.putString path)))
      :open-folder (loop [directory (File. ^String path)]
                     (when directory
                       (if (.isDirectory directory)
                         (ui/open-file directory)
                         (recur (.getParentFile directory)))))
      :select-resource
      (when-let [row (some-> ^TreeItem event .getValue)]
        (let [live-rules (coll/into-> (:packages model) #{} (mapcat :rules) (map :node-id))
              nodes (filterv live-rules (:rule-nodes row))]
          (swap! ui-state assoc :row row :package package :highlighted (set nodes))
          (when (coll/not-empty nodes)
            (g/transact {:undoable false} (app-view/select app-view root nodes))))))))

(defn- view-with-rows-desc [{:keys [parent model preview-state ui-state localization-state rows searching]}]
  (let [package-name (selected-package model ui-state)
        result (:result preview-state)
        ^PackagePreview$Result compiled (get-in result [:value :compiled])
        export-paths (when (and compiled (not searching)) (get (.exportPaths compiled) package-name))
        diagnostics (if compiled (.diagnostics (.plan compiled)) (:diagnostics model))
        diagnostic-text (coll/join-to-string "\n" (coll/into-> [diagnostics (:diagnostics model)] []
                                                   cat
                                                   (map #(.message ^PackageResolver$Diagnostic %))
                                                   (distinct)))
        query (string/lower-case (string/trim (:filter ui-state "")))
        rows (if-not searching
               rows
               (filterv (fn [{:keys [path source]}]
                          (or (string/includes? (string/lower-case path) query)
                              (and source (string/includes? (string/lower-case source) query))))
                        rows))
        displayed-rows (if searching rows (resource-tree rows))
        row (when (= package-name (:package ui-state)) (:row ui-state))
        message #(localization-state (localization/message %))
        status (when-let [error (:error preview-state)]
                 (or (ex-message error) (str error)))]
    {:fx/type fxui/ext-with-anchor-pane-props
     :desc {:fx/type ui/ext-value :value parent}
     :props {:children
             [{:fx/type :v-box
               :anchor-pane/top 0 :anchor-pane/right 0 :anchor-pane/bottom 0 :anchor-pane/left 0
               :padding 12 :spacing 8
               :children
               (filterv some?
                [(when status
                   {:fx/type fxui/label :fx/key :status :text status :wrap-text true :min-width 0})
                {:fx/type fxui/text-field :fx/key :filter :id "liveupdate-filter" :prompt-text (message "liveupdate.search")
                 :text (:filter ui-state "") :on-text-changed {:event/type :filter}}
                {:fx/type :h-box :fx/key :heading :spacing 8 :alignment :baseline-left :style {:-fx-font-size 18}
                 :children
                 (cond-> [(cond-> {:fx/type fxui/label :id "liveupdate-package-heading"
                                  :style {:-fx-text-fill "-df-text-selected"}
                                  :text (if searching
                                          (message "liveupdate.search-results")
                                          (localization-state
                                            (localization/message "liveupdate.package-heading"
                                              {"name" (if (= "base" package-name) (message "liveupdate.base") package-name)})))}
                            (and (not searching) (= "base" package-name))
                            (assoc :content-display :right
                                   :graphic-text-gap 6
                                   :graphic {:fx/type fxui/icon-graphic :type :icon/lock :size 16
                                             :style {:-fx-background-color "-df-text-selected"}}))]
                   (coll/not-empty export-paths)
                   (into (mapv (fn [path]
                                 {:fx/type fxui/label :fx/key path :id "liveupdate-export-path"
                                  :style {:-fx-font-style "italic" :-fx-text-fill "-df-text-selected"}
                                  :min-width 0 :text-overrun :center-ellipsis
                                  :text path
                                  :tooltip {:fx/type :tooltip :text path}
                                  :context-menu {:fx/type :context-menu
                                                 :items [{:fx/type :menu-item :text (message "liveupdate.copy-path")
                                                          :on-action {:event/type :copy-path :path path}}
                                                         {:fx/type :menu-item :text (message "liveupdate.open-folder")
                                                          :on-action {:event/type :open-folder :path path}}]}})
                               export-paths)))}
                {:fx/type fxui/label :fx/key :summary :id "liveupdate-resource-summary"
                 :style {:-fx-text-fill "-df-text-selected"}
                 :text (localization-state (localization/message "liveupdate.resource-summary"
                                             {"count" (count rows)
                                              "size" (debug-util/bytes->string (reduce + 0 (mapv :size rows)))}))}
                {:fx/type fx.ext.tree-table-view/with-selection-props
                 :fx/key :resources
                 :v-box/vgrow :always
                 :props {:on-selected-item-changed {:event/type :select-resource :package package-name}}
                 :desc {:fx/type :tree-table-view :id "liveupdate-resources"
                        :show-root false
                        :root {:fx/type fx/ext-recreate-on-key-changed
                               :key (if searching :search package-name)
                               :desc {:fx/type :tree-item :expanded true
                                      :children (mapv resource-tree-item displayed-rows)}}
                        :column-resize-policy TreeTableView/CONSTRAINED_RESIZE_POLICY_ALL_COLUMNS
                        :placeholder {:fx/type fxui/label :text (message "liveupdate.empty")}
                        :columns [{:fx/type :tree-table-column :text (message "liveupdate.resource") :pref-width 340
                                   :cell-value-factory :path}
                                  {:fx/type :tree-table-column :text (message "liveupdate.compiled-size") :pref-width 130
                                   :cell-value-factory :size
                                   :cell-factory {:fx/cell-type :tree-table-cell
                                                  :describe (fn [size] {:text (when size (debug-util/bytes->string size))})}}
                                  {:fx/type :tree-table-column :text (message "liveupdate.packages") :pref-width 160
                                   :cell-value-factory (fn [row]
                                                         (coll/join-to-string ", " (mapv #(if (= "base" %) (message "liveupdate.base") %)
                                                                                      (:packages row))))}
                                  {:fx/type :tree-table-column :text (message "liveupdate.added-by") :pref-width 240
                                   :cell-value-factory :added-by}]}}
                (when row
                  {:fx/type fxui/label :fx/key :source :text (str (message "liveupdate.source") " " (:source row ""))})
                (when (coll/not-empty (:trace row))
                  {:fx/type :scroll-pane :fx/key :trace :fit-to-width true :max-height 64
                   :content {:fx/type fxui/label :wrap-text true :text (:trace row)}})
                (when (coll/not-empty diagnostic-text)
                  {:fx/type :scroll-pane :fx/key :diagnostics :fit-to-width true :max-height 80
                   :content {:fx/type fxui/label :wrap-text true :min-width 0 :text diagnostic-text}})])}]}}))

(defn- view-desc [{:keys [model preview-state ui-state] :as props}]
  (let [result (:result preview-state)
        searching (not (string/blank? (:filter ui-state)))]
    {:fx/type ui/ext-memo
     :fn resource-rows
     :args [(get-in result [:value :compiled])
            (when-not searching (selected-package model ui-state))
            (get-in result [:value :packages])]
     :key :rows
     :desc (assoc props :fx/type view-with-rows-desc :searching searching)}))

(defn- make-view [^Parent parent manifest-node {:keys [app-view project workspace prefs localization ^Tab tab]}]
  (let [view-node (first (g/tx-nodes-added
                          (g/transact {:undoable false}
                                      (g/make-nodes [view ManifestView]
                                        (g/connect manifest-node :resource view :manifest-resource)
                                        (g/connect manifest-node :packages view :packages)
                                        (g/connect manifest-node :diagnostics view :diagnostics)
                                        (g/connect workspace :resource-map view :resource-map)
                                        (g/connect workspace :live-update-output-paths view :generated-paths)
                                        (g/connect workspace :resource-snapshot view :resource-snapshot)
                                        (g/connect project :dirty-save-data view :save-data)
                                        (g/connect project :selected-node-ids-by-resource-node view :selected-nodes)))))
        controller (preview/controller)
        update-preview! (make-update-preview! controller)
        update-progress! (make-update-progress!)
        ui-state (atom {})
        environment {:view-node view-node :ui-state ui-state :app-view app-view}
        renderer (fx/create-renderer
                   :opts {:fx.opt/map-event-handler #(handle-event! environment %)}
                   :middleware (comp fxui/wrap-dedupe-desc
                                     (fx/wrap-map-desc
                                       (fn [desc]
                                         {:fx/type fx/ext-watcher :ref localization :key :localization-state
                                          :desc (assoc desc :fx/type view-desc :parent parent)}))))
        repainter (doto (ui/->timer 30 "liveupdate-preview"
                                  (fn [_ elapsed-time _]
                                    (when (.isSelected tab)
                                      (let [model (g/node-value view-node :model)]
                                        (update-preview! model (prefs/get prefs [:bundle :last-bob-options]) elapsed-time)
                                        (renderer {:model model :preview-state @controller :ui-state @ui-state})))
                                    (update-progress! (:job @controller) elapsed-time)))
                    ui/timer-start!)]
    (g/user-data! view-node :preview-controller controller)
    (ui/on-closed! tab (fn [_]
                         (preview/close! controller)
                         (update-progress! nil 0.0)
                         (ui/timer-stop! repainter)
                         (renderer nil)))
    view-node))

(defn register-view-types [workspace]
  (workspace/register-view-type workspace
    :id :live-update-manifest
    :label (localization/message "resource.view.lumanifest")
    :make-view-fn #'make-view))
