(ns editor.defold-project
  "Define the concept of a project, and its Project node type. This namespace bridges between Eclipse's workbench and
  ordinary paths."
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.collision-groups :as collision-groups]
            [editor.console :as console]
            [editor.core :as core]
            [editor.error-reporting :as error-reporting]
            [editor.gl :as gl]
            [editor.handler :as handler]
            [editor.ui :as ui]
            [editor.library :as library]
            [editor.progress :as progress]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.resource-update :as resource-update]
            [editor.workspace :as workspace]
            [editor.outline :as outline]
            [editor.validation :as validation]
            [editor.game-project-core :as gpc]
            [editor.settings-core :as settings-core]
            [editor.pipeline :as pipeline]
            [editor.prefs :as prefs]
            [editor.properties :as properties]
            [editor.system :as system]
            [editor.util :as util]
            [service.log :as log]
            [editor.graph-util :as gu]
            [util.digest :as digest]
            [util.http-server :as http-server]
            [util.text-util :as text-util]
            [clojure.string :as str])
  (:import [java.io File]
           [org.apache.commons.codec.digest DigestUtils]
           [org.apache.commons.io IOUtils]
           [editor.resource FileResource]))

(set! *warn-on-reflection* true)

(def ^:dynamic *load-cache* nil)

(defn graph [project]
  (g/node-id->graph-id project))

(defn- load-node [project node-id node-type resource]
  (try
    (let [loaded? (and *load-cache* (contains? @*load-cache* node-id))
          load-fn (some-> resource (resource/resource-type) :load-fn)]
      (if (and load-fn (not loaded?))
        (if (resource/exists? resource)
          (try
            (when *load-cache*
              (swap! *load-cache* conj node-id))
            (concat
              (load-fn project node-id resource)
              (when (instance? FileResource resource)
                (g/connect node-id :save-data project :save-data)))
            (catch Exception e
              (log/warn :msg (format "Unable to load resource '%s'" (resource/proj-path resource)) :exception e)
              (g/mark-defective node-id node-type (validation/invalid-content-error node-id nil :fatal resource))))
          (g/mark-defective node-id node-type (validation/file-not-found-error node-id nil :fatal resource)))
        []))
    (catch Throwable t
      (throw (ex-info (format "Error when loading resource '%s'" (resource/resource->proj-path resource))
                      {:node-type node-type
                       :resource-path (resource/resource->proj-path resource)}
                      t)))))

(defn load-resource-nodes [project node-ids render-progress!]
  (let [evaluation-context (g/make-evaluation-context)
        basis (:basis evaluation-context)
        progress (atom (progress/make "Loading resources..." (count node-ids)))]
    (doall
     (for [node-id node-ids
           :let [type (g/node-type* basis node-id)]
           :when (g/has-output? type :resource)
           :let [resource (g/node-value node-id :resource evaluation-context)]]
       (do
         (when render-progress!
           (render-progress! (swap! progress progress/advance)))
         (load-node project node-id type resource))))))

(defn- load-nodes! [project node-ids render-progress!]
  (g/transact
    (load-resource-nodes project node-ids render-progress!)))

(defn- connect-if-output [src-type src tgt connections]
  (let [outputs (g/output-labels src-type)]
    (for [[src-label tgt-label] connections
          :when (contains? outputs src-label)]
      (g/connect src src-label tgt tgt-label))))

(defn make-resource-node
  ([graph project resource load? connections]
    (make-resource-node graph project resource load? connections nil))
  ([graph project resource load? connections attach-fn]
    (assert resource "resource required to make new node")
    (let [resource-type (resource/resource-type resource)
          found? (some? resource-type)
          node-type (or (:node-type resource-type) resource-node/PlaceholderResourceNode)]
      (g/make-nodes graph [node [node-type :resource resource]]
                      (concat
                        (for [[consumer connection-labels] connections]
                          (connect-if-output node-type node consumer connection-labels))
                        (if (and (some? resource-type) load?)
                          (load-node project node node-type resource)
                          [])
                        (if attach-fn
                          (attach-fn node)
                          []))))))

