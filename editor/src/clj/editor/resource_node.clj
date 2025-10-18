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

(ns editor.resource-node
  "Define the concept of a project, and its Project node type. This namespace bridges between Eclipse's workbench and
  ordinary paths."
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.code.util :as code.util]
            [editor.core :as core]
            [editor.localization :as localization]
            [editor.outline :as outline]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-io :as resource-io]
            [editor.settings-core :as settings-core]
            [editor.types :as types]
            [editor.workspace :as workspace]
            [internal.graph.types :as gt]
            [internal.util :as util]
            [util.coll :as coll :refer [pair]]
            [util.digest :as digest]))

(set! *warn-on-reflection* true)

(def unknown-icon "icons/32/Icons_29-AT-Unknown.png")

(defn make-save-data [node-id resource save-value dirty]
  {:pre [(g/node-id? node-id)
         (resource/resource? resource)
         (boolean? dirty)]}
  {:node-id node-id ; Used to update the source-value after saving.
   :resource resource
   :dirty dirty
   :save-value save-value})

(defn save-content
  ^String [save-value resource-type]
  (when (some? save-value)
    (when-let [write-fn (:write-fn resource-type)]
      (write-fn save-value))))

(defn save-data-content
  ^String [{:keys [resource save-value]}]
  (let [resource-type (resource/resource-type resource)]
    (save-content save-value resource-type)))

(defn save-data-sha256 [save-data]
  (if (g/error-value? save-data)
    save-data
    (if-some [content (save-data-content save-data)]
      ;; TODO(save-value-cleanup): Can we digest the save-value without converting it to a string?
      (digest/string->sha256-hex content)
      (let [resource (:resource save-data)
            node-id (:node-id save-data)
            workspace (resource/workspace resource)
            node-id->disk-sha256 (g/node-value workspace :disk-sha256s-by-node-id)]
        (or (node-id->disk-sha256 node-id)
            (resource-io/with-error-translation resource node-id :sha256
              (resource/resource->sha256-hex resource)))))))

(defn save-value->source-value [save-value resource-type]
  (if (or (nil? save-value)
          (g/error-value? save-value))
    save-value
    (if-some [source-value-fn (:source-value-fn resource-type)]
      (source-value-fn save-value)
      save-value)))

(defn dirty-save-value? [save-value source-value resource-type]
  (and (some? source-value)
       (some? save-value)
       (not= source-value (save-value->source-value save-value resource-type))))

(g/defnk produce-save-data [_node-id resource save-value source-value]
  (let [resource-type (resource/resource-type resource)
        dirty (dirty-save-value? save-value source-value resource-type)]
    (make-save-data _node-id resource save-value dirty)))

(g/defnk produce-source-value [_node-id resource]
  ;; The source-value is managed by the save system. When a file is loaded, we
  ;; store the value returned by the :read-fn (or an ErrorValue in case of an
  ;; error) as user-data for the node, and then subsequently update it whenever
  ;; the file is saved. We make sure to invalidate anything downstream of the
  ;; source-value output when doing so.
  (if (resource/exists? resource)
    (g/user-data _node-id :source-value)
    (resource-io/file-not-found-error _node-id :source-value :fatal resource)))

(defn set-source-value! [node-id source-value]
  (g/user-data! node-id :source-value source-value)
  (g/invalidate-outputs! [(g/endpoint node-id :source-value)]))

(defn merge-source-values! [node-id+source-value-pairs]
  (when-not (coll/empty? node-id+source-value-pairs)
    (let [[invalidated-endpoints
           user-data-values-by-key-by-node-id]
          (util/into-multiple
            (pair []
                  {})
            (pair (map (fn [[node-id]]
                         (g/endpoint node-id :source-value)))
                  (map (fn [[node-id source-value]]
                         (pair node-id {:source-value source-value}))))
            node-id+source-value-pairs)]
      (g/user-data-merge! user-data-values-by-key-by-node-id)
      (g/invalidate-outputs! invalidated-endpoints))))

(g/defnk produce-lines [_node-id resource save-value]
  (if (nil? save-value)
    (resource-io/with-error-translation resource _node-id :lines
      (let [content (slurp resource)]
        (code.util/split-lines content)))
    (let [resource-type (resource/resource-type resource)
          content (save-content save-value resource-type)]
      (code.util/split-lines content))))

(g/defnk produce-sha256 [save-data]
  (save-data-sha256 save-data))

(g/defnode ResourceNode
  (inherits core/Scope)
  (inherits outline/OutlineNode)
  (inherits resource/ResourceNode)

  (property loaded g/Bool :unjammable (default false)
            (dynamic visible (g/constantly false)))

  (output save-data g/Any :cached produce-save-data)
  (output save-value g/Any (g/constantly nil))
  (output source-value g/Any :unjammable produce-source-value)
  (output lines types/Lines produce-lines)

  (output dirty g/Bool (g/fnk [save-data] (:dirty save-data false)))
  (output node-id+resource g/Any :unjammable (g/fnk [_node-id resource] [_node-id resource]))
  (output valid-node-id+type+resource g/Any (g/fnk [_node-id _this resource] [_node-id (g/node-type _this) resource])) ; Jammed when defective.
  (output own-build-errors g/Any (g/constantly nil))
  (output build-targets g/Any (g/constantly []))
  (output node-outline outline/OutlineData :cached
    (g/fnk [_node-id _overridden-properties child-outlines own-build-errors resource source-outline]
           (let [rt (resource/resource-type resource)
                 outline-key (or (:label rt) (:ext rt) "unknown")
                 label (or (:label rt) (:ext rt) (localization/message "outline.unknown"))
                 icon (or (:icon rt) unknown-icon)
                 children (cond-> child-outlines
                            source-outline (into (:children source-outline)))]
             {:node-id _node-id
              :node-outline-key outline-key
              :label label
              :icon icon
              :children children
              :outline-error? (g/error-fatal? own-build-errors)
              :outline-overridden? (not (empty? _overridden-properties))})))
  (output sha256 g/Str :cached produce-sha256))

