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

(ns editor.attachment
  "Extensible definitions for semantical node attachment lists

  Use register, alias, or define-alternative to configure the lists.
  Use defined?, editable?, and reorderable? to query the status of node lists on
  a given node.
  Use get to list attached nodes.
  Use add, remove, reorder and clear to edit the nodes.

  The node attachment state has the following data shape:

  {node-type {:lists {list-kw {:add {node-type tx-attach-fn}
                               :get get-fn
                               :reorder reorder-fn
                               :read-only? read-only-pred
                               :alias node-type
                               :aliases #{node-type}}}
              :alternative alternative-fn}}

  In the list definition map, only :get is required. Fns:
    tx-attach-fn      fn of parent-node, child-node -> txs
    get-fn            fn of node, evaluation-context -> vector of nodes
    reorder-fn        fn of reordered-nodes -> txs
    read-only-pred    fn of parent-node, evaluation-context -> boolean

  Aliasing allows treating a list as the same as another list. Any list
  definition may have at most one of :alias, :aliases keys, where:
    :alias      refers to a node type that this list definition follows, i.e., a
                link to the \"original\"
    :aliases    a set of all node types that follow this definition, i.e., a
                link to \"copies\"

  Similarly to outline's :alt-outline feature, it's possible to define an
  alternative node to look up lists by defining an alternative-fn (fn of node,
  evaluation-context -> alt-node | nil)"
  (:refer-clojure :exclude [alias remove get])
  (:require [dynamo.graph :as g]
            [editor.workspace :as workspace]
            [internal.graph.types :as gt]
            [util.coll :as coll]))

(defn- assoc-list-definition [state node-type list-kw {:keys [aliases] :as definition}]
  (let [state (assoc-in state [node-type :lists list-kw] definition)]
    (if aliases
      (let [aliased-definition (-> definition
                                   (dissoc :aliases)
                                   (assoc :alias node-type))]
        (reduce #(assoc-in %1 [%2 :lists list-kw] aliased-definition) state aliases))
      state)))

;; SDK api
(defn register
  "Create transaction steps that register a semantical list of child components

  The very first registration has to specify :get. Further registrations may
  extend the definition, for example, by adding additional :add entries

  Args:
    workspace    the workspace node id
    node-type    container graph node type to extend
    list-kw      name of the node component list

  Kv-args (at least one is required):
    :add           a map that defines how new items are added to the list; where
                   keys are child node types used for constructing new nodes,
                   and values are attach functions, i.e. functions of
                   parent-node-id (container) and child-node-id (item) that
                   should return transaction steps that connect the child node
                   into the parent
    :get           a function that defines how to read a list of children, will
                   receive 2 args: parent-node-id (container) and
                   evaluation-context; should return a vector of node ids
    :reorder       a function that defines how to reorder a list of children,
                   will receive 1 arg: reordered-node-ids (vector of item node
                   ids, validated to be the same node ids as those returned by
                   :get); should return transaction steps that set the new order
    :read-only?    a function that checks if an editable container node is
                   read-only, i.e., can't be edited, will receive 2 args:
                   node-id (container) and evaluation-context; should return
                   a boolean"
  [workspace node-type list-kw & {:keys [add get reorder read-only?]}]
  {:pre [(g/node-id? workspace)
         (g/node-type? node-type)
         (simple-keyword? list-kw)
         (or (nil? get) (ifn? get))
         (or (nil? add) (map? add))
         (or (nil? reorder) (ifn? reorder))
         (or (nil? read-only?) (ifn? read-only?))
         (or get add reorder read-only?)]}
  (g/update-property
    workspace
    :node-attachments
    (fn [s]
      (let [definition (-> s
                           (clojure.core/get node-type)
                           :lists
                           list-kw
                           (cond->
                             add (update :add coll/merge add)
                             get (assoc :get get)
                             reorder (assoc :reorder reorder)
                             read-only? (assoc :read-only? read-only?)))]
        ;; The first registration has to provide :get
        ;; Subsequent registrations typically add additional :add fns
        (assert (contains? definition :get))
        (assert (not (contains? definition :alias)) "Cannot modify an alias")
        (assoc-list-definition s node-type list-kw definition)))))

(defn alias
  "Create transaction steps that alias a node type's list as another node-type

  The other node type must define a list with the same name"
  [workspace node-type list-kw alias-node-type]
  {:pre [(not= node-type alias-node-type)]}
  (g/update-property
    workspace
    :node-attachments
    (fn [s]
      (let [definition (-> s (clojure.core/get alias-node-type) :lists list-kw)
            _ (assert definition "Can't alias undefined list")]
        (assert definition "Can't alias undefined list")
        (assert (not (contains? definition :alias)) "Can't alias another alias")
        (assoc-list-definition s alias-node-type list-kw (update definition :aliases coll/conj-set node-type))))))

(defn define-alternative
  "Create transaction steps that define an alternative node to use for editing

  If no list was found on a node type, an alternative node will be looked up and
  checked for list definition

  Args:
    workspace         the workspace to transact to
    node-type         the node type that defines an alternative
    alternative-fn    a function that looks up an alternative node id if no
                      matching list was found on the input node; will receive 2
                      args: node id (instance of provided node-type) and
                      evaluation-context, should return an alternative node-id
                      or nil"
  [workspace node-type alternative-fn]
  (g/update-property workspace :node-attachments #(update % node-type assoc :alternative alternative-fn)))

(defn find-alternative
  "Returns first truthy value given an alternative chain

  Args:
    workspace             the workspace that defines node alternatives
    node-id               initial node id
    pred                  predicate fn, will receive 1 arg: node id
    evaluation-context    the evaluation context"
  [workspace node-id pred {:keys [basis] :as evaluation-context}]
  (let [current-state (workspace/node-attachments basis workspace)]
    (loop [node-id node-id]
      (or (pred node-id)
          (when-let [alternative-fn (:alternative (clojure.core/get current-state (g/node-type* basis node-id)))]
            (some-> (alternative-fn node-id evaluation-context) recur))))))

(defn- get-list-definition
  "Internal. Returns either a tuple of node-id + list definition map or nil if
  it does not exist"
  [workspace node-id list-kw {:keys [basis] :as evaluation-context}]
  (let [current-state (workspace/node-attachments basis workspace)]
    (find-alternative
      workspace node-id
      #(some->> (list-kw (:lists (clojure.core/get current-state (g/node-type* basis %)))) (coll/pair %))
      evaluation-context)))

(defn- require-list-definition
  [workspace node-id list-kw evaluation-context]
  (let [list-definition (get-list-definition workspace node-id list-kw evaluation-context)]
    (assert list-definition (str list-kw " is not defined"))
    list-definition))

(defn defined?
  "Checks if a node-type is extended to define a list-kw list"
  [workspace node-id list-kw evaluation-context]
  (some? (get-list-definition workspace node-id list-kw evaluation-context)))

(defn- list-definition-editable? [list-definition node-id evaluation-context]
  (and (contains? list-definition :add)
       (if-let [pred (:read-only? list-definition)]
         (not (pred node-id evaluation-context))
         true)))

(defn editable?
  "Checks if a node list is editable

  Asserts that list exists (see [[defined?]])"
  [workspace node-id list-kw evaluation-context]
  (let [[node-id list-definition] (require-list-definition workspace node-id list-kw evaluation-context)]
    (list-definition-editable? list-definition node-id evaluation-context)))

(defn- list-definition-reorderable? [list-definition]
  (contains? list-definition :reorder))

(defn reorderable?
  "Checks if a node type allows reordering of a list-kw list

  Asserts that lists exists (see [[defined?]])"
  [workspace node-id list-kw evaluation-context]
  (let [[_ list-definition] (require-list-definition workspace node-id list-kw evaluation-context)]
    (list-definition-reorderable? list-definition)))

(defn list-node-id
  "Returns the real node id associated with the list

  The returned node id might be different from the input node id when the input
  node id defines an alternative

  Asserts that the list exists (see [[defined?]])"
  [workspace node-id list-kw evaluation-context]
  (let [[node-id _] (require-list-definition workspace node-id list-kw evaluation-context)]
    node-id))

(defn child-node-types
  "Returns all possible child node types for a list, or nil if there is none

  Returned value is a map from possible node type to attachment fn

  Asserts that the list exists (see [[defined?]])"
  [workspace node-id list-kw evaluation-context]
  (let [[_ list-definition] (require-list-definition workspace node-id list-kw evaluation-context)]
    (:add list-definition)))

(defn- tx-add [evaluation-context workspace parent-node-id list-kw child-node-type init-fn config-fn]
  (let [[parent-node-id list-definition] (require-list-definition workspace parent-node-id list-kw evaluation-context)]
    (assert (list-definition-editable? list-definition parent-node-id evaluation-context))
    (let [tx-attach-fn (-> list-definition :add (clojure.core/get child-node-type))]
      (assert tx-attach-fn)
      (let [child-node-id (first (g/take-node-ids (g/node-id->graph-id parent-node-id) 1))]
        (concat
          (g/add-node (g/construct child-node-type :_node-id child-node-id))
          (init-fn parent-node-id child-node-id)
          (tx-attach-fn parent-node-id child-node-id)
          (config-fn child-node-id))))))

(defn add
  "Create transaction steps for adding an attachment to a node

  Args:
    workspace    the workspace node id that defines attachments
    node-id      container node id that should be extended
    list-kw      keyword that defines a list on a node-id
    node-type    the node-type of a node to create
    init-fn      a function that initializes the newly created node, will
                 receive 2 args: parent-node-id and child-node-id; should return
                 transaction steps that initialize the node before it's attached
    config-fn    a function that additionally configures the initialized and
                 attached node, will receive child-node-id, should return
                 transaction steps that configure the node (e.g. add more
                 attachments to it)

  Asserts that the list exists and is editable (see [[defined?]], [[editable?]])"
  [workspace node-id list-kw node-type init-fn config-fn]
  (g/expand-ec tx-add workspace node-id list-kw node-type init-fn config-fn))

(defn- list-definition-get [list-definition node-id evaluation-context]
  ((:get list-definition) node-id evaluation-context))

(defn get
  "Returns a vector of attached child node ids

  Returns a function of 2 args: node-id and evaluation-context, that returns a
  vector of child node ids when invoked

  Asserts that list is defined. See [[defined?]]"
  [workspace node-id list-kw evaluation-context]
  (let [[node-id list-definition] (require-list-definition workspace node-id list-kw evaluation-context)]
    (list-definition-get list-definition node-id evaluation-context)))

(defn- clear-tx [{:keys [basis] :as evaluation-context} workspace node-id list-kw]
  (let [[node-id list-definition] (require-list-definition workspace node-id list-kw evaluation-context)]
    (assert (list-definition-editable? list-definition node-id evaluation-context))
    (coll/mapcat
      #(g/delete-node (g/override-root basis %))
      (list-definition-get list-definition node-id evaluation-context))))

(defn clear
  "Create transaction steps for clearing a list of container node-id's list

  The implementation will assert that the container node-id defines an editable
  list (see [[editable?]])"
  [workspace node-id list-kw]
  (g/expand-ec clear-tx workspace node-id list-kw))

(defn- remove-tx [{:keys [basis] :as evaluation-context} workspace node-id list-kw child-node-id]
  (let [[node-id list-definition] (require-list-definition workspace node-id list-kw evaluation-context)]
    (assert (list-definition-editable? list-definition node-id evaluation-context))
    (let [children (list-definition-get list-definition node-id evaluation-context)]
      (assert (coll/some #(= child-node-id %) children))
      (g/delete-node (g/override-root basis child-node-id)))))

(defn remove
  "Create transaction steps for removing a child from container node-id

  The implementation will assert that the child node id actually exists in the
  container as defined by [[get]]. It will also assert that the container
  node-id defines an editable list identified by list-kw (see [[editable?]])"
  [workspace node-id list-kw child-node-id]
  (g/expand-ec remove-tx workspace node-id list-kw child-node-id))

(defn- reorder-tx [evaluation-context workspace node-id list-kw reordered-child-node-ids]
  (let [[node-id list-definition] (require-list-definition workspace node-id list-kw evaluation-context)]
    (assert (list-definition-editable? list-definition node-id evaluation-context))
    (assert (list-definition-reorderable? list-definition))
    (let [children-set (set (list-definition-get list-definition node-id evaluation-context))
          reorder-fn (:reorder list-definition)]
      (assert (every? children-set reordered-child-node-ids))
      (assert (= (count children-set) (count reordered-child-node-ids))) ;; no duplicates
      (reorder-fn reordered-child-node-ids))))

(defn reorder
  "Create transaction steps for reordering a list of children

  The implementation will assert that the supplied child node ids are the same
  node ids as defined by [[getter]]. It will also assert that the container
  node-id defines an editable and reorderable list (see [[reorderable?]],
  [[editable?]])"
  [workspace node-id list-kw reordered-child-node-ids]
  (g/expand-ec reorder-tx workspace node-id list-kw reordered-child-node-ids))

(defn nodes-by-type-getter
  "Create a node list getter using a type filter over the :nodes output of a
  non-override node

  Returns a function suitable for use as a :get parameter to [[register]]"
  [child-node-type]
  (fn get-nodes-by-type [node evaluation-context]
    (let [basis (:basis evaluation-context)]
      (coll/transfer (g/explicit-arcs-by-target basis node :nodes) []
        (map gt/source-id)
        (filter #(= child-node-type (g/node-type* basis %)))))))

;; SDK api
(defn nodes-getter
  "Node list getter that returns its :nodes output

  This function is suitable for use as a :get parameter to [[register]]"
  [node evaluation-context]
  (let [basis (:basis evaluation-context)]
    (if (g/override? basis node)
      (g/node-value node :nodes evaluation-context)
      (mapv gt/source-id (g/explicit-arcs-by-target basis node :nodes)))))
