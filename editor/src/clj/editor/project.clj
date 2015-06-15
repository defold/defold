(ns editor.project
  "Define the concept of a project, and its Project node type. This namespace bridges between Eclipse's workbench and
ordinary paths."
  (:require [clojure.java.io :as io]
            [dynamo.file :as file]
            [dynamo.graph :as g]
            [dynamo.property :as dp]
            [dynamo.types :as t]
            [dynamo.util :refer :all]
            [editor.core :as core]
            [editor.workspace :as workspace]
            [editor.handler :as handler]
            [editor.ui :as ui]
            [internal.clojure :as clojure]
            [internal.ui.dialogs :as dialogs]
            [service.log :as log])
  (:import [java.io File]
           [java.nio.file FileSystem FileSystems PathMatcher]))

(g/defnode ResourceNode
  (inherits core/Scope)

  (property resource (t/protocol workspace/Resource) (visible (g/fnk [] false)))
  (property project t/Any (visible (g/fnk [] false)))

  (output save-data t/Any (g/fnk [resource] {:resource resource})))

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
              [new-resource [node-type :resource resource :project-id (g/node-id project) :resource-type resource-type]]
              (g/connect new-resource :self project :nodes)
              (if ((g/outputs' node-type) :save-data)
                (g/connect new-resource :save-data project :save-data)
                []))
            []))))))

(defn- load-nodes [project nodes]
  (let [new-nodes (g/tx-nodes-added (g/transact
                                     (for [node nodes
                                           :let [load-fn (get-in node [:resource-type :load-fn])]
                                           :when load-fn]
                                       (load-fn project node (io/reader (:resource node))))))]
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

(handler/defhandler :save-all :global
    (enabled? [] true)
    (run [project] (let [save-data (g/node-value project :save-data)]
                     (doseq [{:keys [resource content]} save-data]
                       (spit resource content)))))


(handler/defhandler :undo :global
    (enabled? [project-graph] (g/has-undo? project-graph))
    (run [project-graph] (g/undo project-graph)))

(handler/defhandler :redo :global
    (enabled? [project-graph] (g/has-redo? project-graph))
    (run [project-graph] (g/redo project-graph)))

(ui/extend-menu ::menubar :editor.app-view/open
                [{:label "Save All"
                  :acc "Shortcut+S"
                  :command :save-all}])

(g/defnode Project
  (inherits core/Scope)

  (property workspace t/Any)

  (input selected-node-ids t/Any :array)
  (input selected-nodes t/Any :array)
  (input resources t/Any)
  (input resource-types t/Any)
  (input save-data t/Any :array)

  (output selected-node-ids t/Any :cached (g/fnk [selected-node-ids] selected-node-ids))
  (output selected-nodes t/Any :cached (g/fnk [selected-nodes] selected-nodes))
  (output nodes-by-resource t/Any :cached (g/fnk [nodes] (into {} (map (fn [n] [(:resource n) n]) nodes))))
  (output save-data t/Any :cached (g/fnk [save-data] (filter #(and % (:content %)) save-data)))

  workspace/SelectionProvider
  (selection [this] (g/node-value this :selected-node-ids)))

(defn get-resource-type [resource-node]
  (when resource-node (workspace/resource-type (:resource resource-node))))

(defn get-project [resource-node]
  (g/node-by-id (:project-id resource-node)))

(defn filter-resources [resources query]
  (let [file-system ^FileSystem (FileSystems/getDefault)
        matcher (.getPathMatcher file-system (str "glob:" query))]
    (filter (fn [r] (let [path (.getPath file-system (workspace/path r) (into-array String []))] (.matches matcher path))) resources)))

(defn find-resources [project query]
  (let [resource-to-node (g/node-value project :nodes-by-resource)
        resources        (filter-resources (g/node-value project :resources) query)]
    (map (fn [r] [r (get resource-to-node r)]) resources)))

(defn get-resource-node [project resource]
  (let [nodes-by-resource (g/node-value project :nodes-by-resource)]
    (get nodes-by-resource resource)))

(defn resolve-resource-node [base-resource-node path]
  (let [project (get-project base-resource-node)
        resource (workspace/resolve-resource (:resource base-resource-node) path)]
    (get-resource-node project resource)))

(defn select
  [project nodes]
    (concat
      (for [[node label] (g/sources-of project :selected-node-ids)]
        (g/disconnect node label project :selected-node-ids))
      (for [[node label] (g/sources-of project :selected-nodes)]
        (g/disconnect node label project :selected-nodes))
      (for [node nodes]
        (concat
          (g/connect node :node-id project :selected-node-ids)
          (g/connect node :self project :selected-nodes)))))

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
  (first
    (g/tx-nodes-added
      (g/transact
        (g/make-nodes graph
                      [project [Project :workspace workspace]]
                      (g/connect workspace :resource-list project :resources)
                      (g/connect workspace :resource-types project :resource-types))))))
