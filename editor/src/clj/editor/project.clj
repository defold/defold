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
            [internal.clojure :as clojure]
            [internal.ui.dialogs :as dialogs]
            [service.log :as log])
  (:import [java.io File]
           [java.nio.file FileSystem FileSystems PathMatcher]))

(g/defnode ResourceNode
  (inherits core/Scope)
  (property resource (t/protocol workspace/Resource) (visible false)))

(g/defnode PlaceholderResourceNode
  (inherits ResourceNode))

(defn graph [project]
  (g/node->graph-id project))

(defn- make-nodes
  [project resources]
  (let [project-graph (graph project)]
    (g/tx-nodes-added
     (g/transact
      (for [[resource-type resources] (group-by workspace/resource-type resources)
            :when    (boolean resource-type)
            :let     [node-type (:node-type resource-type PlaceholderResourceNode)]
            resource resources]
        (g/make-nodes
         project-graph
         [new-resource [node-type :resource resource :parent project :resource-type resource-type]]
         (g/connect new-resource :self project :nodes)))))))

(defn- load-nodes [project nodes]
  (let [new-nodes (g/tx-nodes-added (g/transact
                                       (for [node nodes
                                             :when (get-in node [:resource-type :load-fn])]
                                         ((get-in node [:resource-type :load-fn]) project node (io/reader (:resource node))))))]
    (when (not (empty? new-nodes))
      (load-nodes project new-nodes))))

(defn load-project [project resources]
  (let [nodes (make-nodes project resources)]
    (load-nodes (g/refresh project) nodes)
    (g/refresh project)))

(defn make-embedded-resource [project type data]
  (when-let [resource-type (get (g/node-value project :resource-types) type)]
    (workspace/make-memory-resource (:workspace project) resource-type data)))

(defn nodes-in-project
  "Return a lazy sequence of all nodes in this project. There is no
guaranteed ordering of the sequence."
  [project-node]
  (g/node-value project-node :nodes))

(g/defnk produce-menu [self]
  (let [project-graph (g/node->graph-id self)]
    [{:label "Edit"
      :children [{:label (fn [] (let [label "Undo"]
                                  (if-let [op-label (:label (last (g/undo-stack project-graph)))]
                                    (format "%s %s" label op-label)
                                    label)))
                  :icon "icons/undo.png"
                  :acc "Shortcut+Z"
                  :handler-fn (fn [event] (g/undo project-graph))
                  :enable-fn (fn [] (g/has-undo? project-graph))}
                 {:label (fn [] (let [label "Redo"]
                                  (if-let [op-label (:label (last (g/redo-stack project-graph)))]
                                    (format "%s %s" label op-label)
                                    label)))
                  :icon "icons/redo.png"
                  :acc "Shift+Shortcut+Z"
                  :handler-fn (fn [event] (g/redo project-graph))
                  :enable-fn (fn [] (g/has-redo? project-graph))}]}]))

(g/defnode Project
  (inherits core/Scope)

  (property workspace t/Any)

  (input selection t/Any)
  (input resources t/Any)
  (input resource-types t/Any)

  (output menu t/Any :cached produce-menu))

(defn get-resource-type [resource-node]
  (when resource-node (workspace/resource-type (:resource resource-node))))

(defn filter-resources [resources query]
  (let [file-system ^FileSystem (FileSystems/getDefault)
        matcher (.getPathMatcher file-system (str "glob:" query))]
    (filter (fn [r] (let [path (.getPath file-system (workspace/path r) (into-array String []))] (.matches matcher path))) resources)))

(defn find-resources [project query]
  (let [nodes            (nodes-in-project project)
        resource-to-node (into {} (map (fn [n] [(:resource n) n]) nodes))
        resources        (filter-resources (g/node-value project :resources) query)]
    (map (fn [r] [r (get resource-to-node r)]) resources)))

(defn get-resource-node [project resource]
  (let [nodes (nodes-in-project project)]
    (first (filter (fn [n] (= resource (:resource n))) nodes))))

(defn resolve-resource-node [base-resource-node path]
  (let [project (:parent base-resource-node)
        resource (workspace/resolve-resource (:resource base-resource-node) path)]
    (get-resource-node project resource)))

(g/defnode Selection
  (input selected-nodes [t/Any])

  (output selection t/Any (g/fnk [selected-nodes] selected-nodes)))

(defn select [project nodes]
  (let [selection-node (g/node-value project :selection)]
    (g/transact
      (concat
        (g/operation-label "Select")
        (for [[node label] (g/sources-of (g/now) selection-node :selected-nodes)]
           (g/disconnect node label selection-node :selected-nodes))
        (for [node nodes]
          (g/connect node :self selection-node :selected-nodes))))))

; TODO - actually remove all code below when there is indeed no callers anymore

#_((defn register-node-type
   [filetype node-type]
   (g/update-property (g/current-scope) :node-types assoc filetype node-type))

(defn register-editor
  [project-node filetype editor-builder]
  (g/update-property project-node :handlers assoc-in [:editor filetype] editor-builder))

(defn register-presenter
  [project-node property presenter]
  (g/update-property project-node :presenter-registry dp/register-presenter property presenter))

(defn- editor-for [project-node ext]
  (or
    (get-in project-node [:handlers :editor ext])
    (fn [& _] (throw (ex-info (str "No editor has been registered that can handle file type " (pr-str ext)) {})))))

(defn node?! [n kind]
  (assert (satisfies? g/Node n) (str kind " functions must return a node. Received " (type n) "."))
  n)

