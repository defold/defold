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

(ns editor.collection-non-editable
  (:require [dynamo.graph :as g]
            [editor.attachment :as attachment]
            [editor.build-target :as bt]
            [editor.collection-common :as collection-common]
            [editor.collection-string-data :as collection-string-data]
            [editor.defold-project :as project]
            [editor.game-object-common :as game-object-common]
            [editor.game-object-non-editable :as game-object-non-editable]
            [editor.graph-util :as gu]
            [editor.localization :as localization]
            [editor.outline :as outline]
            [editor.pose :as pose]
            [editor.properties :as properties]
            [editor.resource :as resource]
            [editor.resource-io :as resource-io]
            [editor.resource-node :as resource-node]
            [editor.scene :as scene]
            [editor.workspace :as workspace]
            [internal.util :as util]
            [util.coll :as coll :refer [pair]])
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

(defn- any-instance-desc->pose [{:keys [position rotation scale3] :as _any-instance-desc}]
  ;; GameObject$InstanceDesc, GameObject$EmbeddedInstanceDesc, or GameObject$CollectionInstanceDesc in map format.
  (let [scale (or scale3 scene/default-scale)]
    (pose/make position rotation scale)))

(defn- any-instance-desc->transform-matrix
  ^Matrix4d [any-instance-desc]
  ;; GameObject$InstanceDesc, GameObject$EmbeddedInstanceDesc, or GameObject$CollectionInstanceDesc in map format.
  (pose/matrix (any-instance-desc->pose any-instance-desc)))

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

(defn- game-object-instance-build-target [game-object-build-target instance-desc pose proj-path->resource-property-build-target]
  ;; GameObject$InstanceDesc or GameObject$EmbeddedInstanceDesc in map format.
  (let [proj-path->source-resource (comp :resource :resource proj-path->resource-property-build-target)
        instance-desc-with-go-props (cond-> (instance-desc-with-go-props instance-desc proj-path->source-resource)

                                            (empty? (:children instance-desc))
                                            (dissoc :children))]
    (collection-common/game-object-instance-build-target game-object-build-target instance-desc-with-go-props pose proj-path->resource-property-build-target)))

(defn- instance-desc->game-object-instance-build-target [instance-desc game-object-build-target proj-path->build-target]
  ;; GameObject$InstanceDesc in map format.
  (let [pose (any-instance-desc->pose instance-desc)]
    (game-object-instance-build-target game-object-build-target instance-desc pose proj-path->build-target)))

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
        embedded-game-object-resource (workspace/make-placeholder-resource workspace :non-editable "go")
        embedded-game-object-build-target (game-object-common/game-object-build-target embedded-game-object-resource collection-node-id component-instance-datas component-build-targets)
        pose (any-instance-desc->pose embedded-instance-desc)]
    (game-object-instance-build-target embedded-game-object-build-target embedded-instance-desc pose proj-path->build-target)))

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

              game-object-instance-build-targets
              (into embedded-game-object-instance-build-targets
                    referenced-game-object-instance-build-targets)

              proj-path->source-resource
              (comp :resource :resource proj-path->build-target)

              collection-instance-build-targets
              (mapv (fn [collection-instance-desc]
                      (let [collection-instance-id (:id collection-instance-desc)
                            pose (any-instance-desc->pose collection-instance-desc)
                            referenced-collection-build-target (proj-path->build-target (:collection collection-instance-desc))
                            instance-property-descs (mapv #(instance-property-desc-with-go-props % proj-path->source-resource) (:instance-properties collection-instance-desc))]
                        (collection-common/collection-instance-build-target collection-instance-id pose instance-property-descs referenced-collection-build-target proj-path->build-target)))
                    collection-instance-descs)]
          [(collection-common/collection-build-target build-resource _node-id name game-object-instance-build-targets collection-instance-build-targets)]))))

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

(defn- collection-desc->referenced-collection-resources [collection-desc proj-path->resource]
  (eduction
    (map :collection)
    (distinct)
    (map proj-path->resource)
    (:collection-instances collection-desc)))

(defn- collection-desc->referenced-game-object-resources [collection-desc proj-path->resource]
  (eduction
    (map :prototype)
    (distinct)
    (map proj-path->resource)
    (:instances collection-desc)))

