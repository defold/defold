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

(ns editor.collection-common
  (:require [dynamo.graph :as g]
            [editor.build-target :as bt]
            [editor.game-object-common :as game-object-common]
            [editor.geom :as geom]
            [editor.gl.pass :as pass]
            [editor.pose :as pose]
            [editor.properties :as properties]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.scene :as scene]
            [editor.workspace :as workspace]
            [internal.util :as util]
            [service.log :as log]
            [util.coll :refer [pair]])
  (:import [com.dynamo.gameobject.proto GameObject$CollectionDesc GameObject$PrototypeDesc]
           [java.io StringReader]
           [javax.vecmath Matrix4d]))

(set! *warn-on-reflection* true)

(def collection-icon "icons/32/Icons_09-Collection.png")

(def path-sep "/")

(defn- read-scale3-or-scale [{:keys [scale3 scale] :as _any-instance-desc}]
  ;; scale is the legacy uniform scale
  ;; check if scale3 has default value and if so, use legacy uniform scale
  (if (and (protobuf/default-read-scale-value? scale3)
           (some? scale)
           (not (zero? scale)))
    [scale scale scale]
    scale3))

(defn- uniform->non-uniform-scale [any-instance-desc]
  ;; GameObject$InstanceDesc, GameObject$EmbeddedInstanceDesc, or GameObject$CollectionInstanceDesc in map format.
  (-> any-instance-desc
      (assoc :scale3 (read-scale3-or-scale any-instance-desc))
      (dissoc :scale)))

(defn- sanitize-any-instance-desc [any-instance-desc component-property-descs-key]
  ;; GameObject$InstanceDesc, GameObject$EmbeddedInstanceDesc, or GameObject$CollectionInstanceDesc in map format.
  ;; The specified key should address a seq of GameObject$ComponentPropertyDescs in map format.
  (-> any-instance-desc
      (uniform->non-uniform-scale)
      (game-object-common/sanitize-component-property-descs-at-key component-property-descs-key)))

(defn- sanitize-embedded-game-object-data [embedded-instance-desc ext->embedded-component-resource-type embed-data-handling]
  ;; GameObject$EmbeddedInstanceDesc in map format.
  (try
    (let [unsanitized-prototype-desc (protobuf/str->map GameObject$PrototypeDesc (:data embedded-instance-desc))
          sanitized-prototype-desc (game-object-common/sanitize-prototype-desc unsanitized-prototype-desc ext->embedded-component-resource-type embed-data-handling)]
      (assoc embedded-instance-desc
        :data (case embed-data-handling
                :embed-data-as-maps sanitized-prototype-desc
                :embed-data-as-strings (protobuf/map->str GameObject$PrototypeDesc sanitized-prototype-desc false))))
    (catch Exception error
      ;; Leave unsanitized.
      (log/warn :msg "Failed to sanitize embedded game object" :exception error)
      embedded-instance-desc)))

(defn- sanitize-instance-desc [instance-desc]
  ;; GameObject$InstanceDesc in map format.
  (-> instance-desc
      (sanitize-any-instance-desc :component-properties)))

(defn- sanitize-embedded-instance-desc [embedded-instance-desc ext->embedded-component-resource-type embed-data-handling]
  ;; GameObject$EmbeddedInstanceDesc in map format.
  (cond-> (sanitize-any-instance-desc embedded-instance-desc :component-properties)
          (string? (:data embedded-instance-desc)) (sanitize-embedded-game-object-data ext->embedded-component-resource-type embed-data-handling)))

