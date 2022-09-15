;; Copyright 2020-2022 The Defold Foundation
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

(ns editor.collection-data
  (:require [dynamo.graph :as g]
            [editor.build-target :as bt]
            [editor.collection-common :as collection-common]
            [editor.collection-string-data :as collection-string-data]
            [editor.defold-project :as project]
            [editor.game-object-common :as game-object-common]
            [editor.game-object-data :as game-object-data]
            [editor.geom :as geom]
            [editor.graph-util :as gu]
            [editor.resource :as resource]
            [editor.resource-io :as resource-io]
            [editor.resource-node :as resource-node]
            [editor.workspace :as workspace]
            [util.coll :refer [pair]])
  (:import [com.dynamo.gameobject.proto GameObject$CollectionDesc]))

(set! *warn-on-reflection* true)

(defn- file-not-found-error [referencing-node-id referencing-resource referenced-proj-path]
  (let [referenced-resource (workspace/resolve-resource referencing-resource referenced-proj-path)]
    (resource-io/file-not-found-error referencing-node-id nil :fatal referenced-resource)))

(g/defnk produce-proj-path->build-target [referenced-component-build-targets referenced-collection-build-targets referenced-game-object-build-targets resource-property-build-targets]
  (bt/make-proj-path->build-target
    (concat referenced-collection-build-targets
            referenced-game-object-build-targets
            referenced-component-build-targets
            resource-property-build-targets)))

(defn- instance-desc->game-object-instance-build-target [instance-desc game-object-build-target proj-path->build-target]
  ;; GameObject$InstanceDesc in map format.
  (let [build-resource (:resource game-object-build-target)
        transform-matrix (game-object-data/any-instance-desc->transform-matrix instance-desc)]
    (collection-common/game-object-instance-build-target build-resource instance-desc transform-matrix game-object-build-target proj-path->build-target)))

(g/defnk produce-referenced-game-object-instance-build-targets [_node-id collection-desc proj-path->build-target resource]
  (let [build-targets
        (mapv (fn [instance-desc]
                (let [referenced-game-object-proj-path (:prototype instance-desc)
                      game-object-build-target (proj-path->build-target referenced-game-object-proj-path)]
                  (if (nil? game-object-build-target)
                    (file-not-found-error _node-id resource referenced-game-object-proj-path)
                    (instance-desc->game-object-instance-build-target instance-desc game-object-build-target proj-path->build-target))))
              (:instances collection-desc))]
    (or (g/flatten-errors build-targets)
        build-targets)))

(defn- embedded-instance-desc->game-object-instance-build-target [embedded-instance-desc collection-node-id component-build-targets embedded-component-desc->build-resource proj-path->build-target ext->resource-type workspace]
  ;; GameObject$EmbeddedInstanceDesc in map format.
  (let [prototype-desc (:data embedded-instance-desc)
        component-instance-datas (game-object-data/prototype-desc->component-instance-datas prototype-desc embedded-component-desc->build-resource proj-path->build-target)
        embedded-game-object-pb-string (collection-string-data/string-encode-game-object-data ext->resource-type prototype-desc)
        embedded-game-object-resource (workspace/make-embedded-resource workspace "go" embedded-game-object-pb-string)
        embedded-game-object-build-resource (workspace/make-build-resource embedded-game-object-resource)
        embedded-game-object-build-target (game-object-common/game-object-build-target embedded-game-object-build-resource collection-node-id component-instance-datas component-build-targets)
        transform-matrix (game-object-data/any-instance-desc->transform-matrix embedded-instance-desc)]
    (collection-common/game-object-instance-build-target embedded-game-object-build-resource embedded-instance-desc transform-matrix embedded-game-object-build-target proj-path->build-target)))

(g/defnk produce-embedded-game-object-instance-build-targets [_node-id collection-desc embedded-component-resource-data->index embedded-component-build-targets referenced-component-build-targets proj-path->build-target]
  (let [project (project/get-project _node-id)
        workspace (project/workspace project)
        ext->resource-type (workspace/get-resource-type-map workspace)
        embedded-component-desc->build-resource (game-object-data/make-embedded-component-desc->build-resource embedded-component-build-targets embedded-component-resource-data->index)
        component-build-targets (into embedded-component-build-targets referenced-component-build-targets)]
    (mapv #(embedded-instance-desc->game-object-instance-build-target % _node-id component-build-targets embedded-component-desc->build-resource proj-path->build-target ext->resource-type workspace)
          (:embedded-instances collection-desc))))

