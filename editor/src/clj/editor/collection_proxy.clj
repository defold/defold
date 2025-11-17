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

(ns editor.collection-proxy
  (:require [dynamo.graph :as g]
            [editor.build-target :as bt]
            [editor.defold-project :as project]
            [editor.graph-util :as gu]
            [editor.localization :as localization]
            [editor.outline :as outline]
            [editor.properties :as properties]
            [editor.protobuf :as protobuf]
            [editor.protobuf-forms-util :as protobuf-forms-util]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.validation :as validation]
            [editor.workspace :as workspace])
  (:import [com.dynamo.gamesys.proto GameSystem$CollectionProxyDesc]))

(set! *warn-on-reflection* true)

(def collection-proxy-icon "icons/32/Icons_52-Collection-proxy.png")

(g/defnk produce-form-data
  [_node-id collection-resource]
  {:form-ops {:user-data {:node-id _node-id}
              :set protobuf-forms-util/set-form-op
              :clear protobuf-forms-util/clear-form-op}
   :navigation false
   :sections [{:localization-key "collectionproxy"
               :fields [{:path [:collection]
                         :localization-key "collectionproxy.collection"
                         :type :resource
                         :filter "collection"}]}]
   :values {[:collection] collection-resource}})

(g/defnk produce-save-value
  [collection-resource exclude]
  (protobuf/make-map-without-defaults GameSystem$CollectionProxyDesc
    :collection (resource/resource->proj-path collection-resource)
    :exclude exclude))

(defn build-collection-proxy
  [resource dep-resources user-data]
  (let [pb-msg (reduce #(assoc %1 (first %2) (second %2))
                       (:pb-msg user-data)
                       (map (fn [[label res]] [label (resource/proj-path (get dep-resources res))]) (:dep-resources user-data)))]
    {:resource resource
     :content (protobuf/map->bytes GameSystem$CollectionProxyDesc pb-msg)}))

(g/defnk produce-build-targets
  [_node-id resource save-value dep-build-targets collection]
  (or (validation/prop-error :fatal _node-id :collection validation/prop-nil? collection "collection")
      (let [dep-build-targets (flatten dep-build-targets)
            deps-by-source (into {} (map #(let [res (:resource %)] [(:resource res) res]) dep-build-targets))
            dep-resources (map (fn [[label resource]] [label (get deps-by-source resource)]) [[:collection collection]])]
        [(bt/with-content-hash
           {:node-id _node-id
            :resource (workspace/make-build-resource resource)
            :build-fn build-collection-proxy
            :user-data {:pb-msg save-value
                        :dep-resources dep-resources}
            :deps dep-build-targets})])))

(defn load-collection-proxy [_project self resource collection-proxy-desc]
  {:pre [(map? collection-proxy-desc)]} ; GameSystem$CollectionProxyDesc in map format.
  (let [resolve-resource #(workspace/resolve-resource resource %)]
    (gu/set-properties-from-pb-map self GameSystem$CollectionProxyDesc collection-proxy-desc
      collection (resolve-resource :collection)
      exclude :exclude)))

(g/defnode CollectionProxyNode
  (inherits resource-node/ResourceNode)

  (input dep-build-targets g/Any)
  (input collection-resource resource/Resource)

  (property collection resource/Resource ; Required protobuf field.
            (value (gu/passthrough collection-resource))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter evaluation-context self old-value new-value
                                            [:resource :collection-resource]
                                            [:build-targets :dep-build-targets])))
            (dynamic error (g/fnk [_node-id collection-resource]
                             (or (validation/prop-error :info _node-id :prototype validation/prop-nil? collection-resource "Collection")
                                 (validation/prop-error :fatal _node-id :prototype validation/prop-resource-not-exists? collection-resource "Collection"))))
            (dynamic edit-type (g/constantly
                                 {:type resource/Resource :ext "collection"}))
            (dynamic label (properties/label-dynamic :collection-proxy :collection))
            (dynamic tooltip (properties/tooltip-dynamic :collection-proxy :collection)))

  (property exclude g/Bool
            (default (protobuf/default GameSystem$CollectionProxyDesc :exclude))
            (dynamic label (properties/label-dynamic :collection-proxy :exclude))
            (dynamic tooltip (properties/tooltip-dynamic :collection-proxy :exclude)))

  (output form-data g/Any produce-form-data)

  (output node-outline outline/OutlineData :cached (g/fnk [_node-id collection]
                                                     (cond-> {:node-id _node-id
                                                              :node-outline-key "Collection Proxy"
                                                              :label (localization/message "outline.collection-proxy")
                                                              :icon collection-proxy-icon}

                                                             (resource/resource? collection)
                                                             (assoc :link collection :outline-reference? false))))

  (output save-value g/Any :cached produce-save-value)
  (output build-targets g/Any :cached produce-build-targets))

(defn register-resource-types
  [workspace]
  (resource-node/register-ddf-resource-type workspace
    :ext "collectionproxy"
    :node-type CollectionProxyNode
    :ddf-type GameSystem$CollectionProxyDesc
    :load-fn load-collection-proxy
    :icon collection-proxy-icon
    :icon-class :property
    :view-types [:cljfx-form-view :text]
    :view-opts {}
    :tags #{:component}
    :tag-opts {:component {:transform-properties #{}}}
    :label (localization/message "resource.type.collectionproxy")))