(defn- sanitize-collection-instance-desc [collection-instance-desc]
  ;; GameObject$CollectionInstanceDesc in map format.
  (-> collection-instance-desc
      (uniform->non-uniform-scale)
      (update :instance-properties (partial mapv #(game-object-common/sanitize-component-property-descs-at-key % :properties)))))

(defn sanitize-collection-desc [collection-desc ext->embedded-component-resource-type embed-data-handling]
  {:pre [(map? collection-desc)
         (ifn? ext->embedded-component-resource-type)
         (case embed-data-handling (:embed-data-as-maps :embed-data-as-strings) true false)]}
  ;; GameObject$CollectionDesc in map format.
  (-> collection-desc
      (update :instances (partial mapv sanitize-instance-desc))
      (update :embedded-instances (partial mapv #(sanitize-embedded-instance-desc % ext->embedded-component-resource-type embed-data-handling)))
      (update :collection-instances (partial mapv sanitize-collection-instance-desc))))

(defn make-collection-dependencies-fn [game-object-resource-type-fn]
  {:pre [(ifn? game-object-resource-type-fn)]}
  ;; TODO: This should probably also consider resource property overrides?
  (let [default-dependencies-fn (resource-node/make-ddf-dependencies-fn GameObject$CollectionDesc)]
    (fn [source-value]
      (let [go-resource-type (game-object-resource-type-fn)
            go-read-fn (:read-fn go-resource-type)
            go-dependencies-fn (:dependencies-fn go-resource-type)]
        (into (default-dependencies-fn source-value)
              (mapcat (fn [embedded-instance-desc]
                        (try
                          (go-dependencies-fn
                            (with-open [reader (StringReader. (:data embedded-instance-desc))]
                              (go-read-fn reader)))
                          (catch Exception error
                            (log/warn :msg (format "Couldn't determine dependencies for embedded instance %s" (:id embedded-instance-desc))
                                      :exception error)
                            nil))))
              (:embedded-instances source-value))))))

(defn game-object-instance-build-target [build-resource instance-desc-with-go-props pose game-object-build-target proj-path->resource-property-build-target]
  {:pre [(workspace/build-resource? build-resource)
         (map? instance-desc-with-go-props) ; GameObject$InstanceDesc or GameObject$EmbeddedInstanceDesc in map format, but GameObject$PropertyDescs must have a :clj-value.
         (not (contains? instance-desc-with-go-props :data)) ; We don't need the :data from GameObject$EmbeddedInstanceDescs.
         (pose/pose? pose)
         (map? game-object-build-target)
         (ifn? proj-path->resource-property-build-target)]}
  ;; Create a build-target for the referenced or embedded game object. Also tag
  ;; on :instance-data with the overrides for this instance. This will later be
  ;; extracted and compiled into the Collection - the overrides do not end up in
  ;; the resulting game object binary.
  ;; Please refer to `/engine/gameobject/proto/gameobject/gameobject_ddf.proto`
  ;; when reading this. It describes how the ddf-message map is structured.
  ;; You might also want to familiarize yourself with how this process works in
  ;; `game_object.clj`, since it is similar but less complicated there.
  ;;
  ;; NOTE: A `go-prop` is basically a PropertyDesc in map form with an
  ;; additional :clj-value entry. See `properties/build-target-go-props`
  ;; for more info.
  (let [build-target-go-props (partial properties/build-target-go-props
                                       proj-path->resource-property-build-target)
        component-property-infos (map (comp build-target-go-props :properties)
                                      (:component-properties instance-desc-with-go-props))
        component-go-props (map first component-property-infos)
        component-property-descs (map #(assoc %1 :properties %2)
                                      (:component-properties instance-desc-with-go-props)
                                      component-go-props)
        go-prop-dep-build-targets (into []
                                        (comp (mapcat second)
                                              (util/distinct-by (comp resource/proj-path :resource)))
                                        component-property-infos)
        game-object-instance-data {:resource build-resource
                                   :pose pose
                                   :property-deps go-prop-dep-build-targets
                                   :instance-msg (if (seq component-property-descs)
                                                   (assoc instance-desc-with-go-props :component-properties component-property-descs)
                                                   instance-desc-with-go-props)}
        build-target (assoc game-object-build-target
                       :resource build-resource
                       :instance-data game-object-instance-data)]
    (bt/with-content-hash build-target)))

(defn- source-resource-component-property-desc [component-property-desc]
  (update component-property-desc :properties properties/source-resource-go-props))

(defn override-property-descs [original-property-descs overridden-property-descs]
  ;; GameObject$PropertyDescs in map format.
  (-> (into {}
            (comp cat
                  (map (juxt :id identity)))
            [original-property-descs overridden-property-descs])
      (vals)
      (vec)))

(defn- flatten-game-object-instance-data [game-object-instance-data collection-instance-id collection-instance-pose child-game-object-instance-id? game-object-instance-id->component-property-descs proj-path->resource-property-build-target]
  (let [{:keys [resource instance-msg pose]} game-object-instance-data
        {:keys [id children component-properties]} instance-msg
        build-target-go-props (partial properties/build-target-go-props proj-path->resource-property-build-target)
        component-properties (map source-resource-component-property-desc component-properties)
        component-properties (override-property-descs component-properties (game-object-instance-id->component-property-descs id))
        component-property-infos (map (comp build-target-go-props :properties) component-properties)
        component-go-props (map first component-property-infos)
        component-property-descs (mapv #(assoc %1 :properties %2)
                                       component-properties
                                       component-go-props)
        go-prop-dep-build-targets (into []
                                        (comp (mapcat second)
                                              (util/distinct-by (comp resource/proj-path :resource)))
                                        component-property-infos)
        instance-msg {:id (str collection-instance-id path-sep id)
                      :children (mapv #(str collection-instance-id path-sep %) children)
                      :component-properties component-property-descs}
        pose (if (child-game-object-instance-id? id)
               pose
               (pose/pre-multiply pose collection-instance-pose))]
    {:resource resource
     :instance-msg instance-msg
     :pose pose
     :property-deps go-prop-dep-build-targets}))

(defn collection-instance-build-target [collection-instance-id pose instance-property-descs collection-build-target proj-path->resource-property-build-target]
  {:pre [(string? collection-instance-id)
         (pose/pose? pose)
         (seqable? instance-property-descs) ; GameObject$InstancePropertyDescs in map format.
         (map? collection-build-target)
         (ifn? proj-path->resource-property-build-target)]}
  (let [game-object-instance-datas (-> collection-build-target :user-data :instance-data)

        child-game-object-instance-id?
        (into #{}
              (mapcat (comp :children :instance-msg)) ; instance-msg is GameObject$InstanceDesc or GameObject$EmbeddedInstanceDesc in map format.
              game-object-instance-datas)

        game-object-instance-id->component-property-descs
        (into {}
              (map (juxt :id :properties))
              instance-property-descs)]
    (bt/with-content-hash
      (assoc-in collection-build-target [:user-data :instance-data]
                (mapv (fn [game-object-instance-data]
                        (flatten-game-object-instance-data game-object-instance-data collection-instance-id pose child-game-object-instance-id? game-object-instance-id->component-property-descs proj-path->resource-property-build-target))
                      game-object-instance-datas)))))

(defn- pose->transform-properties [pose]
  (pose/to-map pose :position :rotation :scale3))

(defn- sort-instance-descs-for-build-output [instance-descs]
  (let [instance-id->parent-id
        (into {}
              (mapcat (fn [instance-desc]
                        (let [instance-id (:id instance-desc)
                              child-ids (:children instance-desc)]
                          (map (fn [child-id]
                                 (pair child-id instance-id))
                               child-ids))))
              instance-descs)

        instance-id->hierarchy-depth
        (fn instance-id->hierarchy-depth [instance-id]
          (->> instance-id
               (iterate instance-id->parent-id)
               (take-while some?)
               (count)))

        instance-desc->order-key
        (fn instance-desc->order-key [instance-desc]
          (let [instance-id (:id instance-desc)
                hierarchy-depth (instance-id->hierarchy-depth instance-id)]
            (pair hierarchy-depth instance-id)))]

    (sort-by instance-desc->order-key
             instance-descs)))

(defn- build-collection [build-resource dep-resources user-data]
  ;; Please refer to `/engine/gameobject/proto/gameobject/gameobject_ddf.proto`
  ;; when reading this. It will clear up how the output binaries are structured.
  ;; Be aware that these structures are also used to store the saved project
  ;; data. Sometimes a field will only be used by the editor *or* the runtime.
  ;; In the case of CollectionDesc, neither the `collection_instances` nor the
  ;; `embedded_instances` fields are read by the runtime. Instead, all the game
  ;; objects brought in from these two fields are recursively collected into a
  ;; flat list of InstanceDesc under the `instances` field, each referencing a
  ;; BuildResource of a PrototypeDesc binary produced from the referenced or
  ;; embedded game objects. However, embedded game objects from different
  ;; collections might have been fused into a single BuildResource if they are
  ;; equivalent. We must update any references to these BuildResources
  ;; to instead point to the resulting fused BuildResource. The same goes for
  ;; resource property overrides inside the InstanceDescs.
  (let [{:keys [name instance-data scale-along-z]} user-data
        build-go-props (partial properties/build-go-props dep-resources)
        go-instance-msgs (map :instance-msg instance-data)
        go-instance-transform-properties (map (comp pose->transform-properties :pose) instance-data)
        go-instance-build-resource-paths (map (comp resource/proj-path dep-resources :resource) instance-data)
        go-instance-component-go-props (map (fn [instance-desc] ; GameObject$InstanceDesc in map form
                                              (map (fn [component-property-desc] ; GameObject$ComponentPropertyDesc in map form
                                                     (build-go-props (:properties component-property-desc)))
                                                   (:component-properties instance-desc)))
                                            go-instance-msgs)
        instance-descs (map (fn [instance-desc transform-properties fused-build-resource-path component-go-props]
                              (-> instance-desc
                                  (merge transform-properties)
                                  (dissoc :data)
                                  (assoc :id (str path-sep (:id instance-desc))
                                         :prototype fused-build-resource-path
                                         :children (mapv (partial str path-sep) (:children instance-desc))
                                         :component-properties (map (fn [component-property-desc go-props]
                                                                      (cond-> (dissoc component-property-desc :properties) ; Runtime uses :property-decls, not :properties
                                                                              (seq go-props) (assoc :property-decls (properties/go-props->decls go-props false))))
                                                                    (:component-properties instance-desc)
                                                                    component-go-props))))
                            go-instance-msgs
                            go-instance-transform-properties
                            go-instance-build-resource-paths
                            go-instance-component-go-props)
        property-resource-paths (into (sorted-set)
                                      (comp cat cat (keep properties/try-get-go-prop-proj-path))
                                      go-instance-component-go-props)
        collection-desc {:name name
                         :instances (sort-instance-descs-for-build-output instance-descs)
                         :scale-along-z (if scale-along-z 1 0)
                         :property-resources property-resource-paths}]
    {:resource build-resource
     :content (protobuf/map->bytes GameObject$CollectionDesc collection-desc)}))

(defn collection-build-target [build-resource node-id name scale-along-z game-object-instance-build-targets collection-instance-build-targets]
  {:pre [(workspace/build-resource? build-resource)
         (g/node-id? node-id)
         (string? name)
         (boolean? scale-along-z)
         (seqable? game-object-instance-build-targets)
         (seqable? collection-instance-build-targets)]}
  ;; Extract the :instance-data from the game object instance build targets
  ;; so that overrides can be embedded in the resulting collection binary.
  ;; We also establish dependencies to build-targets from any resources
  ;; referenced by script property overrides.
  (let [collection-instance-build-targets (flatten collection-instance-build-targets)
        game-object-instance-build-targets (flatten game-object-instance-build-targets)
        instance-data (into (mapv :instance-data game-object-instance-build-targets)
                            (comp (keep :user-data)
                                  (mapcat :instance-data))
                            collection-instance-build-targets)
        property-deps (sequence (comp (mapcat :property-deps)
                                      (util/distinct-by (comp resource/proj-path :resource)))
                                instance-data)]
    (bt/with-content-hash
      {:node-id node-id
       :resource build-resource
       :build-fn build-collection
       :user-data {:name name
                   :instance-data instance-data
                   :scale-along-z scale-along-z}
       :deps (into (vec (concat game-object-instance-build-targets property-deps))
                   (mapcat :deps)
                   collection-instance-build-targets)})))

(defn any-instance-scene [node-id node-outline-key ^Matrix4d transform-matrix source-scene]
  {:pre [(g/node-id? node-id)
         (instance? Matrix4d transform-matrix)
         (or (nil? source-scene) (map? source-scene))]}
  (-> source-scene
      (scene/claim-scene node-id node-outline-key)
      (assoc :transform transform-matrix
             :aabb geom/empty-bounding-box
             :renderable {:passes [pass/selection]})))

(defn collection-scene [node-id child-game-object-and-collection-instance-scenes]
  {:pre [(g/node-id? node-id)
         (not (vector? (first child-game-object-and-collection-instance-scenes)))]}
  {:node-id node-id
   :aabb geom/null-aabb
   :children child-game-object-and-collection-instance-scenes})
