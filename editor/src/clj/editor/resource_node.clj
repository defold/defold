;; Copyright 2020-2023 The Defold Foundation
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

(ns editor.resource-node
  "Define the concept of a project, and its Project node type. This namespace bridges between Eclipse's workbench and
  ordinary paths."
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.core :as core]
            [editor.outline :as outline]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-io :as resource-io]
            [editor.settings-core :as settings-core]
            [editor.workspace :as workspace]
            [internal.graph.types :as gt]
            [util.digest :as digest])
  (:import [java.io StringReader]))

(set! *warn-on-reflection* true)

(def unknown-icon "icons/32/Icons_29-AT-Unknown.png")

(defn resource-node-dependencies [resource-node-id evaluation-context]
  (let [resource (g/node-value resource-node-id :resource evaluation-context)]
    (when-some [dependencies-fn (:dependencies-fn (resource/resource-type resource))]
      (let [source-value (g/node-value resource-node-id :source-value evaluation-context)]
        (dependencies-fn source-value)))))

(defn save-data-sha256 [undecorated-save-data]
  ;; Note: resource-update/keep-existing-node? assumes this will return a
  ;; sha256 hex hash string from the bytes written to disk.
  (let [content (get undecorated-save-data :content ::no-content)]
    (if (= ::no-content content)
      (resource/resource->sha256-hex (:resource undecorated-save-data))
      (digest/string->sha256-hex content))))

(g/defnk produce-undecorated-save-data [_node-id resource save-value]
  (let [write-fn (:write-fn (resource/resource-type resource))]
    (cond-> {:resource resource :value save-value :node-id _node-id}
            (and write-fn save-value) (assoc :content (write-fn save-value)))))

(g/defnk produce-save-data [undecorated-save-data dirty?]
  (assoc undecorated-save-data :dirty? dirty?))

(g/defnode ResourceNode
  (inherits core/Scope)
  (inherits outline/OutlineNode)
  (inherits resource/ResourceNode)

  (property editable g/Bool :unjammable
            (default true)
            (dynamic visible (g/constantly false)))

  (output undecorated-save-data g/Any produce-undecorated-save-data)
  (output save-data g/Any :cached produce-save-data)
  (output source-value g/Any :cached (g/fnk [_node-id resource editable]
                                       (when-some [read-fn (:read-fn (resource/resource-type resource))]
                                         (when (and editable (resource/exists? resource))
                                           (resource-io/with-error-translation resource _node-id :source-value
                                             (read-fn resource))))))
  
  (output save-value g/Any (g/constantly nil))

  (output cleaned-save-value g/Any (g/fnk [_node-id resource save-value editable]
                                          (when editable
                                            (let [resource-type (resource/resource-type resource)
                                                  read-fn (:read-fn resource-type)
                                                  write-fn (:write-fn resource-type)]
                                              (if (and read-fn write-fn)
                                                (with-open [reader (StringReader. (write-fn save-value))]
                                                  (resource-io/with-error-translation resource _node-id :cleaned-save-value
                                                    (read-fn reader)))
                                                save-value)))))
  (output dirty? g/Bool (g/fnk [cleaned-save-value source-value editable]
                          (and editable (some? cleaned-save-value) (not= cleaned-save-value source-value))))
  (output node-id+resource g/Any :unjammable (g/fnk [_node-id resource] [_node-id resource]))
  (output valid-node-id+type+resource g/Any (g/fnk [_node-id _this resource] [_node-id (g/node-type _this) resource])) ; Jammed when defective.
  (output own-build-errors g/Any (g/constantly nil))
  (output build-targets g/Any (g/constantly []))
  (output node-outline outline/OutlineData :cached
    (g/fnk [_node-id _overridden-properties child-outlines own-build-errors resource source-outline]
           (let [rt (resource/resource-type resource)
                 label (or (:label rt) (:ext rt) "unknown")
                 icon (or (:icon rt) unknown-icon)
                 children (cond-> child-outlines
                            source-outline (into (:children source-outline)))]
             {:node-id _node-id
              :node-outline-key label
              :label label
              :icon icon
              :children children
              :outline-error? (g/error-fatal? own-build-errors)
              :outline-overridden? (not (empty? _overridden-properties))})))

  (output sha256 g/Str :cached (g/fnk [resource undecorated-save-data]
                                 ;; Careful! This might throw if resource has been removed
                                 ;; outside the editor. Use from editor.engine.native-extensions seems
                                 ;; to catch any exceptions.
                                 (if (some? undecorated-save-data)
                                   (save-data-sha256 undecorated-save-data)
                                   (resource/resource->sha256-hex resource)))))

(g/defnode NonEditableResourceNode
  (inherits ResourceNode)

  (property editable g/Bool :unjammable
            (default false)
            (dynamic visible (g/constantly false)))

  (output source-value g/Any (g/constantly nil))
  (output save-value g/Any (g/constantly nil))
  (output cleaned-save-value g/Any (g/constantly nil))
  (output dirty? g/Bool (g/constantly false))
  (output undecorated-save-data g/Any (g/fnk [_node-id resource save-value] {:resource resource :value save-value :node-id _node-id})))

