(ns editor.project
  "Define the concept of a project, and its Project node type. This namespace bridges between Eclipse's workbench and
ordinary paths."
  (:require [dynamo.file :as file]
            [dynamo.graph :as g]
            [dynamo.node :as n]
            [dynamo.property :as dp]
            [dynamo.selection :as selection]
            [dynamo.system :as ds]
            [dynamo.types :as t]
            [dynamo.util :refer :all]
            [editor.core :as core]
            [internal.clojure :as clojure]
            [internal.ui.dialogs :as dialogs]
            [service.log :as log])
  (:import [java.io File]))

(defn register-node-type
  [project-node filetype node-type]
  (g/update-property project-node :node-types assoc filetype node-type))

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
        project-graph (g/nref->gid (g/node-id project-node))
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
  (let [view-graph   (ds/attach-graph (g/make-graph :volatility 100))
        content-node (t/lookup project-node path)]
    (build-editor-node project-node path content-node view-graph)))

(defn project-enclosing
  [node]
  (first (g/query (ds/now) [[:_id (g/node-id node)] '(output :self) (list 'protocol `ProjectRoot)])))

(defn scope-enclosing
  [node]
  (first (g/query (ds/now) [[:_id (g/node-id node)] '(output :self) (list 'protocol `t/NamingContext)])))

(defn nodes-in-project
  "Return a lazy sequence of all nodes in this project. There is no
  guaranteed ordering of the sequence."
  [project-node]
  (g/query (ds/now) [[:_id (g/node-id project-node)] '(input :nodes)]))

(defn nodes-with-filename
  "Return a lazy sequence of all nodes in the project that match this
  filename. There is no guaranteed ordering of the sequence."
  [project-node path]
  (g/query (ds/now) [[:_id (g/node-id project-node)] '(input :nodes) [:filename path]]))

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

(g/defnode Project
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
            (first (g/query (ds/now) [[:_id (g/node-id this)] '(input :nodes) [:filename path]]))))

  t/FileContainer
  (node-for-path [this path]
                 (new-node-for-path this path Placeholder))

  (on :destroy
      (ds/transact
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
    (ds/transact
     (for [resource resources]
       (load-resource project-node
                      (file/make-project-path project-node resource))))))

(defn load-project
  [graph root branch]
  (first
   (ds/tx-nodes-added
    (ds/transact
     (g/make-node graph Project
                  :content-root root
                  :branch       branch
                  :node-types   {"clj" clojure/ClojureSourceNode})))))

(defn- post-load
  [message project-node resource-nodes]
  (doseq [resource-node resource-nodes]
    (log/logging-exceptions
     (str message (:filename resource-node))
     (when (satisfies? g/MessageTarget resource-node)
       (g/process-one-event resource-node {:type :load :project project-node})))))

(defn load-project-and-tools
  [root branch]
  (let [project-node    (some-> (load-project root branch) ds/transact ds/tx-nodes-added first)
        resources       (group-by clojure/clojure-source? (remove #(.isDirectory ^File %) (file-seq root)))
        clojure-sources (get resources true)
        non-sources     (get resources false)]
    ;; TODO - it's possible for project-node to be nil, if the transaction failed.
    (apply post-load "Compiling"
           (load-resource-nodes project-node clojure-sources))
    (apply post-load "Loading asset from"
           (load-resource-nodes project-node non-sources))
    project-node))

(defn- unload-nodes
  [nodes]
  (doseq [n nodes]
    (n/dispatch-message n :unload)))

(defn- replace-nodes
  [project-node nodes-to-replace f]
  (ds/transact
   (for [old nodes-to-replace]
     (g/become (f old))))
  )

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
    (apply post-load "Compiling"          (load-resource-nodes (g/refresh project-node) (:load-clojure with-placeholders) nil))
    (apply post-load "Loading asset from" (load-resource-nodes (g/refresh project-node) (:load-other   with-placeholders) nil))
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
  (-> (g/refresh project-node)
    (update-added-resources changeset)
    (update-deleted-resources changeset)
    (update-changed-resources changeset)))

(defn open-project
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
