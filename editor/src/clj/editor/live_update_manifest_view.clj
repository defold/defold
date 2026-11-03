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
            [editor.defold-project :as project]
            [editor.fs :as fs]
            [editor.fxui :as fxui]
            [editor.live-update-preview :as preview]
            [editor.localization :as localization]
            [editor.progress :as progress]
            [editor.resource :as resource]
            [editor.ui :as ui]
            [editor.view :as view]
            [editor.workspace :as workspace]
            [util.coll :as coll])
  (:import [clojure.lang PersistentQueue]
           [com.dynamo.bob.archive.liveupdate PackagePreview$Result PackageResolver$Contribution PackageResolver$Diagnostic PackageResolver$Package PackageResolver$Plan PackageResolver$Resource PackageResolver$RuleId]
           [java.io File]
           [java.util.concurrent CancellationException]
           [javafx.event Event]
           [javafx.scene Parent]
           [javafx.scene.control Tab TreeItem TreeTableView]
           [javafx.scene.input Clipboard ClipboardContent]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(g/defnode ManifestView
  (inherits view/WorkbenchView)
  (input project-root g/Str)
  (input packages g/Any)
  (input diagnostics g/Any)
  (input generated-paths g/Any)
  (input resource-snapshot g/Any)
  (input save-data g/Any)
  (input selected-nodes g/Any)
  (output fingerprint g/Any :cached
          (g/fnk [resource-snapshot save-data generated-paths] [resource-snapshot save-data generated-paths]))
  (output model g/Any :cached
          (g/fnk [resource-node project-root packages diagnostics selected-nodes fingerprint]
            {:resource-node resource-node
             :project-directory (File. ^String project-root)
             :packages packages
             :diagnostics diagnostics
             :selection (get selected-nodes resource-node)
             :fingerprint fingerprint})))

(defn- selection-package [packages selected-node-ids]
  (or (coll/some (fn [{:keys [node-id rules value]}]
                   (when (or (contains? selected-node-ids node-id)
                             (coll/any? #(contains? selected-node-ids (:node-id %)) rules))
                     (:name value)))
                 packages)
      "base"))

(defn- check-cancelled! [cancelled?]
  (when (cancelled?)
    (throw (CancellationException.))))

(defn- base-dependency-paths [^PackageResolver$Plan plan cancelled?]
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
        (let [_ (check-cancelled! cancelled?)
              children (filterv #(not (contains? paths %)) (dependencies path))]
          (recur (reduce #(assoc %1 %2 (conj (paths path) %2)) paths children)
                 (into (pop pending) (sort children))
                 (disj remaining path)))
        (if-let [root (first remaining)]
          ;; Keep disconnected cycles visible when an ancestor is packaged.
          (recur (assoc paths root [root]) (conj pending root) remaining)
          paths)))))

(defn- index-preview [{:keys [^PackagePreview$Result compiled packages]} cancelled?]
  (when compiled
    (let [plan (.plan compiled)
          memberships (reduce (fn [memberships [name paths]]
                                (check-cancelled! cancelled?)
                                (reduce #(update %1 %2 (fnil conj []) name) memberships paths))
                              {}
                              (coll/into-> (sort-by key (.packages plan)) [["base" (.base plan)]]
                                (map (fn [[name ^PackageResolver$Package package]] [name (.resources package)]))))
          rule-map (coll/into-> packages {}
                     (mapcat (fn [{:keys [rules value]}]
                               (mapv (fn [index rule] [(PackageResolver$RuleId. (:name value) index) rule])
                                     (range (count rules)) rules))))]
      {:plan plan
       :memberships memberships
       :base-paths (base-dependency-paths plan cancelled?)
       :rule-map rule-map})))

(defn- resource-rows [{:keys [^PackageResolver$Plan plan memberships base-paths rule-map] :as index} package-name cancelled?]
  (if-not index
    []
    (let [^PackageResolver$Package package (get (.packages plan) package-name)
          paths (case package-name
                  nil (coll/sorted-key-set memberships)
                  "base" (.base plan)
                  (when package (.resources package)))]
      (mapv (fn [path]
              (check-cancelled! cancelled?)
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

(defn- resource-tree [rows cancelled?]
  (let [path->row (coll/pair-map-by :path identity rows)
        tree (reduce (fn [tree path]
                       (check-cancelled! cancelled?)
                       (update-in tree (filterv #(contains? path->row %) path) #(or % {})))
                     {}
                     (eduction (mapcat :dependency-paths) rows))]
    (letfn [(children [tree]
              (mapv (fn [[path descendants]]
                      (check-cancelled! cancelled?)
                      (assoc (path->row path) :children (children descendants)))
                    (sort-by key tree)))]
      (children tree))))

(defn- resource-tree-item [row cancelled?]
  (check-cancelled! cancelled?)
  {:fx/type :tree-item
   :fx/key (:path row)
   :expanded true
   :value row
   :children (mapv #(resource-tree-item % cancelled?) (:children row))})

(defn- selected-package [model ui-state]
  (if (= (set (:selection model)) (:highlighted ui-state))
    (:package ui-state "base")
    (selection-package (:packages model) (set (:selection model)))))

(defn- make-update-rows! [controller]
  ;; Only the worker uses this cache. Superseded work finishes before the next
  ;; request starts, and a new compiled preview releases the previous index.
  (let [cache (volatile! nil)
        result-key (volatile! [nil nil])]
    (fn update-rows! [model preview-state ui-state]
      (let [result (:result preview-state)
            package-name (selected-package model ui-state)
            query (string/lower-case (string/trim (:filter ui-state "")))
            searching (not (string/blank? query))
            rows-package (when-not searching package-name)]
        ;; Comparing entire compiled plans on every UI frame would itself walk
        ;; the dependency graph. Each completion gets an identity token instead.
        (when (not (identical? result (first @result-key)))
          (vreset! result-key [result (Object.)]))
        (preview/update! controller [(second @result-key) package-name query]
                         (fn prepare-rows! [cancelled?]
                           (assert (not (ui/on-ui-thread?)))
                           (check-cancelled! cancelled?)
                           (when (not (identical? result (:preview @cache)))
                             (vreset! cache {:preview result :index (index-preview (:value result) cancelled?)}))
                           (let [all-rows (or (get-in @cache [:rows rows-package])
                                              (let [rows (resource-rows (:index @cache) rows-package cancelled?)]
                                                (vswap! cache assoc-in [:rows rows-package] rows)
                                                rows))
                                 rows (if-not searching
                                        all-rows
                                        (filterv (fn [{:keys [path source]}]
                                                   (check-cancelled! cancelled?)
                                                   (or (string/includes? (string/lower-case path) query)
                                                       (and source (string/includes? (string/lower-case source) query))))
                                                 all-rows))
                                 root (or (when-not searching (get-in @cache [:trees rows-package]))
                                          (let [displayed-rows (if searching rows (resource-tree rows cancelled?))
                                                root {:fx/type :tree-item :expanded true
                                                      :children (mapv #(resource-tree-item % cancelled?) displayed-rows)}]
                                            (when-not searching (vswap! cache assoc-in [:trees rows-package] root))
                                            root))]
                             {:preview result :package-name package-name :searching searching
                              :rows rows :rows-by-path (coll/pair-map-by :path identity rows) :root root})))))))

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

(defn- update-preview! [project controller model]
  (let [fingerprint (:fingerprint model)]
    (when (not= fingerprint (:fingerprint @controller))
      ;; Follow the normal editor build: capture the graph on the UI thread,
      ;; evaluate on the worker, and keep build targets in the shared cache.
      (let [evaluation-context (g/make-evaluation-context)]
        (preview/update! controller fingerprint
                         (fn build-preview! [cancelled?]
                           (try
                             {:compiled (preview/resolve-graph project (:resource-node model) evaluation-context cancelled?)
                              :packages (:packages model)}
                             (finally
                               (ui/run-later
                                 (project/update-system-cache-build-targets! evaluation-context))))))))))

(defn- make-update-preview! [project controller]
  (let [pending (volatile! nil)]
    (fn [model elapsed-time]
      (let [fingerprint (:fingerprint model)]
        (when (not= fingerprint (:fingerprint @pending))
          (preview/invalidate! controller)
          (vreset! pending {:fingerprint fingerprint :since elapsed-time}))
        (when (<= 0.2 (- (double elapsed-time) (double (:since @pending))))
          (update-preview! project controller model))))))

(defn- handle-event! [{:keys [view-node app-view ui-state parent]} {:keys [event/type fx/event package path]}]
  (let [model (g/node-value view-node :model)
        root (:resource-node model)]
    (case type
      :filter (swap! ui-state assoc :filter event :selected-path nil)
      :clear-filter (do
                      (swap! ui-state assoc :filter "" :selected-path nil)
                      (ui/request-focus! (.lookup ^Parent parent "#liveupdate-filter")))
      :open-source (ui/run-command (.getTarget ^Event event) :file.open path)
      :copy-path (.setContent (Clipboard/getSystemClipboard) (doto (ClipboardContent.) (.putString path)))
      :open-folder (loop [directory (File. ^String path)]
                     (when directory
                       (if (.isDirectory directory)
                         (ui/open-file directory)
                         (recur (.getParentFile directory)))))
      :select-resource
      (let [row (some-> ^TreeItem event .getValue)]
        (if-not row
          (swap! ui-state assoc :selected-path nil)
          (let [live-rules (coll/into-> (:packages model) #{} (mapcat :rules) (map :node-id))
                nodes (filterv live-rules (:rule-nodes row))]
            (swap! ui-state assoc :selected-path (:path row) :package package :highlighted (set nodes))
            (when (coll/not-empty nodes)
              (g/transact {:undoable false} (app-view/select app-view root nodes)))))))))

(defn- view-desc [{:keys [parent model preview-error rows-state ui-state localization-state]}]
  (let [data (get-in rows-state [:result :value])
        package-name (or (:package-name data) (selected-package model ui-state))
        searching (:searching data)
        rows (:rows data [])
        result (:preview data)
        ^PackagePreview$Result compiled (get-in result [:value :compiled])
        export-paths (when (and compiled (not searching)) (get (.exportPaths compiled) package-name))
        diagnostics (if compiled (.diagnostics (.plan compiled)) (:diagnostics model))
        messages (coll/into-> [diagnostics (:diagnostics model)] []
                   cat
                   (map #(.message ^PackageResolver$Diagnostic %)))
        error (or preview-error (:error rows-state))
        diagnostic-text (coll/join-to-string "\n"
                                             (coll/into-> (cond-> messages error (conj (or (ex-message error) (str error)))) []
                                               (distinct)))
        row (when (= package-name (:package ui-state))
              (get (:rows-by-path data) (:selected-path ui-state)))
        message #(localization-state (localization/message %))]
    {:fx/type fxui/ext-with-anchor-pane-props
     :desc {:fx/type ui/ext-value :value parent}
     :props {:children
             [{:fx/type :v-box
               :anchor-pane/top 0 :anchor-pane/right 0 :anchor-pane/bottom 0 :anchor-pane/left 0
               :padding 12 :spacing 8
               :children
               (filterv some?
                        [{:fx/type :h-box :fx/key :filter :id "liveupdate-search" :spacing 4
                          :children [{:fx/type fxui/text-field :id "liveupdate-filter" :h-box/hgrow :always
                                      :prompt-text (message "liveupdate.search")
                                      :text (:filter ui-state "") :on-text-changed {:event/type :filter}}
                                     {:fx/type fxui/button :id "liveupdate-clear-search"
                                      :text (message "liveupdate.clear-search")
                                      :disable (coll/empty? (:filter ui-state))
                                      :on-action {:event/type :clear-filter}}]}
                         {:fx/type :v-box :fx/key :heading :spacing 4
                          :children
                          (cond-> [(cond-> {:fx/type fxui/label :id "liveupdate-package-heading"
                                            :style {:-fx-font-size 18 :-fx-text-fill "-df-text-selected"}
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
                                          (let [file (File. ^String path)
                                                project-directory (:project-directory model)]
                                            {:fx/type fxui/label :fx/key path :id "liveupdate-export-path"
                                             :color :text :style {:-fx-font-style "italic"}
                                             :min-width 0 :text-overrun :center-ellipsis
                                             :text (if (fs/below-directory? file project-directory)
                                                     (resource/relative-path project-directory file)
                                                     path)
                                             :context-menu {:fx/type :context-menu
                                                            :items [{:fx/type :menu-item :text (message "liveupdate.copy-path")
                                                                     :on-action {:event/type :copy-path :path path}}
                                                                    {:fx/type :menu-item :text (message "liveupdate.open-folder")
                                                                     :on-action {:event/type :open-folder :path path}}]}}))
                                        export-paths)))}
                         {:fx/type fxui/label :fx/key :summary :id "liveupdate-resource-summary"
                          :style {:-fx-text-fill "-df-text-selected"}
                          :text (localization-state (localization/message "liveupdate.resource-summary"
                                                                          {"count" (count rows)}))}
                         {:fx/type fx.ext.tree-table-view/with-selection-props
                          :fx/key :resources
                          :v-box/vgrow :always
                          :props {:on-selected-item-changed {:event/type :select-resource :package package-name}}
                          :desc {:fx/type :tree-table-view :id "liveupdate-resources"
                                 :show-root false
                                 :root {:fx/type fx/ext-recreate-on-key-changed
                                        :key (if searching :search package-name)
                                        :desc {:fx/type fxui/ext-dedupe-identical-desc
                                               :desc (or (:root data) {:fx/type :tree-item :expanded true})}}
                                 :column-resize-policy TreeTableView/CONSTRAINED_RESIZE_POLICY_ALL_COLUMNS
                                 :placeholder {:fx/type fxui/label :text (message "liveupdate.empty")}
                                 :columns [{:fx/type :tree-table-column :text (message "liveupdate.resource")
                                            :min-width 240 :pref-width 600
                                            :cell-value-factory :path}
                                           {:fx/type :tree-table-column :text (message "liveupdate.packages")
                                            :min-width 160 :pref-width 180 :max-width 240
                                            :cell-value-factory (fn [row]
                                                                  (coll/join-to-string ", " (mapv #(if (= "base" %) (message "liveupdate.base") %)
                                                                                                  (:packages row))))}
                                           {:fx/type :tree-table-column :text (message "liveupdate.added-by")
                                            :min-width 80 :pref-width 120 :max-width 180
                                            :cell-value-factory :added-by}]}}
                         (when-let [source (:source row)]
                           {:fx/type :h-box :fx/key :source :spacing 4 :alignment :baseline-left
                            :children [{:fx/type fxui/label :text (message "liveupdate.source")}
                                       {:fx/type :hyperlink :id "liveupdate-source" :text source
                                        :on-action {:event/type :open-source :path source}}]})
                         (when (coll/not-empty (:trace row))
                           {:fx/type :scroll-pane :fx/key :trace :fit-to-width true :max-height 64
                            :content {:fx/type fxui/label :wrap-text true :text (:trace row)}})
                         (when (coll/not-empty diagnostic-text)
                           {:fx/type :scroll-pane :fx/key :diagnostics :fit-to-width true :max-height 80
                            :content {:fx/type fxui/label :wrap-text true :min-width 0 :text diagnostic-text}})])}]}}))

(defn- make-view [^Parent parent manifest-node {:keys [app-view project workspace localization ^Tab tab]}]
  (let [view-node (first (g/tx-nodes-added
                           (g/transact {:undoable false}
                             (g/make-nodes [view ManifestView]
                               (g/connect workspace :root view :project-root)
                               (g/connect manifest-node :packages view :packages)
                               (g/connect manifest-node :diagnostics view :diagnostics)
                               (g/connect project :live-update-output-paths view :generated-paths)
                               (g/connect workspace :resource-snapshot view :resource-snapshot)
                               (g/connect project :dirty-save-data view :save-data)
                               (g/connect project :selected-node-ids-by-resource-node view :selected-nodes)))))
        controller (preview/controller)
        rows-controller (preview/controller)
        update-preview! (make-update-preview! project controller)
        update-rows! (make-update-rows! rows-controller)
        update-progress! (make-update-progress!)
        ui-state (atom {})
        environment {:view-node view-node :ui-state ui-state :app-view app-view :parent parent}
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
                                          (update-preview! model elapsed-time)
                                          (let [preview-state @controller
                                                ui-state @ui-state]
                                            (update-rows! model preview-state ui-state)
                                            (renderer {:model model :preview-error (:error preview-state) :rows-state @rows-controller :ui-state ui-state}))))
                                      (update-progress! (or (:job @controller) (:job @rows-controller)) elapsed-time)))
                    ui/timer-start!)]
    (g/user-data! view-node :preview-controller controller)
    (g/user-data! view-node :rows-controller rows-controller)
    (ui/on-closed! tab (fn [_]
                         (preview/close! controller)
                         (preview/close! rows-controller)
                         (update-progress! nil 0.0)
                         (ui/timer-stop! repainter)
                         (renderer nil)))
    view-node))

(defn register-view-types [workspace]
  (workspace/register-view-type workspace
    :id :live-update-manifest
    :label (localization/message "resource.view.lumanifest")
    :make-view-fn #'make-view))
