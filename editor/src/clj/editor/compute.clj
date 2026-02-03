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

(ns editor.compute
  (:require [dynamo.graph :as g]
            [editor.build-target :as bt]
            [editor.code.shader-compilation :as shader-compilation]
            [editor.defold-project :as project]
            [editor.graph-util :as gu]
            [editor.localization :as localization]
            [editor.protobuf :as protobuf]
            [editor.protobuf-forms-util :as protobuf-forms-util]
            [editor.render-program-utils :as render-program-utils]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.validation :as validation]
            [editor.workspace :as workspace]
            [util.murmur :as murmur])
  (:import [com.dynamo.render.proto Compute$ComputeDesc]))

(set! *warn-on-reflection* true)

(def ^:private form-data
  {:navigation false
   :sections
   [{:localization-key "compute"
     :fields
     [{:path [:compute-program]
       :localization-key "compute.compute-program"
       :type :resource
       :filter "cp"}
      (render-program-utils/gen-form-data-constants "compute.constants" :constants)
      (render-program-utils/gen-form-data-samplers "compute.samplers" :samplers)]}]})

(g/defnk produce-form-data [_node-id compute-program constants samplers :as args]
  (let [values (select-keys args (mapcat :path (get-in form-data [:sections 0 :fields])))
        form-values (into {} (map (fn [[k v]] [[k] v]) values))]
    (-> form-data
        (assoc :values form-values)
        (assoc :form-ops {:user-data {:node-id _node-id}
                          :set protobuf-forms-util/set-form-op
                          :clear protobuf-forms-util/clear-form-op}))))

(g/defnk produce-save-value [compute-program constants samplers]
  (protobuf/make-map-without-defaults Compute$ComputeDesc
    :compute-program (resource/resource->proj-path compute-program)
    :constants (render-program-utils/editable-constants->constants constants)
    :samplers (render-program-utils/editable-samplers->samplers samplers)))

(defn- build-compute [resource build-resource->fused-build-resource user-data]
  (let [build-resource->fused-build-resource-path (comp resource/proj-path build-resource->fused-build-resource)
        compute-desc-with-fused-build-resource-paths
        (-> (:compute-desc-with-build-resources user-data)
            (update :compute-program build-resource->fused-build-resource-path))]
    {:resource resource
     :content (protobuf/map->bytes Compute$ComputeDesc compute-desc-with-fused-build-resource-paths)}))

(defn- build-target-samplers [samplers]
  (mapv (fn [sampler]
          (assoc sampler :name-hash (murmur/hash64 (:name sampler))))
        samplers))

(defn- prop-resource-error [_node-id prop-kw prop-value prop-name resource-ext]
  (validation/prop-error :fatal _node-id prop-kw validation/prop-resource-ext? prop-value resource-ext prop-name))

(g/defnk produce-build-targets [_node-id save-value resource shader-source-info compute-program]
  (or (g/flatten-errors
        (prop-resource-error _node-id :compute-program compute-program "Compute Program" "cp"))
      (let [compute-shader-build-target (shader-compilation/make-shader-build-target _node-id [shader-source-info] 0 true)
            dep-build-targets [compute-shader-build-target]
            compute-desc-with-build-resources (assoc save-value
                                                :compute-program (:resource compute-shader-build-target)
                                                :samplers (build-target-samplers (:samplers save-value)))]
        [(bt/with-content-hash
           {:node-id _node-id
            :resource (workspace/make-build-resource resource)
            :build-fn build-compute
            :user-data {:compute-desc-with-build-resources compute-desc-with-build-resources}
            :deps dep-build-targets})])))

(g/defnode ComputeNode
  (inherits resource-node/ResourceNode)

  (property compute-program resource/Resource ; Required protobuf field.
            (dynamic visible (g/constantly false))
            (value (gu/passthrough program-resource))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter evaluation-context self old-value new-value
                                            [:resource :program-resource]
                                            [:shader-source-info :shader-source-info]))))
  (property constants g/Any ; Nil is valid default.
            (dynamic visible (g/constantly false)))
  (property samplers g/Any ; Nil is valid default.
            (dynamic visible (g/constantly false))
            (value (gu/passthrough samplers)))

  (input program-resource resource/Resource)
  (input shader-source-info g/Any)

  (output form-data g/Any :cached produce-form-data)
  (output save-value g/Any :cached produce-save-value)
  (output build-targets g/Any :cached produce-build-targets)
  (output samplers [g/KeywordMap] (gu/passthrough samplers)))

(defn- sanitize-compute [compute-desc]
  {:pre [(map? compute-desc)]} ; Compute$ComputeDesc in map format.
  (protobuf/sanitize-repeated compute-desc :constants render-program-utils/sanitize-constant))

(defn load-compute [_project self resource compute-desc]
  {:pre [(map? compute-desc)]} ; Compute$ComputeDesc in map format.
  (let [resolve-resource #(workspace/resolve-resource resource %)]
    (gu/set-properties-from-pb-map self Compute$ComputeDesc compute-desc
      compute-program (resolve-resource :compute-program)
      constants (render-program-utils/constants->editable-constants :constants)
      samplers (render-program-utils/samplers->editable-samplers :samplers))))

(defn register-resource-types [workspace]
  (resource-node/register-ddf-resource-type workspace
    :ext "compute"
    :label (localization/message "resource.type.compute")
    :node-type ComputeNode
    :ddf-type Compute$ComputeDesc
    :load-fn load-compute
    :sanitize-fn sanitize-compute
    :icon "icons/32/Icons_31-Material.png"
    :icon-class :property
    :category (localization/message "resource.category.shaders")
    :view-types [:cljfx-form-view :text]))
