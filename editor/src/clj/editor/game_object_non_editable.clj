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

(ns editor.game-object-non-editable
  (:require [dynamo.graph :as g]
            [editor.attachment :as attachment]
            [editor.build-target :as bt]
            [editor.collection-string-data :as collection-string-data]
            [editor.defold-project :as project]
            [editor.game-object-common :as game-object-common]
            [editor.graph-util :as gu]
            [editor.localization :as localization]
            [editor.outline :as outline]
            [editor.pose :as pose]
            [editor.properties :as properties]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.workspace :as workspace]
            [internal.graph :as ig]
            [internal.graph.types :as gt]
            [internal.util :as util]
            [util.coll :refer [flipped-pair pair]])
  (:import [com.dynamo.gameobject.proto GameObject$PrototypeDesc]
           [javax.vecmath Matrix4d]))

(set! *warn-on-reflection* true)

(def ^:private embedded-component-connections
  [[:build-targets :embedded-component-build-targets]
   [:scene :embedded-component-scenes]])

(def ^:private referenced-component-connections
  [[:build-targets :referenced-component-build-targets]
   [:resource :referenced-component-resources]
   [:resource-property-build-targets :other-resource-property-build-targets]
   [:scene :referenced-component-scenes]])

(defn- add-embedded-component-resource-node [host-node-id embedded-component-resource-data project]
  (let [embedded-resource-ext (:type embedded-component-resource-data)
        embedded-resource-pb-map (:data embedded-component-resource-data)
        embedded-resource (project/make-embedded-resource project :non-editable embedded-resource-ext embedded-resource-pb-map)
        embedded-resource-node-type (project/resource-node-type embedded-resource)
        graph (g/node-id->graph-id host-node-id)]
    (g/make-nodes graph [embedded-resource-node-id [embedded-resource-node-type :resource embedded-resource]]
      (project/load-embedded-resource-node project embedded-resource-node-id embedded-resource embedded-resource-pb-map)
      (gu/connect-existing-outputs embedded-resource-node-type embedded-resource-node-id host-node-id embedded-component-connections))))

(g/defnk produce-embedded-component-resource-data->scene-index [embedded-component-resource-data->index resource]
  (let [workspace (resource/workspace resource)
        ext->resource-type (workspace/get-resource-type-map workspace :non-editable)]
    (into {}
          (comp (keep (fn [[embedded-component-resource-data]]
                        (let [embedded-resource-ext (:type embedded-component-resource-data)
                              embedded-resource-type (ext->resource-type embedded-resource-ext)
                              embedded-resource-node-type (:node-type embedded-resource-type)]
                          (when (g/has-output? embedded-resource-node-type :scene)
                            embedded-component-resource-data))))
                (map-indexed flipped-pair))
          (sort-by val embedded-component-resource-data->index))))

(defn resources->proj-path->index
  ([resources]
   (resources->proj-path->index resources nil))
  ([resources resource-pred]
   (into {}
         (cond->> (map-indexed (fn [index resource]
                                 (pair (resource/proj-path resource)
                                       index)))
                  resource-pred (comp (filter resource-pred)))
         resources)))

