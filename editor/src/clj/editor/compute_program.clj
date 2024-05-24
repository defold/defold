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

(ns editor.compute-program
  (:require [dynamo.graph :as g]
            [editor.build-target :as bt]
            [editor.code.shader :as code.shader]
            [editor.defold-project :as project]
            [editor.graph-util :as gu]
            [editor.material :as material]
            [editor.protobuf :as protobuf]
            [editor.protobuf-forms :as protobuf-forms]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.validation :as validation]
            [editor.workspace :as workspace]
            [util.murmur :as murmur])
  (:import [com.dynamo.render.proto ComputeProgram$ComputeProgramDesc Material$MaterialDesc$ConstantType Material$MaterialDesc$FilterModeMag Material$MaterialDesc$FilterModeMin Material$MaterialDesc$WrapMode]))

(set! *warn-on-reflection* true)

(def ^:private form-data
  (let [constant-values (protobuf/enum-values Material$MaterialDesc$ConstantType)]
    {:navigation false
     :sections
     [{:title "Compute Program"
       :fields
       [{:path [:program]
         :label "Compute Program"
         :type :resource :filter "cp"}
        {:path [:constants]
         :label "Constants"
         :type :table
         :columns [{:path [:name] :label "Name" :type :string}
                   {:path [:type]
                    :label "Type"
                    :type :choicebox
                    :options (protobuf-forms/make-options constant-values)
                    :default (ffirst constant-values)}
                   {:path [:value] :label "Value" :type :vec4}]}
        {:path [:samplers]
         :label "Samplers"
         :type :table
         :columns (let [wrap-options (protobuf/enum-values Material$MaterialDesc$WrapMode)
                        min-options (protobuf/enum-values Material$MaterialDesc$FilterModeMin)
                        mag-options (protobuf/enum-values Material$MaterialDesc$FilterModeMag)]
                    [{:path [:name] :label "Name" :type :string}
                     {:path [:wrap-u]
                      :label "Wrap U"
                      :type :choicebox
                      :options (protobuf-forms/make-options wrap-options)
                      :default (ffirst wrap-options)}
                     {:path [:wrap-v]
                      :label "Wrap V"
                      :type :choicebox
                      :options (protobuf-forms/make-options wrap-options)
                      :default (ffirst wrap-options)}
                     {:path [:filter-min]
                      :label "Filter Min"
                      :type :choicebox
                      :options (protobuf-forms/make-options min-options)
                      :default (ffirst min-options)}
                     {:path [:filter-mag]
                      :label "Filter Mag"
                      :type :choicebox
                      :options (protobuf-forms/make-options mag-options)
                      :default (ffirst mag-options)}
                     {:path [:max-anisotropy]
                      :label "Max Anisotropy"
                      :type :number}])}]}]}))

(g/defnk produce-form-data [_node-id name program constants samplers :as args]
  (let [values (select-keys args (mapcat :path (get-in form-data [:sections 0 :fields])))
        form-values (into {} (map (fn [[k v]] [[k] v]) values))]
    (-> form-data
        (assoc :values form-values))))

;; Load/Save/PB
(g/defnk produce-base-pb-msg [program constants samplers :as base-pb-msg]
  (-> base-pb-msg
      (update :program resource/resource->proj-path)
      (update :constants material/hack-upgrade-constants)))

(defn- build-compute-program [resource build-resource->fused-build-resource user-data]
  (println "poopt" resource build-resource->fused-build-resource user-data)
  (let [build-resource->fused-build-resource-path (comp resource/proj-path build-resource->fused-build-resource)
        compute-desc-with-fused-build-resource-paths
        (-> (:compute-program-desc-with-build-resources user-data)
            (update :program build-resource->fused-build-resource-path))]
    {:resource resource
     :content (protobuf/map->bytes ComputeProgram$ComputeProgramDesc compute-desc-with-fused-build-resource-paths)}))

(defn- build-target-samplers [samplers]
  (mapv (fn [sampler]
          (assoc sampler :name-hash (murmur/hash64 (:name sampler))))
        samplers))

(defn- prop-resource-error [_node-id prop-kw prop-value prop-name resource-ext]
  (validation/prop-error :fatal _node-id prop-kw validation/prop-resource-ext? prop-value resource-ext prop-name))

(g/defnk produce-build-targets [_node-id base-pb-msg resource shader-source-info program]
  (or (g/flatten-errors
        (prop-resource-error _node-id :program program "Compute Program" "cp"))
      (let [compile-spirv true
            compute-shader-build-target (code.shader/make-shader-build-target shader-source-info compile-spirv 0)
            dep-build-targets [compute-shader-build-target]
            compute-program-desc-with-build-resources (assoc base-pb-msg
                                                        :program (:resource compute-shader-build-target)
                                                        :samplers (build-target-samplers (:samplers base-pb-msg)))]
        [(bt/with-content-hash
           {:node-id _node-id
            :resource (workspace/make-build-resource resource)
            :build-fn build-compute-program
            :user-data {:compute-program-desc-with-build-resources compute-program-desc-with-build-resources}
            :deps dep-build-targets})])))

(g/defnode ComputeProgramNode
  (inherits resource-node/ResourceNode)

  (property name g/Str (dynamic visible (g/constantly false)))

  (property program resource/Resource
            (dynamic visible (g/constantly false))
            (value (gu/passthrough program-resource))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter evaluation-context self old-value new-value
                                            [:resource :program-resource]
                                            [:shader-source-info :shader-source-info]))))
  (property constants g/Any (dynamic visible (g/constantly false)))
  (property samplers g/Any
            (dynamic visible (g/constantly false))
            (value (gu/passthrough samplers)))

  (input program-resource resource/Resource)
  (input shader-source-info g/Any)

  (output form-data g/Any :cached produce-form-data)
  (output base-pb-msg g/Any produce-base-pb-msg)
  (output save-value g/Any (gu/passthrough base-pb-msg))
  (output build-targets g/Any :cached produce-build-targets)
  (output samplers [g/KeywordMap] (gu/passthrough samplers)))

(defn load-compute-program [project self resource pb]
  (concat
    (g/set-property self :program (workspace/resolve-resource resource (:program pb)))
    (g/set-property self :constants (material/hack-downgrade-constants (:constants pb)))
    (g/set-property self :samplers (:samplers pb))))

(defn register-resource-types [workspace]
  (resource-node/register-ddf-resource-type workspace
    :ext "compute_program"
    :label "Compute Program"
    :node-type ComputeProgramNode
    :ddf-type ComputeProgram$ComputeProgramDesc
    :load-fn load-compute-program
    :icon "icons/32/Icons_31-Material.png"
    :icon-class :property
    :view-types [:cljfx-form-view :text]))
