;; Copyright 2020-2025 The Defold Foundation
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
  "Extensible definitions for semantical node attachments"
  (:refer-clojure :exclude [remove alias])
  (:require [dynamo.graph :as g]
            [editor.workspace :as workspace]
            [internal.graph.types :as gt]
            [util.coll :as coll]))

(defn- assoc-list-definition [state node-type list-kw {:keys [aliases] :as definition}]
  (let [state (update state node-type assoc list-kw definition)]
    (if aliases
      (let [aliased-definition (-> definition
                                   (dissoc :aliases)
                                   (assoc :alias node-type))]
        (reduce #(update %1 %2 assoc list-kw aliased-definition) state aliases))
      state)))

;; SDK api
(defn register
  "Create transaction steps that register a semantical list of child components

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
  "Define a node list as an alias of another node type's list with the same name"
  [workspace node-type list-kw alias-node-type]
  {:pre [(not= node-type alias-node-type)]}
  (g/update-property
    workspace
    :node-attachments
    (fn [s]
      (let [definition (list-kw (clojure.core/get s alias-node-type))
            _ (assert definition "Can't alias undefined list")]
        (assert definition "Can't alias undefined list")
        (assert (not (contains? definition :alias)) "Can't alias another alias")
        (assoc-list-definition s alias-node-type list-kw (update definition :aliases coll/conj-set node-type))))))

(defmacro ^:private with-read-only-check [parent-node-type-expr parent-node-id-expr list-kw-expr read-only?-expr & body]
  `(let [parent-node-id# ~parent-node-id-expr
         read-only?# ~read-only?-expr
         parent-node-type# ~parent-node-type-expr
         list-kw# ~list-kw-expr]
     (if read-only?#
       (g/expand-ec
         (fn [evaluation-context#]
           (when (read-only?# parent-node-id# evaluation-context#)
             (throw (ex-info (str (name (:k parent-node-type#)) " instance is read-only")
                             {:parent-node-id parent-node-id# :list-kw list-kw#})))
           (do ~@body)))
       (do ~@body))))

(defn add-impl [current-state parent-node-type parent-node-id attachment-tree init-fn]
  (coll/mapcat
    (fn [[list-kw {:keys [init add node-type] :as attachment}]]
      (when-not node-type
        (throw (ex-info "node type is required" {:parent-node-type parent-node-id :list-kw list-kw :attachment attachment})))
      (let [definition (-> current-state (get parent-node-type) list-kw)]
        (if-let [node-type->tx-attach-fn (:add definition)]
          (let [tx-attach-fn (or (node-type->tx-attach-fn node-type)
                                 (throw (ex-info (str (name (:k parent-node-type)) " does not support " (name list-kw) " attachments of type " (name (:k node-type)))
                                                 {:parent-node-type parent-node-id :list-kw list-kw :node-type node-type})))
                child-node-id (first (g/take-node-ids (g/node-id->graph-id parent-node-id) 1))]
            (with-read-only-check
              parent-node-type parent-node-id list-kw (:read-only? definition)
              (concat
                (g/add-node (g/construct node-type :_node-id child-node-id))
                (init-fn parent-node-id node-type child-node-id init)
                (tx-attach-fn parent-node-id child-node-id)
                (add-impl current-state node-type child-node-id add init-fn))))
          (throw (ex-info (str (name (:k parent-node-type)) " does not define a " (name list-kw) " attachment list")
                          {:parent-node-type parent-node-type
                           :list-kw list-kw})))))
    attachment-tree))

(defn add
  "Create transaction steps for adding a tree to some node

  Args:
    basis              graph basis to use when looking up node-id's type
    workspace          the workspace node id that defines attachments
    node-id            container node id that should be extended
    attachment-tree    a tree that describes several (potentially recursive)
                       additions; a coll of 2-element tuples where first element
                       is a list-kw keyword, and second is a map with the
                       following keys:
                         :init         required, data structure that will be
                                       supplied to the init-fn to create element
                                       initialization transaction steps
                         :node-type    required, node type of child node
                         :add          optional, attachment tree of the created
                                       item

    init-fn            a function that initializes the newly created element
                       node, will receive 4 args:
                         parent-node-id     container node id
                         child-node-type    created node type
                         child-node-id      created node id
                         init-value         init value from the attachment tree
                       the function should return transaction steps that
                       initialize the node

  Example attachment tree for an atlas node:
    [[:images {:init {:image \"/foo.png\"}}]
     [:images {:init {:image \"/bar.png\"}}]
     [:animations
      {:init {:id \"foo-bar\"}
       :add [[:images {:init {:image \"/foo.png\"}}]
             [:images {:init {:image \"/bar.png\"}}]]}]]"
  [basis workspace node-id attachment-tree init-fn]
  (add-impl (workspace/node-attachments basis workspace) (g/node-type* basis node-id) node-id attachment-tree init-fn))

(defn defines?
  "Checks if a node-type is extended to define a list-kw list"
  [basis workspace node-type list-kw]
  (-> basis (workspace/node-attachments workspace) (get node-type) (contains? list-kw)))

(defn read-only?
  "Checks if a node is read-only, i.e., can't be edited even when editable

  Asserts that list exists (see [[defines?]])"
  [workspace node-id list-kw evaluation-context]
  (let [{:keys [basis]} evaluation-context
        node-type (g/node-type* basis node-id)]
    (assert (defines? basis workspace node-type list-kw))
    (if-let [pred (-> basis (workspace/node-attachments workspace) (get node-type) list-kw :read-only?)]
      (pred node-id evaluation-context)
      false)))

(defn editable?
  "Checks if a node-type supports editing items of the list"
  [basis workspace node-type list-kw]
  (-> basis (workspace/node-attachments workspace) (get node-type) list-kw (contains? :add)))

(defn reorderable?
  "Checks if a node type allows reordering of a list-kw list"
  [basis workspace node-type list-kw]
  (-> basis (workspace/node-attachments workspace) (get node-type) list-kw (contains? :reorder)))

(defn child-node-types
  "Returns defined child node-types for a parent node-type's list-kw

  Returns a map from child node type to tx-attach-fn

  Asserts that it exists. See [[editable?]]"
  [basis workspace node-type list-kw]
  {:post [(some? %)]}
  (-> basis (workspace/node-attachments workspace) (get node-type) list-kw :add))

(defn getter
  "Returns a getter function for a container node-type's list-kw

  Returns a function of 2 args: node-id and evaluation-context, that returns a
  vector of child node ids when invoked

  Asserts that it exists. See [[defines?]]"
  [basis workspace node-type list-kw]
  {:post [(some? %)]}
  (-> basis (workspace/node-attachments workspace) (get node-type) list-kw :get))

(defn- clear-tx [evaluation-context workspace node-id list-kw]
  (let [basis (:basis evaluation-context)
        node-type (g/node-type* basis node-id)
        get-fn (getter basis workspace node-type list-kw)]
    (assert (editable? basis workspace node-type list-kw))
    (assert (not (read-only? workspace node-id list-kw evaluation-context)))
    (mapcat
      #(g/delete-node (g/override-root basis %))
      (get-fn node-id evaluation-context))))

(defn clear
  "Create transaction steps for clearing a list of container node-id's list

  The implementation will assert that the container node-id defines an editable
  list (see [[editable?]]) and is not [[read-only?]]"
  [workspace node-id list-kw]
  (g/expand-ec clear-tx workspace node-id list-kw))

(defn- remove-tx [evaluation-context workspace node-id list-kw child-node-id]
  (let [basis (:basis evaluation-context)
        node-type (g/node-type* basis node-id)
        get-fn (getter basis workspace node-type list-kw)
        children (get-fn node-id evaluation-context)]
    (assert (editable? basis workspace node-type list-kw))
    (assert (not (read-only? workspace node-id list-kw evaluation-context)))
    (assert (some #(= child-node-id %) children))
    (g/delete-node (g/override-root basis child-node-id))))

(defn remove
  "Create transaction steps for removing a child from container node-id

  The implementation will assert that the child node id actually exists in the
  container as defined by [[getter]]. It will also assert that the container
  node-id defines an editable list identified by list-kw (see [[editable?]]) and
  is not [[read-only?]]"
  [workspace node-id list-kw child-node-id]
  (g/expand-ec remove-tx workspace node-id list-kw child-node-id))

(defn- reorder-tx [evaluation-context workspace node-id list-kw reordered-child-node-ids]
  (let [basis (:basis evaluation-context)
        node-type (g/node-type* basis node-id)
        get-fn (getter basis workspace node-type list-kw)
        children-set (set (get-fn node-id evaluation-context))
        reorder-fn (-> basis (workspace/node-attachments workspace) (get node-type) list-kw :reorder)]
    (assert (editable? basis workspace node-type list-kw))
    (assert (not (read-only? workspace node-id list-kw evaluation-context)))
    (assert (every? children-set reordered-child-node-ids))
    (assert (= (count children-set) (count reordered-child-node-ids))) ;; no duplicates
    (assert reorder-fn)
    (reorder-fn reordered-child-node-ids)))

(defn reorder
  "Create transaction steps for reordering a list of children

  The implementation will assert that the supplied child node ids are the same
  node ids as defined by [[getter]]. It will also assert that the container
  node-id defines an editable and reorderable list (see [[reorderable?]],
  [[editable?]]) and is not [[read-only?]]"
  [workspace node-id list-kw reordered-child-node-ids]
  (g/expand-ec reorder-tx workspace node-id list-kw reordered-child-node-ids))

(defn nodes-by-type-getter
  "Create a node list getter using a type filter over the :nodes output

  Returns a function suitable for use as a :get parameter to [[register!]]"
  [child-node-type]
  (fn get-nodes-by-type [node evaluation-context]
    (let [basis (:basis evaluation-context)]
      (coll/transfer (g/explicit-arcs-by-target basis node :nodes) []
        (map gt/source-id)
        (filter #(= child-node-type (g/node-type* basis %)))))))

;; SDK api
(defn nodes-getter
  "node list getter that returns :nodes output

  This function is suitable for use as a :get parameter to [[register]]"
  [node evaluation-context]
  (mapv gt/source-id (g/explicit-arcs-by-target (:basis evaluation-context) node :nodes)))
