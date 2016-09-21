(ns editor.factory
  (:require [clojure.string :as s]
            [plumbing.core :as pc]
            [dynamo.graph :as g]
            [editor.colors :as colors]
            [editor.defold-project :as project]
            [editor.graph-util :as gu]
            [editor.handler :as handler]
            [editor.outline :as outline]
            [editor.properties :as properties]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.types :as types]
            [editor.validation :as validation]
            [editor.workspace :as workspace])
  (:import [com.dynamo.gamesystem.proto
            GameSystem$FactoryDesc
            GameSystem$CollectionFactoryDesc]))

(set! *warn-on-reflection* true)

(def ^:const factory-types
  {:game-object {:icon "icons/32/Icons_07-Factory.png"
                 :title "Factory"
                 :ext "go"
                 :pb-type GameSystem$FactoryDesc}
   :collection  {:icon "icons/32/Icons_08-Collection-factory.png"
                 :title "Collection Factory"
                 :ext "collection"
                 :pb-type GameSystem$CollectionFactoryDesc}})

(g/defnk produce-form-data
  [_node-id factory-type prototype-resource]
  {:form-ops {:user-data {}
              :set (fn [v path val]
                     (g/set-property! _node-id :prototype val))
              :clear (fn [path]
                       (g/clear-property! _node-id :prototype))}
   :sections [{:title (get-in factory-types [factory-type :title])
               :fields [{:path [:prototype]
                         :label "Prototype"
                         :type :resource
                         :filter (get-in factory-types [factory-type :ext])}]}]
   :values {[:prototype] (resource/resource->proj-path prototype-resource)}})

(g/defnk produce-pb-msg
  [prototype-resource]
  {:prototype (resource/resource->proj-path prototype-resource)})

(g/defnk produce-save-data [resource factory-type pb-msg]
  {:resource resource
   :content (protobuf/map->str (get-in factory-types [factory-type :pb-type]) pb-msg)})

(defn build-factory
  [self basis resource dep-resources user-data]
  (let [pb-msg (reduce #(assoc %1 (first %2) (second %2))
                       (:pb-msg user-data)
                       (map (fn [[label res]] [label (resource/proj-path (get dep-resources res))]) (:dep-resources user-data)))]
    {:resource resource
     :content (protobuf/map->bytes (:pb-type user-data) pb-msg)}))

(g/defnk produce-build-targets
  [_node-id resource factory-type prototype pb-msg dep-build-targets]
  (let [dep-build-targets (flatten dep-build-targets)
        deps-by-resource (into {} (map (juxt (comp :resource :resource) :resource) dep-build-targets))
        dep-resources (map (fn [[label resource]]
                             [label (get deps-by-resource resource)])
                           [[:prototype prototype]])]
      [{:node-id _node-id
        :resource (workspace/make-build-resource resource)
        :build-fn build-factory
        :user-data {:pb-msg pb-msg
                    :pb-type (get-in factory-types [factory-type :pb-type])
                    :dep-resources dep-resources}
        :deps dep-build-targets}]))

(defn load-factory
  [factory-type project self resource]
  (let [pb-type (get-in factory-types [factory-type :pb-type])
        factory (protobuf/read-text pb-type resource)]
    (g/set-property self
                    :factory-type factory-type
                    :prototype (workspace/resolve-resource resource (:prototype factory)))))


(g/defnode FactoryNode
  (inherits project/ResourceNode)

  (input dep-build-targets g/Any)
  (input prototype-resource resource/Resource)

  (property factory-type g/Any
            (dynamic visible (g/always false)))

  (property prototype resource/Resource
            (value (gu/passthrough prototype-resource))
            (set (fn [basis self old-value new-value]
                   (project/resource-setter basis self old-value new-value
                                            [:resource :prototype-resource]
                                            [:build-targets :dep-build-targets])))
            (dynamic error (g/fnk [_node-id prototype-resource]
                                  (or (validation/prop-error :info _node-id :prototype validation/prop-nil? prototype-resource "Prototype")
                                      (validation/prop-error :fatal _node-id :prototype validation/prop-resource-not-exists? prototype-resource "Prototype"))))
            (dynamic edit-type (g/fnk [factory-type]
                                 {:type resource/Resource :ext (get-in factory-types [factory-type :ext])})))

  (output form-data g/Any produce-form-data)

  (output node-outline outline/OutlineData :cached (g/fnk [_node-id factory-type]
                                                     {:node-id _node-id
                                                      :label (get-in factory-types [factory-type :title])
                                                      :icon (get-in factory-types [factory-type :icon])}))

  (output pb-msg g/Any :cached produce-pb-msg)
  (output save-data g/Any :cached produce-save-data)
  (output build-targets g/Any :cached produce-build-targets))


(defn register-resource-types
  [workspace]
  (concat
   (workspace/register-resource-type workspace
                                     :ext "factory"
                                     :node-type FactoryNode
                                     :load-fn (partial load-factory :game-object)
                                     :icon (get-in factory-types [:game-object :icon])
                                     :view-types [:form-view :text]
                                     :view-opts {}
                                     :tags #{:component}
                                     :label "Factory Object")
   (workspace/register-resource-type workspace
                                     :ext "collectionfactory"
                                     :node-type FactoryNode
                                     :load-fn (partial load-factory :collection)
                                     :icon (get-in factory-types [:collection :icon])
                                     :view-types [:form-view :text]
                                     :view-opts {}
                                     :tags #{:component}
                                     :label "Collection Factory Object")))
