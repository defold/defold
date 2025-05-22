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
  (:refer-clojure :exclude [remove])
  (:require [dynamo.graph :as g]
            [util.coll :as coll]))

(defonce ^:private state-atom
  ;; :add -> type -> list-kw -> type -> {:node-type ... :tx-attach-fn ...}
  ;; :get -> type -> list-kw -> get-fn
  ;;
  ;; tx-attach-fn: fn of parent-node, child-node -> txs
  ;; get-fn: fn of node, evaluation-context -> vector of nodes
  (atom {:add {} :get {}}))

(defn register!
  "Register a semantical list of child components

  Args:
    node-type    container graph node type to extend
    list-kw      name of the node component list

  Kv-args:
    :add    required, a map that defines how new items are added to the list;
            with the following keys:
              :node-type       child node type used for constructing new added
                               items
              :tx-attach-fn    a function of parent-node-id (container),
                               child-node-id (element); should return
                               transaction steps that connect the child node
                               into the parent
    :get    required, a function that defines how to read a list of children,
            will receive 2 args: parent-node-id (container), evaluation-context;
            should return a vector of node ids"
  [node-type list-kw & {:keys [add get]}]
  {:pre [(g/node-type? (:node-type add)) (ifn? (:tx-attach-fn add)) (ifn? get)]}
  (swap! state-atom (fn [s]
                      (-> s
                          (update :add update node-type assoc list-kw add)
                          (update :get update node-type assoc list-kw get))))
  nil)

(defn add-impl [current-state parent-node-type parent-node-id attachment-tree init-fn]
  (coll/mapcat
    (fn [[list-kw {:keys [init add]}]]
      (if-let [{:keys [node-type tx-attach-fn]} (-> current-state :add (get parent-node-type) list-kw)]
        (let [child-node-id (first (g/take-node-ids (g/node-id->graph-id parent-node-id) 1))]
          (concat
            (g/add-node (g/construct node-type :_node-id child-node-id))
            (init-fn parent-node-id node-type child-node-id init)
            (tx-attach-fn parent-node-id child-node-id)
            (add-impl current-state node-type child-node-id add init-fn)))
        (throw (ex-info (str (name (:k parent-node-type)) " does not define a " (name list-kw) " attachment list")
                        {:node-type parent-node-type
                         :list-kw list-kw}))))
    attachment-tree))

(defn add
  "Create transaction steps for adding a tree to some node

  Args:
    basis              graph basis to use when looking up node-id's type
    node-id            container node id that should be extended
    attachment-tree    a tree that describes several (potentially recursive)
                       additions; a coll of 2-element tuples where first element
                       is a list-kw keyword, and second is a map with the
                       following keys:
                         :init    required, data structure that will be supplied
                                  to the init-fn to create element
                                  initialization transaction steps
                         :add     optional, attachment tree of the created item
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
  [basis node-id attachment-tree init-fn]
  (add-impl @state-atom (g/node-type* basis node-id) node-id attachment-tree init-fn))

(defn defines?
  "Checks if a node-type is extended to define a list-kw list"
  [node-type list-kw]
  (-> @state-atom :add (get node-type) (contains? list-kw)))

(defn child-node-type
  "Returns defined child node-type for a parent node-type's list-kw

  Asserts that it exists. See [[defines?]]"
  [node-type list-kw]
  {:post (some? %)}
  (-> @state-atom :add (get node-type) (get list-kw) :node-type))

(defn getter
  "Returns a getter function for a container node-type's list-kw

  Returns a function of 2 args: node-id and evaluation-context, that returns a
  vector of child node ids when invoked

  Asserts that it exists. See [[defines?]]"
  [node-type list-kw]
  {:post [(some? %)]}
  (-> @state-atom :get (get node-type) (get list-kw)))

(defn- clear-tx [evaluation-context node-id list-kw]
  (let [basis (:basis evaluation-context)
        get-fn (getter (g/node-type* basis node-id) list-kw)]
    (mapcat
      #(g/delete-node (g/override-root basis %))
      (get-fn node-id evaluation-context))))

(defn clear
  "Create transaction steps for clearing a list of container node-id's list"
  [node-id list-kw]
  (g/expand-ec clear-tx node-id list-kw))

(defn- remove-tx [evaluation-context node-id list-kw child-node-id]
  (let [basis (:basis evaluation-context)
        get-fn (getter (g/node-type* basis node-id) list-kw)
        children (get-fn node-id evaluation-context)]
    (assert (some #(= child-node-id %) children))
    (g/delete-node (g/override-root basis child-node-id))))

(defn remove
  "Create transaction steps for removing a child from container node-id

  The implementation will assert that the child node id actually exists in the
  container as defined by [[getter]]. It will also assert that the container
  node-id defines a list identified by list-kw (see [[defines?]])"
  [node-id list-kw child-node-id]
  (g/expand-ec remove-tx node-id list-kw child-node-id))
