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
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-io :as resource-io]
            [editor.settings-core :as settings-core]
            [editor.workspace :as workspace]
            [editor.outline :as outline]
            [internal.graph.types :as gt]
            [util.coll :refer [pair]]
            [util.text-util :as text-util])
  (:import [org.apache.commons.codec.digest DigestUtils]))

(set! *warn-on-reflection* true)

(def unknown-icon "icons/32/Icons_29-AT-Unknown.png")

(defn resource-node-dependencies [resource-node-id evaluation-context]
  (let [resource (g/node-value resource-node-id :resource evaluation-context)]
    (when-some [dependencies-fn (:dependencies-fn (resource/resource-type resource))]
      (when-some [source-value (g/node-value resource-node-id :source-value evaluation-context)]
        (if (g/error? source-value)
          (throw (ex-info (str "Error reading resource: " (resource/proj-path resource))
                          {:resource resource
                           :error-value source-value}))
          (dependencies-fn source-value))))))

(defn make-save-data [node-id resource save-value dirty]
  {:pre [(g/node-id? node-id)
         (resource/resource? resource)
         (boolean? dirty)]}
  {:node-id node-id ; Used to invalidate the :source-value output after saving.
   :resource resource
   :dirty dirty
   :save-value save-value})

(g/defnk produce-save-data [_node-id resource save-value source-value]
  (let [dirty (and (some? save-value)
                   (not= source-value save-value))]
    (make-save-data _node-id resource save-value dirty)))

(g/defnk produce-source-value [_node-id resource]
  (when-some [read-fn (:read-fn (resource/resource-type resource))]
    (when (resource/exists? resource)
      (resource-io/with-error-translation resource _node-id :source-value
        (read-fn resource)))))

(g/defnk produce-sha256 [_node-id resource save-value]
  (if (nil? save-value)
    (resource-io/with-error-translation resource _node-id :sha256
      (with-open [input-stream (io/input-stream resource)]
        (DigestUtils/sha256Hex ^java.io.InputStream input-stream)))
    (let [resource-type (resource/resource-type resource)
          write-fn (:write-fn resource-type)
          ^String content (write-fn save-value)]
      ;; TODO(save-value): Can we digest the save-value without converting it to a string?
      (DigestUtils/sha256Hex content))))

(g/defnode ResourceNode
  (inherits core/Scope)
  (inherits outline/OutlineNode)
  (inherits resource/ResourceNode)

  (output save-data g/Any :cached produce-save-data)
  (output save-value g/Any (g/constantly nil))
  (output source-value g/Any :cached :unjammable produce-source-value)

  (output dirty g/Bool (g/fnk [save-data] (:dirty save-data false)))
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
  (output sha256 g/Str :cached produce-sha256))

(g/defnode NonEditableResourceNode
  (inherits ResourceNode)

  (output save-data g/Any (g/constantly nil))
  (output save-value g/Any (g/constantly nil))
  (output source-value g/Any :unjammable produce-source-value)) ; No need to cache, but used during load.

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

(defn- make-ddf-dependencies-fn-raw [ddf-type]
  (let [get-fields (protobuf/get-fields-fn (protobuf/resource-field-paths ddf-type))]
    (fn [source-value]
      (into []
            (comp
              (filter seq)
              (distinct))
            (get-fields source-value)))))

(def make-ddf-dependencies-fn (memoize make-ddf-dependencies-fn-raw))

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

(defn save-data-content
  ^String [save-data]
  (when-some [save-value (:save-value save-data)]
    (let [resource (:resource save-data)
          resource-type (resource/resource-type resource)
          write-fn (:write-fn resource-type)]
      (when write-fn
        (write-fn save-value)))))

(defn sha256-or-throw
  ^String [resource-node-id evaluation-context]
  (let [sha256-or-error (g/node-value resource-node-id :sha256 evaluation-context)]
    (if-not (g/error? sha256-or-error)
      sha256-or-error
      (let [basis (:basis evaluation-context)
            resource (resource basis resource-node-id)
            proj-path (resource/proj-path resource)]
        (throw (ex-info (str "Failed to calculate sha256 hash of resource: " proj-path)
                        {:proj-path proj-path
                         :error-value sha256-or-error}))))))

(defn search-ddf-save-data
  ([^String search-string]
   (let [enum-search-string (protobuf/enum-name->keyword-name search-string)
         text-re-pattern (text-util/search-string->re-pattern search-string :case-insensitive)
         enum-re-pattern (text-util/search-string->re-pattern enum-search-string :case-insensitive)]
     (pair text-re-pattern enum-re-pattern)))
  ([save-data [text-re-pattern enum-re-pattern]]
   ;; TODO(save-value): Converting to string is inefficient. Search the pb-map structure in :save-value instead.
   ;; TODO(save-value): Use coll/search-with-path match enum-re-pattern against keyword values.
   (let [resource (:resource save-data)
         resource-type (resource/resource-type resource)
         write-fn (:write-fn resource-type)
         save-value (:save-value save-data)
         save-content (write-fn save-value)]
     (text-util/string->text-matches save-content text-re-pattern))))

(defn register-ddf-resource-type [workspace & {:keys [editable ext node-type ddf-type read-defaults load-fn dependencies-fn sanitize-fn search-fn string-encode-fn icon view-types tags tag-opts label] :as args}]
  (let [read-defaults (if (nil? read-defaults) true read-defaults)
        read-raw-fn (if read-defaults
                      (partial protobuf/read-map-with-defaults ddf-type)
                      (partial protobuf/read-map-without-defaults ddf-type))
        read-fn (cond->> read-raw-fn
                         (some? sanitize-fn) (comp sanitize-fn))
        write-fn (cond-> (partial protobuf/map->str ddf-type)
                         (some? string-encode-fn) (comp string-encode-fn))
        search-fn (or search-fn search-ddf-save-data)
        args (-> args
                 (dissoc :string-encode-fn)
                 (assoc :textual? true
                        :dependencies-fn (or dependencies-fn (make-ddf-dependencies-fn ddf-type))
                        :read-raw-fn read-raw-fn
                        :read-fn read-fn
                        :write-fn write-fn
                        :search-fn search-fn))]
    (apply workspace/register-resource-type workspace (mapcat identity args))))

(defn register-settings-resource-type [workspace & {:keys [ext node-type load-fn meta-settings icon view-types tags tag-opts label] :as args}]
  {:pre [(seqable? meta-settings)]}
  (let [read-fn (fn [resource]
                  (with-open [setting-reader (io/reader resource)]
                    (settings-core/parse-settings setting-reader)))
        args (assoc args
               :textual? true
               :read-fn read-fn
               :write-fn (comp #(settings-core/settings->str % meta-settings :multi-line-list)
                               settings-core/settings-with-value))]
    (apply workspace/register-resource-type workspace (mapcat identity args))))