(g/defnk produce-build-targets [_node-id resource collection-desc embedded-game-object-instance-build-targets referenced-game-object-instance-build-targets proj-path->build-target]
  ;; In CollectionNode,
  ;; Collection Instances are connected to :sub-build-targets.
  ;; Game Object Instances are connected to :dep-build-targets.

  ;; TODO: Monday
  ;; We should focus on making build targets for ourself and each embedded game
  ;; object, and connect referenced .go and .collection files as dep-build-targets.
  ;; In addition, every resource referenced from a script property or override
  ;; in our .collection will need to be in :deps as well.

  ;; The sideband of resource-property-build-targets are there to support the
  ;; properties/build-target-go-props function as we flatten instance-data.
  ;; We should take these from referenced .go and .collection nodes, synthesize
  ;; our own from any overrides we might be applying, and pass our
  ;; property-descs through properties/build-target-go-props and eventually
  ;; obtain the distinct set of go-prop-dep-build-targets to add as our deps.

  ;; Our build target needs to feature :instance-data, so we can be referenced
  ;; by "real" CollectionInstanceNodes. We should probably use the existing
  ;; collection/build-collection function as our :build-fn.
  (let [instance-descs (:instances collection-desc)
        embedded-instance-descs (:embedded-instances collection-desc)
        collection-instance-descs (:collection-instances collection-desc)]
    (or (let [dup-ids (game-object-common/any-descs->duplicate-ids
                        (concat instance-descs
                                embedded-instance-descs
                                collection-instance-descs))]
          (game-object-common/maybe-duplicate-id-error _node-id dup-ids))
        (let [build-resource (workspace/make-build-resource resource)
              name (:name collection-desc)
              scale-along-z (not= 0 (:scale-along-z collection-desc))

              game-object-instance-build-targets
              (into embedded-game-object-instance-build-targets
                    referenced-game-object-instance-build-targets)

              collection-instance-build-targets
              (mapv (fn [collection-instance-desc]
                      (let [collection-instance-id (:id collection-instance-desc)
                            transform-matrix (game-object-data/any-instance-desc->transform-matrix collection-instance-desc)
                            instance-property-descs (:instance-properties collection-instance-desc)
                            referenced-collection-build-target (proj-path->build-target (:collection collection-instance-desc))]
                        (collection-common/collection-instance-build-target collection-instance-id transform-matrix instance-property-descs referenced-collection-build-target proj-path->build-target)))
                    collection-instance-descs)]
          [(collection-common/collection-build-target build-resource _node-id name scale-along-z game-object-instance-build-targets collection-instance-build-targets)]))))

(defn component-property-desc-overrides-properties? [component-property-desc]
  (not (empty? (:properties component-property-desc))))

(defn- maybe-instance-property-desc [game-object-instance-id component-property-descs]
  (when-some [component-property-descs (not-empty (filterv component-property-desc-overrides-properties? component-property-descs))]
    {:id game-object-instance-id
     :properties component-property-descs}))

(defn- component-property-descs->component-id->property-descs [component-property-descs]
  (into {}
        (keep (fn [component-property-desc]
                (let [component-id (:id component-property-desc)
                      property-descs (:properties component-property-desc)]
                  (when (seq property-descs)
                    (pair component-id property-descs)))))
        component-property-descs))

(defn override-component-property-descs [original-component-property-descs override-component-property-descs]
  ;; Takes two sequences of GameObject$ComponentPropertyDescs in map format, and
  ;; returns a sequence of GameObject$ComponentPropertyDescs in map format.
  (map (fn [[component-id property-descs]]
         {:id component-id
          :properties property-descs})
       (merge-with collection-common/override-property-descs
                   (component-property-descs->component-id->property-descs original-component-property-descs)
                   (component-property-descs->component-id->property-descs override-component-property-descs))))