;; TODO(save-value-cleanup): Can we remove this now?
(g/defnode NonEditableResourceNode
  (inherits ResourceNode)

  (output save-data g/Any (g/constantly nil))
  (output save-value g/Any (g/constantly nil)))

(definline node-loaded? [basis resource-node]
  `(g/raw-property-value* ~basis ~resource-node :loaded))

(defn loaded?
  "Returns true if the specified node-id corresponds to a resource that has been
  loaded. The node-id must refer to an existing resource node. A resource node
  can exist in the project graph in an unloaded state, for example if its
  proj-path matches a pattern listed in the .defunload file."
  ([resource-node-id]
   (loaded? (g/now) resource-node-id))
  ([basis resource-node-id]
   (let [resource-node (g/node-by-id basis resource-node-id)]
     (assert (some? resource-node) (str "Node not found: " resource-node-id))
     (assert (g/node-instance*? ResourceNode resource-node))
     (node-loaded? basis resource-node))))

(defn resource
  ([resource-node-id]
   (resource (g/now) resource-node-id))
  ([basis resource-node-id]
   (let [resource-node (g/node-by-id basis resource-node-id)]
     (assert (g/node-instance*? resource/ResourceNode resource-node))
     (resource/node-resource basis resource-node))))

(defn as-resource
  ([node-id]
   (as-resource (g/now) node-id))
  ([basis node-id]
   (when-some [node (g/node-by-id basis node-id)]
     (when (g/node-instance*? resource/ResourceNode node)
       (resource/node-resource basis node)))))

(defn as-resource-original
  ([node-id]
   (as-resource-original (g/now) node-id))
  ([basis node-id]
   (when-some [node (g/node-by-id basis node-id)]
     (when (and (nil? (gt/original node))
                (g/node-instance*? resource/ResourceNode node))
       (resource/node-resource basis node)))))

(defn dirty?
  ([resource-node-id]
   (g/valid-node-value resource-node-id :dirty))
  ([resource-node-id evaluation-context]
   (g/valid-node-value resource-node-id :dirty evaluation-context)))

(defn- make-ddf-dependencies-fn-raw [ddf-type]
  (let [get-fields (protobuf/get-fields-fn (protobuf/resource-field-path-specs ddf-type))]
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
              (some? (resource/proj-path (resource/node-resource basis node))))

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

(defn default-ddf-resource-search-fn
  ([search-string]
   (protobuf/make-map-search-match-fn search-string))
  ([pb-map match-fn]
   (mapv (fn [[text-match path]]
           (assoc text-match
             :match-type :match-type-protobuf
             :path path))
         (coll/search-with-path pb-map [] match-fn))))

(defn make-ddf-resource-search-fn [init-path path-fn]
  (defn search-fn
    ([search-string]
     (protobuf/make-map-search-match-fn search-string))
    ([pb-map match-fn]
     (mapv (fn [[text-match path]]
             (assoc text-match
               :match-type :match-type-protobuf
               :path (path-fn pb-map path)))
           (coll/search-with-path pb-map init-path match-fn)))))

(defn register-ddf-resource-type [workspace & {:keys [editable ext node-type ddf-type read-defaults load-fn dependencies-fn sanitize-fn search-fn string-encode-fn icon view-types tags tag-opts label built-pb-class] :as args}]
  {:pre [(protobuf/pb-class? ddf-type)
         (or (nil? built-pb-class) (protobuf/pb-class? built-pb-class))]}
  (let [read-defaults (boolean read-defaults)
        read-raw-fn (if read-defaults
                      (partial protobuf/read-map-with-defaults ddf-type)
                      (partial protobuf/read-map-without-defaults ddf-type))
        read-fn (cond->> read-raw-fn
                         (some? sanitize-fn) (comp sanitize-fn))
        write-fn (cond-> (partial protobuf/map->str ddf-type)
                         (some? string-encode-fn) (comp string-encode-fn))
        search-fn (or search-fn default-ddf-resource-search-fn)
        args (-> args
                 (dissoc :read-defaults :string-encode-fn)
                 (assoc :textual? true
                        :dependencies-fn (or dependencies-fn (make-ddf-dependencies-fn ddf-type))
                        :read-fn read-fn
                        :write-fn write-fn
                        :search-fn search-fn
                        :test-info {:type :ddf
                                    :ddf-type ddf-type
                                    :read-defaults read-defaults
                                    :built-pb-class (or built-pb-class ddf-type)}))]
    (apply workspace/register-resource-type workspace (mapcat identity args))))

(defn register-settings-resource-type [workspace & {:keys [ext node-type load-fn meta-settings icon view-types tags tag-opts label] :as args}]
  {:pre [(seqable? meta-settings)]}
  (let [read-fn (fn [resource]
                  (with-open [setting-reader (io/reader resource)]
                    (settings-core/parse-settings setting-reader)))
        write-fn (comp #(settings-core/settings->str % meta-settings :multi-line-list)
                       settings-core/settings-with-value)
        args (assoc args
               :textual? true
               :read-fn read-fn
               :write-fn write-fn
               :search-fn settings-core/raw-settings-search-fn
               :test-info {:type :settings
                           :meta-settings meta-settings})]
    (apply workspace/register-resource-type workspace (mapcat identity args))))
