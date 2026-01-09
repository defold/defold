;; Copyright 2020-2026 The Defold Foundation
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

(ns editor.game-object-common
  (:require [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.build-target :as bt]
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
            [service.log :as log])
  (:import [com.dynamo.gameobject.proto GameObject$PrototypeDesc]
           [java.io StringReader]
           [javax.vecmath Matrix4d]))

(set! *warn-on-reflection* true)

(def game-object-icon "icons/32/Icons_06-Game-object.png")

(def component-transform-property-keys (set (keys scene/identity-transform-properties)))

(defn template-pb-map
  ([workspace resource-type]
   (g/with-auto-evaluation-context evaluation-context
     (template-pb-map workspace resource-type evaluation-context)))
  ([workspace resource-type evaluation-context]
   (let [template (workspace/template workspace resource-type evaluation-context)
         read-fn (:read-fn resource-type)]
     (with-open [reader (StringReader. template)]
       (read-fn reader)))))

(defn strip-default-scale-from-component-desc [component-desc]
  ;; GameObject$ComponentDesc or GameObject$EmbeddedComponentDesc in map format.
  ;; TODO(save-value-cleanup): Can we get rid of this now?
  (if-let [scale (:scale component-desc)]
    (if (scene/significant-scale? scale)
      component-desc
      (dissoc component-desc :scale))
    component-desc))

(defn add-default-scale-to-component-desc [component-desc]
  ;; GameObject$ComponentDesc or GameObject$EmbeddedComponentDesc in map format.
  (cond-> component-desc
          (not (contains? component-desc :scale))
          (assoc :scale scene/default-scale)))

(defn- sanitize-component-property-desc [component-property-desc]
  ;; GameObject$ComponentPropertyDesc or GameObject$ComponentDesc in map format.
  (-> component-property-desc
      (protobuf/sanitize-repeated :properties properties/sanitize-property-desc)))

(defn sanitize-component-property-descs-at-key [any-desc component-property-descs-key]
  ;; GameObject$ComponentDesc, GameObject$InstanceDesc, GameObject$EmbeddedInstanceDesc, or GameObject$CollectionInstanceDesc in map format.
  ;; The specified key should address a seq of GameObject$ComponentPropertyDescs in map format.
  (protobuf/sanitize-repeated any-desc component-property-descs-key sanitize-component-property-desc))

(defn- sanitize-component-desc [component-desc]
  ;; GameObject$ComponentDesc in map format.
  (-> component-desc
      (sanitize-component-property-desc)
      (strip-default-scale-from-component-desc)))

(defn- sanitize-embedded-component-data [embedded-component-desc ext->embedded-component-resource-type]
  ;; GameObject$EmbeddedComponentDesc in map format.
  (let [component-ext (:type embedded-component-desc)
        resource-type (ext->embedded-component-resource-type component-ext)]
    (if (nil? resource-type)
      embedded-component-desc ; Unknown resource-type. Leave unsanitized.
      (let [tag-opts (:tag-opts resource-type)
            read-fn (:read-fn resource-type)
            sanitize-embedded-component-fn (:sanitize-embedded-component-fn (:component tag-opts))
            unsanitized-data-string (:data embedded-component-desc)]
        (try
          (let [sanitized-data
                (with-open [reader (StringReader. unsanitized-data-string)]
                  (read-fn reader))

                [embedded-component-desc sanitized-data]
                (if sanitize-embedded-component-fn
                  (sanitize-embedded-component-fn embedded-component-desc sanitized-data)
                  [embedded-component-desc sanitized-data])]
            (assoc embedded-component-desc :data sanitized-data))
          (catch Exception error
            ;; Leave unsanitized.
            (log/warn :msg (str "Failed to sanitize embedded component of type: " (or component-ext "nil")) :exception error)
            embedded-component-desc))))))

(defn- sanitize-embedded-component-desc [embedded-component-desc ext->embedded-component-resource-type]
  ;; GameObject$EmbeddedComponentDesc in map format.
  (-> embedded-component-desc
      (sanitize-embedded-component-data ext->embedded-component-resource-type)
      (strip-default-scale-from-component-desc)))