(defn- collection-desc->referenced-component-resources [collection-desc proj-path->resource]
  (eduction
    (map :data)
    (mapcat game-object-non-editable/prototype-desc->referenced-component-proj-paths)
    (distinct)
    (map proj-path->resource)
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
  ;; own-resource-property-build-targets input of our NonEditableCollectionNode.
  ;;
  ;; Elsewhere, any referenced collection, game object, and component will have
  ;; their resource-property-build-targets output connected to our
  ;; other-resource-property-build-targets input to ensure we have access to the
  ;; non-overridden resource property dependencies. As a result, our
  ;; resource-property-build-targets output will include not only our own
  ;; overrides, but the set union of all original and overridden resource
  ;; property dependencies. This is also how it works for mutable collections.
  (-> collection-desc
      collection-desc->instance-property-descs
      (instance-property-descs->resources proj-path->resource)))

(g/defnk produce-node-outline [_node-id]
  {:node-id _node-id
   :node-outline-key "Non-Editable Collection"
   :label (localization/message "outline.non-editable-collection")
   :icon collection-common/collection-icon})

(defn- make-desc->instance-scene [node-id desc->source-scene child-id->desc]
  {:pre [(ifn? desc->source-scene)
         (ifn? child-id->desc)]}
  (letfn [(desc->instance-scene [desc]
            (let [id (:id desc)
                  transform-matrix (any-instance-desc->transform-matrix desc)
                  source-scene (desc->source-scene desc)
                  instance-scene (collection-common/any-instance-scene node-id id transform-matrix source-scene)
                  child-instance-scenes (map (comp desc->instance-scene child-id->desc)
                                             (:children desc))]
              (cond-> instance-scene
                      (seq child-instance-scenes)
                      (update :children util/intov child-instance-scenes))))]
    desc->instance-scene))

(g/defnk produce-scene [_node-id collection-desc embedded-component-resource-data->scene-index embedded-component-scenes referenced-component-proj-path->scene-index referenced-component-scenes referenced-collection-proj-path->index referenced-collection-scenes referenced-game-object-proj-path->index referenced-game-object-scenes]
  (let [prototype-desc->scene
        (game-object-non-editable/make-prototype-desc->scene embedded-component-resource-data->scene-index embedded-component-scenes referenced-component-proj-path->scene-index referenced-component-scenes)

        any-game-object-instance-desc->source-scene
        (fn any-game-object-instance-desc->source-scene [game-object-instance-desc]
          {:pre [(map? game-object-instance-desc)]} ; GameObject$InstanceDesc or GameObject$EmbeddedInstanceDesc in map format.
          ;; If the game-object-instance-desc contains a :prototype key, it is a
          ;; GameObject$InstanceDesc. If not, it is a GameObject$EmbeddedInstanceDesc.
          (if-some [referenced-game-object-proj-path (:prototype game-object-instance-desc)]
            (if (coll/empty? referenced-game-object-proj-path)
              (game-object-common/game-object-scene _node-id nil)
              (-> referenced-game-object-proj-path
                  referenced-game-object-proj-path->index
                  referenced-game-object-scenes))
            (-> game-object-instance-desc
                :data
                (prototype-desc->scene _node-id))))

        collection-instance-desc->source-scene
        (fn collection-instance-desc->source-scene [collection-instance-desc]
          (let [referenced-collection-proj-path (:collection collection-instance-desc)]
            (if (coll/empty? referenced-collection-proj-path)
              (collection-common/collection-scene _node-id nil)
              (-> referenced-collection-proj-path
                  referenced-collection-proj-path->index
                  referenced-collection-scenes))))

        game-object-instance-descs
        (concat (:embedded-instances collection-desc)
                (:instances collection-desc))

        [game-object-instance-id->game-object-instance-desc child-game-object-instance-id?]
        (util/into-multiple [{} #{}]
                            [(map (juxt :id identity))
                             (mapcat :children)]
                            game-object-instance-descs)

        any-game-object-instance-desc->instance-scene
        (make-desc->instance-scene _node-id any-game-object-instance-desc->source-scene game-object-instance-id->game-object-instance-desc)

        collection-instance-desc->instance-scene
        (make-desc->instance-scene _node-id collection-instance-desc->source-scene {})

        top-level-game-object-instance-scenes
        (into []
              (keep (fn [game-object-instance-desc]
                      (when-not (child-game-object-instance-id? (:id game-object-instance-desc))
                        (any-game-object-instance-desc->instance-scene game-object-instance-desc))))
              game-object-instance-descs)

        top-level-game-object-and-collection-instance-scenes
        (into top-level-game-object-instance-scenes
              (map collection-instance-desc->instance-scene)
              (:collection-instances collection-desc))]
    (collection-common/collection-scene _node-id top-level-game-object-and-collection-instance-scenes)))

(def ^:private referenced-collection-connections
  [[:build-targets :referenced-collection-build-targets]
   [:resource :referenced-collection-resources]
   [:resource-property-build-targets :other-resource-property-build-targets]
   [:scene :referenced-collection-scenes]])

(def ^:private referenced-game-object-connections
  [[:build-targets :referenced-game-object-build-targets]
   [:resource :referenced-game-object-resources]
   [:resource-property-build-targets :other-resource-property-build-targets]
   [:scene :referenced-game-object-scenes]])

(def ^:private resource-property-connections
  [[:build-targets :own-resource-property-build-targets]])

(defn connect-referenced-game-objects-tx-data [evaluation-context self referenced-game-object-resources]
  (game-object-non-editable/connect-referenced-resources-tx-data evaluation-context self referenced-game-object-resources :referenced-game-object-resources referenced-game-object-connections))

(defn connect-referenced-collections-tx-data [evaluation-context self referenced-collection-resources]
  (game-object-non-editable/connect-referenced-resources-tx-data evaluation-context self referenced-collection-resources :referenced-collection-resources referenced-collection-connections))

(g/defnode NonEditableCollectionNode
  (inherits game-object-non-editable/ComponentHostResourceNode)

  (property collection-desc g/Any ; No protobuf counterpart.
            (dynamic visible (g/constantly false))
            (set (fn [evaluation-context self _old-value new-value]
                   (let [basis (:basis evaluation-context)
                         project (project/get-project basis self)
                         workspace (project/workspace project evaluation-context)
                         proj-path->resource (workspace/make-proj-path->resource-fn workspace evaluation-context)]
                     (letfn [(connect-resource [proj-path-or-resource connections]
                               (:tx-data (project/connect-resource-node evaluation-context project proj-path-or-resource self connections)))]
                       (-> (g/set-property self :embedded-component-resource-data->index
                             (collection-desc->embedded-component-resource-data->index new-value))
                           (into (game-object-non-editable/connect-referenced-components-tx-data evaluation-context self (collection-desc->referenced-component-resources new-value proj-path->resource)))
                           (into (connect-referenced-game-objects-tx-data evaluation-context self (collection-desc->referenced-game-object-resources new-value proj-path->resource)))
                           (into (connect-referenced-collections-tx-data evaluation-context self (collection-desc->referenced-collection-resources new-value proj-path->resource)))
                           (into (game-object-non-editable/disconnect-connected-nodes-tx-data basis self :own-resource-property-build-targets resource-property-connections))
                           (into (mapcat #(connect-resource % resource-property-connections))
                                 (collection-desc->referenced-property-resources new-value proj-path->resource))))))))

  (input referenced-collection-build-targets g/Any :array)
  (input referenced-collection-resources g/Any :array)
  (input referenced-collection-scenes g/Any :array)
  (input referenced-game-object-build-targets g/Any :array)
  (input referenced-game-object-resources g/Any :array)
  (input referenced-game-object-scenes g/Any :array)

  ;; Internal outputs.
  (output proj-path->build-target g/Any :cached produce-proj-path->build-target)
  (output referenced-collection-proj-path->index g/Any :cached (g/fnk [referenced-collection-resources] (game-object-non-editable/resources->proj-path->index referenced-collection-resources)))
  (output referenced-game-object-instance-build-targets g/Any :cached produce-referenced-game-object-instance-build-targets)
  (output referenced-game-object-proj-path->index g/Any :cached (g/fnk [referenced-game-object-resources] (game-object-non-editable/resources->proj-path->index referenced-game-object-resources)))
  (output embedded-game-object-instance-build-targets g/Any :cached produce-embedded-game-object-instance-build-targets)

  ;; Collection interface.
  (output save-value g/Any (gu/passthrough collection-desc))
  (output build-targets g/Any :cached produce-build-targets)
  (output ddf-properties g/Any :cached produce-ddf-properties)
  (output node-outline outline/OutlineData produce-node-outline)
  (output scene g/Any produce-scene))

(defn- sanitize-non-editable-collection [workspace collection-desc]
  (let [ext->embedded-component-resource-type (workspace/get-resource-type-map workspace :non-editable)]
    (collection-common/sanitize-collection-desc collection-desc ext->embedded-component-resource-type)))

(defn- string-encode-non-editable-collection [workspace collection-desc]
  (let [ext->embedded-component-resource-type (workspace/get-resource-type-map workspace :non-editable)]
    (collection-string-data/string-encode-collection-desc ext->embedded-component-resource-type collection-desc)))

(defn- load-non-editable-collection [_project self resource collection-desc]
  ;; Validate the collection-desc.
  ;; We want to throw an exception if we encounter corrupt data to ensure our
  ;; node gets marked defective at load-time.
  (doseq [embedded-instance-desc (:embedded-instances collection-desc)]
    (collection-string-data/verify-string-decoded-embedded-instance-desc! embedded-instance-desc resource)
    (let [prototype-desc (:data embedded-instance-desc)]
      (doseq [embedded-component-desc (:embedded-components prototype-desc)]
        (collection-string-data/verify-string-decoded-embedded-component-desc! embedded-component-desc resource))))

  (g/set-property self :collection-desc collection-desc))

(defn register-resource-types [workspace]
  (concat
    (attachment/register workspace NonEditableCollectionNode :children :get (constantly []))
    (resource-node/register-ddf-resource-type workspace
      :editable false
      :ext "collection"
      :label "Non-Editable Collection"
      :node-type NonEditableCollectionNode
      :ddf-type GameObject$CollectionDesc
      :dependencies-fn (collection-common/make-collection-dependencies-fn #(workspace/get-resource-type workspace :non-editable "go"))
      :sanitize-fn (partial sanitize-non-editable-collection workspace)
      :string-encode-fn (partial string-encode-non-editable-collection workspace)
      :load-fn load-non-editable-collection
      :allow-unloaded-use true
      :icon collection-common/collection-icon
      :icon-class :design
      :view-types [:scene :text]
      :view-opts {:scene {:grid true}})))
