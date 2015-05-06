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

  (property resource (t/protocol workspace/Resource) (visible false))

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
              [new-resource [node-type :resource resource :parent project :resource-type resource-type]]
              (g/connect new-resource :self project :nodes)
              (if ((g/outputs' node-type) :save-data)
                (g/connect new-resource :save-data project :save-data)
                []))
            []))))))

(defn- load-nodes [project nodes]
  (let [new-nodes (g/tx-nodes-added (g/transact
                                       (for [node nodes
                                             :when (get-in node [:resource-type :load-fn])]
                                         ((get-in node [:resource-type :load-fn]) project node (io/reader (:resource node))))))]
    (when (not (empty? new-nodes))
      (load-nodes project new-nodes))))

(defn load-project [project]
  (let [nodes (make-nodes project (g/node-value project :resources))]
    (load-nodes (g/refresh project) nodes)
    (g/refresh project)))

(defn make-embedded-resource [project type data]
  (when-let [resource-type (get (g/node-value project :resource-types) type)]
    (workspace/make-memory-resource (:workspace project) resource-type data)))

(handler/defhandler :save-all
    (enabled? [] true)
    (run [project] (let [save-data (g/node-value project :save-data)]
                     (doseq [{:keys [resource content]} save-data]
                       (spit resource content)))))


(handler/defhandler :undo
    (enabled? [project-graph] (g/has-undo? project-graph))
    (run [project-graph] (g/undo project-graph)))

(handler/defhandler :redo
    (enabled? [project-graph] (g/has-redo? project-graph))
    (run [project-graph] (g/redo project-graph)))

(ui/extend-menu ::menubar :editor.app-view/new
                [{:label "Save All"
                  :acc "Shortcut+S"
                  :command :save-all}])

(ui/extend-menu ::menubar :editor.app-view/file
                [{:label "Edit"
                  :children [{:label "Undo"
                              :acc "Shortcut+Z"
                              :icon "icons/undo.png"
                              :command :undo}
                             {:label "Redo"
                              :acc "Shift+Shortcut+Z"
                              :icon "icons/redo.png"
                              :command :redo}]}])

(g/defnode Project
  (inherits core/Scope)

  (property workspace t/Any)

  (input selection t/Any :array)
  (input resources t/Any)
  (input resource-types t/Any)
  (input save-data t/Any :array)

  (output selection t/Any :cached (g/fnk [selection] selection))
  (output nodes-by-resource t/Any :cached (g/fnk [nodes] (into {} (map (fn [n] [(:resource n) n]) nodes))))
  (output save-data t/Any :cached (g/fnk [save-data] (filter #(and % (:content %)) save-data))))

(defn get-resource-type [resource-node]
  (when resource-node (workspace/resource-type (:resource resource-node))))

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
  (let [project (:parent base-resource-node)
        resource (workspace/resolve-resource (:resource base-resource-node) path)]
    (get-resource-node project resource)))

(defn select
  ([project nodes]
    (select project nodes (gensym)))
  ([project nodes op-seq]
    (g/transact
      (concat
        (g/operation-sequence op-seq)
        (g/operation-label "Select")
        (for [[node label] (g/sources-of (g/now) project :selection)]
           (g/disconnect node label project :selection))
        (for [node nodes]
          (g/connect node :self project :selection))))))

(defn make-project [graph workspace]
  (first
    (g/tx-nodes-added
      (g/transact
        (g/make-nodes graph
                      [project [Project :workspace workspace]]
                      (g/connect workspace :resource-list project :resources)
                      (g/connect workspace :resource-types project :resource-types))))))
