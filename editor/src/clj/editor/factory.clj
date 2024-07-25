;; Copyright 2020-2024 The Defold Foundation
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

(ns editor.factory
  (:require [dynamo.graph :as g]
            [editor.build-target :as bt]
            [editor.defold-project :as project]
            [editor.graph-util :as gu]
            [editor.outline :as outline]
            [editor.protobuf :as protobuf]
            [editor.protobuf-forms-util :as protobuf-forms-util]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.validation :as validation]
            [editor.workspace :as workspace])
  (:import [com.dynamo.gamesys.proto GameSystem$CollectionFactoryDesc GameSystem$FactoryDesc]))

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
  [_node-id factory-type prototype-resource load-dynamically dynamic-prototype]
  {:form-ops {:user-data {:node-id _node-id}
              :set protobuf-forms-util/set-form-op
              :clear protobuf-forms-util/clear-form-op}
   :navigation false
   :sections [{:title (get-in factory-types [factory-type :title])
               :fields [{:path [:prototype]
                         :label "Prototype"
                         :type :resource
                         :filter (get-in factory-types [factory-type :ext])}
                        {:path [:load-dynamically]
                         :label "Load Dynamically"
                         :type :boolean}
                        {:path [:dynamic-prototype]
                         :label "Dynamic Prototype"
                         :type :boolean}]}]
   :values {[:prototype] prototype-resource
            [:load-dynamically] load-dynamically
            [:dynamic-prototype] dynamic-prototype}})

(g/defnk produce-save-value
  [prototype-resource load-dynamically dynamic-prototype factory-type]
  (let [pb-class (-> factory-types factory-type :pb-type)]
    (protobuf/make-map-without-defaults pb-class
      :prototype (resource/resource->proj-path prototype-resource)
      :load-dynamically load-dynamically
      :dynamic-prototype dynamic-prototype)))

(defn build-factory
  [resource dep-resources user-data]
  (let [pb-msg (reduce #(assoc %1 (first %2) (second %2))
                       (:pb-msg user-data)
                       (map (fn [[label res]] [label (resource/proj-path (get dep-resources res))]) (:dep-resources user-data)))]
    {:resource resource
     :content (protobuf/map->bytes (:pb-type user-data) pb-msg)}))

(g/defnk produce-build-targets
  [_node-id resource factory-type prototype save-value dep-build-targets]
  (or (validation/prop-error :fatal _node-id :prototype validation/prop-nil? prototype "prototype")
      (let [dep-build-targets (flatten dep-build-targets)
            deps-by-resource (into {} (map (juxt (comp :resource :resource) :resource) dep-build-targets))
            dep-resources (map (fn [[label resource]]
                                 [label (get deps-by-resource resource)])
                               [[:prototype prototype]])]
        [(bt/with-content-hash
           {:node-id _node-id
            :resource (workspace/make-build-resource resource)
            :build-fn build-factory
            :user-data {:pb-msg save-value
                        :pb-type (get-in factory-types [factory-type :pb-type])
                        :dep-resources dep-resources}
            :deps dep-build-targets})])))

(defn load-factory
  [factory-type _project self resource any-factory-desc]
  {:pre [(contains? factory-types factory-type)
         (map? any-factory-desc)]} ; GameSystem$FactoryDesc or GameSystem$CollectionFactoryDesc in map format.
  (let [pb-class (:pb-type (get factory-types factory-type))
        resolve-resource #(workspace/resolve-resource resource %)]
    (into [(g/set-property self :factory-type factory-type)]
          (gu/set-properties-from-pb-map self pb-class any-factory-desc
            prototype (resolve-resource :prototype)
            load-dynamically :load-dynamically
            dynamic-prototype :dynamic-prototype))))

;; For these fields, we use the default from GameSystem$FactoryDesc in both
;; cases since we share a single defnode between two protobuf classes.
(assert (= (protobuf/default GameSystem$FactoryDesc :load-dynamically)
           (protobuf/default GameSystem$CollectionFactoryDesc :load-dynamically)))

(assert (= (protobuf/default GameSystem$FactoryDesc :dynamic-prototype)
           (protobuf/default GameSystem$CollectionFactoryDesc :dynamic-prototype)))

(g/defnode FactoryNode
  (inherits resource-node/ResourceNode)

  (input dep-build-targets g/Any)
  (input prototype-resource resource/Resource)

  (property factory-type g/Any ; Always assigned in load-fn.
            (dynamic visible (g/constantly false)))

  (property prototype resource/Resource ; Required protobuf field.
            (value (gu/passthrough prototype-resource))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter evaluation-context self old-value new-value
                                            [:resource :prototype-resource]
                                            [:build-targets :dep-build-targets])))
            (dynamic error (g/fnk [_node-id prototype-resource]
                                  (or (validation/prop-error :info _node-id :prototype validation/prop-nil? prototype-resource "Prototype")
                                      (validation/prop-error :fatal _node-id :prototype validation/prop-resource-not-exists? prototype-resource "Prototype"))))
            (dynamic edit-type (g/fnk [factory-type]
                                 {:type resource/Resource :ext (get-in factory-types [factory-type :ext])})))
  (property load-dynamically g/Bool (default (protobuf/default GameSystem$FactoryDesc :load-dynamically)))
  (property dynamic-prototype g/Bool (default (protobuf/default GameSystem$FactoryDesc :dynamic-prototype)))

  (output form-data g/Any produce-form-data)

  (output node-outline outline/OutlineData :cached (g/fnk [_node-id factory-type prototype]
                                                     (let [label (get-in factory-types [factory-type :title])
                                                           icon (get-in factory-types [factory-type :icon])]
                                                       (cond-> {:node-id _node-id
                                                                :node-outline-key label
                                                                :label label
                                                                :icon icon}

                                                               (resource/openable-resource? prototype)
                                                               (assoc :link prototype :outline-reference? false)))))

  (output save-value g/Any :cached produce-save-value)
  (output build-targets g/Any :cached produce-build-targets))


(defn register-resource-types
  [workspace]
  (concat
    (resource-node/register-ddf-resource-type workspace
      :textual? true
      :ext "factory"
      :node-type FactoryNode
      :ddf-type GameSystem$FactoryDesc
      :load-fn (partial load-factory :game-object)
      :icon (get-in factory-types [:game-object :icon])
      :icon-class :property
      :view-types [:cljfx-form-view :text]
      :view-opts {}
      :tags #{:component}
      :tag-opts {:component {:transform-properties #{}}}
      :label "Factory")
    (resource-node/register-ddf-resource-type workspace
      :textual? true
      :ext "collectionfactory"
      :node-type FactoryNode
      :ddf-type GameSystem$CollectionFactoryDesc
      :load-fn (partial load-factory :collection)
      :icon (get-in factory-types [:collection :icon])
      :icon-class :property
      :view-types [:cljfx-form-view :text]
      :view-opts {}
      :tags #{:component}
      :tag-opts {:component {:transform-properties #{}}}
      :label "Collection Factory")))
