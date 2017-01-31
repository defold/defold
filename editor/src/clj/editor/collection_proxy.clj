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
   [editor.types :as types]
   [editor.validation :as validation]
   [editor.workspace :as workspace])
  (:import
   (com.dynamo.gamesystem.proto GameSystem$CollectionProxyDesc)))

(set! *warn-on-reflection* true)

(def collection-proxy-icon "icons/32/Icons_09-Collection.png")

(g/defnk produce-form-data
  [_node-id collection-resource]
  {:form-ops {:user-data {}
              :set (fn [v [property] val]
                     (g/set-property! _node-id property val))
              :clear (fn [property]
                       (g/clear-property! _node-id property))}
   :sections [{:title "Collection Proxy"
               :fields [{:path [:collection]
                         :label "Collection"
                         :type :resource
                         :filter "collection"}]}]
   :values {[:collection] (resource/resource->proj-path collection-resource)}})

(g/defnk produce-pb-msg
  [collection-resource]
  {:collection (resource/resource->proj-path collection-resource)})

(g/defnk produce-save-data [resource pb-msg]
  {:resource resource
   :content (protobuf/map->str GameSystem$CollectionProxyDesc pb-msg)})

(defn build-collection-proxy
  [self basis resource dep-resources user-data]
  (let [pb-msg (reduce #(assoc %1 (first %2) (second %2))
                       (:pb-msg user-data)
                       (map (fn [[label res]] [label (resource/proj-path (get dep-resources res))]) (:dep-resources user-data)))]
    {:resource resource
     :content (protobuf/map->bytes GameSystem$CollectionProxyDesc pb-msg)}))

(g/defnk produce-build-targets
  [_node-id resource pb-msg dep-build-targets collection]
  (let [dep-build-targets (flatten dep-build-targets)
        deps-by-source (into {} (map #(let [res (:resource %)] [(:resource res) res]) dep-build-targets))
        dep-resources (map (fn [[label resource]] [label (get deps-by-source resource)]) [[:collection collection]])]
    [{:node-id _node-id
      :resource (workspace/make-build-resource resource)
      :build-fn build-collection-proxy
      :user-data {:pb-msg pb-msg
                  :dep-resources dep-resources}
      :deps dep-build-targets}]))

(defn load-collection-proxy
  [project self resource]
  (let [collection-proxy (protobuf/read-text GameSystem$CollectionProxyDesc resource)]
    (g/set-property self
                    :collection (workspace/resolve-resource resource (:collection collection-proxy)))))

(g/defnode CollectionProxyNode
  (inherits project/ResourceNode)

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
  
  (output form-data g/Any produce-form-data)

  (output node-outline outline/OutlineData :cached (g/fnk [_node-id]
                                                     {:node-id _node-id
                                                      :label "Collection Proxy"
                                                      :icon collection-proxy-icon}))

  (output pb-msg g/Any :cached produce-pb-msg)
  (output save-data g/Any :cached produce-save-data)
  (output build-targets g/Any :cached produce-build-targets))


(defn register-resource-types
  [workspace]
  (workspace/register-resource-type workspace
                                    :textual? true
                                    :ext "collectionproxy"
                                    :node-type CollectionProxyNode
                                    :load-fn load-collection-proxy
                                    :icon collection-proxy-icon
                                    :view-types [:form-view :text]
                                    :view-opts {}
                                    :tags #{:component}
                                    :label "Collection Proxy"))
