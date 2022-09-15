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

(ns editor.game-object-common
  (:require [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.build-target :as bt]
            [editor.properties :as properties]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.workspace :as workspace]
            [internal.util :as util]
            [service.log :as log])
  (:import [com.dynamo.gameobject.proto GameObject$PrototypeDesc]
           [java.io StringReader]
           [javax.vecmath Matrix4d]))

(set! *warn-on-reflection* true)

(def game-object-icon "icons/32/Icons_06-Game-object.png")

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

(defn- embedded-component-desc->dependencies [{:keys [id type data] :as _embedded-component-desc} ext->resource-type]
  (when-some [component-resource-type (ext->resource-type type)]
    (let [component-read-fn (:read-fn component-resource-type)
          component-dependencies-fn (:dependencies-fn component-resource-type)]
      (try
        (component-dependencies-fn
          (with-open [reader (StringReader. data)]
            (component-read-fn reader)))
        (catch Exception error
          (log/warn :msg (format "Couldn't determine dependencies for embedded component %s." id) :exception error)
          nil)))))

(defn make-game-object-dependencies-fn [workspace]
  ;; TODO: This should probably also consider resource property overrides?
  (let [default-dependencies-fn (resource-node/make-ddf-dependencies-fn GameObject$PrototypeDesc)]
    (fn [prototype-desc]
      (let [ext->resource-type (workspace/get-resource-type-map workspace)]
        (into (default-dependencies-fn prototype-desc)
              (mapcat #(embedded-component-desc->dependencies % ext->resource-type))
              (:embedded-components prototype-desc))))))

(defn embedded-component-instance-data [build-resource embedded-component-desc ^Matrix4d transform-matrix]
  {:pre [(workspace/build-resource? build-resource)
         (map? embedded-component-desc) ; EmbeddedComponentDesc in map format.
         (instance? Matrix4d transform-matrix)]}
  {:resource build-resource
   :transform transform-matrix
   :instance-msg embedded-component-desc})

(defn referenced-component-instance-data [build-resource component-desc ^Matrix4d transform-matrix proj-path->resource-property-build-target]
  {:pre [(workspace/build-resource? build-resource)
         (map? component-desc) ; ComponentDesc in map format, but PropertyDescs must have a :clj-value.
         (instance? Matrix4d transform-matrix)
         (ifn? proj-path->resource-property-build-target)]}
  (let [go-props-with-source-resources (:properties component-desc) ; Every PropertyDesc must have a :clj-value with actual Resource, etc.
        [go-props go-prop-dep-build-targets] (properties/build-target-go-props proj-path->resource-property-build-target go-props-with-source-resources)]
    {:resource build-resource
     :transform transform-matrix
     :property-deps go-prop-dep-build-targets
     :instance-msg (if (seq go-props)
                     (assoc component-desc :properties go-props)
                     component-desc)}))

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
  ;; the resulting fused BuildResource. We also extract :instance-data from the
  ;; component build targets and embed these as ComponentDesc instances in the
  ;; PrototypeDesc that represents the game object.
  (let [build-go-props (partial properties/build-go-props dep-resources)
        instance-data (:instance-data user-data)
        component-msgs (map :instance-msg instance-data)
        component-go-props (map (comp build-go-props :properties) component-msgs)
        component-build-resource-paths (map (comp resource/proj-path dep-resources :resource) instance-data)
        component-descs (map (fn [component-msg fused-build-resource-path go-props]
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

(defn game-object-build-target [build-resource host-resource-node-id component-instance-datas component-build-targets]
  {:pre [(workspace/build-resource? build-resource)
         (g/node-id? host-resource-node-id)
         (vector? component-instance-datas)
         (vector? component-build-targets)]}
  ;; Extract the :instance-data from the component build targets so that
  ;; overrides can be embedded in the resulting game object binary. We also
  ;; establish dependencies to build-targets from any resources referenced
  ;; by script property overrides.
  (bt/with-content-hash
    {:node-id host-resource-node-id
     :resource build-resource
     :build-fn build-game-object
     :user-data {:instance-data component-instance-datas}
     :deps (into component-build-targets
                 (comp (mapcat :property-deps)
                       (util/distinct-by (comp resource/proj-path :resource)))
                 component-instance-datas)}))
