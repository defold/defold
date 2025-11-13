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

(ns editor.animation-set
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.build-target :as bt]
            [editor.defold-project :as project]
            [editor.graph-util :as gu]
            [editor.localization :as localization]
            [editor.model-scene :as model-scene]
            [editor.protobuf :as protobuf]
            [editor.protobuf-forms-util :as protobuf-forms-util]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.workspace :as workspace]
            [service.log :as log]
            [util.murmur :as murmur])
  (:import [com.dynamo.bob.pipeline AnimationSetBuilder LoaderException ModelUtil]
           [com.dynamo.rig.proto Rig$AnimationSet Rig$AnimationSetDesc]
           [java.util ArrayList]))

(set! *warn-on-reflection* true)

(def ^:private animation-set-icon "icons/32/Icons_24-AT-Animation.png")

(defn is-animation-set? [resource]
  (= (:ext resource) "animationset"))

(defn- animation-instance-desc-pb-msg [resource]
  {:animation (resource/resource->proj-path resource)})

(g/defnk produce-save-value [animation-resources]
  (protobuf/make-map-without-defaults Rig$AnimationSetDesc
    :animations (mapv animation-instance-desc-pb-msg animation-resources)))

(defn- update-animation-info [resource animations-resource info]
  (let [parent-resource (:parent-resource info)
        animation-set-resource (or animations-resource resource) ; animations-resource is nil when resource is an animation set
        ;; If we're at the top level, the parent id should be ""
        parent-id (if (or (nil? parent-resource)
                          (= parent-resource animation-set-resource))
                    ""
                    (resource/base-name parent-resource))]
    (-> info
        (assoc :parent-id parent-id)
        (assoc :parent-resource (or parent-resource resource)))))

;; Also used by model.clj
(g/defnk produce-animation-info [resource animations-resource animation-infos]
  (into []
        (comp cat
              (map (partial update-animation-info resource animations-resource)))
        animation-infos))

(defn- load-and-validate-animation-set [resource animations-resource animation-info]
  (let [animations-resource (if (nil? animations-resource) resource animations-resource)
        is-animation-set (is-animation-set? animations-resource)
        paths (map (fn [x] (:path x)) animation-info)
        streams (map (fn [x] (io/input-stream (:resource x))) animation-info)
        ;; clean up the parent-id if it's the current resource
        parent-ids (map (fn [x] (:parent-id x)) animation-info)
        parent-resources (map (fn [x] (:parent-resource x)) animation-info)

        animation-set-builder (Rig$AnimationSet/newBuilder)
        animation-ids (ArrayList.)
        workspace (resource/workspace resource)
        project-directory (workspace/project-directory workspace)
        data-resolver (ModelUtil/createFileDataResolver project-directory)]

    (AnimationSetBuilder/buildAnimations is-animation-set paths streams data-resolver parent-ids animation-set-builder animation-ids)
    (let [animation-set (protobuf/pb->map-with-defaults (.build animation-set-builder))]
      {:animation-set animation-set
       :animation-ids (vec animation-ids)})))

;; Also used by model.clj
(g/defnk produce-animation-set-info [_node-id resource animations-resource animation-info]
  (try
    (if (empty? animation-info)
      {:animation-set []
       :animation-ids []}
      (load-and-validate-animation-set resource animations-resource animation-info))
    (catch LoaderException e
      (log/error :message (str "Error loading: " (resource/resource->proj-path resource)) :exception e)
      (g/->error _node-id :animations :fatal resource
                 (str "Failed to build " (resource/resource->proj-path resource)
                      ": " (.getMessage e))))))

(g/defnk produce-animation-set [animation-set-info]
  (:animation-set animation-set-info))

(g/defnk produce-animation-ids [resource animation-set-info]
  (:animation-ids animation-set-info))

;; used by the model.clj
(defn hash-animation-set-ids [animation-set]
  (update animation-set :animations (partial mapv #(update % :id murmur/hash64))))

(defn- build-animation-set [resource _dep-resources user-data]
  {:resource resource
   :content (protobuf/map->bytes Rig$AnimationSet (:animation-set user-data))})

(g/defnk produce-animation-set-build-target [_node-id resource animation-set]
  (when (not (empty? animation-set))
    (bt/with-content-hash
       {:node-id _node-id
        :resource (workspace/make-build-resource resource)
        :build-fn build-animation-set
        :user-data {:animation-set animation-set}})))

(def ^:private form-sections
  {:navigation false
   :sections
   [{:localization-key "animationset"
     :fields [{:path [:animations]
               :type :list
               :localization-key "animationset.animations"
               :element {:type :resource
                         :filter model-scene/animation-file-types
                         :default nil}}]}]})

(g/defnk produce-form-data [_node-id animation-resources]
  (let [values {[:animations] animation-resources}]
    (-> form-sections
        (assoc :form-ops {:user-data {:node-id _node-id}
                          :set protobuf-forms-util/set-form-op
                          :clear protobuf-forms-util/clear-form-op})
        (assoc :values values))))

(g/defnode AnimationSetNode
  (inherits resource-node/ResourceNode)

  (input animations-resource resource/Resource)
  (input animation-resources resource/Resource :array)
  (input animation-sets g/Any :array)
  (input animation-infos g/Any :array)
  (output animation-info g/Any :cached produce-animation-info)

  (property animations resource/ResourceVec ; Nil is valid default.
            (value (gu/passthrough animation-resources))
            (set (fn [evaluation-context self old-value new-value]
                   (let [project (project/get-project (:basis evaluation-context) self)
                         connections [[:resource :animation-resources]
                                      [:animation-set :animation-sets]
                                      [:animation-info :animation-infos]]]
                     (concat
                       (for [old-resource old-value]
                         (if old-resource
                           (project/disconnect-resource-node evaluation-context project old-resource self connections)
                           (g/disconnect project :nil-resource self :animation-resources)))
                       (for [new-resource new-value]
                         (if new-resource
                           (:tx-data (project/connect-resource-node evaluation-context project new-resource self connections))
                           (g/connect project :nil-resource self :animation-resources)))))))
            (dynamic visible (g/constantly false)))

  (output form-data g/Any :cached produce-form-data)
  (output save-value g/Any :cached produce-save-value)
  (output animation-set-info g/Any :cached produce-animation-set-info)
  (output animation-set g/Any produce-animation-set)
  (output animation-ids g/Any produce-animation-ids)
  (output animation-set-build-target g/Any :cached produce-animation-set-build-target))

(defn- load-animation-set [_project self resource animation-set-desc]
  {:pre [(map? animation-set-desc)]} ; Rig$AnimationSetDesc in map format
  (let [resolve-resource #(workspace/resolve-resource resource %)
        animation-instance-descs->animation-resources #(mapv (comp resolve-resource :animation) %)]
    (gu/set-properties-from-pb-map self Rig$AnimationSetDesc animation-set-desc
      animations (animation-instance-descs->animation-resources :animations))))

(defn- sanitize-animation-set [animation-set-desc]
  (dissoc animation-set-desc :skeleton)) ; Deprecated field.

(defn register-resource-types [workspace]
  (resource-node/register-ddf-resource-type workspace
    :ext "animationset"
    :icon animation-set-icon
    :icon-class :property
    :label (localization/message "resource.type.animationset")
    :load-fn load-animation-set
    :sanitize-fn sanitize-animation-set
    :node-type AnimationSetNode
    :ddf-type Rig$AnimationSetDesc
    :view-types [:cljfx-form-view :text]))