(defn- embedded-instance-desc->instance-property-descs [embedded-instance-desc]
  ;; GameObject$EmbeddedInstanceDesc in map format.
  (let [game-object-instance-id (:id embedded-instance-desc)
        component-property-descs (override-component-property-descs
                                   (game-object-data/prototype-desc->component-property-descs (:data embedded-instance-desc))
                                   (:component-properties embedded-instance-desc))]
    (some-> (maybe-instance-property-desc game-object-instance-id component-property-descs)
            vector)))

(defn- instance-desc->instance-property-descs [instance-desc]
  ;; GameObject$InstanceDesc in map format.
  (some-> (maybe-instance-property-desc (:id instance-desc) (:component-properties instance-desc))
          vector))

(defn- collection-instance-desc->instance-property-descs [collection-instance-desc]
  ;; GameObject$CollectionInstanceDesc in map format.
  (keep (fn [instance-property-desc]
          (maybe-instance-property-desc (:id instance-property-desc) (:properties instance-property-desc)))
        (:instance-properties collection-instance-desc)))

(defn- collection-desc->instance-property-descs [collection-desc]
  ;; vector of InstancePropertyDesc
  ;; => {:id game-object-instance-id
  ;;     :properties [ComponentPropertyDesc]}
  ;; passed through
  ;; EmbeddedGOInstanceNode.ddf-properties
  ;; ReferencedGOInstanceNode.ddf-properties
  ;; CollectionNode.ddf-properties
  ;; CollectionInstanceNode.sub-ddf-properties
  ;; from
  ;; GameObjectNode.ddf-component-properties
  ;; a vector of ComponentPropertyDesc
  ;; => {:id (:id component-ddf)
  ;;     :properties [PropertyDesc](:properties component-ddf)}
  ;; from
  ;; EmbeddedComponent.ddf-message
  ;; ReferencedComponent.ddf-message
  ;; containing :properties [PropertyDesc]
  ;; a vector of PropertyDesc
  ;; => {:id "script_property_name"
  ;;     :type :property-type-vector4
  ;;     :value "0.0, 0.0, 0.0, 1.0"}
  #_
          [{:id "game_object_id"
            :properties [{:id "component_id"
                          :properties [{:id "property_name"
                                        :value "150"
                                        :type :property-type-number}]}]}]
  (vec
    (concat
      (keep instance-desc->instance-property-descs (:instances collection-desc))
      (keep embedded-instance-desc->instance-property-descs (:embedded-instances collection-desc))
      (keep collection-instance-desc->instance-property-descs (:collection-instances collection-desc)))))

(g/defnk produce-ddf-properties [collection-desc]
  (collection-desc->instance-property-descs collection-desc))

