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

(ns editor.core
  "Essential node types"
  (:require [cognitect.transit :as transit]
            [dynamo.graph :as g]
            [internal.graph.types :as gt]
            [internal.util :as util]))

(set! *warn-on-reflection* true)

;; ---------------------------------------------------------------------------
;; Copy/paste support
;; ---------------------------------------------------------------------------
(def ^:dynamic *serialization-handlers*
  {:read  {"class"         (transit/read-handler
                            (fn [rep] (java.lang.Class/forName ^String rep)))}
   :write {java.lang.Class (transit/write-handler
                            (constantly "class")
                            (fn [^Class v] (.getName v)))}})

(defn register-read-handler!
  [tag handler]
  (alter-var-root #'*serialization-handlers* assoc-in [:read tag] handler))

(defn register-write-handler!
  [class handler]
  (alter-var-root #'*serialization-handlers* assoc-in [:write class] handler))

(defn register-record-type!
  [type]
  (alter-var-root #'*serialization-handlers*
                  #(-> %
                       (update :read merge (transit/record-read-handlers type))
                       (update :write merge (transit/record-write-handlers type)))))

(defn read-handlers [] (:read *serialization-handlers*))

(defn write-handlers [] (:write *serialization-handlers*))

;; ---------------------------------------------------------------------------
;; Bootstrapping the core node types
;; ---------------------------------------------------------------------------

(defn direct-owned-node-ids
  ([scope-id]
   (direct-owned-node-ids (g/now) scope-id))
  ([basis scope-id]
   {:pre [(g/node-id? scope-id)]}
   (let [incoming-arcs (g/explicit-arcs-by-target basis scope-id)
         scope-node-type (g/node-type* basis scope-id)
         cascade-delete-input-labels (g/cascade-deletes scope-node-type)
         arc-connects-to-cascade-delete-input? (comp cascade-delete-input-labels gt/target-label)]
     (eduction
       (filter arc-connects-to-cascade-delete-input?)
       (map gt/source-id)
       (distinct)
       incoming-arcs))))

(defn recursive-owned-node-ids
  ([scope-id]
   (recursive-owned-node-ids (g/now) scope-id))
  ([basis scope-id]
   (tree-seq some?
             #(direct-owned-node-ids basis %)
             scope-id)))

(defn owner-node-id
  ([node-id]
   (owner-node-id (g/now) node-id))
  ([basis node-id]
   {:pre [(g/node-id? node-id)]}
   (let [targets (g/targets basis node-id)
         target-id->labels (util/group-into {} [] #(% 0) #(% 1) targets)]
     (some (fn [[target-id labels]]
             (let [target-node-type (g/node-type* basis target-id)
                   cascade-delete-input-label? (g/cascade-deletes target-node-type)]
               (when (some cascade-delete-input-label? labels)
                 target-id)))
           target-id->labels))))

(g/defnode Scope
  "Scope provides a level of grouping for nodes. Scopes nest.
When a node is added to a Scope, the node's :_node-id output will be
connected to the Scope's :nodes input.

When a Scope is deleted, all nodes within that scope will also be deleted."
  (input nodes g/Any :array :cascade-delete))

(defn scope
  ([node-id]
   (scope (g/now) node-id))
  ([basis node-id]
   {:pre [(g/node-id? node-id)]}
   (some (fn [outgoing-arc]
           (when (= :nodes (gt/target-label outgoing-arc))
             (gt/target-id outgoing-arc)))
         (gt/arcs-by-source basis node-id :_node-id))))

(defn scope-of-type
  ([node-id node-type]
   (scope-of-type (g/now) node-id node-type))
  ([basis node-id node-type]
   (when-let [scope-id (scope basis node-id)]
     (if (g/node-instance? basis node-type scope-id)
       scope-id
       (recur basis scope-id node-type)))))

(defprotocol Adaptable
  (adapt [this t]))
