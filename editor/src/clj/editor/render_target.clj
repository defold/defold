;; Copyright 2020-2023 The Defold Foundation
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

(ns editor.render-target
  (:require [editor.build-target :as bt]
            [dynamo.graph :as g]
            [editor.graph-util :as gu]
            [editor.protobuf :as protobuf]
            [editor.resource-node :as resource-node]
            [editor.workspace :as workspace])
  (:import [com.dynamo.render.proto RenderTarget$RenderTargetDesc]))

(def ^:const atlas-icon "icons/32/Icons_13-Atlas.png")

(defn- set-form-op [{:keys [node-id]} [property] value]
  (g/set-property! node-id property value))

(defn- clear-form-op [{:keys [node-id]} [property]]
  (g/clear-property! node-id property))

(g/defnk produce-form-data
  [_node-id width height]
  {:form-ops {:user-data {:node-id _node-id}
              :set set-form-op
              :clear clear-form-op}
   :navigation false
   :sections [{:title "Render Target"
               :fields [{:path [:width]
                         :label "Width"
                         :type :number}
                        {:path [:height]
                         :label "Height"
                         :type :number}]}]
   :values {[:width] width
            [:height] height}})

(g/defnk produce-pb-msg
  [width height]
  {:width width
   :height height})

(defn build-render-target
  [resource dep-resources user-data]
  {:resource resource
   :content (protobuf/map->bytes RenderTarget$RenderTargetDesc (:pb-msg user-data))})

(g/defnk produce-build-targets
  [_node-id resource pb-msg]
  [(bt/with-content-hash
     {:node-id _node-id
      :resource (workspace/make-build-resource resource)
      :build-fn build-render-target
      :user-data {:pb-msg pb-msg}})])

(g/defnode RenderTargetNode
  (inherits resource-node/ResourceNode)

  (property width g/Num)
  (property height g/Num)

  (output pb-msg g/Any :cached produce-pb-msg)
  (output save-value g/Any (gu/passthrough pb-msg))
  (output form-data g/Any produce-form-data)
  (output build-targets g/Any :cached produce-build-targets))

(defn load-render-target [project self resource render-target]
  (g/set-property self
    :width (:width render-target)
    :height (:height render-target)))

(defn register-resource-types
  [workspace]
  (resource-node/register-ddf-resource-type workspace
                                            :ext "render_target"
                                            :node-type RenderTargetNode
                                            :ddf-type RenderTarget$RenderTargetDesc
                                            :load-fn load-render-target
                                            :icon atlas-icon
                                            :view-types [:cljfx-form-view :text]
                                            :view-opts {}
                                            :label "Render Target"))
