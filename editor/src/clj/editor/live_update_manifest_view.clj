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
            [editor.build :as build]
            [editor.defold-project :as project]
            [editor.fs :as fs]
            [editor.fxui :as fxui]
            [editor.localization :as localization]
            [editor.progress :as progress]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.ui :as ui]
            [editor.view :as view]
            [editor.workspace :as workspace]
            [util.coll :as coll])
  (:import [clojure.lang PersistentQueue]
           [com.dynamo.bob.archive.liveupdate PackageResolver PackageResolver$Loading PackageResolver$Contribution PackageResolver$Diagnostic PackageResolver$Package PackageResolver$Plan PackageResolver$Resource PackageResolver$RuleId PackageResolver$Selection]
           [com.dynamo.liveupdate.proto LiveUpdateManifest$ManifestDesc LiveUpdateManifest$PackageDesc]
           [java.io File]
           [java.nio.file Path]
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

(defn- check-cancelled! []
  (when (.isInterrupted (Thread/currentThread))
    (throw (CancellationException.))))

(defn- resolve-graph [project manifest-node evaluation-context]
  (let [_ (check-cancelled!)
        ;; As in the normal editor build, evaluate the captured graph on the
        ;; worker thread. The tracer also cancels during uncached evaluation.
        evaluation-context (assoc evaluation-context :tracer (fn [_ _ _ _] (check-cancelled!)))
        workspace (project/workspace project evaluation-context)
        source-paths (g/node-value manifest-node :source-paths evaluation-context)
        manifest (protobuf/map->pb LiveUpdateManifest$ManifestDesc (g/node-value manifest-node :save-value evaluation-context))
        directory (g/node-value manifest-node :resolved-output-directory evaluation-context)
        selected (g/node-value manifest-node :selected-paths evaluation-context)
        game-project (project/get-resource-node project "/game.project" evaluation-context)
        roots (g/node-value game-project :build-targets evaluation-context)
        custom-paths (coll/into-> (g/node-value game-project :custom-resource+versions evaluation-context) #{}
                       (map (comp resource/proj-path first)))]
    (when-let [error (g/flatten-errors [selected roots directory])]
      (throw (ex-info (@(workspace/localization workspace evaluation-context) (g/error-message error)) {:error error})))
    (let [game-target (first roots)
          selected-targets (coll/into-> selected {}
                             (remove custom-paths)
                             (map (fn [path]
                                    (let [node (project/get-resource-node project path evaluation-context)
                                          source (g/node-value node :resource evaluation-context)]
                                      [path (when-not (resource/placeholder-resource-type? (resource/resource-type source))
                                              (g/node-value node :build-targets evaluation-context))]))))
          targets (build/resolve-dependencies (into [game-target] (coll/vals selected-targets)) project evaluation-context)]
      (when (g/error? targets)
        (throw (ex-info (@(workspace/localization workspace evaluation-context) (g/error-message targets)) {:error targets})))
      (let [targets (filterv #(not (contains? #{"project" "texture_profiles"} (resource/ext (:resource (:resource %))))) targets)
            ;; Custom files retain their source path and have no dependencies.
            ;; Their contents are not needed for the preview.
            custom-files (coll/into-> selected-targets custom-paths
                           (keep (fn [[path targets]] (when (coll/empty? targets) path))))
            source-path (fn source-path [target]
                          (or (resource/proj-path (:resource (:resource target)))
                              (some-> (resource-node/owner-resource (:basis evaluation-context) (:node-id target)) resource/proj-path)))
            source-outputs (reduce (fn [mapping target]
                                     (check-cancelled!)
                                     (if-let [source (source-path target)]
                                       (update mapping source (fnil conj []) (resource/proj-path (:resource target)))
                                       mapping))
                                   (coll/pair-map-by identity (constantly []) source-paths)
                                   targets)
            source-outputs (coll/into-> custom-files source-outputs (map (fn [path] [path [path]])))
            primary-sources (coll/into-> targets (zipmap custom-files custom-files)
                              (keep (fn [target]
                                      (when-let [source (source-path target)]
                                        [(resource/proj-path (:resource target)) source]))))
            graph (coll/into-> targets (coll/pair-map-by identity (constantly (PackageResolver$Resource. {})) custom-files)
                    (map (fn [target]
                           (check-cancelled!)
                           (let [output (:resource target)
                                 dynamic (get-in target [:user-data :pb-map :load-dynamically])
                                 loading (case (resource/ext output)
                                           "collectionproxyc" PackageResolver$Loading/DEFERRED
                                           "factoryc" (if dynamic PackageResolver$Loading/DEFERRED PackageResolver$Loading/EAGER)
                                           "collectionfactoryc" (if dynamic PackageResolver$Loading/DESCRIPTOR PackageResolver$Loading/EAGER)
                                           PackageResolver$Loading/EAGER)]
                             [(resource/proj-path output)
                              (PackageResolver$Resource.
                                (coll/into-> (:deps target) {}
                                  (map #(vector (resource/proj-path (:resource %)) loading))))]))))
            bootstrap (coll/into-> (get-in game-target [:user-data :path->built-resource-settings]) []
                        (remove #(= ["graphics" "texture_profiles"] (key %)))
                        (keep #(some-> (val %) resource/proj-path)))
            plan (PackageResolver/resolve manifest source-outputs primary-sources graph bootstrap custom-paths true)
            paths (coll/into-> (.getPackagesList manifest) {}
                    (map (fn [^LiveUpdateManifest$PackageDesc package]
                           [(.getName package)
                            [(str (case (.getProvider package)
                                    "zip" (.resolve ^Path directory (str (.getName package) ".zip"))
                                    "folder" (.resolve ^Path directory (.getName package))
                                    ;; Custom names need final payloads.
                                    directory))]])))]
        (check-cancelled!)
        {:plan plan :export-paths paths}))))

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
        (let [_ (check-cancelled!)
              children (filterv #(not (contains? paths %)) (dependencies path))]
          (recur (reduce #(assoc %1 %2 (conj (paths path) %2)) paths children)
                 (into (pop pending) (sort children))
                 (disj remaining path)))
        (if-let [root (first remaining)]
          ;; Keep disconnected cycles visible when an ancestor is packaged.
          (recur (assoc paths root [root]) (conj pending root) remaining)
          paths)))))

(defn- index-preview [{:keys [compiled packages]}]
  (when compiled
    (let [^PackageResolver$Plan plan (:plan compiled)
          memberships (reduce (fn [memberships [name paths]]
                                (check-cancelled!)
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
       :base-paths (base-dependency-paths plan)
       :rule-map rule-map})))

(defn- resource-rows [{:keys [^PackageResolver$Plan plan memberships base-paths rule-map] :as index} package-name]
  (if-not index
    []
    (let [^PackageResolver$Package package (get (.packages plan) package-name)
          paths (case package-name
                  nil (coll/sorted-key-set memberships)
                  "base" (.base plan)
                  (when package (.resources package)))]
      (mapv (fn [path]
              (check-cancelled!)
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

(defn- resource-tree [rows]
  (let [path->row (coll/pair-map-by :path identity rows)
        tree (reduce (fn [tree path]
                       (check-cancelled!)
                       (update-in tree (filterv #(contains? path->row %) path) #(or % {})))
                     {}
                     (eduction (mapcat :dependency-paths) rows))]
    (letfn [(children [tree]
              (mapv (fn [[path descendants]]
                      (check-cancelled!)
                      (assoc (path->row path) :children (children descendants)))
                    (sort-by key tree)))]
      (children tree))))

(defn- resource-tree-item [row]
  (check-cancelled!)
  {:fx/type :tree-item
   :fx/key (:path row)
   :expanded true
   :value row
   :children (mapv #(resource-tree-item %) (:children row))})

(defn- selected-package [model ui-state]
  (if (= (set (:selection model)) (:highlighted ui-state))
    (:package ui-state "base")
    (selection-package (:packages model) (set (:selection model)))))

(defn- prepare-preview [project model previous package-name query evaluation-context]
  (let [fingerprint (:fingerprint model)
        preview (if (= fingerprint (:fingerprint previous))
                  previous
                  (let [compiled (resolve-graph project (:resource-node model) evaluation-context)]
                    {:fingerprint fingerprint :compiled compiled
                     :index (index-preview {:compiled compiled :packages (:packages model)})}))
        searching (not (string/blank? query))
        rows-package (when-not searching package-name)
        all-rows (or (get-in preview [:row-cache rows-package])
                     (resource-rows (:index preview) rows-package))
        rows (if-not searching
               all-rows
               (filterv (fn [{:keys [path source]}]
                          (check-cancelled!)
                          (or (string/includes? (string/lower-case path) query)
                              (and source (string/includes? (string/lower-case source) query))))
                        all-rows))]
    (assoc (assoc-in preview [:row-cache rows-package] all-rows)
      :package-name package-name :searching searching
      :rows rows :rows-by-path (coll/pair-map-by :path identity rows)
      :root {:fx/type :tree-item :expanded true
             :children (mapv resource-tree-item (if searching rows (resource-tree rows)))})))

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
            (future-cancel job)))))))

(defn- make-update-preview! [project state]
  (let [pending (volatile! nil)]
    (fn update-preview! [model ui-state elapsed-time]
      (let [package-name (selected-package model ui-state)
            query (string/lower-case (string/trim (:filter ui-state "")))
            request [(:fingerprint model) package-name query]]
        (when (not= request (:request @pending))
          (some-> (:job @state) future-cancel)
          (swap! state assoc :generation (Object.) :job nil :error nil)
          (vreset! pending {:request request :since elapsed-time}))
        (when (and (not (:started @pending))
                   (<= 0.2 (- (double elapsed-time) (double (:since @pending)))))
          (vswap! pending assoc :started true)
          ;; Like the search view, capture graph state on the UI thread and
          ;; prepare everything on a cancellable future. Only the current
          ;; request may apply its result, including after the view is closed.
          (let [{:keys [generation result]} @state
                evaluation-context (g/make-evaluation-context)
                job (future
                      (let [completion (try
                                         (let [manifest (:resource-node model)
                                               inputs (g/node-value manifest :selection-inputs evaluation-context)]
                                           (when (not= inputs (:inputs (g/node-value manifest :matched-diagnostics evaluation-context)))
                                             (let [selection (g/node-value manifest :selection evaluation-context)]
                                               (ui/run-later
                                                 (when (and (identical? generation (:generation @state))
                                                            (g/node-by-id manifest))
                                                   (g/transact {:undoable false}
                                                     (g/set-property manifest :matched-diagnostics
                                                       {:inputs inputs
                                                        :diagnostics (.diagnostics ^PackageResolver$Selection selection)})))))))
                                         {:result (prepare-preview project model result package-name query evaluation-context)}
                                         (catch InterruptedException _ {})
                                         (catch CancellationException _ {})
                                         (catch Exception error {:error error}))]
                        (ui/run-later
                          (project/update-system-cache-build-targets! evaluation-context)
                          (swap! state #(if-not (identical? generation (:generation %))
                                          %
                                          (merge % {:job nil} (when-not (future-cancelled? (:job %)) completion)))))))]
            (swap! state assoc :job job)))))))

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

(defn- view-desc [{:keys [parent model preview-state ui-state localization-state]}]
  (let [data (:result preview-state)
        package-name (or (:package-name data) (selected-package model ui-state))
        searching (:searching data)
        rows (:rows data [])
        compiled (:compiled data)
        export-paths (when (and compiled (not searching)) (get (:export-paths compiled) package-name))
        diagnostics (if compiled (.diagnostics ^PackageResolver$Plan (:plan compiled)) (:diagnostics model))
        messages (coll/into-> [diagnostics (:diagnostics model)] []
                   cat
                   (map #(.message ^PackageResolver$Diagnostic %)))
        error (:error preview-state)
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
                               (g/connect manifest-node :generated-paths view :generated-paths)
                               (g/connect workspace :resource-snapshot view :resource-snapshot)
                               (g/connect project :dirty-save-data view :save-data)
                               (g/connect project :selected-node-ids-by-resource-node view :selected-nodes)))))
        preview-state (atom {})
        update-preview! (make-update-preview! project preview-state)
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
                                      (when (some-> (:job @preview-state) future-cancelled?)
                                        (swap! preview-state assoc :generation (Object.) :job nil))
                                      (when (.isSelected tab)
                                        (let [model (g/node-value view-node :model)
                                              ui-state @ui-state]
                                          (update-preview! model ui-state elapsed-time)
                                          (renderer {:model model :preview-state @preview-state :ui-state ui-state})))
                                      (update-progress! (:job @preview-state) elapsed-time)))
                    ui/timer-start!)]
    (g/user-data! view-node :preview-state preview-state)
    (ui/on-closed! tab (fn [_]
                         (let [[previous] (reset-vals! preview-state {})]
                           (some-> (:job previous) future-cancel))
                         (update-progress! nil 0.0)
                         (ui/timer-stop! repainter)
                         (renderer nil)))
    view-node))

(defn register-view-types [workspace]
  (workspace/register-view-type workspace
    :id :live-update-manifest
    :label (localization/message "resource.view.lumanifest")
    :make-view-fn #'make-view))
