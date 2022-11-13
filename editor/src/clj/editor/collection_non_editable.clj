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

(ns editor.collection-non-editable
  (:require [dynamo.graph :as g]
            [editor.build-target :as bt]
            [editor.collection-common :as collection-common]
            [editor.defold-project :as project]
            [editor.game-object-common :as game-object-common]
            [editor.game-object-non-editable :as game-object-non-editable]
            [editor.geom :as geom]
            [editor.graph-util :as gu]
            [editor.math :as math]
            [editor.outline :as outline]
            [editor.properties :as properties]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-io :as resource-io]
            [editor.resource-node :as resource-node]
            [editor.workspace :as workspace]
            [internal.util :as util]
            [util.coll :refer [pair]])
  (:import [com.dynamo.gameobject.proto GameObject$CollectionDesc]
           [javax.vecmath Matrix4d]))

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

(defn- any-instance-desc->transform-matrix
  ^Matrix4d [{:keys [position rotation scale3] :as any-instance-desc}]
  ;; GameObject$InstanceDesc, GameObject$EmbeddedInstanceDesc, or GameObject$CollectionInstanceDesc in map format.
  (let [corrected-scale (if (or (nil? scale3)
                                (protobuf/default-read-scale-value? scale3))
                          (or (:scale any-instance-desc) 1.0) ; Legacy file format - use uniform scale.
                          scale3)]
    (math/clj->mat4 position rotation corrected-scale)))

