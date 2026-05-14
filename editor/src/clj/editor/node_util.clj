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

(ns editor.node-util
  (:require [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.core :as core]
            [editor.outline :as outline]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [internal.graph.types :as gt]
            [internal.node :as in]
            [util.coll :as coll]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn node-qualifier-label
  "Given a node-id, returns a string that identifies the node for the user.
  Typically, this will be the URL that uniquely identifies the node inside its
  owner resource, or the id that the user has specified for the node. Returns
  nil if there is no suitable qualifier for the given node."
  ([node-id]
   (g/with-auto-evaluation-context evaluation-context
     (node-qualifier-label node-id evaluation-context)))
  ([node-id {:keys [basis] :as evaluation-context}]
   (when-some [node (g/node-by-id-at basis node-id)]
     (let [node-type (g/node-type node)]
       (or (when (in/behavior node-type :url)
             (let [value (in/node-value node :url evaluation-context)]
               (when (and (string? value)
                          (coll/not-empty value))
                 ;; Convert urls to a more readable representation.
                 ;;   "#book_script" -> "book_script"
                 ;;   "/referenced_book#book_script" -> "referenced_book/book_script"
                 (-> value
                     (subs 1)
                     (string/replace "#" "/")))))
           (when (in/behavior node-type :id)
             (let [value (in/node-value node :id evaluation-context)]
               (when (string? value)
                 (coll/not-empty value)))))))))

(defn node-debug-label
  ([node-id]
   (g/with-auto-evaluation-context evaluation-context
     (node-debug-label node-id evaluation-context)))
  ([node-id {:keys [basis] :as evaluation-context}]
   (let [node (g/node-by-id basis node-id)
         node-type (g/node-type node)]
     (or (when (in/inherits? node-type resource/ResourceNode)
           (let [resource (resource-node/resource basis node-id)]
             (cond
               (resource/memory-resource? resource)
               (str "embedded." (resource/ext resource))

               (some? (gt/original node))
               (let [proj-path (resource/proj-path resource)]
                 (if-let [owner-resource (resource-node/owner-resource basis node-id)]
                   (str proj-path " override in " (resource/proj-path owner-resource))
                   (str proj-path " override")))

               :else
               (resource/proj-path resource))))
         (node-qualifier-label node-id evaluation-context)
         (when (in/inherits? node-type outline/OutlineNode)
           (coll/not-empty (:label (g/maybe-node-value node-id :node-outline evaluation-context))))
         (let [name (g/maybe-node-value node-id :name evaluation-context)]
           (when (string? name)
             (coll/not-empty name)))
         (str (name (:k node-type)) \# node-id)))))

(defn node-debug-label-path
  ([node-id]
   (g/with-auto-evaluation-context evaluation-context
     (node-debug-label-path node-id evaluation-context)))
  ([node-id {:keys [basis] :as evaluation-context}]
   (let [graph-id (g/node-id->graph-id node-id)
         project-node-id (g/graph-value basis graph-id :project-id)]
     (->> node-id
          (iterate #(core/owner-node-id basis %))
          (take-while #(some-> % (not= project-node-id)))
          (reverse)
          (mapv #(node-debug-label % evaluation-context))))))

(defn node-debug-info
  ([node-id]
   (g/with-auto-evaluation-context evaluation-context
     (node-debug-info node-id evaluation-context)))
  ([node-id {:keys [basis] :as evaluation-context}]
   (let [node-type-kw (g/node-type-kw basis node-id)
         node-debug-label-path (node-debug-label-path node-id evaluation-context)
         owner-resource-node-id (try
                                  (resource-node/owner-resource-node-id basis node-id)
                                  (catch Exception _
                                    nil))
         owner-resource-node-type-kw (some->> owner-resource-node-id (g/node-type-kw basis))]
     {:node-type-kw node-type-kw
      :node-debug-label-path node-debug-label-path
      :owner-resource-node-id owner-resource-node-id
      :owner-resource-node-type-kw owner-resource-node-type-kw})))
