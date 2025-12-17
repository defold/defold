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

(ns editor.render-pb
  (:require [dynamo.graph :as g]
            [editor.build-target :as bt]
            [editor.core :as core]
            [editor.defold-project :as project]
            [editor.graph-util :as gu]
            [editor.localization :as localization]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.validation :as validation]
            [editor.workspace :as workspace])
  (:import [com.dynamo.render.proto Render$RenderPrototypeDesc Render$RenderPrototypeDesc$RenderResourceDesc]))

(g/defnode NamedRenderResource
  (property name g/Str ; Required protobuf field.
            (dynamic visible (g/constantly false)))
  (property render-resource resource/Resource ; Required protobuf field.
            (value (gu/passthrough resource))
            (set (fn [evaluation-context self old-value new-value]
                   (let [project (project/get-project (:basis evaluation-context) self)
                         connections [[:resource :resource]
                                      [:build-targets :dep-build-targets]]]
                     (concat
                       (if old-value
                         (project/disconnect-resource-node evaluation-context project old-value self connections)
                         (g/disconnect project :nil-resource self :resource))
                       (if new-value
                         (:tx-data (project/connect-resource-node evaluation-context project new-value self connections))
                         (g/connect project :nil-resource self :resource))))))
            (dynamic visible (g/constantly false)))

  (input resource resource/Resource)
  (input dep-build-targets g/Any)

  (output dep-build-targets g/Any (gu/passthrough dep-build-targets))
  (output named-render-resource g/Any (g/fnk [_node-id name render-resource]
                                        {:name name
                                         :path render-resource})))

(defn- make-named-render-resource-node
  [graph-id render-node name render-resource-resource]
  (g/make-nodes
    graph-id
    [named-render-resource [NamedRenderResource :name name :render-resource render-resource-resource]]
    (g/connect named-render-resource :_node-id render-node :nodes)
    (g/connect named-render-resource :named-render-resource render-node :named-render-resources)
    (g/connect named-render-resource :dep-build-targets render-node :dep-build-targets)))


(def ^:private form-sections
  {:navigation false
   :sections [{:localization-key "render"
               :fields [{:path [:script]
                         :type :resource
                         :filter "render_script"
                         :localization-key "render.script"}
                        {:path [:named-render-resources]
                         :type :table
                         :localization-key "render.render-resources"
                         :columns [{:path [:name]
                                    :localization-key "render.render-resources.name"
                                    :type :string
                                    :default "New Render Resource"}
                                   {:path [:path]
                                    :localization-key "render.render-resources.resource"
                                    :type :resource
                                    :filter ["material" "render_target" "compute"]
                                    :default nil}]}]}]})

(defn- set-form-op [{:keys [node-id] :as user-data} path value]
  (condp = path
    [:script] (g/set-property node-id :script value)
    [:named-render-resources] (let [graph-id (g/node-id->graph-id node-id)]
                                (concat
                                  (for [[named-render-resource-id _] (g/sources-of node-id :named-render-resources)]
                                    (g/delete-node named-render-resource-id))
                                  (for [{:keys [name path]} value]
                                    (make-named-render-resource-node graph-id node-id name path))))))

(g/defnk produce-form-data [_node-id script-resource named-render-resources]
  (-> form-sections
      (assoc :form-ops {:user-data {:node-id _node-id}
                        :set set-form-op})
      (assoc :values {[:script] script-resource
                      [:named-render-resources] named-render-resources})))

(g/defnk produce-save-value [script-resource named-render-resources]
  (protobuf/make-map-without-defaults Render$RenderPrototypeDesc
    :script (resource/resource->proj-path script-resource)
    :render-resources (mapv (fn [{:keys [name path]}]
                              (protobuf/make-map-without-defaults Render$RenderPrototypeDesc$RenderResourceDesc
                                :name name
                                :path (resource/resource->proj-path path)))
                            named-render-resources)))

(defn- build-render [resource dep-resources user-data]
  (let [{:keys [pb-msg built-resources]} user-data
        built-pb (reduce (fn [pb [path built-resource]]
                           (assoc-in pb path (resource/proj-path (get dep-resources built-resource))))
                         pb-msg
                         built-resources)]
    {:resource resource :content (protobuf/map->bytes Render$RenderPrototypeDesc built-pb)}))

(defn- build-errors
  [_node-id script named-render-resources]
  (when-let [errors (->> (into [(or (validation/prop-error :fatal _node-id :script validation/prop-resource-missing? script "Script")
                                    (validation/prop-error :fatal _node-id :script validation/prop-resource-ext? script "render_script" "Script"))]
                               (for [{:keys [name path]} named-render-resources]
                                 (validation/prop-error :fatal _node-id :path validation/prop-resource-missing? path name)))
                         (remove nil?)
                         (seq))]
    (g/error-aggregate errors)))

(g/defnk produce-build-targets [_node-id resource save-value dep-build-targets script named-render-resources]
  (or (build-errors _node-id script named-render-resources)
      (let [dep-build-targets (flatten dep-build-targets)
            deps-by-source (into {} (map #(let [build-resource (:resource %)
                                                source-resource (:resource build-resource)]
                                            [source-resource build-resource])
                                         dep-build-targets))
            built-resources (into [[[:script] (when script (deps-by-source script))]]
                                  (map-indexed (fn [i {:keys [path] :as named-render-resource}]
                                                 [[:render-resources i :path]
                                                  (when path (deps-by-source path))])
                                               named-render-resources))]
        [(bt/with-content-hash
           {:node-id _node-id
            :resource (workspace/make-build-resource resource)
            :build-fn build-render
            :user-data {:pb-msg save-value
                        :built-resources built-resources}
            :deps dep-build-targets})])))

(g/defnode RenderNode
  (inherits core/Scope)
  (inherits resource-node/ResourceNode)

  (property script resource/Resource ; Required protobuf field.
            (dynamic visible (g/constantly false))
            (value (gu/passthrough script-resource))
            (set (fn [evaluation-context self old-value new-value]
                   (project/resource-setter evaluation-context self old-value new-value
                                            [:resource :script-resource]
                                            [:build-targets :dep-build-targets]))))
  (input script-resource resource/Resource)

  (input named-render-resources g/Any :array)
  (input dep-build-targets g/Any :array)

  (output form-data g/Any :cached produce-form-data)
  (output save-value g/Any :cached produce-save-value)
  (output build-targets g/Any :cached produce-build-targets))

(defn- load-render [project self resource render-ddf]
  (let [graph-id (g/node-id->graph-id self)
        {script-path :script render-resources :render-resources} render-ddf]
    (concat
      (g/set-property self :script (workspace/resolve-resource resource script-path))
      (for [{:keys [name path]} render-resources]
        (let [render-resource (workspace/resolve-resource resource path)]
          (make-named-render-resource-node graph-id self name render-resource))))))

(defn- sanitize-render [render-ddf]
  (let [migrated-materials (mapv (fn [material-desc]
                                   {:name (:name material-desc)
                                    :path (:material material-desc)})
                                 (:materials render-ddf))]
    (-> render-ddf
        (dissoc :materials)
        (protobuf/assign-repeated :render-resources (into migrated-materials (:render-resources render-ddf))))))

(defn register-resource-types [workspace]
  (resource-node/register-ddf-resource-type workspace
    :ext "render"
    :node-type RenderNode
    :ddf-type Render$RenderPrototypeDesc
    :load-fn load-render
    :sanitize-fn sanitize-render
    :icon "icons/32/Icons_30-Render.png"
    :icon-class :property
    :view-types [:cljfx-form-view :text]
    :label (localization/message "resource.type.render")))
