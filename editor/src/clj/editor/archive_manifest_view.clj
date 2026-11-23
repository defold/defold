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

(ns editor.archive-manifest-view
  (:require [cljfx.api :as fx]
            [cljfx.ext.tree-table-view :as fx.ext.tree-table-view]
            [cljfx.fx.context-menu :as fx.context-menu]
            [cljfx.fx.h-box :as fx.h-box]
            [cljfx.fx.label :as fx.label]
            [cljfx.fx.menu-item :as fx.menu-item]
            [cljfx.fx.progress-indicator :as fx.progress-indicator]
            [cljfx.fx.scroll-pane :as fx.scroll-pane]
            [cljfx.fx.tooltip :as fx.tooltip]
            [cljfx.fx.tree-item :as fx.tree-item]
            [cljfx.fx.tree-table-cell :as fx.tree-table-cell]
            [cljfx.fx.tree-table-column :as fx.tree-table-column]
            [cljfx.fx.tree-table-view :as fx.tree-table-view]
            [cljfx.fx.v-box :as fx.v-box]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.app-view :as app-view]
            [editor.archive-manifest :as manifest]
            [editor.build :as build]
            [editor.defold-project :as project]
            [editor.fs :as fs]
            [editor.fxui :as fxui]
            [editor.localization :as localization]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.ui :as ui]
            [editor.view :as view]
            [editor.workspace :as workspace]
            [internal.graph.types :as gt]
            [util.coll :as coll]
            [util.fn :as fn])
  (:import [clojure.lang PersistentQueue]
           [com.dynamo.bob.archive.packages PackageResolver PackageResolver$Loading PackageResolver$Contribution PackageResolver$Package PackageResolver$Plan PackageResolver$Resource PackageResolver$RuleId PackageResolver$Selection]
           [com.dynamo.liveupdate.proto LiveUpdateManifest$ManifestDesc LiveUpdateManifest$PackageDesc]
           [java.io File]
           [java.nio.file Path]
           [java.util.concurrent CancellationException]
           [javafx.event Event]
           [javafx.scene Node]
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
  (output fingerprint g/Any
          (g/fnk [resource-snapshot save-data generated-paths] [resource-snapshot save-data generated-paths]))
  (output model g/Any :cached
          (g/fnk [resource-node project-root packages diagnostics selected-nodes]
            {:resource-node resource-node
             :project-directory (File. ^String project-root)
             :packages packages
             :diagnostics diagnostics
             :selection (get selected-nodes resource-node)})))

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

(defn- shader-input-outputs [basis targets]
  (reduce (fn [mapping {:keys [node-id deps]}]
            (check-cancelled!)
            (let [outputs (coll/into-> deps []
                            (keep #(when (= "spc" (resource/ext (:resource %)))
                                     (resource/proj-path (:resource %)))))]
              (if (coll/empty? outputs)
                mapping
                (let [shaders (coll/into-> (g/inputs basis node-id) []
                                (filter #(= :shader-source-info (gt/source-label %)))
                                (map gt/source-id))
                      inputs (g/pre-traverse basis shaders
                                             (fn shader-includes [basis node-id]
                                               (check-cancelled!)
                                               (coll/into-> (g/inputs basis node-id :included-proj-paths+full-lines) []
                                                 (map gt/source-id))))]
                  (reduce (fn [mapping input]
                            (update mapping (resource/proj-path (resource-node/resource basis input)) into outputs))
                          mapping
                          inputs)))))
          {}
          targets))