(definline ^:private resource-node-resource [basis resource-node]
  ;; This is faster than g/node-value, and doesn't require creating an
  ;; evaluation-context. The resource property is unjammable and properties
  ;; aren't cached, so there is no need to do a full g/node-value.
  `(gt/get-property ~resource-node ~basis :resource))

(defn resource
  ([resource-node-id]
   (resource (g/now) resource-node-id))
  ([basis resource-node-id]
   (let [resource-node (g/node-by-id basis resource-node-id)]
     (assert (g/node-instance*? resource/ResourceNode resource-node))
     (resource-node-resource basis resource-node))))

(defn as-resource
  ([node-id]
   (as-resource (g/now) node-id))
  ([basis node-id]
   (when-some [node (g/node-by-id basis node-id)]
     (when (g/node-instance*? resource/ResourceNode node)
       (resource-node-resource basis node)))))

(defn as-resource-original
  ([node-id]
   (as-resource-original (g/now) node-id))
  ([basis node-id]
   (when-some [node (g/node-by-id basis node-id)]
     (when (and (nil? (gt/original node))
                (g/node-instance*? resource/ResourceNode node))
       (resource-node-resource basis node)))))

(defn defective? [resource-node]
  (let [value (g/node-value resource-node :valid-node-id+type+resource)]
    (and (g/error? value)
         (g/error-fatal? value))))

(defn owner-resource-node-id
  ([node-id]
   (owner-resource-node-id (g/now) node-id))
  ([basis node-id]
   ;; The owner resource node is the first non-override ResourceNode we find by
   ;; traversing the explicit :cascade-delete connections between nodes. I.e, we
   ;; want to find the ResourceNode that will delete our node if the resource is
   ;; deleted from the project.
   (let [node (g/node-by-id basis node-id)]
     ;; Embedded resources will be represented by ResourceNodes, but we want the
     ;; ultimate owner of the embedded resource, so we keep looking if we
     ;; encounter an override node or a ResourceNode whose resource does not
     ;; have a valid proj-path.
     (if (and (nil? (gt/original node))
              (g/node-instance*? ResourceNode node)
              (some? (resource/proj-path (resource-node-resource basis node))))

       ;; We found our owner ResourceNode. Return its node-id.
       node-id

       ;; This is not our owner ResourceNode. Recursively follow the outgoing
       ;; connections that connect to a :cascade-delete input.
       (some->> (core/owner-node-id basis node-id)
                (owner-resource-node-id basis))))))

(defn owner-resource
  ([node-id]
   (owner-resource (g/now) node-id))
  ([basis node-id]
   (some->> (owner-resource-node-id basis node-id)
            (resource basis))))

(defn make-ddf-dependencies-fn [ddf-type]
  (fn [source-value]
    (into []
          (comp
            (filter seq)
            (distinct))
          ((protobuf/get-fields-fn (protobuf/resource-field-paths ddf-type)) source-value))))

(defn register-ddf-resource-type [workspace & {:keys [editable ext node-type ddf-type load-fn dependencies-fn sanitize-fn icon view-types tags tag-opts label] :as args}]
  (let [read-raw-fn (partial protobuf/read-text ddf-type)
        read-fn (cond->> read-raw-fn
                         (some? sanitize-fn) (comp sanitize-fn))
        args (assoc args
               :textual? true
               :load-fn (fn [project self resource]
                          (let [[source-value disk-sha256] (resource/read-source-value+sha256-hex resource read-fn)]
                            (cond->> (load-fn project self resource source-value)
                                     disk-sha256 (concat (workspace/set-disk-sha256 workspace self disk-sha256)))))
               :dependencies-fn (or dependencies-fn (make-ddf-dependencies-fn ddf-type))
               :read-raw-fn read-raw-fn
               :read-fn read-fn
               :write-fn (partial protobuf/map->str ddf-type)
               :test-info {:type :ddf
                           :ddf-type ddf-type})]
    (apply workspace/register-resource-type workspace (mapcat identity args))))

(defn register-settings-resource-type [workspace & {:keys [ext node-type load-fn meta-settings icon view-types tags tag-opts label] :as args}]
  {:pre [(seqable? meta-settings)]}
  (let [read-fn (fn [resource]
                  (with-open [setting-reader (io/reader resource)]
                    (settings-core/parse-settings setting-reader)))
        args (assoc args
               :textual? true
               :load-fn (fn [project self resource]
                          (let [[source-value disk-sha256] (resource/read-source-value+sha256-hex resource read-fn)]
                            (cond->> (load-fn project self resource source-value)
                                     disk-sha256 (concat (workspace/set-disk-sha256 workspace self disk-sha256)))))
               :read-fn read-fn
               :write-fn (comp #(settings-core/settings->str % meta-settings :multi-line-list)
                               settings-core/settings-with-value)
               :test-info {:type :settings
                           :meta-settings meta-settings})]
    (apply workspace/register-resource-type workspace (mapcat identity args))))