(defn- make-nodes! [project resources]
  (let [project-graph (graph project)]
    (g/tx-nodes-added
      (g/transact
        (for [[resource-type resources] (group-by resource/resource-type resources)
              resource resources]
          (if (not= (resource/source-type resource) :folder)
            (make-resource-node project-graph project resource false {project [[:_node-id :nodes]
                                                                               [:resource :node-resources]]})
            []))))))

(defn get-resource-node [project path-or-resource]
  (when-let [resource (cond
                        (string? path-or-resource) (workspace/find-resource (g/node-value project :workspace) path-or-resource)
                        (satisfies? resource/Resource path-or-resource) path-or-resource
                        :else (assert false (str (type path-or-resource) " is neither a path nor a resource: " (pr-str path-or-resource))))]
    (let [nodes-by-resource-path (g/node-value project :nodes-by-resource-path)]
      (get nodes-by-resource-path (resource/proj-path resource)))))

(defn load-project
  ([project]
   (load-project project (g/node-value project :resources)))
  ([project resources]
   (load-project project resources progress/null-render-progress!))
  ([project resources render-progress!]
   (with-bindings {#'*load-cache* (atom (into #{} (g/node-value project :nodes)))}
     (let [nodes (make-nodes! project resources)]
       (load-nodes! project nodes render-progress!)
       (when-let [game-project (get-resource-node project "/game.project")]
         (g/transact
           (concat
             (g/connect game-project :display-profiles-data project :display-profiles)
             (g/connect game-project :texture-profiles-data project :texture-profiles)             
             (g/connect game-project :settings-map project :settings))))
       project))))

(defn make-embedded-resource [project type data]
  (when-let [resource-type (get (g/node-value project :resource-types) type)]
    (resource/make-memory-resource (g/node-value project :workspace) resource-type data)))

(defn all-save-data [project]
  (g/node-value project :save-data))

(defn dirty-save-data
  [project]
  (g/node-value project :dirty-save-data))

(defn write-save-data-to-disk! [project {:keys [render-progress!]
                                         :or {render-progress! progress/null-render-progress!}
                                         :as opts}]
  (render-progress! (progress/make "Saving..."))
  (let [save-data (dirty-save-data project)]
    (if (g/error? save-data)
      (throw (Exception. ^String (properties/error-message save-data)))
      (do
        (progress/progress-mapv
          (fn [{:keys [resource content value node-id]} _]
            (when-not (resource/read-only? resource)
              ;; If the file is non-binary, convert line endings to the
              ;; type used by the existing file.
              (if (and (:textual? (resource/resource-type resource))
                    (resource/exists? resource)
                    (= :crlf (text-util/guess-line-endings (io/make-reader resource nil))))
                (spit resource (text-util/lf->crlf content))
                (spit resource content))))
          save-data
          render-progress!
          (fn [{:keys [resource]}] (and resource (str "Saving " (resource/resource->proj-path resource)))))
        (g/invalidate-outputs! (mapv (fn [sd] [(:node-id sd) :source-value]) save-data))))))

(defn workspace [project]
  (g/node-value project :workspace))

(defn save-all!
  ([project]
   (save-all! project ui/default-render-progress!))
  ([project render-progress-fn]
   (let [workspace     (workspace project)]
     (ui/with-progress [render-fn render-progress-fn]
       (write-save-data-to-disk! project {:render-progress! render-fn}))
     (workspace/resource-sync! workspace false [] progress/null-render-progress!))))

(defn build [project node evaluation-context {:keys [render-progress! render-error!]
                                              :or   {render-progress! progress/null-render-progress!}
                                              :as   opts}]
  (let [build-targets (g/node-value node :build-targets evaluation-context)]
    (if (g/error? build-targets)
      (do
        (when render-error!
          (render-error! build-targets))
        nil)
      (pipeline/build! (workspace project) build-targets))))

(handler/defhandler :undo :global
  (enabled? [project-graph] (g/has-undo? project-graph))
  (run [project-graph] (g/undo! project-graph)))

(handler/defhandler :redo :global
  (enabled? [project-graph] (g/has-redo? project-graph))
  (run [project-graph] (g/redo! project-graph)))

(def ^:private bundle-targets
  (into []
        (concat (when (util/is-mac-os?) [[:ios "iOS Application..."]]) ; macOS is required to sign iOS ipa.
                [[:android "Android Application..."]
                 [:macos   "macOS Application..."]
                 [:windows "Windows Application..."]
                 [:linux   "Linux Application..."]
                 [:html5   "HTML5 Application..."]])))

(ui/extend-menu ::menubar :editor.app-view/edit
                [{:label "Project"
                  :id ::project
                  :children (vec (remove nil? [{:label "Build"
                                                :command :build}
                                               {:label "Rebuild"
                                                :command :rebuild}
                                               {:label "Build HTML5"
                                                :command :build-html5}
                                               {:label "Bundle"
                                                :children (mapv (fn [[platform label]]
                                                                  {:label label
                                                                   :command :bundle
                                                                   :user-data {:platform platform}})
                                                                bundle-targets)}
                                               {:label "Fetch Libraries"
                                                :command :fetch-libraries}
                                               {:label "Live Update Settings"
                                                :command :live-update-settings}
                                               {:label "Sign iOS App..."
                                                :command :sign-ios-app}
                                               {:label :separator
                                                :id ::project-end}]))}])

(defn- update-selection [s open-resource-nodes active-resource-node selection-value]
  (->> (assoc s active-resource-node selection-value)
    (filter (comp (set open-resource-nodes) first))
    (into {})))

(defn- perform-selection [project all-selections]
  (let [all-node-ids (->> all-selections
                       vals
                       (reduce into [])
                       distinct
                       vec)]
    (concat
      (g/set-property project :all-selections all-selections)
      (for [[node-id label] (g/sources-of project :all-selected-node-ids)]
        (g/disconnect node-id label project :all-selected-node-ids))
      (for [[node-id label] (g/sources-of project :all-selected-node-properties)]
        (g/disconnect node-id label project :all-selected-node-properties))
      (for [node-id all-node-ids]
        (concat
          (g/connect node-id :_node-id    project :all-selected-node-ids)
          (g/connect node-id :_properties project :all-selected-node-properties))))))

(defn select
  ([project resource-node node-ids open-resource-nodes]
    (assert (every? some? node-ids) "Attempting to select nil values")
    (let [node-ids (if (seq node-ids)
                     (-> node-ids distinct vec)
                     [resource-node])
          all-selections (-> (g/node-value project :all-selections)
                           (update-selection open-resource-nodes resource-node node-ids))]
      (perform-selection project all-selections))))

(defn- perform-sub-selection
  ([project all-sub-selections]
    (g/set-property project :all-sub-selections all-sub-selections)))

(defn sub-select
  ([project resource-node sub-selection open-resource-nodes]
    (g/update-property project :all-sub-selections update-selection open-resource-nodes resource-node sub-selection)))

(defn- remap-selection [m key-m val-fn]
  (reduce (fn [m [old new]]
            (if-let [v (get m old)]
              (-> m
                (dissoc old)
                (assoc new (val-fn [new v])))
              m))
    m key-m))

(defn- make-resource-nodes-by-path-map [nodes]
  (into {} (map (fn [n] [(resource/proj-path (g/node-value n :resource)) n]) nodes)))

(defn- perform-resource-change-plan [plan project render-progress!]
  (binding [*load-cache* (atom (into #{} (g/node-value project :nodes)))]
    (let [old-nodes-by-path (g/node-value project :nodes-by-resource-path)
          resource->old-node (comp old-nodes-by-path resource/proj-path)
          new-nodes (make-nodes! project (:new plan))
          resource-path->new-node (into {} (map (fn [resource-node]
                                                  (let [resource (g/node-value resource-node :resource)]
                                                    [(resource/proj-path resource) resource-node]))
                                                new-nodes))
          resource->new-node (comp resource-path->new-node resource/proj-path)
          ;; when transfering overrides and arcs, the target is either a newly created or already (still!)
          ;; existing node.
          resource->node (fn [resource]
                           (or (resource->new-node resource)
                               (resource->old-node resource)))]

      (g/transact
        (for [[target-resource source-node] (:transfer-overrides plan)]
          (let [target-node (resource->node target-resource)]
            (g/transfer-overrides source-node target-node))))

      (g/transact
        (for [node (:delete plan)]
          (g/delete-node node)))

      (load-nodes! project new-nodes render-progress!)

      (g/transact
        (for [[source-resource output-arcs] (:transfer-outgoing-arcs plan)]
          (let [source-node (resource->node source-resource)
                existing-arcs (set (gu/explicit-outputs source-node))]
            (for [[source-label [target-node target-label]] (remove existing-arcs output-arcs)]
              ;; if (g/node-by-id target-node), the target of the outgoing arc
              ;; has not been deleted above - implying it has not been replaced by a
              ;; new version.
              ;; Otherwise, the target-node has probably been replaced by another version
              ;; (reloaded) and that should have reestablished any incoming arcs to it already
              (if (g/node-by-id target-node)
                (g/connect source-node source-label target-node target-label)
                [])))))

      (g/transact
        (for [[resource-node new-resource] (:redirect plan)]
          (g/set-property resource-node :resource new-resource)))

      (g/transact
        (for [node (:mark-deleted plan)]
          (let [flaw (validation/file-not-found-error node nil :fatal (g/node-value node :resource))]
            (g/mark-defective node flaw))))

      (let [all-outputs (mapcat (fn [node]
                                    (map (fn [[output _]] [node output]) (gu/outputs node)))
                                (:invalidate-outputs plan))]
        (g/invalidate-outputs! all-outputs))

      (let [old->new (into {} (map (fn [[p n]] [(old-nodes-by-path p) n]) resource-path->new-node))]
        (g/transact
          (concat
            (let [all-selections (-> (g/node-value project :all-selections)
                                     (remap-selection old->new (comp vector first)))]
              (perform-selection project all-selections))
            (let [all-sub-selections (-> (g/node-value project :all-sub-selections)
                                         (remap-selection old->new (constantly [])))]
              (perform-sub-selection project all-sub-selections)))))

      ;; invalidating outputs is the only change that does not reset the undo history
      (when (some seq (vals (dissoc plan :invalidate-outputs)))
        (g/reset-undo! (graph project))))))

(defn- handle-resource-changes [project changes render-progress!]
  (-> (resource-update/resource-change-plan (g/node-value project :nodes-by-resource-path) changes)
      ;; for debugging resource loading/reloading issues: (resource-update/print-plan)
      (perform-resource-change-plan project render-progress!)))

(g/defnk produce-collision-groups-data
  [collision-group-nodes]
  (collision-groups/make-collision-groups-data collision-group-nodes))

(defn parse-filter-param
  [_node-id ^String s]
  (cond
    (.equalsIgnoreCase "nearest" s) gl/nearest
    (.equalsIgnoreCase "linear" s) gl/linear
    :else (g/error-fatal (format "Invalid value for filter param: '%s'" s))))

(g/defnk produce-default-tex-params
  [_node-id settings]
  (let [min (parse-filter-param _node-id (get settings ["graphics" "default_texture_min_filter"]))
        mag (parse-filter-param _node-id (get settings ["graphics" "default_texture_mag_filter"]))
        errors (filter g/error? [min mag])]
    (if (seq errors)
      (g/error-aggregate errors)
      {:min-filter min
       :mag-filter mag})))

(g/defnode Project
  (inherits core/Scope)

  (extern workspace g/Any)

  (property all-selections g/Any)
  (property all-sub-selections g/Any)

  (input all-selected-node-ids g/Any :array)
  (input all-selected-node-properties g/Any :array)
  (input resources g/Any)
  (input resource-map g/Any)
  (input resource-types g/Any)
  (input save-data g/Any :array :substitute gu/array-subst-remove-errors)
  (input node-resources resource/Resource :array)
  (input settings g/Any :substitute (constantly (gpc/default-settings)))
  (input display-profiles g/Any)
  (input texture-profiles g/Any)
  (input collision-group-nodes g/Any :array)
  (input build-settings g/Any)

  (output selected-node-ids-by-resource-node g/Any :cached (g/fnk [all-selected-node-ids all-selections]
                                                             (let [selected-node-id-set (set all-selected-node-ids)]
                                                               (->> all-selections
                                                                 (map (fn [[key vals]] [key (filterv selected-node-id-set vals)]))
                                                                 (into {})))))
  (output selected-node-properties-by-resource-node g/Any :cached (g/fnk [all-selected-node-properties all-selections]
                                                                    (let [props (->> all-selected-node-properties
                                                                                  (map (fn [p] [(:node-id p) p]))
                                                                                  (into {}))]
                                                                      (->> all-selections
                                                                        (map (fn [[key vals]] [key (vec (keep props vals))]))
                                                                        (into {})))))
  (output sub-selections-by-resource-node g/Any :cached (g/fnk [all-selected-node-ids all-sub-selections]
                                                               (let [selected-node-id-set (set all-selected-node-ids)]
                                                                 (->> all-sub-selections
                                                                   (map (fn [[key vals]] [key (filterv (comp selected-node-id-set first) vals)]))
                                                                   (into {})))))
  (output resource-map g/Any (gu/passthrough resource-map))
  (output nodes-by-resource-path g/Any :cached (g/fnk [node-resources nodes] (make-resource-nodes-by-path-map nodes)))
  (output save-data g/Any :cached (g/fnk [save-data] (filterv #(and % (:content %)) save-data)))
  (output dirty-save-data g/Any :cached (g/fnk [save-data] (filterv #(and (:dirty? %)
                                                                       (when-let [r (:resource %)]
                                                                         (not (resource/read-only? r)))) save-data)))
  (output settings g/Any :cached (gu/passthrough settings))
  (output display-profiles g/Any :cached (gu/passthrough display-profiles))
  (output texture-profiles g/Any :cached (gu/passthrough texture-profiles))
  (output nil-resource resource/Resource (g/constantly nil))
  (output collision-groups-data g/Any :cached produce-collision-groups-data)
  (output default-tex-params g/Any :cached produce-default-tex-params)
  (output build-settings g/Any (gu/passthrough build-settings)))

(defn get-resource-type [resource-node]
  (when resource-node (resource/resource-type (g/node-value resource-node :resource))))

(defn get-project [node]
  (g/graph-value (g/node-id->graph-id node) :project-id))

(defn find-resources [project query]
  (let [resource-path-to-node (g/node-value project :nodes-by-resource-path)
        resources        (resource/filter-resources (g/node-value project :resources) query)]
    (map (fn [r] [r (get resource-path-to-node (resource/proj-path r))]) resources)))

(defn build-and-write-project [project evaluation-context build-options]
  (let [game-project  (get-resource-node project "/game.project")
        clear-errors! (:clear-errors! build-options)]
    (try
      (ui/with-progress [render-fn ui/default-render-progress!]
        (clear-errors!)
        (not (empty? (build project game-project evaluation-context (assoc build-options :render-progress! render-fn)))))
      (catch Throwable error
        (error-reporting/report-exception! error)
        false))))

(defn settings [project]
  (g/node-value project :settings))

(defn project-dependencies [project]
  (when-let [settings (settings project)]
    (settings ["project" "dependencies"])))

(defn- disconnect-from-inputs [src tgt connections]
  (let [outputs (set (g/output-labels (g/node-type* src)))
        inputs (set (g/input-labels (g/node-type* tgt)))]
    (for [[src-label tgt-label] connections
          :when (and (outputs src-label) (inputs tgt-label))]
      (g/disconnect src src-label tgt tgt-label))))

(defn disconnect-resource-node [project path-or-resource consumer-node connections]
  (let [resource (if (string? path-or-resource)
                   (workspace/resolve-workspace-resource (workspace project) path-or-resource)
                   path-or-resource)
        node (get-resource-node project resource)]
    (disconnect-from-inputs node consumer-node connections)))

(defn connect-resource-node
  ([project path-or-resource consumer-node connections]
    (connect-resource-node project path-or-resource consumer-node connections nil))
  ([project path-or-resource consumer-node connections attach-fn]
    (if-let [resource (if (string? path-or-resource)
                        (workspace/resolve-workspace-resource (workspace project) path-or-resource)
                        path-or-resource)]
      (if-let [node (get-resource-node project resource)]
        (concat
          (if *load-cache*
            (load-node project node (g/node-type* node) resource)
            [])
          (connect-if-output (g/node-type* node) node consumer-node connections)
          (if attach-fn
            (attach-fn node)
            []))
        (make-resource-node (g/node-id->graph-id project) project resource true {project [[:_node-id :nodes]
                                                                                          [:resource :node-resources]]
                                                                                 consumer-node connections}
                            attach-fn))
      [])))

(deftype ProjectResourceListener [project-id]
  resource/ResourceListener
  (handle-changes [this changes render-progress!]
    (handle-resource-changes project-id changes render-progress!)))

(defn make-project [graph workspace-id]
  (let [project-id
        (first
          (g/tx-nodes-added
            (g/transact
              (g/make-nodes graph
                            [project [Project :workspace workspace-id]]
                            (g/connect workspace-id :build-settings project :build-settings)
                            (g/connect workspace-id :resource-list project :resources)
                            (g/connect workspace-id :resource-map project :resource-map)
                            (g/connect workspace-id :resource-types project :resource-types)
                            (g/set-graph-value graph :project-id project)))))]
    (workspace/add-resource-listener! workspace-id (ProjectResourceListener. project-id))
    project-id))

(defn- read-dependencies [game-project-resource]
  (with-open [game-project-reader (io/reader game-project-resource)]
    (-> (settings-core/parse-settings game-project-reader)
        (settings-core/get-setting ["project" "dependencies"])
        (library/parse-library-urls))))

(defn- cache-save-data! [project]
  ;; Save data is required for the Search in Files feature so we pull
  ;; it in the background here to cache it.
  (let [evaluation-context (g/make-evaluation-context)]
    (future
      ;; TODO progress reporting
      (g/node-value project :save-data evaluation-context)
      (ui/run-later (g/update-cache-from-evaluation-context! evaluation-context)))))

(defn open-project! [graph workspace-id game-project-resource render-progress! login-fn]
  (let [progress (atom (progress/make "Updating dependencies..." 3))]
    (render-progress! @progress)
    (workspace/set-project-dependencies! workspace-id (read-dependencies game-project-resource))
    (workspace/update-dependencies! workspace-id (progress/nest-render-progress render-progress! @progress) login-fn)
    (render-progress! (swap! progress progress/advance 1 "Syncing resources"))
    (workspace/resource-sync! workspace-id false [] (progress/nest-render-progress render-progress! @progress))
    (render-progress! (swap! progress progress/advance 1 "Loading project"))
    (let [project (make-project graph workspace-id)
          populated-project (load-project project (g/node-value project :resources) (progress/nest-render-progress render-progress! @progress))]
      (cache-save-data! populated-project)
      populated-project)))

(defn resource-setter [self old-value new-value & connections]
  (let [project (get-project self)]
    (concat
     (when old-value (disconnect-resource-node project old-value self connections))
     (when new-value (connect-resource-node project new-value self connections)))))