(g/defnode Placeholder
  "A Placeholder node represents a file-based asset that doesn't have any specific
behavior."
  (inherits core/ResourceNode)
  (output content t/Any (g/fnk [] nil))
  (inherits core/OutlineNode)
  (output outline-label t/Str (g/fnk [filename] (t/local-name filename))))

(defn- new-node-for-path
  [project-node path type-if-not-registered]
  (let [type          (get-in project-node [:node-types (t/extension path)] type-if-not-registered)
        project-graph (g/node->graph-id project-node)
        temp          (g/tempid project-graph)]
    [(g/make-node project-graph type :filename path :_id temp)
     (g/connect temp :self project-node :nodes)]))

(defn load-resource
  "Load a resource, usually from file."
  [project-node path]
  (new-node-for-path project-node path Placeholder))

(defn- build-editor-node
  [project-node path content-node view-graph]
  ((editor-for project-node (t/extension path)) project-node content-node view-graph))

(defn make-editor
  [project-node path]
  (let [view-graph   (g/make-graph! :volatility 100)
        content-node (t/lookup project-node path)]
    (build-editor-node project-node path content-node view-graph)))

(defn project-enclosing
  [node]
  (first (g/query (g/now) [[:_id (g/node-id node)] '(output :self) (list 'protocol `ProjectRoot)])))

(defn scope-enclosing
  [node]
  (first (g/query (g/now) [[:_id (g/node-id node)] '(output :self) (list 'protocol `t/NamingContext)])))

(defn nodes-in-project
  "Return a lazy sequence of all nodes in this project. There is no
  guaranteed ordering of the sequence."
  [project-node]
  (g/query (g/now) [[:_id (g/node-id project-node)] '(input :nodes)]))

(defn nodes-with-filename
  "Return a lazy sequence of all nodes in the project that match this
  filename. There is no guaranteed ordering of the sequence."
  [project-node path]
  (g/query (g/now) [[:_id (g/node-id project-node)] '(input :nodes) [:filename path]]))

(defn nodes-with-extensions
  [project-node extensions]
  (let [extensions (into #{} extensions)
        pred       (fn [node] (and (:filename node)
                                   (some #{(t/extension (:filename node))} extensions)))]
    (into #{}
      (filter pred (nodes-in-project project-node)))))

(defn select-resources
  ([project-node extensions]
    (select-resources project-node extensions "Select resource" false))
  ([project-node extensions title]
    (select-resources project-node extensions title false))
  ([project-node extensions title multiselect?]
    (dialogs/resource-selection-dialog title multiselect? (nodes-with-extensions project-node extensions))))

(defprotocol ProjectRoot)

(defn project-root? [node]
  (satisfies? ProjectRoot node))

#_(g/defnode Project
  (inherits core/Scope)

  (property tag                t/Keyword (default :project))
  (property content-root       File)
  (property branch             t/Str)
  (property presenter-registry t/Registry)
  (property node-types         {t/Str t/Symbol})
  (property handlers           {t/Keyword {t/Str t/fn-schema}})
  (property clipboard          t/Any (default (constantly (ref nil))))

  ProjectRoot
  t/NamingContext
  (lookup [this name]
          (let [path (if (instance? dynamo.file.ProjectPath name) name (file/make-project-path this name))]
            (first (g/query (g/now) [[:_id (g/node-id this)] '(input :nodes) [:filename path]]))))

  t/FileContainer
  (node-for-path [this path]
                 (new-node-for-path this path Placeholder))

  (on :destroy
      (g/transact
       (g/delete-node self))))

(defn project-root-node
  "Finds and returns the ProjectRoot node starting from any node."
  [node]
  (when node
    (if (project-root? node)
      node
      (recur (scope-enclosing node)))))

(defn load-resource-nodes
  [project-node resources]
  (let [project-node (g/refresh project-node)]
    (g/transact
     (for [resource resources]
       (load-resource project-node
                      (file/make-project-path project-node resource))))))

(defn- post-load
  [message project-node resource-nodes]
  (doseq [resource-node resource-nodes]
    (log/logging-exceptions
     (str message (:filename resource-node))
     (when (satisfies? g/MessageTarget resource-node)
       (g/process-one-event resource-node {:type :load :project project-node})))))

(defn load-project-and-tools
  [root branch]
  (let [project-node    (some-> (load-project root branch) g/transact g/tx-nodes-added first)
        resources       (group-by clojure/clojure-source? (remove #(.isDirectory ^File %) (file-seq root)))
        clojure-sources (get resources true)
        non-sources     (get resources false)]
    ;; TODO - it's possible for project-node to be nil, if the transaction failed.
    (apply post-load "Compiling"
           (load-resource-nodes project-node clojure-sources))
    (apply post-load "Loading asset from"
           (load-resource-nodes project-node non-sources))
    project-node))

; ---------------------------------------------------------------------------
; Lifecycle, Called during connectToBranch
; ---------------------------------------------------------------------------
(defn open-project
  "Called from com.dynamo.cr.editor.Activator when opening a project. You should not call this function directly."
  [root branch]
  (let [project-node (load-project-and-tools root branch)
        ;listener     (resources/listen-for-change #(update-resources project-node %))
        ]
    #_(resources/listen-for-close project-node listener)))

(g/defnode ProjectRoot
  (inherits core/Scope))

(defn project-graph
  []
  (g/make-graph :volatility 0))

)

; TODO - THE CODE ABOVE IS COMMENTED OUT ON PURPOSE, SEE ABOVE
