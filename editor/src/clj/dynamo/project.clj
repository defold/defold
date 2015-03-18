(ns dynamo.project
  "Define the concept of a project, and its Project node type. This namespace bridges between Eclipse's workbench and
ordinary paths."
  (:require [clojure.java.io :as io]
            [dynamo.file :as file]
            [dynamo.graph :as g]
            [dynamo.node :as n]
            [dynamo.property :as dp]
            [dynamo.selection :as selection]
            [dynamo.system :as ds]
            [dynamo.types :as t]
            [dynamo.ui :as ui]
            [dynamo.util :refer :all]
            [internal.clojure :as clojure]
            [internal.transaction :as it]
            [internal.ui.dialogs :as dialogs]
            [service.log :as log])
  (:import [java.io File]))

(defn register-node-type
  [filetype node-type]
  (g/update-property (ds/current-scope) :node-types assoc filetype node-type))

(defn register-editor
  [filetype editor-builder]
  (g/update-property (ds/current-scope) :handlers assoc-in [:editor filetype] editor-builder))

(defn register-presenter
  [property presenter]
  (g/update-property (ds/current-scope) :presenter-registry dp/register-presenter property presenter))

(defn- editor-for [project-scope ext]
  (or
    (get-in project-scope [:handlers :editor ext])
    (fn [& _] (throw (ex-info (str "No editor has been registered that can handle file type " (pr-str ext)) {})))))

(defn node?! [n kind]
  (assert (satisfies? g/Node n) (str kind " functions must return a node. Received " (type n) "."))
  n)

(g/defnode Placeholder
  "A Placeholder node represents a file-based asset that doesn't have any specific
behavior."
  (inherits g/ResourceNode)
  (output content t/Any (g/fnk [] nil))
  (inherits g/OutlineNode)
  (output outline-label t/Str (g/fnk [filename] (t/local-name filename))))

(defn- new-node-for-path
  [project-node path type-if-not-registered]
  (n/construct
    (get-in project-node [:node-types (t/extension path)] type-if-not-registered)
    :filename path))

(defn load-resource
  "Load a resource, usually from file. This will create a node of the appropriate type (as defined by
`register-node-type` and send it a :load message."
  [project-node path]
  (g/transactional
    (ds/in project-node
      (g/add
        (new-node-for-path project-node path Placeholder)))))

(defn- build-editor-node
  [project-node path content-node]
  (let [editor-factory (editor-for project-node (t/extension path))]
    (node?! (editor-factory project-node content-node) "Editor")))

(defn- build-selection-node
  [editor-node selected-nodes]
  (ds/in editor-node
    (let [selection-node  (g/add (n/construct selection/Selection))]
      (doseq [node selected-nodes]
        (g/connect node :self selection-node :selected-nodes))
      selection-node)))

(defn make-editor
  [project-node path]
  (g/transactional
   (ds/in project-node
          (let [content-node   (t/lookup project-node path)
                editor-node    (build-editor-node project-node path content-node)
                selection-node (build-selection-node editor-node [content-node])]
            (when ((g/inputs editor-node) :presenter-registry)
              (g/connect project-node :presenter-registry editor-node :presenter-registry))
            (when (and ((g/inputs editor-node) :saveable) ((g/outputs content-node) :save))
              (g/connect content-node :save editor-node :saveable))
            (when (and ((g/inputs editor-node) :dirty) ((g/outputs content-node) :dirty))
              (g/connect content-node :dirty editor-node :dirty))
            editor-node))))

(defn- send-project-scope-message
  [txn graph self label kind inputs-affected]
  (mapcat #(it/send-message {:_id %} {:type :project-scope :scope self})
          (when (inputs-affected :nodes)
            (:nodes-added txn))))

(defn project-enclosing
  [node]
  (first (ds/query (:graph @(:world-ref node) [[:_id (:_id node)] '(output :self) (list 'protocol `ProjectRoot)]))))

(defn scope-enclosing
  [node]
  (first (ds/query (:graph @(:world-ref node)) [[:_id (:_id node)] '(output :self) (list 'protocol `t/NamingContext)])))

