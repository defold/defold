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
            ; TODO - HACK
            [internal.graph.types :as gt])
  (:import [java.io File]
           [java.nio.file FileSystem FileSystems PathMatcher]))

(g/defnode ResourceNode
  (inherits core/Scope)

  (property resource (g/protocol workspace/Resource) (dynamic visible (g/always false)))
  (property project-id g/NodeID (dynamic visible (g/always false)))

  (output save-data g/Any (g/fnk [resource] {:resource resource}))
  (output build-targets g/Any (g/always []))
  (output outline g/Any :cached (g/fnk [_node-id resource] (let [rt (resource/resource-type resource)]
                                                            {:node-id _node-id
                                                             :label (or (:label rt) (:ext rt))
                                                             :icon (:icon rt)}))))

(g/defnode PlaceholderResourceNode
  (inherits ResourceNode))

(defn graph [project]
  (g/node->graph-id project))

(defn- make-nodes [project resources]
  (let [project-graph (graph project)]
    (g/tx-nodes-added
      (g/transact
        (for [[resource-type resources] (group-by workspace/resource-type resources)
              :let     [node-type (:node-type resource-type PlaceholderResourceNode)]
              resource resources]
          (if (not= (workspace/source-type resource) :folder)
            (g/make-nodes
              project-graph
              [new-resource [node-type :resource resource :project-id (g/node-id project)]]
              (g/connect new-resource :_self (g/node-id project) :nodes)
              (if ((g/output-labels node-type) :save-data)
                (g/connect new-resource :save-data (g/node-id project) :save-data)
                []))
            []))))))

(defn- load-nodes [project nodes]
  (let [all-nodes-tx (for [node nodes
                           :let [resource (:resource node)
                                 load-fn (and resource (:load-fn (resource/resource-type resource)))]
                           :when load-fn]
                       (load-fn project node (io/reader resource)))
        new-nodes (g/tx-nodes-added (g/transact all-nodes-tx))]
    (when (not (empty? new-nodes))
      (recur project new-nodes))))

(defn load-project
  ([project] (load-project project (g/node-value project :resources)))
  ([project resources]
   (let [nodes (make-nodes project resources)]
     (load-nodes (g/refresh project) nodes)
     (g/refresh project))))

(defn make-embedded-resource [project type data]
  (when-let [resource-type (get (g/node-value project :resource-types) type)]
    (workspace/make-memory-resource (:workspace project) resource-type data)))

