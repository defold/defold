(ns editor.project
  "Define the concept of a project, and its Project node type. This namespace bridges between Eclipse's workbench and
ordinary paths."
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.core :as core]
            [editor.handler :as handler]
            [editor.ui :as ui]
            [editor.resource :as resource]
            [editor.workspace :as workspace]
            [editor.outline :as outline]
            [editor.validation :as validation]
            [service.log :as log]
            ; TODO - HACK
            [internal.graph.types :as gt])
  (:import [java.io File InputStream]
           [java.nio.file FileSystem FileSystems PathMatcher]
           [java.lang Process ProcessBuilder]
           [editor.resource FileResource]
           [com.defold.editor Platform]))

(def ^:dynamic *load-cache* nil)

(def ^:private unknown-icon "icons/32/Icons_29-AT-Unkown.png")

(g/defnode ResourceNode
  (inherits core/Scope)
  (inherits outline/OutlineNode)
  (inherits resource/ResourceNode)

  (output save-data g/Any (g/fnk [resource] {:resource resource}))
  (output build-targets g/Any (g/always []))
  (output node-outline outline/OutlineData :cached
    (g/fnk [_node-id resource] (let [rt (resource/resource-type resource)]
                                {:node-id _node-id
                                 :label (or (:label rt) (:ext rt) "unknown")
                                 :icon (or (:icon rt) unknown-icon)}))))

(g/defnode PlaceholderResourceNode
  (inherits ResourceNode))

(defn graph [project]
  (g/node-id->graph-id project))

(defn- load-node [project node-id node-type resource]
  (let [loaded? (and *load-cache* (contains? @*load-cache* node-id))]
    (if-let [load-fn (and resource (not loaded?) (:load-fn (resource/resource-type resource)))]
      (try
        (when *load-cache*
          (swap! *load-cache* conj node-id))
        (concat
         (load-fn project node-id resource)
         (when (instance? FileResource resource)
           (g/connect node-id :save-data project :save-data)))
        (catch java.io.IOException e
          (log/warn :exception e)
          (g/mark-defective node-id node-type (g/error-severe {:type :invalid-content :message (format "The file '%s' could not be loaded." (resource/proj-path resource))}))))
      [])))

(defn- load-nodes! [project node-ids]
  (g/transact
    (for [node-id node-ids
          :when (g/has-output? (g/node-type* node-id) :resource)
          :let [resource (g/node-value node-id :resource)]]
      (load-node project node-id (g/node-type* node-id) resource))))

(defn- connect-if-output [src-type src tgt connections]
  (let [outputs (g/output-labels src-type)]
    (for [[src-label tgt-label] connections
          :when (contains? outputs src-label)]
      (g/connect src src-label tgt tgt-label))))

(defn make-resource-node [graph project resource load? connections]
  (assert resource "resource required to make new node")
  (let [resource-type (resource/resource-type resource)
        found? (some? resource-type)
        node-type (:node-type resource-type PlaceholderResourceNode)]
    (g/make-nodes graph [node [node-type :resource resource]]
      (if (some? resource-type)
        (concat
          (for [[consumer connection-labels] connections]
            (connect-if-output node-type node consumer connection-labels))
          (if load?
            (load-node project node node-type resource)
            []))
        (g/connect node :_node-id project :nodes)))))

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

(defn load-project
  ([project] (load-project project (g/node-value project :resources)))
  ([project resources]
   (with-bindings {#'*load-cache* (atom (into #{} (g/node-value project :nodes)))}
     (let [nodes (make-nodes! project resources)]
       (load-nodes! project nodes)
       project))))

(defn make-embedded-resource [project type data]
  (when-let [resource-type (get (g/node-value project :resource-types) type)]
    (resource/make-memory-resource (g/node-value project :workspace) resource-type data)))

(defn save-data [project]
  (g/node-value project :save-data :skip-validation true))

(defn save-all [project]
  (let [save-data (save-data project)]
    (if-not (g/error? save-data)
      (do
        (doseq [{:keys [resource content]} save-data
                :when (not (resource/read-only? resource))]
          (spit resource content))
        (workspace/resource-sync! (g/node-value project :workspace) false))
      ;; TODO: error message somewhere...
      (println (validation/error-message save-data)))))

(handler/defhandler :save-all :global
    (enabled? [] true)
    (run [project] (save-all project)))

(defn- target-key [target]
  [(:resource (:resource target))
   (:build-fn target)
   (:user-data target)])

(defn- build-target [basis target all-targets build-cache]
  (let [resource (:resource target)
        key (:key target)
        cache (let [cache (get @build-cache resource)] (and (= key (:key cache)) cache))]
    (if cache
     cache
     (let [node (:node-id target)
           dep-resources (into {} (map #(let [resource (:resource %)
                                              key (target-key %)] [resource (:resource (get all-targets key))]) (:deps target)))
           result ((:build-fn target) node basis resource dep-resources (:user-data target))
           result (assoc result :key key)]
       (swap! build-cache assoc resource (assoc result :cached true))
       result))))