(g/defnk produce-referenced-component-proj-path->scene-index [referenced-component-resources]
  (resources->proj-path->index referenced-component-resources
                               #(-> % project/resource-node-type (g/has-output? :scene))))

(defn mapv-source-ids [source-id->tx-data basis target-id target-label]
  (into []
        (comp (map gt/source-id)
              (distinct)
              (mapcat source-id->tx-data))
        (ig/explicit-inputs basis target-id target-label)))

(defn delete-connected-nodes-tx-data [basis target-id target-label]
  (mapv-source-ids g/delete-node basis target-id target-label))

(defn disconnect-connected-nodes-tx-data [basis target-id target-label connections]
  (mapv-source-ids (fn [source-id]
                     (map (fn [[source-label target-label]]
                            (g/disconnect source-id source-label target-id target-label))
                          connections))
                   basis target-id target-label))

(defn data->index-setter [evaluation-context self new-value old-sources-input-label add-resource-node-fn]
  (let [basis (:basis evaluation-context)
        project (project/get-project basis self)]
    (into (delete-connected-nodes-tx-data basis self old-sources-input-label)
          (mapcat (fn [[data]]
                    (add-resource-node-fn self data project)))
          (sort-by val new-value))))

(defn connect-referenced-resources-tx-data [evaluation-context self new-value old-sources-input-label resource-connections]
  (let [basis (:basis evaluation-context)
        project (project/get-project basis self)]
    (into (disconnect-connected-nodes-tx-data basis self old-sources-input-label resource-connections)
          (mapcat (fn [resource]
                    (:tx-data (project/connect-resource-node evaluation-context project resource self resource-connections))))
          new-value)))

(defn connect-referenced-components-tx-data [evaluation-context self referenced-component-resources]
  (connect-referenced-resources-tx-data evaluation-context self referenced-component-resources :referenced-component-resources referenced-component-connections))

(g/defnode ComponentHostResourceNode
  (inherits resource-node/NonEditableResourceNode)

  (property embedded-component-resource-data->index g/Any ; No protobuf counterpart.
            (dynamic visible (g/constantly false))
            (set (fn [evaluation-context self _old-value new-value]
                   (data->index-setter evaluation-context self new-value :embedded-component-build-targets add-embedded-component-resource-node))))

  (input embedded-component-build-targets g/Any :array :cascade-delete)
  (input embedded-component-scenes g/Any :array)
  (input referenced-component-build-targets g/Any :array)
  (input referenced-component-resources resource/Resource :array)
  (input referenced-component-scenes g/Any :array)
  (input own-resource-property-build-targets g/Any :array)
  (input other-resource-property-build-targets g/Any :array)

  (output embedded-component-build-targets g/Any :cached (g/fnk [embedded-component-build-targets] (mapv util/only-or-throw embedded-component-build-targets)))
  (output embedded-component-resource-data->scene-index g/Any :cached produce-embedded-component-resource-data->scene-index)
  (output embedded-component-scenes g/Any :cached (gu/passthrough embedded-component-scenes))
  (output referenced-component-build-targets g/Any :cached (g/fnk [referenced-component-build-targets] (mapv util/only-or-throw referenced-component-build-targets)))
  (output referenced-component-proj-path->index g/Any :cached (g/fnk [referenced-component-resources] (resources->proj-path->index referenced-component-resources)))
  (output referenced-component-proj-path->scene-index g/Any :cached produce-referenced-component-proj-path->scene-index)
  (output referenced-component-scenes g/Any :cached (gu/passthrough referenced-component-scenes))
  (output resource-property-build-targets g/Any :cached (g/fnk [other-resource-property-build-targets own-resource-property-build-targets] (into other-resource-property-build-targets own-resource-property-build-targets))))

;; -----------------------------------------------------------------------------

(defn- any-component-desc->pose [{:keys [position rotation scale]}]
  ;; GameObject$ComponentDesc or GameObject$EmbeddedInstanceDesc in map format.
  (pose/make position rotation scale))

(defn- any-component-desc->transform-matrix
  ^Matrix4d [any-component-desc]
  ;; GameObject$ComponentDesc or GameObject$EmbeddedInstanceDesc in map format.
  (pose/matrix (any-component-desc->pose any-component-desc)))

(defn prototype-desc->referenced-component-proj-paths [prototype-desc]
  (eduction
    (keep (comp not-empty :component))
    (distinct)
    (:components prototype-desc)))

(defn prototype-desc->referenced-component-resources [prototype-desc proj-path->resource]
  (eduction
    (map proj-path->resource)
    (prototype-desc->referenced-component-proj-paths prototype-desc)))

(defn- embedded-component-desc->embedded-component-resource-data [embedded-component-desc]
  {:pre [(map? embedded-component-desc) ; GameObject$EmbeddedComponentDesc in map format.
         (string? (:type embedded-component-desc)) ; File extension.
         (map? (:data embedded-component-desc))]}
  (select-keys embedded-component-desc [:type :data]))

(defn prototype-desc->embedded-component-resource-datas [prototype-desc]
  (eduction
    (map embedded-component-desc->embedded-component-resource-data)
    (distinct)
    (:embedded-components prototype-desc)))

(defn- prototype-desc->embedded-component-resource-data->index [prototype-desc]
  (into {}
        (map-indexed flipped-pair)
        (prototype-desc->embedded-component-resource-datas prototype-desc)))

(defn prototype-desc->component-property-descs [prototype-desc]
  (into []
        (keep (fn [component-desc]
                (let [component-id (:id component-desc)
                      property-descs (:properties component-desc)]
                  (when (seq property-descs)
                    {:id component-id
                     :properties property-descs}))))
        (:components prototype-desc)))

(defn component-property-descs->resources [component-property-descs proj-path->resource]
  (eduction
    (mapcat :properties)
    (map #(dissoc % :id))
    (distinct)
    (keep #(properties/property-desc->resource % proj-path->resource))
    component-property-descs))

(defn- prototype-desc->referenced-property-resources [prototype-desc proj-path->resource]
  (-> prototype-desc
      prototype-desc->component-property-descs
      (component-property-descs->resources proj-path->resource)))

(defn- component-desc->component-instance-data [component-desc proj-path->build-target]
  {:pre [(map? component-desc)]} ; GameObject$ComponentDesc in map format.
  (let [build-resource (-> component-desc :component proj-path->build-target :resource)
        pose (any-component-desc->pose component-desc)
        proj-path->source-resource (comp :resource :resource proj-path->build-target)
        go-props-with-source-resources (mapv #(properties/property-desc->go-prop % proj-path->source-resource) (:properties component-desc))
        component-desc (assoc component-desc :properties go-props-with-source-resources)]
    (game-object-common/referenced-component-instance-data build-resource component-desc pose proj-path->build-target)))

(defn- embedded-component-desc->component-instance-data [embedded-component-desc embedded-component-desc->build-resource]
  {:pre [(map? embedded-component-desc) ; GameObject$EmbeddedComponentDesc in map format.
         (map? (:data embedded-component-desc))]} ; We must be in our unaltered sanitized state for the build resource lookup to work.
  (let [build-resource (embedded-component-desc->build-resource embedded-component-desc)
        pose (any-component-desc->pose embedded-component-desc)
        embedded-component-desc (game-object-common/add-default-scale-to-component-desc embedded-component-desc)]
    (game-object-common/embedded-component-instance-data build-resource embedded-component-desc pose)))

(defn prototype-desc->component-instance-datas [prototype-desc embedded-component-desc->build-resource proj-path->build-target]
  {:pre [(or (nil? prototype-desc) (map? prototype-desc))]} ; GameObject$PrototypeDesc in map format.
  (-> []
      (into (map #(component-desc->component-instance-data % proj-path->build-target))
            (:components prototype-desc))
      (into (map #(embedded-component-desc->component-instance-data % embedded-component-desc->build-resource))
            (:embedded-components prototype-desc))))

(defn- make-embedded-component-desc->index [embedded-component-resource-data->index]
  (comp embedded-component-resource-data->index
        embedded-component-desc->embedded-component-resource-data))

(defn make-embedded-component-desc->build-resource [embedded-component-build-targets embedded-component-resource-data->index]
  {:pre [(vector? embedded-component-build-targets)
         (not (vector? (first embedded-component-build-targets)))]}
  (comp :resource
        embedded-component-build-targets
        (make-embedded-component-desc->index embedded-component-resource-data->index)))

(defn- make-any-component-desc->source-scene [embedded-component-scenes embedded-component-resource-data->scene-index referenced-component-scenes referenced-component-proj-path->scene-index]
  {:pre [(vector? embedded-component-scenes)
         (not (vector? (first embedded-component-scenes)))
         (map? embedded-component-resource-data->scene-index)
         (vector? referenced-component-scenes)
         (not (vector? (first referenced-component-scenes)))
         (map? referenced-component-proj-path->scene-index)]}
  (fn any-component-desc->source-scene [any-component-desc]
    ;; GameObject$ComponentDesc or GameObject$EmbeddedComponentDesc in map format.
    (if-some [referenced-component-proj-path (:component any-component-desc)]
      (some-> referenced-component-proj-path
              referenced-component-proj-path->scene-index
              referenced-component-scenes)
      (some-> any-component-desc
              embedded-component-desc->embedded-component-resource-data
              embedded-component-resource-data->scene-index
              embedded-component-scenes))))

(defn- make-any-component-desc->component-scene [any-component-desc->source-scene]
  {:pre [(ifn? any-component-desc->source-scene)]}
  (fn any-component-desc->component-scene [any-component-desc node-id]
    (let [node-outline-key (:id any-component-desc)
          transform-matrix (any-component-desc->transform-matrix any-component-desc)
          source-scene (any-component-desc->source-scene any-component-desc)]
      ;; TODO: Currently we return an empty scene for components that do not
      ;; have a scene output. Can we exclude these or is it necessary for
      ;; selection to work, or something like that?
      (game-object-common/component-scene node-id node-outline-key transform-matrix source-scene))))

(defn make-prototype-desc->scene [embedded-component-resource-data->scene-index embedded-component-scenes referenced-component-proj-path->scene-index referenced-component-scenes]
  (let [any-component-desc->source-scene
        (make-any-component-desc->source-scene embedded-component-scenes embedded-component-resource-data->scene-index referenced-component-scenes referenced-component-proj-path->scene-index)

        any-component-desc->component-scene
        (make-any-component-desc->component-scene any-component-desc->source-scene)]
    (fn prototype-desc->scene [prototype-desc node-id]
      (let [component-scenes
            (into []
                  (comp cat
                        (map #(any-component-desc->component-scene % node-id)))
                  (pair (:embedded-components prototype-desc)
                        (:components prototype-desc)))]
        (game-object-common/game-object-scene node-id component-scenes)))))

;; -----------------------------------------------------------------------------

(g/defnk produce-proj-path->build-target [referenced-component-build-targets resource-property-build-targets]
  (bt/make-proj-path->build-target
    (into referenced-component-build-targets
          (mapcat identity)
          resource-property-build-targets)))

(g/defnk produce-build-targets [_node-id resource prototype-desc embedded-component-resource-data->index embedded-component-build-targets referenced-component-build-targets proj-path->build-target]
  (or (let [dup-ids (game-object-common/any-descs->duplicate-ids
                      (concat (:components prototype-desc)
                              (:embedded-components prototype-desc)))]
        (game-object-common/maybe-duplicate-id-error _node-id dup-ids))
      (let [embedded-component-desc->build-resource
            (make-embedded-component-desc->build-resource embedded-component-build-targets embedded-component-resource-data->index)

            component-instance-datas
            (when prototype-desc
              (prototype-desc->component-instance-datas prototype-desc embedded-component-desc->build-resource proj-path->build-target))

            component-build-targets
            (into referenced-component-build-targets
                  embedded-component-build-targets)]

        [(game-object-common/game-object-build-target resource _node-id component-instance-datas component-build-targets)])))

(g/defnk produce-ddf-component-properties [prototype-desc]
  (prototype-desc->component-property-descs prototype-desc))

(g/defnk produce-node-outline [_node-id]
  {:node-id _node-id
   :node-outline-key "Non-Editable Game Object"
   :label (localization/message "outline.non-editable-game-object")
   :icon game-object-common/game-object-icon})

(g/defnk produce-scene [_node-id prototype-desc embedded-component-resource-data->scene-index embedded-component-scenes referenced-component-proj-path->scene-index referenced-component-scenes]
  (let [prototype-desc->scene (make-prototype-desc->scene embedded-component-resource-data->scene-index embedded-component-scenes referenced-component-proj-path->scene-index referenced-component-scenes)]
    (prototype-desc->scene prototype-desc _node-id)))

(def ^:private resource-property-connections
  [[:build-targets :own-resource-property-build-targets]])

(g/defnode NonEditableGameObjectNode
  (inherits ComponentHostResourceNode)

  (property prototype-desc g/Any ; No protobuf counterpart.
            (dynamic visible (g/constantly false))
            (set (fn [evaluation-context self _old-value new-value]
                   (let [basis (:basis evaluation-context)
                         project (project/get-project basis self)
                         workspace (project/workspace project evaluation-context)
                         proj-path->resource (workspace/make-proj-path->resource-fn workspace evaluation-context)]
                     (letfn [(connect-resource [proj-path-or-resource connections]
                               (:tx-data (project/connect-resource-node evaluation-context project proj-path-or-resource self connections)))]
                       (-> (g/set-property self :embedded-component-resource-data->index
                             (prototype-desc->embedded-component-resource-data->index new-value))
                           (into (connect-referenced-components-tx-data evaluation-context self (prototype-desc->referenced-component-resources new-value proj-path->resource)))
                           (into (disconnect-connected-nodes-tx-data basis self :own-resource-property-build-targets resource-property-connections))
                           (into (mapcat #(connect-resource % resource-property-connections))
                                 (prototype-desc->referenced-property-resources new-value proj-path->resource))))))))

  ;; Internal outputs.
  (output proj-path->build-target g/Any :cached produce-proj-path->build-target)

  ;; GameObject interface.
  (output save-value g/Any (gu/passthrough prototype-desc))
  (output build-targets g/Any :cached produce-build-targets)
  (output ddf-component-properties g/Any :cached produce-ddf-component-properties)
  (output node-outline outline/OutlineData produce-node-outline)
  (output scene g/Any produce-scene))

(defn- sanitize-non-editable-game-object [workspace prototype-desc]
  (let [ext->embedded-component-resource-type (workspace/get-resource-type-map workspace :non-editable)]
    (game-object-common/sanitize-prototype-desc prototype-desc ext->embedded-component-resource-type)))

(defn- string-encode-non-editable-game-object [workspace prototype-desc]
  (let [ext->embedded-component-resource-type (workspace/get-resource-type-map workspace :non-editable)]
    (collection-string-data/string-encode-prototype-desc ext->embedded-component-resource-type prototype-desc)))

(defn- load-non-editable-game-object [_project self resource prototype-desc]
  ;; Validate the prototype-desc.
  ;; We want to throw an exception if we encounter corrupt data to ensure our
  ;; node gets marked defective at load-time.
  (doseq [embedded-component-desc (:embedded-components prototype-desc)]
    (collection-string-data/verify-string-decoded-embedded-component-desc! embedded-component-desc resource))

  (g/set-property self :prototype-desc prototype-desc))

(defn register-resource-types [workspace]
  (concat
    (attachment/register workspace NonEditableGameObjectNode :components :get (constantly []))
    (resource-node/register-ddf-resource-type workspace
      :editable false
      :ext "go"
      :label (localization/message "resource.type.go.non-editable")
      :node-type NonEditableGameObjectNode
      :ddf-type GameObject$PrototypeDesc
      :dependencies-fn (game-object-common/make-game-object-dependencies-fn #(workspace/get-resource-type-map workspace :non-editable))
      :sanitize-fn (partial sanitize-non-editable-game-object workspace)
      :string-encode-fn (partial string-encode-non-editable-game-object workspace)
      :load-fn load-non-editable-game-object
      :allow-unloaded-use true
      :icon game-object-common/game-object-icon
      :icon-class :design
      :view-types [:scene :text]
      :view-opts {:scene {:grid true}})))