(defn- instance-property-descs->resources [instance-property-descs proj-path->resource]
  (eduction
    (map :properties)
    (mapcat #(game-object-data/component-property-descs->resources % proj-path->resource))
    (distinct)
    instance-property-descs))

(defn- collection-desc->referenced-collection-proj-paths [collection-desc]
  (eduction
    (map :collection)
    (distinct)
    (:collection-instances collection-desc)))

(defn- collection-desc->referenced-game-object-proj-paths [collection-desc]
  (eduction
    (map :prototype)
    (distinct)
    (:instances collection-desc)))

(defn- collection-desc->referenced-component-proj-paths [collection-desc]
  (eduction
    (map :data)
    (mapcat game-object-data/prototype-desc->referenced-component-proj-paths)
    (distinct)
    (:embedded-instances collection-desc)))

(defn- collection-desc->embedded-component-resource-datas [collection-desc]
  (eduction
    (map :data)
    (mapcat game-object-data/prototype-desc->embedded-component-resource-datas)
    (distinct)
    (:embedded-instances collection-desc)))

(defn- collection-desc->embedded-component-resource-data->index [collection-desc]
  (into {}
        (map-indexed (fn [index embedded-component-resource-data]
                       (pair embedded-component-resource-data index)))
        (collection-desc->embedded-component-resource-datas collection-desc)))

(defn- collection-desc->referenced-property-resources [collection-desc proj-path->resource]
  ;; This returns a sequence of all distinct resources referenced by
  ;; ComponentPropertyDesc property overrides in the CollectionDescs contained
  ;; InstanceDescs, EmbeddedInstanceDescs, and CollectionInstanceDescs.
  ;;
  ;; The resulting resources build targets will be connected to the
  ;; resource-property-build-targets input of our CollectionDataNode.
  ;;
  ;; Elsewhere, any referenced collection, game object, and component will also
  ;; have their resource-property-build-targets output connected to our input to
  ;; ensure we have access to the non-overridden resource property dependencies.
  ;; As a result, resource-property-build-targets will include not only our own
  ;; overrides, but the set union of all original and overridden resource
  ;; property dependencies. This is also how it works for mutable collections.
  (-> collection-desc
      collection-desc->instance-property-descs
      (instance-property-descs->resources proj-path->resource)))

(g/defnk produce-go-inst-ids []
  ;; TODO! Can't produce, since we don't have GameObjectInstanceNodes.
  {})

(g/defnk produce-node-outline [_node-id]
  {:node-id _node-id
   :node-outline-key "Collection Data"
   :label "Collection Data"
   :icon collection-common/collection-icon})

(g/defnk produce-scene [_node-id]
  {:node-id _node-id
   :aabb geom/null-aabb})

(def ^:private referenced-collection-connections
  [[:build-targets :referenced-collection-build-targets]
   [:resource-property-build-targets :resource-property-build-targets]])

(def ^:private referenced-game-object-connections
  [[:build-targets :referenced-game-object-build-targets]
   [:resource-property-build-targets :resource-property-build-targets]])

(def ^:private referenced-component-connections
  [[:build-targets :referenced-component-build-targets]
   [:resource-property-build-targets :resource-property-build-targets]])

(def ^:private resource-property-connections
  [[:build-targets :resource-property-build-targets]])

(g/defnode CollectionDataNode
  (inherits game-object-data/EmbeddedComponentHostResourceNode)

  (property collection-desc g/Any
            (dynamic visible (g/constantly false))
            (set (fn [evaluation-context self old-value new-value]
                   ;; We use default evaluation-context in queries to ensure the
                   ;; results are cached. See comment in connect-resource-node.
                   (let [basis (:basis evaluation-context)
                         project (project/get-project basis self)
                         workspace (project/workspace project)
                         proj-path->resource (g/node-value workspace :resource-map)]
                     (letfn [(disconnect-resource [proj-path-or-resource connections]
                               (project/disconnect-resource-node evaluation-context project proj-path-or-resource self connections))
                             (connect-resource [proj-path-or-resource connections]
                               (:tx-data (project/connect-resource-node evaluation-context project proj-path-or-resource self connections)))]
                       (-> (g/set-property self :embedded-component-resource-data->index
                             (collection-desc->embedded-component-resource-data->index new-value))
                           (into (mapcat #(disconnect-resource % referenced-collection-connections))
                                 (collection-desc->referenced-collection-proj-paths old-value))
                           (into (mapcat #(disconnect-resource % referenced-game-object-connections))
                                 (collection-desc->referenced-game-object-proj-paths old-value))
                           (into (mapcat #(disconnect-resource % referenced-component-connections))
                                 (collection-desc->referenced-component-proj-paths old-value))
                           (into (mapcat #(disconnect-resource % resource-property-connections))
                                 (collection-desc->referenced-property-resources old-value proj-path->resource))
                           (into (mapcat #(connect-resource % referenced-collection-connections))
                                 (collection-desc->referenced-collection-proj-paths new-value))
                           (into (mapcat #(connect-resource % referenced-game-object-connections))
                                 (collection-desc->referenced-game-object-proj-paths new-value))
                           (into (mapcat #(connect-resource % referenced-component-connections))
                                 (collection-desc->referenced-component-proj-paths new-value))
                           (into (mapcat #(connect-resource % resource-property-connections))
                                 (collection-desc->referenced-property-resources new-value proj-path->resource))))))))

  (input referenced-collection-build-targets g/Any :array)
  (input referenced-game-object-build-targets g/Any :array)
  (input referenced-component-build-targets g/Any :array)
  (input resource-property-build-targets g/Any :array)

  ;; Internal outputs.
  (output proj-path->build-target g/Any :cached produce-proj-path->build-target)
  (output referenced-game-object-instance-build-targets g/Any :cached produce-referenced-game-object-instance-build-targets)
  (output embedded-game-object-instance-build-targets g/Any :cached produce-embedded-game-object-instance-build-targets)

  ;; Collection interface.
  (output build-targets g/Any :cached produce-build-targets)
  (output ddf-properties g/Any :cached produce-ddf-properties)
  (output go-inst-ids g/Any produce-go-inst-ids)
  (output node-outline g/Any produce-node-outline)
  (output resource-property-build-targets g/Any (gu/passthrough resource-property-build-targets))
  (output scene g/Any produce-scene))

(defn- load-collection-data [_project self resource pb-map]
  (let [workspace (resource/workspace resource)
        ext->resource-type (workspace/get-resource-type-map workspace)
        collection-desc (collection-string-data/string-decode-collection-data ext->resource-type pb-map)]
    (g/set-property self :collection-desc collection-desc)))

(defn register-resource-types [workspace]
  (resource-node/register-ddf-resource-type workspace
    :ext "collection"
    :label "Collection Data"
    :node-type CollectionDataNode
    :ddf-type GameObject$CollectionDesc
    :dependencies-fn (collection-common/make-collection-dependencies-fn workspace)
    :load-fn load-collection-data
    :auto-connect-save-data? false
    :icon collection-common/collection-icon
    :view-types [:scene :text]
    :view-opts {:scene {:grid true}}))

;; Parent CollectionNode -> CollectionInstanceNode
[:base-url :base-url]
[:id-counts :id-counts]

;; CollectionInstanceNode -> Parent CollectionNode
[:_node-id :nodes]
[:build-targets :sub-build-targets]
[:ddf-message :ref-coll-ddf]
[:go-inst-ids :go-inst-ids]
[:id :ids]
[:node-outline :child-outlines]
[:resource-property-build-targets :resource-property-build-targets]
[:scene :child-scenes]
[:sub-ddf-properties :ddf-properties]

;; ReferencedGOInstanceNode -> Parent CollectionNode
[:_node-id :nodes]
[:build-targets :dep-build-targets]
[:ddf-message :ddf-label]
[:ddf-properties :ddf-properties]
[:go-inst-ids :go-inst-ids]
[:id :ids]
[:node-outline :child-outlines]
[:resource-property-build-targets :resource-property-build-targets]
[:scene :child-scenes]

;; Parent CollectionNode -> ReferencedGOInstanceNode
[:base-url :base-url]
[:id-counts :id-counts]

;; ReferencedGOInstanceNode -> ReferencedGOInstanceNode
[:_node-id :nodes]
[:id :child-ids]
[:node-outline :child-outlines]
[:scene :child-scenes]

;; Referenced GameObjectNode -> ReferencedGOInstanceNode
[:_node-id :source-id]
[:build-targets :source-build-targets]
[:ddf-component-properties :ddf-component-properties]
[:node-outline :source-outline]
[:resource :source-resource]
[:resource-property-build-targets :resource-property-build-targets]
[:scene :scene]

;; ReferencedGOInstanceNode -> Referenced GameObjectNode
[:url :base-url]

;; Referenced CollectionNode -> CollectionInstanceNode
[:_node-id :source-id]
[:build-targets :build-targets]
[:ddf-properties :ddf-properties]
[:go-inst-ids :go-inst-ids]
[:node-outline :source-outline]
[:resource :source-resource]
[:resource-property-build-targets :resource-property-build-targets]
[:scene :scene]

;; CollectionInstanceNode -> Referenced CollectionNode
[:url :base-url]

;; Referenced CollectionNode -> CollectionProxyNode
[:build-targets :dep-build-targets]
[:resource :collection-resource]

;; Referenced CollectionNode -> FactoryNode
[:build-targets :dep-build-targets]
[:resource :prototype-resource]

;; Referenced GameObjectNode -> FactoryNode
[:build-targets :dep-build-targets]
[:resource :prototype-resource]