(defn nodes-in-project
  "Return a lazy sequence of all nodes in this project. There is no
guaranteed ordering of the sequence."
  [project-node]
  (ds/query (:graph @(:world-ref project-node)) [[:_id (:_id project-node)] '(input :nodes)]))

(defn nodes-with-filename
  "Return a lazy sequence of all nodes in the project that match this filename.
There is no guaranteed ordering of the sequence."
  [project-node path]
  (ds/query (:graph @(:world-ref project-node)) [[:_id (:_id project-node)] '(input :nodes) [:filename path]]))

(defn nodes-with-extensions
  [project-node extensions]
  (let [extensions (into #{} extensions)
        pred (fn [node] (and (:filename node)
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

(g/defnode Project
  (inherits g/Scope)

  (trigger notify-content-nodes :input-connections send-project-scope-message)

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
      (first (ds/query (:graph @(:world-ref this)) [[:_id (:_id this)] '(input :nodes) [:filename path]]))))

  t/FileContainer
  (node-for-path [this path]
    (new-node-for-path this path Placeholder))

  (on :destroy
    (g/delete self)))

(defn project-root-node
  "Finds and returns the ProjectRoot node starting from any node."
  [node]
  (when node
    (if (project-root? node)
      node
      (recur (scope-enclosing node)))))

(defn load-resource-nodes
  [project-node resources]
  (g/transactional
   (ds/in project-node
          [project-node
           (doall
            (for [resource resources
                  :let [p (file/make-project-path project-node resource)]]
              (load-resource project-node p)))])))

(defn load-project
  [root branch]
  (g/transactional
   (g/add
    (n/construct Project
                 :content-root root
                 :branch branch
                 :node-types {"clj" clojure/ClojureSourceNode}))))

(defn- post-load
  [message project-node resource-nodes]
  (doseq [resource-node resource-nodes]
    (log/logging-exceptions
     (str message (:filename resource-node))
     (when (satisfies? g/MessageTarget resource-node)
       (ds/in project-node
              (g/process-one-event resource-node {:type :load :project project-node}))))))

(defn load-project-and-tools
  [root branch]
  (let [project-node    (load-project root branch)
        resources       (group-by clojure/clojure-source? (remove #(.isDirectory ^File %) (file-seq root)))
        clojure-sources (get resources true)
        non-sources     (get resources false)]
    (apply post-load "Compiling"
           (load-resource-nodes (ds/refresh project-node) clojure-sources))
    (apply post-load "Loading asset from"
           (load-resource-nodes (ds/refresh project-node) non-sources))
    project-node))

(defn- unload-nodes
  [nodes]
  (g/transactional
    (doseq [n nodes]
      (ds/send-after n {:type :unload}))))

(defn- replace-nodes
  [project-node nodes-to-replace f]
  (g/transactional
    (doseq [old nodes-to-replace
            :let [new (f old)]]
      (ds/become old new)
      (ds/send-after old {:type :load :project project-node}))))

(defn- add-or-replace?
  [project-node resource]
  (let [[node] (nodes-with-filename project-node (file/make-project-path project-node resource))]
    (cond
      (not (nil? node))                   :replace-existing
      (clojure/clojure-source? resource)  :load-clojure
      :else                               :load-other)))

(defn- update-added-resources
  [project-node {:keys [added]}]
  (let [with-placeholders (group-by #(add-or-replace? project-node %) added)
        replacements      (mapcat #(nodes-with-filename project-node (file/make-project-path project-node %)) (:replace-existing with-placeholders))]
    (unload-nodes replacements)
    (replace-nodes project-node replacements #(t/node-for-path project-node (:filename %)))
    (apply post-load "Compiling"          (load-resource-nodes (ds/refresh project-node) (:load-clojure with-placeholders) nil))
    (apply post-load "Loading asset from" (load-resource-nodes (ds/refresh project-node) (:load-other   with-placeholders) nil))
    project-node))

(defn- update-deleted-resources
  [project-node {:keys [deleted]}]
  (let [nodes-to-delete (mapcat #(nodes-with-filename project-node (file/make-project-path project-node %)) deleted)]
    (unload-nodes nodes-to-delete)
    (replace-nodes project-node nodes-to-delete #(new-node-for-path project-node (:filename %) Placeholder)))
  project-node)

(defn- update-changed-resources
  [project-node {:keys [changed]}]
  (let [nodes-to-replace (map #(first (nodes-with-filename project-node (file/make-project-path project-node %))) changed)]
    (unload-nodes nodes-to-replace)
    (replace-nodes project-node nodes-to-replace #(new-node-for-path project-node (:filename %) Placeholder))))

(defn- update-resources
  [project-node changeset]
  (-> (ds/refresh project-node)
    (update-added-resources changeset)
    (update-deleted-resources changeset)
    (update-changed-resources changeset)))

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