(defn sanitize-prototype-desc [prototype-desc ext->embedded-component-resource-type]
  {:pre [(map? prototype-desc)
         (ifn? ext->embedded-component-resource-type)]}
  ;; GameObject$PrototypeDesc in map format.
  (-> prototype-desc
      (dissoc :property-resources)
      (protobuf/sanitize-repeated :components sanitize-component-desc)
      (protobuf/sanitize-repeated :embedded-components #(sanitize-embedded-component-desc % ext->embedded-component-resource-type))))

(defn any-descs->duplicate-ids [any-instance-descs]
  ;; GameObject$ComponentDesc, GameObject$EmbeddedComponentDesc, GameObject$InstanceDesc, GameObject$EmbeddedInstanceDesc, or GameObject$CollectionInstanceDesc in map format.
  (into (sorted-set)
        (keep (fn [[id count]]
                (when (> count 1)
                  id)))
        (frequencies
          (map :id
               any-instance-descs))))

(defn maybe-duplicate-id-error [node-id duplicate-ids]
  (when (not-empty duplicate-ids)
    (g/->error node-id :build-targets :fatal nil (format "The following ids are not unique: %s" (string/join ", " duplicate-ids)))))

(defn- embedded-component-desc->dependencies [{:keys [id type data] :as _embedded-component-desc} ext->embedded-component-resource-type]
  ;; If sanitation failed (due to a corrupt file), the embedded data might still
  ;; be a string. In that case we report no dependencies. The load-fn will
  ;; eventually mark our resource node as defective, so it doesn't matter.
  (when (map? data)
    (when-some [component-resource-type (ext->embedded-component-resource-type type)]
      (let [component-dependencies-fn (:dependencies-fn component-resource-type)]
        (try
          (component-dependencies-fn data)
          (catch Exception error
            (log/warn :msg (format "Couldn't determine dependencies for embedded component '%s'." id) :exception error)
            nil))))))

(defn make-game-object-dependencies-fn [make-ext->embedded-component-resource-type-fn]
  {:pre [(ifn? make-ext->embedded-component-resource-type-fn)]}
  ;; TODO: This should probably also consider resource property overrides?
  (let [default-dependencies-fn (resource-node/make-ddf-dependencies-fn GameObject$PrototypeDesc)]
    (fn [prototype-desc]
      (let [ext->embedded-component-resource-type (make-ext->embedded-component-resource-type-fn)]
        (into (default-dependencies-fn prototype-desc)
              (mapcat #(embedded-component-desc->dependencies % ext->embedded-component-resource-type))
              (:embedded-components prototype-desc))))))

(defn embedded-component-instance-data [build-resource embedded-component-desc pose]
  {:pre [(workspace/build-resource? build-resource)
         (map? embedded-component-desc) ; GameObject$EmbeddedComponentDesc in map format.
         (pose/pose? pose)]}
  {:resource build-resource
   :pose pose
   :component-msg (-> embedded-component-desc
                      (dissoc :type :data)
                      (add-default-scale-to-component-desc))})

(defn referenced-component-instance-data [build-resource component-desc pose proj-path->resource-property-build-target]
  {:pre [(workspace/build-resource? build-resource)
         (map? component-desc) ; GameObject$ComponentDesc in map format, but PropertyDescs must have a :clj-value.
         (pose/pose? pose)
         (ifn? proj-path->resource-property-build-target)]}
  (let [go-props-with-source-resources (:properties component-desc) ; Every PropertyDesc must have a :clj-value with actual Resource, etc.
        [go-props go-prop-dep-build-targets] (properties/build-target-go-props proj-path->resource-property-build-target go-props-with-source-resources)]
    {:resource build-resource
     :pose pose
     :property-deps go-prop-dep-build-targets
     :component-msg (-> component-desc
                        (add-default-scale-to-component-desc)
                        (protobuf/assign-repeated :properties go-props))}))

(defn- build-game-object [build-resource dep-resources user-data]
  ;; Please refer to `/engine/gameobject/proto/gameobject/gameobject_ddf.proto`
  ;; when reading this. It will clear up how the output binaries are structured.
  ;; Be aware that these structures are also used to store the saved project
  ;; data. Sometimes a field will only be used by the editor *or* the runtime.
  ;; At this point, all referenced and embedded components will have emitted
  ;; BuildResources. The engine does not have a concept of an EmbeddedComponent.
  ;; They are written as separate binaries and referenced just like any other
  ;; ReferencedComponent. However, embedded components from different sources
  ;; might have been fused into one BuildResource if they had the same contents.
  ;; We must update any references to these BuildResources to instead point to
  ;; the resulting fused BuildResource. We also extract :component-instance-data
  ;; from the component build targets and embed these as ComponentDesc instances
  ;; in the PrototypeDesc that represents the game object.
  (let [component-instance-data->fused-build-resource-proj-path
        (fn component-instance-data->fused-build-resource-proj-path [component-instance-data]
          (let [build-resource (:resource component-instance-data)]
            (if-let [fused-build-resource (dep-resources build-resource)]
              (resource/proj-path fused-build-resource)
              (throw (ex-info (format "Failed to resolve fused build resource from '%s' referenced by component '%s'."
                                      (resource/proj-path build-resource)
                                      (-> component-instance-data :component-msg :id))
                              {:resource-reference build-resource})))))

        build-go-props (partial properties/build-go-props dep-resources)
        component-instance-datas (:component-instance-datas user-data)
        component-msgs (map :component-msg component-instance-datas)
        component-go-props (map (comp build-go-props :properties) component-msgs)
        component-build-resource-paths (map component-instance-data->fused-build-resource-proj-path component-instance-datas)
        component-descs (mapv (fn [component-msg fused-build-resource-path go-props]
                                (-> component-msg
                                    (dissoc :data :properties :type) ; Runtime uses :property-decls, not :properties
                                    (assoc :component fused-build-resource-path)
                                    (cond-> (seq go-props)
                                            (assoc :property-decls (properties/go-props->decls go-props false)))))
                              component-msgs
                              component-build-resource-paths
                              component-go-props)
        property-resource-paths (into (sorted-set)
                                      (comp cat (keep properties/try-get-go-prop-proj-path))
                                      component-go-props)
        prototype-desc {:components component-descs
                        :property-resources property-resource-paths}]
    {:resource build-resource
     :content (protobuf/map->bytes GameObject$PrototypeDesc prototype-desc)}))

(defn game-object-build-target [source-resource host-resource-node-id component-instance-datas component-build-targets]
  {:pre [(workspace/source-resource? source-resource)
         (g/node-id? host-resource-node-id)
         (vector? component-build-targets)]}
  ;; Extract the :component-instance-datas from the component build targets so
  ;; that overrides can be embedded in the resulting game object binary. We also
  ;; establish dependencies to build-targets from any resources referenced by
  ;; script property overrides.
  (bt/with-content-hash
    {:node-id host-resource-node-id
     :resource (workspace/make-build-resource source-resource)
     :build-fn build-game-object
     :user-data {:component-instance-datas (mapv #(dissoc % :property-deps)
                                                 component-instance-datas)}
     :deps (into component-build-targets
                 (comp (mapcat :property-deps)
                       (util/distinct-by (comp resource/proj-path :resource)))
                 component-instance-datas)}))

(defn component-scene [node-id node-outline-key ^Matrix4d transform-matrix source-component-resource-scene]
  {:pre [(g/node-id? node-id)
         (instance? Matrix4d transform-matrix)
         (or (nil? source-component-resource-scene) (map? source-component-resource-scene))]}
  (if source-component-resource-scene

    ;; We have a source scene. This is usually the case.
    (let [transform (if-some [^Matrix4d source-component-resource-transform (:transform source-component-resource-scene)]
                      (doto (Matrix4d. transform-matrix)
                        (.mul source-component-resource-transform))
                      transform-matrix)
          scene (-> source-component-resource-scene
                    (scene/claim-scene node-id node-outline-key)
                    (assoc :transform transform))
          updatable (:updatable source-component-resource-scene)]
      (if (nil? updatable)
        scene
        (let [claimed-updatable (assoc updatable :node-id node-id)]
          (scene/map-scene #(assoc % :updatable claimed-updatable)
                           scene))))

    ;; This handles the case of no scene from actual component. It could be bad
    ;; data, but then there are also components that don't have a scene output,
    ;; such as the Script component. The bad data case is covered by for
    ;; instance unknown_components.go in the test project.
    {:node-id node-id
     :transform transform-matrix
     :aabb geom/empty-bounding-box
     :renderable {:passes [pass/selection]}}))

(defn game-object-scene [node-id component-scenes]
  {:pre [(g/node-id? node-id)
         (or (nil? component-scenes)
             (vector? component-scenes))
         (not (vector? (first component-scenes)))]}
  (cond-> {:node-id node-id
           :aabb geom/null-aabb}

          (pos? (count component-scenes))
          (assoc :children component-scenes)))