(defn save-all [project]
  (let [save-data (g/node-value project :save-data)]
    (doseq [{:keys [resource content]} save-data
            :when (not (workspace/read-only? resource))]
      (spit resource content)))
  (workspace/fs-sync (:workspace project) false))

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
     (let [node (g/node-by-id basis (:node-id target))
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
        build-cache (:build-cache project)
        build-targets (targets-by-key (mapcat #(tree-seq (comp boolean :deps) :deps %) (g/node-value node :build-targets)))]
    (prune-build-cache! build-cache build-targets)
    (mapv #(build-target basis (second %) build-targets build-cache) build-targets)))

(defn- prune-fs [files-on-disk built-files]
  (let [files-on-disk (reverse files-on-disk)
        built (set built-files)]
    (doseq [file files-on-disk
            :let [dir? (.isDirectory file)
                  empty? (= 0 (count (.listFiles file)))
                  keep? (or (and dir? (not empty?)) (contains? built file))]]
      (when (not keep?)
        (.delete file)))))

(defn prune-fs-build-cache! [cache build-results]
  (let [build-resources (set (map :resource build-results))]
    (reset! cache (into {} (filter (fn [[resource key]] (contains? build-resources resource)) @cache)))))

(defn clear-build-cache [project]
  (reset! (:build-cache project) {}))

(defn clear-fs-build-cache [project]
  (reset! (:fs-build-cache project) {}))

(defn build-and-write [project node]
  (clear-build-cache project)
  (clear-fs-build-cache project)
  (let [files-on-disk (file-seq (io/file (workspace/build-path (:workspace project))))
        build-results (build project node)
        fs-build-cache (:fs-build-cache project)]
    (prune-fs files-on-disk (map #(File. (workspace/abs-path (:resource %))) build-results))
    (prune-fs-build-cache! fs-build-cache build-results)
    (doseq [result build-results
            :let [{:keys [resource content key]} result
                  abs-path (workspace/abs-path resource)
                  mtime (let [f (File. abs-path)]
                          (if (.exists f)
                            (.lastModified f)
                            0))
                  build-key [key mtime]
                  cached? (= (get @fs-build-cache resource) build-key)]]
      (when (not cached?)
        (let [parent (-> (File. ^String (workspace/abs-path resource))
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
                             {:label "Save All"
                              :acc "Shortcut+S"
                              :command :save-all}]}])

(defn get-resource-node [project path-or-resource]
  (let [resource (if (string? path-or-resource)
                   (workspace/find-resource (:workspace project) path-or-resource)
                   path-or-resource)]
    (let [nodes-by-resource (g/node-value project :nodes-by-resource)]
      (get nodes-by-resource resource))))

(defn- outputs [node]
  (mapv #(do [(second (gt/head %)) (gt/tail %)]) (gt/arcs-by-head (g/now) (g/node-id node))))

(defn- loadable? [resource]
  (not (nil? (:load-fn (workspace/resource-type resource)))))

(defn- add-resources [project resources]
  (let [resources (filter loadable? resources)
        nodes (make-nodes project resources)]
   (load-nodes (g/refresh project) nodes)))

(defn- remove-resources [project resources]
  (let [internal (filter loadable? resources)
        external (filter (complement loadable?) resources)]
    (g/transact
      (for [resource internal
            :let [node-id (g/node-id (get-resource-node project resource))]]
        ; TODO - recreate a polluted node
        (g/delete-node node-id)))
    (doseq [resource external
            :let [resource-node (get-resource-node project resource)
                  nid (g/node-id resource-node)]]
      (g/invalidate! (mapv #(do [nid (first %)]) (outputs resource-node))))))

(defn- handle-resource-changes [project changes]
  (let [all (reduce into [] (vals changes))
        reset-undo? (reduce #(or %1 %2) false (map (fn [resource] (loadable? resource)) all))]
    (add-resources project (:added changes))
    (remove-resources project (:removed changes))
    (doseq [resource (:changed changes)
            :let [resource-node (get-resource-node project resource)]
            :when resource-node]
      (let [current-outputs (outputs resource-node)]
        (if (loadable? resource)
          (let [current-outputs (outputs resource-node)
                nodes (make-nodes project [resource])]
            (load-nodes (g/refresh project) nodes)
            (let [new-node (first nodes)
                  new-outputs (set (outputs new-node))
                  outputs-to-make (filter #(not (contains? new-outputs %)) current-outputs)]
              (g/transact
                (concat
                  (g/delete-node (g/node-id resource-node))
                  (for [[src-label [tgt-node tgt-label]] outputs-to-make]
                    (g/connect (g/node-id new-node) src-label tgt-node tgt-label))))))
          (let [nid (g/node-id resource-node)]
            (g/invalidate! (mapv #(do [nid (first %)]) current-outputs))))))
    (when reset-undo?
      (g/reset-undo! (g/node->graph-id project)))))

(g/defnode Project
  (inherits core/Scope)

  (property workspace g/Any)
  (property build-cache g/Any)
  (property fs-build-cache g/Any)

  (input selected-node-ids g/Any :array)
  (input selected-nodes g/Any :array)
  (input selected-node-properties g/Any :array)
  (input resources g/Any)
  (input resource-types g/Any)
  (input save-data g/Any :array)

  (output selected-node-ids g/Any :cached (g/fnk [selected-node-ids] selected-node-ids))
  (output selected-nodes g/Any :cached (g/fnk [selected-nodes] selected-nodes))
  (output selected-node-properties g/Any :cached (g/fnk [selected-node-properties] selected-node-properties))
  (output nodes-by-resource g/Any :cached (g/fnk [nodes] (into {} (map (fn [n] [(:resource n) n]) nodes))))
  (output save-data g/Any :cached (g/fnk [save-data] (filter #(and % (:content %)) save-data)))

  workspace/SelectionProvider
  (selection [this] (g/node-value this :selected-node-ids))
  workspace/ResourceListener
  (handle-changes [this changes]
                  (handle-resource-changes (g/refresh this) changes)))

(defn get-resource-type [resource-node]
  (when resource-node (workspace/resource-type (:resource resource-node))))

(defn get-project [resource-node]
  (g/node-by-id (g/node-value resource-node :project-id)))

(defn filter-resources [resources query]
  (let [file-system ^FileSystem (FileSystems/getDefault)
        matcher (.getPathMatcher file-system (str "glob:" query))]
    (filter (fn [r] (let [path (.getPath file-system (workspace/path r) (into-array String []))] (.matches matcher path))) resources)))

(defn find-resources [project query]
  (let [resource-to-node (g/node-value project :nodes-by-resource)
        resources        (filter-resources (g/node-value project :resources) query)]
    (map (fn [r] [r (get resource-to-node r)]) resources)))

(handler/defhandler :build :global
    (enabled? [] true)
    (run [project] (let [workspace (:workspace project)
                         game-project (get-resource-node project (workspace/file-resource workspace "/game.project"))]
                     (build-and-write project game-project))))

(defn- connect-if-output [src-type src tgt connections]
  (let [outputs (g/output-labels src-type)]
    (for [[src-label tgt-label] connections
          :when (contains? outputs src-label)]
      (g/connect src src-label tgt tgt-label))))

(defn connect-resource-node [project resource consumer-node connections]
  (let [node (get-resource-node project resource)]
    (if node
      (connect-if-output (g/node-type node) (g/node-id node) consumer-node connections)
      (let [resource-type (workspace/resource-type resource)
            node-type (:node-type resource-type PlaceholderResourceNode)]
        (g/make-nodes
          (g/node->graph-id project)
          [new-resource [node-type :resource resource :project-id (g/node-id project)]]
          (g/connect new-resource :_self (g/node-id project) :nodes)
          (if ((g/output-labels node-type) :save-data)
            (g/connect new-resource :save-data (g/node-id project) :save-data)
            [])
          (connect-if-output node-type new-resource consumer-node connections))))))

(defn select
  [project nodes]
    (let [project-id (g/node-id project)]
      (concat
        (for [[node label] (g/sources-of (g/node-id project) :selected-node-ids)]
          (g/disconnect (g/node-id node) label project-id :selected-node-ids))
        (for [[node label] (g/sources-of (g/node-id project) :selected-nodes)]
          (g/disconnect (g/node-id node) label project-id :selected-nodes))
        (for [[node label] (g/sources-of (g/node-id project) :selected-node-properties)]
          (g/disconnect (g/node-id node) label project-id :selected-node-properties))
        (for [node nodes
              :let [node-id (g/node-id node)]]
          (concat
            (g/connect node-id :_node-id project-id :selected-node-ids)
            (g/connect node-id :_self project-id :selected-nodes)
            (g/connect node-id :_properties project-id :selected-node-properties))))))

(defn select!
  ([project nodes]
    (select! project nodes (gensym)))
  ([project nodes op-seq]
    (let [old-nodes (g/node-value project :selected-nodes)]
      (when (not= nodes old-nodes)
        (g/transact
          (concat
            (g/operation-sequence op-seq)
            (g/operation-label "Select")
            (select project nodes)))))))

(defn make-project [graph workspace]
  (let [project
        (first
          (g/tx-nodes-added
            (g/transact
              (g/make-nodes graph
                            [project [Project :workspace workspace :build-cache (atom {}) :fs-build-cache (atom {})]]
                            (g/connect (g/node-id workspace) :resource-list project :resources)
                            (g/connect (g/node-id workspace) :resource-types project :resource-types)))))]
    (workspace/add-resource-listener! workspace project)
    project))

;;; Support for copy&paste, drag&drop
(def resource? (partial g/node-instance? ResourceNode))

(defrecord ResourceReference [path label])

(defn make-reference [node label] (ResourceReference. (workspace/proj-path (:resource node)) label))

(defn resolve-reference [workspace project reference]
  (let [resource      (workspace/resolve-resource project (:path reference))
        resource-node (get-resource-node project resource)]
    [(g/node-id resource-node) (:label reference)]))

(defn copy
  "Copy a fragment of the project graph. This returns a 'fragment' that
   can be pasted back into the graph elsewhere. The fragment will include
   the root nodes passed in as arguments, along with all their inputs.

   Collecting the inputs stops when it reaches a resource node. Those are
   replaced with references. When the fragment is pasted, the copies will
   use the same resource nodes, rather than copying files."
  [root-ids]
  (g/copy root-ids {:continue? (comp not resource?)
                    :write-handlers {ResourceNode make-reference}}))

(defn paste
  "Paste a fragment into the project graph. This returns a map with two keys:
   - :root-node-ids - Contains a list of the new node IDs for the roots of the fragment.
   - :tx-data - Transaction steps that will ultimately build the fragment.

   When this call returns, it has only created the :tx-data for you. You still need
   to wrap it in a call to dynamo.graph/transact.

   This allows you to do things like:
   (g/transact
     [(g/operation-label \"paste sprite\")
  (:tx-data (g/paste fragment))])"
  [workspace project fragment]
  (g/paste (g/node-id->graph-id (g/node-id project)) fragment {:read-handlers {ResourceReference (partial resolve-reference workspace project)}}))

(defn workspace [project]
  (:workspace project))
