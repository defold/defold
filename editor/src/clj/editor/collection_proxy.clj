(ns editor.collection-proxy
  (:require
   [clojure.string :as s]
   [plumbing.core :as pc]
   [dynamo.graph :as g]
   [editor.defold-project :as project]
   [editor.graph-util :as gu]
   [editor.outline :as outline]
   [editor.properties :as properties]
   [editor.protobuf :as protobuf]
   [editor.resource :as resource]
   [editor.resource-node :as resource-node]
   [editor.types :as types]
   [editor.validation :as validation]
   [editor.workspace :as workspace])
  (:import
   (com.dynamo.gamesystem.proto GameSystem$CollectionProxyDesc)))

(set! *warn-on-reflection* true)

(def collection-proxy-icon "icons/32/Icons_52-Collection-proxy.png")

(defn- set-form-op [{:keys [node-id]} [property] value]
  (g/set-property! node-id property value))

(defn- clear-form-op [{:keys [node-id]} [property]]
  (g/clear-property! node-id property))

(g/defnk produce-form-data
  [_node-id collection-resource]
  {:form-ops {:user-data {:node-id _node-id}
              :set set-form-op
              :clear clear-form-op}
   :sections [{:title "Collection Proxy"
               :fields [{:path [:collection]
                         :label "Collection"
                         :type :resource
                         :filter "collection"}]}]
   :values {[:collection] collection-resource}})

(g/defnk produce-pb-msg
  [collection-resource exclude]
  (cond-> {:collection (resource/resource->proj-path collection-resource)}
    (some? exclude) (assoc :exclude exclude)))

(defn build-collection-proxy
  [self basis resource dep-resources user-data]
  (let [pb-msg (reduce #(assoc %1 (first %2) (second %2))
                       (:pb-msg user-data)
                       (map (fn [[label res]] [label (resource/proj-path (get dep-resources res))]) (:dep-resources user-data)))]
    {:resource resource
     :content (protobuf/map->bytes GameSystem$CollectionProxyDesc pb-msg)}))

(g/defnk produce-build-targets
  [_node-id resource pb-msg dep-build-targets collection]
  (or (validation/prop-error :fatal _node-id :collection validation/prop-nil? collection "collection")
      (let [dep-build-targets (flatten dep-build-targets)
            deps-by-source (into {} (map #(let [res (:resource %)] [(:resource res) res]) dep-build-targets))
            dep-resources (map (fn [[label resource]] [label (get deps-by-source resource)]) [[:collection collection]])]
        [{:node-id _node-id
          :resource (workspace/make-build-resource resource)
          :build-fn build-collection-proxy
          :user-data {:pb-msg pb-msg
                      :dep-resources dep-resources}
          :deps dep-build-targets}])))

(defn load-collection-proxy [project self resource collection-proxy]
  (concat
    (g/set-property self :collection (workspace/resolve-resource resource (:collection collection-proxy)))
    (g/set-property self :exclude (boolean (:exclude collection-proxy)))))

(g/defnode CollectionProxyNode
  (inherits resource-node/ResourceNode)

  (input dep-build-targets g/Any)
  (input collection-resource resource/Resource)

  (property collection resource/Resource
            (value (gu/passthrough collection-resource))
            (set (fn [basis self old-value new-value]
                   (project/resource-setter basis self old-value new-value
                                            [:resource :collection-resource]
                                            [:build-targets :dep-build-targets])))
            (dynamic error (g/fnk [_node-id collection-resource]
                                  (or (validation/prop-error :info _node-id :prototype validation/prop-nil? collection-resource "Collection")
                                      (validation/prop-error :fatal _node-id :prototype validation/prop-resource-not-exists? collection-resource "Collection"))))
            (dynamic edit-type (g/constantly
                                 {:type resource/Resource :ext "collection"})))

  (property exclude g/Bool)
  
  (output form-data g/Any produce-form-data)

  (output node-outline outline/OutlineData :cached (g/fnk [_node-id]
                                                     {:node-id _node-id
                                                      :label "Collection Proxy"
                                                      :icon collection-proxy-icon}))

  (output pb-msg g/Any :cached produce-pb-msg)
  (output save-value g/Any (gu/passthrough pb-msg))
  (output build-targets g/Any :cached produce-build-targets))


(defn register-resource-types
  [workspace]
  (resource-node/register-ddf-resource-type workspace
                                    :ext "collectionproxy"
                                    :node-type CollectionProxyNode
                                    :ddf-type GameSystem$CollectionProxyDesc
                                    :load-fn load-collection-proxy
                                    :icon collection-proxy-icon
                                    :view-types [:form-view :text]
                                    :view-opts {}
                                    :tags #{:component}
                                    :tag-opts {:component {:transform-properties #{}}}
                                    :label "Collection Proxy"))