(defn- raw-source-dependencies [project read-opts evaluation-context path]
  (check-cancelled!)
  ;; Transpiled sources have targets without project resource nodes.
  (when-let [node (project/get-resource-node project path evaluation-context)]
    (let [source (g/node-value node :resource evaluation-context)
          resource-type (resource/resource-type source (:basis evaluation-context))]
      (when-let [dependencies-fn (:dependencies-fn resource-type)]
        (let [value (g/node-value node :save-value evaluation-context)
              dependencies (dependencies-fn read-opts source value)]
          (if (not= "collection" (resource/type-ext source))
            dependencies
            ;; Collection instances are flattened from compiled collections.
            (let [instances (coll/into-> (:collection-instances value) #{} (map :collection))]
              (coll/into-> dependencies [] (remove instances)))))))))

(defn- resolve-graph [project manifest-node transpiled-targets evaluation-context]
  (let [_ (check-cancelled!)
        ;; As in the normal editor build, evaluate the captured graph on the
        ;; worker thread. The tracer also cancels during uncached evaluation.
        evaluation-context (assoc evaluation-context :tracer (fn [_ _ _ _] (check-cancelled!)))
        basis (:basis evaluation-context)
        workspace (project/workspace project evaluation-context)
        unloaded-proj-path? (g/raw-property-value basis workspace :unloaded-proj-path?)
        check-loaded! (fn check-loaded! [path node]
                        (when (unloaded-proj-path? path)
                          (when-let [node (or node (project/get-resource-node project path evaluation-context))]
                            (when-not (resource-node/loaded? basis node)
                              (let [localize @(workspace/localization workspace evaluation-context)]
                                (throw (ex-info (localize (localization/message "liveupdate.preview-unloaded-resource" {"path" path}))
                                                {:path path})))))))
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
                                              (check-loaded! path node)
                                              (g/node-value node :build-targets evaluation-context))]))))
          targets (build/resolve-dependencies-with-build-output
                    (into [game-target] (coll/vals selected-targets))
                    project (or transpiled-targets {}) evaluation-context)]
      (when (g/error? targets)
        (let [localize @(workspace/localization workspace evaluation-context)
              message (localize (g/error-message targets))
              code-transpilers (project/code-transpilers (:basis evaluation-context))
              needs-build (and (nil? transpiled-targets)
                               (coll/any? #(project/get-resource-node project (:build-file-proj-path %) evaluation-context)
                                          (g/node-value code-transpilers :transpiler-infos evaluation-context)))]
          (throw (ex-info (if needs-build
                            (str message "\n" (localize (localization/message "liveupdate.preview-transpiled-dependencies")))
                            message)
                          {:error targets}))))
      (when-not (identical? fn/constantly-false unloaded-proj-path?)
        ;; Collection instances contribute to their parent's output and are
        ;; absent from the resolved build targets.
        (g/pre-traverse basis
                        (coll/into-> targets []
                          (map :resource)
                          (remove #(contains? custom-paths (resource/proj-path %)))
                          (keep #(resource/proj-path (:resource %))))
                        (fn check-collection-inputs [_ path]
                          (check-cancelled!)
                          (check-loaded! path nil)
                          (when (= "collection" (resource/filename->type-ext path))
                            (when-let [node (project/get-resource-node project path evaluation-context)]
                              (coll/into-> (:collection-instances (g/node-value node :save-value evaluation-context)) []
                                (keep :collection)))))))
      (let [targets (filterv #(not (contains? #{"project" "texture_profiles"} (resource/ext (:resource (:resource %))))) targets)
            ;; Bob's GLSL include output is build-only. Other selected shader
            ;; sources and explicit custom resources retain their raw copies.
            custom-files (coll/into-> selected-targets custom-paths
                           (keep (fn [[path targets]]
                                   (when (and (coll/empty? targets)
                                              (not (string/ends-with? path ".glsl")))
                                     path))))
            source-path (fn source-path [target]
                          (or (resource/proj-path (:resource (:resource target)))
                              (some-> (resource-node/owner-resource (:basis evaluation-context) (:node-id target)) resource/proj-path)))
            read-opts (workspace/make-read-opts (:basis evaluation-context) workspace)
            source-dependencies (memoize (partial raw-source-dependencies project read-opts evaluation-context))
            ;; Follow program owners so shared shader targets retain every
            ;; source and include, without attributing material outputs to them.
            shader-inputs (shader-input-outputs (:basis evaluation-context) targets)
            ;; Raw inputs contribute to the owner's outputs; runtime
            ;; dependencies already have their own build targets.
            source-outputs (reduce-kv (fn [mapping path source-targets]
                                        (check-cancelled!)
                                        (if-not path
                                          mapping
                                          (let [runtime-sources (coll/into-> source-targets #{} (mapcat :deps) (map source-path))
                                                inputs (g/pre-traverse (:basis evaluation-context) [path]
                                                                       (fn build-inputs [_basis path]
                                                                         (coll/into-> (source-dependencies path) []
                                                                           (remove runtime-sources)
                                                                           (remove shader-inputs))))
                                                outputs (mapv (comp resource/proj-path :resource) source-targets)]
                                            (reduce #(update %1 %2 into outputs) mapping inputs))))
                                      (merge-with into shader-inputs
                                                  (coll/into-> custom-files
                                                    (coll/pair-map-by identity (constantly []) source-paths)
                                                    (map (fn [path] [path [path]]))))
                                      ;; Opaque copies use custom-files.
                                      (group-by source-path
                                                (filterv #(not (contains? custom-paths (resource/proj-path (:resource %))))
                                                         targets)))
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
                            [(str (case (.getProvider (PackageResolver/containerPreset manifest (.getContainerPreset package)))
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

(defn- resource-rows [{:keys [^PackageResolver$Plan plan memberships base-paths rule-map] :as index} package-name basis workspace]
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
                 :open-source-path (when (and source (app-view/file-open-user-data->openable-resources basis workspace source))
                                     source)
                 :rule-nodes (coll/into-> rules [] (keep #(get-in rule-map [% :node-id])))
                 :added-by (coll/join-to-string ", " (coll/into-> rules [] (keep #(get-in rule-map [% :value :name]))))
                 :dependency-paths (if (coll/not-empty contributions)
                                     (mapv #(vec (.dependencyPath ^PackageResolver$Contribution %)) contributions)
                                     [(get base-paths path [path])])
                 :trace (if (coll/not-empty contributions)
                          (coll/join-to-string "\n" (mapv #(coll/join-to-string " → " (.dependencyPath ^PackageResolver$Contribution %)) contributions))
                          (coll/join-to-string " → " (get base-paths path)))}))
            (sort paths)))))

(defn- resource-tree-item [{:keys [row children] :or {children []}}]
  {:fx/type fx.tree-item/lifecycle
   :expanded true
   :value row
   :children children})

(defn- resource-tree [rows]
  (let [path->row (coll/pair-map-by :path identity rows)
        tree (reduce (fn [tree path]
                       (update-in tree (filterv #(contains? path->row %) path) #(or % {})))
                     {}
                     (eduction (mapcat :dependency-paths) rows))]
    (letfn [(children [tree]
              (mapv (fn [[path descendants]]
                      {:fx/type resource-tree-item
                       :fx/key path
                       :row (path->row path)
                       :children (children descendants)})
                    (sort-by key tree)))]
      (children tree))))

(defn- selected-package [model {:keys [package rule-nodes]}]
  (let [packages (:packages model)
        selection (set (:selection model))
        package-name (or package "base")]
    (if (and (= selection rule-nodes)
             (or (= "base" package-name)
                 (coll/any? #(= package-name (get-in % [:value :name])) packages)))
      package-name
      (selection-package packages selection))))

(defn- preview-query [filter]
  (string/lower-case (string/trim filter)))

(defn- prepare-preview [project view-node {:keys [revision packages resource-node package-name query]} previous evaluation-context]
  (let [fingerprint (if (identical? revision (:revision previous))
                      (:fingerprint previous)
                      (g/node-value view-node :fingerprint evaluation-context))
        transpiled-targets (:transpiled-targets revision)
        same-graph (and (= fingerprint (:fingerprint previous))
                        (identical? transpiled-targets (get-in previous [:revision :transpiled-targets])))
        preview (if (and same-graph (= packages (:packages previous)))
                  previous
                  (let [compiled (if same-graph
                                   (:compiled previous)
                                   (resolve-graph project resource-node transpiled-targets evaluation-context))]
                    {:fingerprint fingerprint
                     :compiled compiled
                     :packages packages
                     :index (index-preview {:compiled compiled :packages packages})}))
        searching (not (string/blank? query))
        rows-package (when-not searching package-name)
        all-rows (or (get-in preview [:row-cache rows-package])
                     (resource-rows (:index preview) rows-package (:basis evaluation-context)
                                    (project/workspace project evaluation-context)))
        rows (if-not searching
               all-rows
               (filterv (fn [{:keys [path source]}]
                          (check-cancelled!)
                          (or (string/includes? (string/lower-case path) query)
                              (and source (string/includes? (string/lower-case source) query))))
                        all-rows))]
    (assoc (assoc-in preview [:row-cache rows-package] all-rows)
      :revision revision
      :package-name package-name
      :query query
      :rows rows
      :root {:fx/type fx.tree-item/lifecycle
             :expanded true
             :children (if-not searching
                         (resource-tree rows)
                         (mapv (fn [row]
                                 {:fx/type resource-tree-item
                                  :fx/key (:path row)
                                  :row row})
                               rows))})))

(defn- start-preview-job! [project view-node revision-current? state request]
  ;; Capture the graph on the UI thread. Only the current request may
  ;; publish results from the cancellable worker.
  (let [{:keys [generation result]} @state
        revision (:revision request)
        evaluation-context (g/make-evaluation-context)
        job (future
              (let [completion (try
                                 (let [manifest (:resource-node request)
                                       inputs (g/node-value manifest :selection-inputs evaluation-context)]
                                   (when (not= inputs (:inputs (g/node-value manifest :matched-diagnostics evaluation-context)))
                                     (let [selection (g/node-value manifest :selection evaluation-context)]
                                       (when-not (g/error? selection)
                                         (ui/run-later
                                           (when (and (identical? generation (:generation @state))
                                                      (revision-current? revision))
                                             (g/transact {:undoable false}
                                               (g/set-property manifest :matched-diagnostics
                                                 {:inputs inputs
                                                  :diagnostics (.diagnostics ^PackageResolver$Selection selection)}))))))))
                                 {:result (prepare-preview project view-node request result evaluation-context)
                                  :error nil}
                                 (catch InterruptedException _ {})
                                 (catch CancellationException _ {})
                                 (catch Exception error {:error error}))]
                (ui/run-later
                  (when (identical? generation (:generation @state))
                    (let [current (revision-current? revision)]
                      (when current
                        (project/update-system-cache-build-targets! evaluation-context))
                      (swap! state #(cond-> (dissoc % :job :job-started-at :show-progress)
                                      current (merge completion))))))))]
    (swap! state assoc :job job :job-started-at (System/nanoTime))))

(defn- make-preview-worker [project view-node revision-current? state]
  (let [pending (volatile! nil)]
    (letfn [(submit! [request]
              (when (not= request (:request @pending))
                (some-> (:job @state) future-cancel)
                (swap! state #(assoc (dissoc % :job :job-started-at :show-progress)
                                :generation (Object.)))
                (vreset! pending {:request request
                                  :submitted-at (System/nanoTime)}))
              nil)

            (start-ready-job! []
              (let [{:keys [request submitted-at]} @pending]
                (when (and submitted-at
                           (>= (unchecked-subtract (System/nanoTime) (long submitted-at)) 200000000))
                  (vswap! pending dissoc :submitted-at)
                  (start-preview-job! project view-node revision-current? state request))))]
      {:submit! submit!
       :start-ready-job! start-ready-job!})))

(defn- select-resource! [view-node app-view swap-state package-name ^TreeItem item]
  (when-let [row (some-> item .getValue)]
    (let [{:keys [resource-node packages]} (g/node-value view-node :model)
          live-rules (coll/into-> packages #{} (mapcat :rules) (map :node-id))
          nodes (filterv live-rules (:rule-nodes row))]
      (swap-state assoc :highlighted {:package package-name :rule-nodes (set nodes)})
      (when (coll/not-empty nodes)
        (g/transact {:undoable false} (app-view/select app-view resource-node nodes))))))

(defn- export-path-view [{:keys [path project-directory localization-state]}]
  (let [file (File. ^String path)]
    {:fx/type fxui/label
     :id "liveupdate-export-path"
     :color :text
     :style {:-fx-font-style "italic"}
     :min-width 0
     :text-overrun :center-ellipsis
     :text (if (fs/below-directory? file project-directory)
             (resource/relative-path project-directory file)
             path)
     :context-menu
     {:fx/type fx.context-menu/lifecycle
      :items [{:fx/type fx.menu-item/lifecycle
               :text (localization-state (localization/message "liveupdate.copy-path"))
               :on-action (fn copy-path! [_]
                            (.setContent (Clipboard/getSystemClipboard)
                                         (doto (ClipboardContent.) (.putString path))))}
              {:fx/type fx.menu-item/lifecycle
               :text (localization-state (localization/message "liveupdate.open-folder"))
               :on-action (fn open-folder! [_]
                            (loop [directory file]
                              (when directory
                                (if (.isDirectory directory)
                                  (ui/open-file directory)
                                  (recur (.getParentFile directory))))))}]}}))

(defn- preview-heading [{:keys [package-name searching export-paths project-directory resource-count show-progress localization-state]}]
  (let [message #(localization-state (localization/message %))]
    {:fx/type fx.v-box/lifecycle
     :spacing 1
     :children
     [(cond-> {:fx/type fx.label/lifecycle
               :id "liveupdate-package-heading"
               :style-class ["label" "cljfx-form-title"]
               :text (if searching
                       (message "liveupdate.search-results")
                       (localization-state
                         (localization/message "liveupdate.package-heading"
                                               {"name" (if (= "base" package-name)
                                                         (message "liveupdate.base")
                                                         package-name)})))}
        (and (not searching) (= "base" package-name))
        (assoc :content-display :right
               :graphic-text-gap 6
               :graphic {:fx/type fxui/icon-graphic
                         :type :icon/lock
                         :size 16
                         :style {:-fx-background-color :-df-text}}))
      {:fx/type fx.h-box/lifecycle
       :spacing 6
       :alignment :center-left
       :fill-height false
       :children
       (cond-> (mapv (fn [path]
                       {:fx/type export-path-view
                        :fx/key path
                        :path path
                        :project-directory project-directory
                        :localization-state localization-state})
                     export-paths)
         resource-count
         (conj {:fx/type fxui/label
                :fx/key :summary
                :id "liveupdate-resource-summary"
                :h-box/margin (if (coll/not-empty export-paths) {:left 10} 0)
                :text (localization-state
                        (localization/message "liveupdate.resource-summary"
                                              {"count" resource-count}))})

         show-progress
         (conj {:fx/type fx.progress-indicator/lifecycle
                :fx/key :spinner
                :id "liveupdate-preview-spinner"
                :h-box/margin {:left 10}
                :progress -1.0
                :min-width 16
                :max-width 16
                :min-height 16
                :max-height 16
                :style {:-fx-progress-color :-df-text-dark}
                :mouse-transparent true}
               {:fx/type fxui/label
                :fx/key :progress
                :color :text
                :text (message "liveupdate.preview-progress")}))}]}))

(defn- describe-resource-cell [localization-state {:keys [path source open-source-path trace]}]
  (let [tooltip (when source
                  {:fx/type fx.tooltip/lifecycle
                   :wrap-text true
                   :max-width 440
                   :text (if (string/blank? trace)
                           source
                           (str source "\n\n" (localization-state (localization/message "liveupdate.dependency-path")) "\n" trace))})]
    (cond-> {:text path
             :text-overrun :center-ellipsis}
      (and source (not open-source-path))
      (assoc :tooltip tooltip)

      open-source-path
      (assoc :content-display :right
             :graphic-text-gap 6
             :graphic {:fx/type fxui/button
                       :style-class "liveupdate-source-open"
                       :focus-traversable true
                       :graphic {:fx/type ui/image-icon
                                 :style-class "liveupdate-source-arrow"
                                 :path "icons/32/Icons_S_14_linkarrow.png"
                                 :size 16.0}
                       :tooltip tooltip
                       :on-action (fn open-source! [^Event event]
                                    (ui/run-command (.getSource event) :file.open open-source-path))}))))

(defn- resource-table [{:keys [package-name searching root on-selected-item-changed localization-state]}]
  (let [message #(localization-state (localization/message %))
        base-label (message "liveupdate.base")]
    {:fx/type fx.ext.tree-table-view/with-selection-props
     :props {:on-selected-item-changed on-selected-item-changed}
     :desc {:fx/type fx.tree-table-view/lifecycle
            :id "liveupdate-resources"
            :show-root false
            :fixed-cell-size 26
            :root {:fx/type fx/ext-recreate-on-key-changed
                   :key (if searching :search package-name)
                   :desc {:fx/type fxui/ext-dedupe-identical-desc
                          :desc (or root {:fx/type fx.tree-item/lifecycle
                                          :expanded true})}}
            :column-resize-policy TreeTableView/CONSTRAINED_RESIZE_POLICY_ALL_COLUMNS
            :placeholder {:fx/type fxui/label
                          :text (if root (message "liveupdate.empty") "")}
            :columns [{:fx/type fx.tree-table-column/lifecycle
                       :text (message "liveupdate.resource")
                       :min-width 240
                       :pref-width 600
                       :cell-value-factory identity
                       :cell-factory {:fx/cell-type fx.tree-table-cell/lifecycle
                                      :describe (fn/partial describe-resource-cell localization-state)}}
                      {:fx/type fx.tree-table-column/lifecycle
                       :text (message "liveupdate.packages")
                       :min-width 160
                       :pref-width 180
                       :max-width 240
                       :cell-value-factory (fn package-labels [row]
                                             (->> row
                                                  :packages
                                                  (mapv #(if (= "base" %) base-label %))
                                                  (coll/join-to-string ", ")))}
                      {:fx/type fx.tree-table-column/lifecycle
                       :text (message "liveupdate.added-by")
                       :min-width 80
                       :pref-width 120
                       :max-width 180
                       :cell-value-factory :added-by}]}}))

(ui/defc ^:private manifest-content
  {:compose [{:fx/type fx/ext-state
              :initial-state {:filter ""}}
             {:fx/type fx/ext-effect
              :fn (:request-preview! props)
              :args (let [{:keys [model state]} props]
                      [(assoc (select-keys model [:resource-node :revision :packages])
                         :package-name (selected-package model (:highlighted state))
                         :query (preview-query (:filter state)))])}
             {:fx/type fx/ext-watcher
              :ref (:localization props)
              :key :localization-state}
             {:fx/type fx/ext-watcher
              :ref (:preview-state props)
              :key :preview}]}
  [{:keys [view-node app-view model preview state swap-state localization-state]}]
  (let [{:keys [result error show-progress]} preview
        {:keys [filter highlighted]} state
        selected-package-name (selected-package model highlighted)
        query (preview-query filter)
        searching (not (string/blank? (if error query (:query result))))
        result (when (or (nil? error)
                         (and (= query (:query result))
                              (or searching (= selected-package-name (:package-name result)))))
                 result)
        package-name (or (:package-name result) selected-package-name)
        compiled (:compiled result)
        diagnostics (if compiled (.diagnostics ^PackageResolver$Plan (:plan compiled)) (:diagnostics model))
        messages (coll/into-> [diagnostics (:diagnostics model)] []
                   cat
                   (map #(localization-state (manifest/diagnostic-message %))))
        diagnostic-text (coll/join-to-string "\n"
                                             (coll/into-> (cond-> messages error (conj (or (ex-message error) (str error)))) []
                                               (distinct)))
        message #(localization-state (localization/message %))]
    {:fx/type fx.v-box/lifecycle
     :style-class ["cljfx-form" "liveupdate-manifest"]
     :spacing 8
     :children
     (cond-> [{:fx/type preview-heading
               :fx/key :heading
               :package-name package-name
               :searching searching
               :export-paths (when (and compiled (not searching))
                               (get (:export-paths compiled) package-name))
               :project-directory (:project-directory model)
               :resource-count (when result (count (:rows result)))
               :show-progress show-progress
               :localization-state localization-state}
              {:fx/type fx.h-box/lifecycle
               :fx/key :filter
               :id "liveupdate-search"
               :spacing 4
               :children [{:fx/type fxui/text-field
                           :id "liveupdate-filter"
                           :h-box/hgrow :always
                           :prompt-text (message "liveupdate.search")
                           :text filter
                           :on-text-changed #(swap-state assoc :filter %)}
                          {:fx/type fxui/button
                           :id "liveupdate-clear-search"
                           :text (message "liveupdate.clear-search")
                           :disable (coll/empty? filter)
                           :on-action (fn clear-filter! [^Event event]
                                        (swap-state assoc :filter "")
                                        (let [box (.getParent ^Node (.getSource event))]
                                          (ui/request-focus! (.lookup box "#liveupdate-filter"))))}]}
              {:fx/type resource-table
               :fx/key :resources
               :v-box/vgrow :always
               :package-name package-name
               :searching searching
               :root (:root result)
               :on-selected-item-changed #(select-resource! view-node app-view swap-state package-name %)
               :localization-state localization-state}]
       (coll/not-empty diagnostic-text)
       (conj {:fx/type fx.scroll-pane/lifecycle
              :fx/key :diagnostics
              :id "liveupdate-diagnostics"
              :fit-to-width true
              :max-height 80
              :content {:fx/type fxui/label
                        :wrap-text true
                        :min-width 0
                        :text diagnostic-text}}))}))

(defn- manifest-view [{:keys [parent] :as props}]
  {:fx/type fxui/ext-with-anchor-pane-props
   :desc {:fx/type ui/ext-value
          :value parent}
   :props {:children [(assoc (dissoc props :parent)
                        :fx/type manifest-content
                        :anchor-pane/top 0
                        :anchor-pane/right 0
                        :anchor-pane/bottom 0
                        :anchor-pane/left 0)]}})

(defn- make-view [parent manifest-node {:keys [app-view project workspace localization ^Tab tab]}]
  (let [view-node (first (g/tx-nodes-added
                           (g/transact {:undoable false}
                             (g/make-nodes [view ManifestView]
                               (g/connect workspace :root view :project-root)
                               (g/connect manifest-node :packages view :packages)
                               (g/connect manifest-node :diagnostics view :diagnostics)
                               (g/connect workspace :archive-output-paths view :generated-paths)
                               (g/connect workspace :resource-snapshot view :resource-snapshot)
                               (g/connect project :dirty-save-data view :save-data)
                               (g/connect project :selected-node-ids-by-resource-node view :selected-nodes)))))
        preview-state (atom {})
        fingerprint-endpoint (g/endpoint view-node :fingerprint)
        transpiler-endpoint (g/endpoint (project/code-transpilers (g/now)) :build-output)
        cached-transpiler-targets (fn cached-transpiler-targets []
                                    (let [targets (get (g/cache) transpiler-endpoint)]
                                      (when-not (g/error? targets)
                                        targets)))
        revision-current? (fn revision-current? [{:keys [invalidate-counters transpiled-targets]}]
                            (and invalidate-counters
                                 (not ((g/endpoint-invalidated-pred invalidate-counters) fingerprint-endpoint))
                                 (identical? transpiled-targets (cached-transpiler-targets))))
        {:keys [submit! start-ready-job!]} (make-preview-worker project view-node revision-current? preview-state)
        last-render (volatile! nil)
        repainter (doto (ui/->timer 30 "liveupdate-preview"
                                    (fn repaint! [_ _ _]
                                      (when (.isSelected tab)
                                        (let [model (g/node-value view-node :model)
                                              revision (:revision @last-render)
                                              current (revision-current? revision)]
                                          (when (or (not current) (not (identical? model (:model @last-render))))
                                            (let [revision (if current
                                                             revision
                                                             {:invalidate-counters (g/invalidate-counters)
                                                              :transpiled-targets (cached-transpiler-targets)})]
                                              (ui/advance-graph-user-data-component!
                                                view-node :view
                                                {:fx/type manifest-view
                                                 :parent parent
                                                 :view-node view-node
                                                 :app-view app-view
                                                 :localization localization
                                                 :model (assoc model :revision revision)
                                                 :request-preview! submit!
                                                 :preview-state preview-state})
                                              (vreset! last-render {:model model :revision revision}))))
                                        (start-ready-job!))
                                      (when-let [started-at (:job-started-at @preview-state)]
                                        (when (>= (unchecked-subtract (System/nanoTime) (long started-at)) 200000000)
                                          (swap! preview-state #(assoc (dissoc % :job-started-at) :show-progress true))))))
                    ui/timer-start!)]
    (g/user-data! view-node :preview-state preview-state)
    (ui/on-closed! tab (fn close-view! [_]
                         (ui/timer-stop! repainter)
                         (let [[previous] (reset-vals! preview-state {})]
                           (some-> (:job previous) future-cancel))
                         (ui/advance-graph-user-data-component! view-node :view nil)))
    view-node))

(defn register-view-types [workspace]
  (workspace/register-view-type workspace
    :id :live-update-manifest
    :label (localization/message "resource.view.archivemanifest")
    :make-view-fn #'make-view))