(defn- component-property-desc-with-go-props [component-property-desc proj-path->source-resource]
  ;; GameObject$ComponentPropertyDesc in map format.
  (if-some [property-descs (not-empty (:properties component-property-desc))]
    (assoc component-property-desc :properties (mapv #(properties/property-desc->go-prop % proj-path->source-resource) property-descs))
    (dissoc component-property-desc :properties)))

(defn- instance-desc-with-go-props [instance-desc proj-path->source-resource]
  ;; GameObject$InstanceDesc or GameObject$EmbeddedInstanceDesc in map format.
  (if-some [component-property-descs (not-empty (:component-properties instance-desc))]
    (assoc instance-desc :component-properties (mapv #(component-property-desc-with-go-props % proj-path->source-resource) component-property-descs))
    (dissoc instance-desc :component-properties)))

(defn- instance-property-desc-with-go-props [instance-property-desc proj-path->source-resource]
  ;; GameObject$InstancePropertyDesc in map format.
  (if-some [component-property-descs (not-empty (:properties instance-property-desc))]
    (assoc instance-property-desc :properties (mapv #(component-property-desc-with-go-props % proj-path->source-resource) component-property-descs))
    (dissoc instance-property-desc :properties)))

(defn- game-object-instance-build-target [build-resource instance-desc ^Matrix4d transform-matrix game-object-build-target proj-path->resource-property-build-target]
  ;; GameObject$InstanceDesc or GameObject$EmbeddedInstanceDesc in map format.
  (let [proj-path->source-resource (comp :resource :resource proj-path->resource-property-build-target)
        instance-desc-with-go-props (cond-> (instance-desc-with-go-props instance-desc proj-path->source-resource)

                                            (empty? (:children instance-desc))
                                            (dissoc :children))]
    (collection-common/game-object-instance-build-target build-resource instance-desc-with-go-props transform-matrix game-object-build-target proj-path->resource-property-build-target)))

(defn- instance-desc->game-object-instance-build-target [instance-desc game-object-build-target proj-path->build-target]
  ;; GameObject$InstanceDesc in map format.
  (let [build-resource (:resource game-object-build-target)
        transform-matrix (any-instance-desc->transform-matrix instance-desc)]
    (game-object-instance-build-target build-resource instance-desc transform-matrix game-object-build-target proj-path->build-target)))

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

(defn- embedded-instance-desc->game-object-instance-build-target [embedded-instance-desc collection-node-id component-build-targets embedded-component-desc->build-resource proj-path->build-target workspace]
  {:pre [(map? embedded-instance-desc)]} ; GameObject$EmbeddedInstanceDesc in map format.
  (let [prototype-desc (:data embedded-instance-desc)
        embedded-instance-desc (dissoc embedded-instance-desc :data) ; We don't need or want the :data in the GameObject$EmbeddedInstanceDesc.
        component-instance-datas (game-object-non-editable/prototype-desc->component-instance-datas prototype-desc embedded-component-desc->build-resource proj-path->build-target)
        embedded-game-object-build-target (game-object-common/game-object-build-target nil collection-node-id component-instance-datas component-build-targets)
        embedded-game-object-resource (workspace/make-embedded-resource workspace :non-editable "go" (:content-hash embedded-game-object-build-target)) ; Content determines hash for merging with embedded components in other .go files.
        embedded-game-object-build-resource (workspace/make-build-resource embedded-game-object-resource)
        transform-matrix (any-instance-desc->transform-matrix embedded-instance-desc)]
    (game-object-instance-build-target embedded-game-object-build-resource embedded-instance-desc transform-matrix embedded-game-object-build-target proj-path->build-target)))

(g/defnk produce-embedded-game-object-instance-build-targets [_node-id collection-desc embedded-component-resource-data->index embedded-component-build-targets referenced-component-build-targets resource proj-path->build-target]
  (let [workspace (resource/workspace resource)
        embedded-component-desc->build-resource (game-object-non-editable/make-embedded-component-desc->build-resource embedded-component-build-targets embedded-component-resource-data->index)
        component-build-targets (into referenced-component-build-targets embedded-component-build-targets)]
    (mapv #(embedded-instance-desc->game-object-instance-build-target % _node-id component-build-targets embedded-component-desc->build-resource proj-path->build-target workspace)
          (:embedded-instances collection-desc))))

(g/defnk produce-build-targets [_node-id resource collection-desc embedded-game-object-instance-build-targets referenced-game-object-instance-build-targets proj-path->build-target]
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

              proj-path->source-resource
              (comp :resource :resource proj-path->build-target)

              collection-instance-build-targets
              (mapv (fn [collection-instance-desc]
                      (let [collection-instance-id (:id collection-instance-desc)
                            transform-matrix (any-instance-desc->transform-matrix collection-instance-desc)
                            referenced-collection-build-target (proj-path->build-target (:collection collection-instance-desc))
                            instance-property-descs (mapv #(instance-property-desc-with-go-props % proj-path->source-resource) (:instance-properties collection-instance-desc))]
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

(defn- embedded-instance-desc->instance-property-desc [embedded-instance-desc]
  ;; GameObject$EmbeddedInstanceDesc in map format.
  (let [game-object-instance-id (:id embedded-instance-desc)
        component-property-descs (override-component-property-descs
                                   (game-object-non-editable/prototype-desc->component-property-descs (:data embedded-instance-desc))
                                   (:component-properties embedded-instance-desc))]
    (maybe-instance-property-desc game-object-instance-id component-property-descs)))

(defn- instance-desc->instance-property-desc [instance-desc]
  ;; GameObject$InstanceDesc in map format.
  (maybe-instance-property-desc (:id instance-desc) (:component-properties instance-desc)))

(defn- collection-instance-desc->instance-property-descs [collection-instance-desc]
  ;; GameObject$CollectionInstanceDesc in map format.
  (keep (fn [instance-property-desc]
          (maybe-instance-property-desc (:id instance-property-desc) (:properties instance-property-desc)))
        (:instance-properties collection-instance-desc)))

(defn- collection-desc->instance-property-descs [collection-desc]
  (vec
    (concat
      (keep instance-desc->instance-property-desc (:instances collection-desc))
      (keep embedded-instance-desc->instance-property-desc (:embedded-instances collection-desc))
      (mapcat collection-instance-desc->instance-property-descs (:collection-instances collection-desc)))))

(g/defnk produce-ddf-properties [collection-desc]
  (collection-desc->instance-property-descs collection-desc))

(defn- instance-property-descs->resources [instance-property-descs proj-path->resource]
  (eduction
    (map :properties)
    (mapcat #(game-object-non-editable/component-property-descs->resources % proj-path->resource))
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
    (mapcat game-object-non-editable/prototype-desc->referenced-component-proj-paths)
    (distinct)
    (:embedded-instances collection-desc)))

(defn- collection-desc->embedded-component-resource-datas [collection-desc]
  (eduction
    (map :data)
    (mapcat game-object-non-editable/prototype-desc->embedded-component-resource-datas)
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
  ;; resource-property-build-targets input of our NonEditableCollectionNode.
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

(g/defnode NonEditableCollectionNode
  (inherits game-object-non-editable/EmbeddedComponentHostResourceNode)

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
  (output referenced-component-build-targets g/Any :cached (g/fnk [referenced-component-build-targets] (mapv util/only-or-throw referenced-component-build-targets)))
  (output proj-path->build-target g/Any :cached produce-proj-path->build-target)
  (output referenced-game-object-instance-build-targets g/Any :cached produce-referenced-game-object-instance-build-targets)
  (output embedded-game-object-instance-build-targets g/Any :cached produce-embedded-game-object-instance-build-targets)

  ;; Collection interface.
  (output build-targets g/Any :cached produce-build-targets)
  (output ddf-properties g/Any :cached produce-ddf-properties)
  (output node-outline outline/OutlineData produce-node-outline)
  (output resource-property-build-targets g/Any (gu/passthrough resource-property-build-targets))
  (output scene g/Any produce-scene))

(defn- sanitize-non-editable-collection [workspace collection-desc]
  (let [ext->embedded-component-resource-type (workspace/get-resource-type-map workspace :non-editable)]
    (collection-common/sanitize-collection-desc collection-desc ext->embedded-component-resource-type :embed-data-as-maps)))

(defn- load-non-editable-collection [_project self _resource collection-desc]
  (g/set-property self :collection-desc collection-desc))

(defn register-resource-types [workspace]
  (resource-node/register-ddf-resource-type workspace
    :editable false
    :ext "collection"
    :label "Non-Editable Collection"
    :node-type NonEditableCollectionNode
    :ddf-type GameObject$CollectionDesc
    :dependencies-fn (collection-common/make-collection-dependencies-fn #(workspace/get-resource-type workspace :non-editable "go"))
    :sanitize-fn (partial sanitize-non-editable-collection workspace)
    :load-fn load-non-editable-collection
    :icon collection-common/collection-icon
    :view-types [:scene :text]
    :view-opts {:scene {:grid true}}))