(defn targets-by-key [build-targets]
  (into {} (map #(let [key (target-key %)] [key (assoc % :key key)]) build-targets)))

(defn prune-build-cache! [cache build-targets]
  (reset! cache (into {} (filter (fn [[resource result]] (contains? build-targets (:key result))) @cache))))

(defn build [project node]
  (let [basis (g/now)
        build-cache (g/node-value project :build-cache)
        build-targets (targets-by-key (mapcat #(tree-seq (comp boolean :deps) :deps %) (g/node-value node :build-targets)))]
    (prune-build-cache! build-cache build-targets)
    (mapv #(build-target basis (second %) build-targets build-cache) build-targets)))

(defn- prune-fs [files-on-disk built-files]
  (let [files-on-disk (reverse files-on-disk)
        built (set built-files)]
    (doseq [^File file files-on-disk
            :let [dir? (.isDirectory file)
                  empty? (= 0 (count (.listFiles file)))
                  keep? (or (and dir? (not empty?)) (contains? built file))]]
      (when (not keep?)
        (.delete file)))))

(defn prune-fs-build-cache! [cache build-results]
  (let [build-resources (set (map :resource build-results))]
    (reset! cache (into {} (filter (fn [[resource key]] (contains? build-resources resource)) @cache)))))

(defn clear-build-cache [project]
  (reset! (g/node-value project :build-cache) {}))

(defn clear-fs-build-cache [project]
  (reset! (g/node-value project :fs-build-cache) {}))

(defn- pump-engine-output [^InputStream stdout]
  (let [buf (byte-array 1024)]
    (loop []
      (let [n (.read stdout buf)]
        (when (> n -1)
          (print (String. buf 0 n))
          (flush)
          (recur))))))

(defn- launch-engine [launch-dir]
  (let [suffix (.getExeSuffix (Platform/getHostPlatform))
        path   (format "%s/dmengine%s" (System/getProperty "defold.exe.path") suffix)
        pb     (doto (ProcessBuilder. ^java.util.List (list path))
                 (.redirectErrorStream true)
                 (.directory launch-dir))]
    (let [p (.start pb)
          is (.getInputStream p)]
      (.start (Thread. (fn [] (pump-engine-output is)))))))

(defn build-and-write [project node]
  (clear-build-cache project)
  (clear-fs-build-cache project)
  (let [files-on-disk (file-seq (io/file (workspace/build-path (g/node-value project :workspace))))
        build-results (build project node)
        fs-build-cache (g/node-value project :fs-build-cache)]
    (prune-fs files-on-disk (map #(File. (resource/abs-path (:resource %))) build-results))
    (prune-fs-build-cache! fs-build-cache build-results)
    (doseq [result build-results
            :let [{:keys [resource content key]} result
                  abs-path (resource/abs-path resource)
                  mtime (let [f (File. abs-path)]
                          (if (.exists f)
                            (.lastModified f)
                            0))
                  build-key [key mtime]
                  cached? (= (get @fs-build-cache resource) build-key)]]
      (when (not cached?)
        (let [parent (-> (File. (resource/abs-path resource))
                       (.getParentFile))]
          ; Create underlying directories
          (when (not (.exists parent))
            (.mkdirs parent))
          ; Write bytes
          (with-open [out (io/output-stream resource)]
            (.write out ^bytes content))
          (let [f (File. abs-path)]
            (swap! fs-build-cache assoc resource [key (.lastModified f)])))))))

(handler/defhandler :undo :global
    (enabled? [project-graph] (g/has-undo? project-graph))
    (run [project-graph] (g/undo! project-graph)))

(handler/defhandler :redo :global
    (enabled? [project-graph] (g/has-redo? project-graph))
    (run [project-graph] (g/redo! project-graph)))

(ui/extend-menu ::menubar :editor.app-view/open
                [{:label "Save All"
                  :id ::save-all
                  :acc "Shortcut+S"
                  :command :save-all}])

(ui/extend-menu ::menubar :editor.app-view/edit
                [{:label "Project"
                  :id ::project
                  :children [{:label "Build"
                              :acc "Shortcut+B"
                              :command :build}
                             {:label "Fetch Libraries"
                              :command :fetch-libraries}]}])

(defn get-resource-node [project path-or-resource]
  (when-let [resource (cond
                        (string? path-or-resource) (workspace/find-resource (g/node-value project :workspace) path-or-resource)
                        (satisfies? resource/Resource path-or-resource) path-or-resource
                        :else (assert false (str (type path-or-resource) " is neither a path nor a resource")))]
    (let [nodes-by-resource-path (g/node-value project :nodes-by-resource-path)]
      (get nodes-by-resource-path (resource/proj-path resource)))))

(defn- outputs [node]
  (mapv #(do [(second (gt/head %)) (gt/tail %)]) (gt/arcs-by-head (g/now) node)))

(defn- loadable? [resource]
  (not (nil? (:load-fn (resource/resource-type resource)))))

(defn- add-resources [project resources]
  (load-nodes! project (make-nodes! project resources)))

(defn- remove-resources [project resources]
  (let [internal (filter loadable? resources)
        external (filter (complement loadable?) resources)]
    (doseq [resource internal
            :let [resource-node (get-resource-node project resource)]
            :when resource-node]
      (let [current-outputs (outputs resource-node)
            new-node (first (make-nodes! project [resource]))
            new-outputs (set (outputs new-node))
            outputs-to-make (filter #(not (contains? new-outputs %)) current-outputs)]
        (g/transact
          (concat
            (g/mark-defective new-node (g/error-severe {:type :file-not-found :message (format "The file '%s' could not be found." (resource/proj-path resource))}))
            (g/delete-node resource-node)
            (for [[src-label [tgt-node tgt-label]] outputs-to-make]
              (g/connect new-node src-label tgt-node tgt-label))))))
    (doseq [resource external
            :let [resource-node (get-resource-node project resource)]
            :when resource-node]
      (g/invalidate! (mapv #(do [resource-node (first %)]) (outputs resource-node))))))

(defn- move-resources [project moved]
  (g/transact
    (for [[from to] moved
          :let [resource-node (get-resource-node project from)]
          :when resource-node]
      (g/set-property resource-node :resource to))))

(defn- handle-resource-changes [project changes]
  (with-bindings {#'*load-cache* (atom (into #{} (g/node-value project :nodes)))}
    (let [moved (:moved changes)
          all (reduce into [] (vals (select-keys changes [:added :removed :changed])))
          reset-undo? (or (some loadable? all)
                          (not (empty? moved)))
          unknown-changed (filter #(nil? (get-resource-node project %)) (:changed changes))
          to-reload (concat (:changed changes) (doall (filter #(some? (get-resource-node project %)) (:added changes))))
          to-add (filter #(nil? (get-resource-node project %)) (:added changes))]
      ;; Order is important, since move-resources reuses/patches the resource node
      (move-resources project moved)
      (add-resources project to-add)
      (remove-resources project (:removed changes))
      (doseq [resource to-reload
              :let [resource-node (get-resource-node project resource)]
              :when resource-node]
        (let [current-outputs (outputs resource-node)]
          (if (loadable? resource)
            (let [nodes (make-nodes! project [resource])]
              (load-nodes! project nodes)
              (let [new-node (first nodes)
                    new-outputs (set (outputs new-node))
                    outputs-to-make (filter #(not (contains? new-outputs %)) current-outputs)]
                (g/transact
                  (concat
                    (g/delete-node resource-node)
                    (for [[src-label [tgt-node tgt-label]] outputs-to-make]
                      (g/connect new-node src-label tgt-node tgt-label))))))
            (g/invalidate! (mapv #(do [resource-node (first %)]) current-outputs)))))
      (when reset-undo?
        (g/reset-undo! (graph project)))
      (assert (empty? unknown-changed) (format "The following resources were changed but never loaded before: %s"
                                               (clojure.string/join ", " (map resource/proj-path unknown-changed)))))))



(g/defnode Project
  (inherits core/Scope)

  (extern workspace g/Any)

  (property build-cache g/Any)
  (property fs-build-cache g/Any)

  (input selected-node-ids g/Any :array)
  (input selected-nodes g/Any :array)
  (input selected-node-properties g/Any :array)
  (input resources g/Any)
  (input resource-types g/Any)
  (input save-data g/Any :array)
  (input node-resources g/Any :array)
  (input settings g/Any)

  (output selected-node-ids g/Any :cached (g/fnk [selected-node-ids] selected-node-ids))
  (output selected-nodes g/Any :cached (g/fnk [selected-nodes] selected-nodes))
  (output selected-node-properties g/Any :cached (g/fnk [selected-node-properties] selected-node-properties))
  (output nodes-by-resource-path g/Any :cached (g/fnk [node-resources nodes] (into {} (map (fn [n] [(resource/proj-path (g/node-value n :resource)) n]) nodes))))
  (output save-data g/Any :cached (g/fnk [save-data] (filter #(and % (:content %)) save-data)))
  (output settings g/Any :cached (g/fnk [settings] settings)))

(defn get-resource-type [resource-node]
  (when resource-node (resource/resource-type (g/node-value resource-node :resource))))

(defn get-project [resource-node]
  (g/graph-value (g/node-id->graph-id resource-node) :project-id))

(defn filter-resources [resources query]
  (let [file-system ^FileSystem (FileSystems/getDefault)
        matcher (.getPathMatcher file-system (str "glob:" query))]
    (filter (fn [r] (let [path (.getPath file-system (resource/path r) (into-array String []))] (.matches matcher path))) resources)))

(defn find-resources [project query]
  (let [resource-path-to-node (g/node-value project :nodes-by-resource-path)
        resources        (filter-resources (g/node-value project :resources) query)]
    (map (fn [r] [r (get resource-path-to-node (resource/proj-path r))]) resources)))

(handler/defhandler :build :global
    (enabled? [] true)
    (run [project] (let [workspace (g/node-value project :workspace)
                         game-project (get-resource-node project "/game.project")
                         launch-path (workspace/project-path (g/node-value project :workspace))]
                     (build-and-write project game-project)
                     (launch-engine (io/file launch-path)))))

(defn workspace [project]
  (g/node-value project :workspace))

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

(defn connect-resource-node [project path-or-resource consumer-node connections]
  (if-let [resource (if (string? path-or-resource)
                      (workspace/resolve-workspace-resource (workspace project) path-or-resource)
                      path-or-resource)]
    (if-let [node (get-resource-node project resource)]
      (concat
        (if *load-cache*
          (load-node project node (g/node-type* node) resource)
          [])
        (connect-if-output (g/node-type* node) node consumer-node connections))
      (make-resource-node (g/node-id->graph-id project) project resource true {project [[:_node-id :nodes]
                                                                                        [:resource :node-resources]]
                                                                               consumer-node connections}))
    []))

(defn select
  [project-id node-ids]
    (let [nil-node-ids (filter nil? node-ids)
          node-ids (if (not (empty? nil-node-ids))
                     (filter some? node-ids)
                     node-ids)]
      (assert (empty? nil-node-ids) "Attempting to select nil values")
      (concat
        (for [[node-id label] (g/sources-of project-id :selected-node-ids)]
          (g/disconnect node-id label project-id :selected-node-ids))
        (for [[node-id label] (g/sources-of project-id :selected-nodes)]
          (g/disconnect node-id label project-id :selected-nodes))
        (for [[node-id label] (g/sources-of project-id :selected-node-properties)]
          (g/disconnect node-id label project-id :selected-node-properties))
        (for [node-id node-ids]
          (concat
            (g/connect node-id :_node-id    project-id :selected-node-ids)
            (g/connect node-id :_node-id         project-id :selected-nodes)
            (g/connect node-id :_properties project-id :selected-node-properties))))))

(defn select!
  ([project node-ids]
    (select! project node-ids (gensym)))
  ([project node-ids op-seq]
    (let [old-nodes (g/node-value project :selected-nodes)]
      (when (not= node-ids old-nodes)
        (g/transact
          (concat
            (g/operation-sequence op-seq)
            (g/operation-label "Select")
            (select project node-ids)))))))

(deftype ProjectResourceListener [project-id]
  resource/ResourceListener
  (handle-changes [this changes]
    (handle-resource-changes project-id changes)))

(deftype ProjectSelectionProvider [project-id]
  workspace/SelectionProvider
  (selection [this] (g/node-value project-id :selected-node-ids)))

(defn selection-provider [project-id] (ProjectSelectionProvider. project-id))

(defn make-project [graph workspace-id]
  (let [project-id
        (first
          (g/tx-nodes-added
            (g/transact
              (g/make-nodes graph
                            [project [Project :workspace workspace-id :build-cache (atom {}) :fs-build-cache (atom {})]]
                            (g/connect workspace-id :resource-list project :resources)
                            (g/connect workspace-id :resource-types project :resource-types)
                            (g/set-graph-value graph :project-id project)))))]
    (workspace/add-resource-listener! workspace-id (ProjectResourceListener. project-id))
    project-id))

(defn gen-resource-setter [connections]
  (fn [basis self old-value new-value]
    (let [project (get-project self)]
      (concat
       (when old-value (disconnect-resource-node project old-value self connections))
       (when new-value (connect-resource-node project new-value self connections))))))
