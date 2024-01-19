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
            [editor.gl.texture :as texture]
            [editor.protobuf :as protobuf]
            [editor.protobuf-forms :as protobuf-forms]
            [editor.resource-node :as resource-node]
            [editor.types :as types]
            [editor.workspace :as workspace])
  (:import [com.dynamo.graphics.proto Graphics$TextureImage$Type Graphics$TextureImage$TextureFormat]
           [com.dynamo.render.proto RenderTarget$RenderTargetDesc]))

(def ^:const atlas-icon "icons/32/Icons_13-Atlas.png")

(defn- set-form-op [{:keys [node-id]} [property] value]
  (g/set-property! node-id property value))

(defn- clear-form-op [{:keys [node-id]} [property]]
  (g/clear-property! node-id property))

(g/defnk produce-form-data
  [_node-id width height attachment-count attachment-type attachment-format depth-stencil]
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
                         :type :number}
                        {:path [:attachment-count]
                         :label "Attachment Count"
                         :type :number}
                        {:path [:attachment-type]
                         :label "Attachment Type"
                         :type :choicebox
                         :options (protobuf-forms/make-options (protobuf/enum-values Graphics$TextureImage$Type))
                         :default (ffirst (protobuf/enum-values Graphics$TextureImage$Type))}
                        {:path [:attachment-format]
                         :label "Attachment Format"
                         :type :choicebox
                         :options (protobuf-forms/make-options (protobuf/enum-values Graphics$TextureImage$TextureFormat))
                         :default (ffirst (protobuf/enum-values Graphics$TextureImage$TextureFormat))}
                        {:path [:depth-stencil]
                         :label "Depth / Stencil"
                         :type :boolean}]}]
   :values {[:width] width
            [:height] height
            [:attachment-count] attachment-count
            [:attachment-type] attachment-type
            [:attachment-format] attachment-format
            [:depth-stencil] depth-stencil}})

(g/defnk produce-pb-msg
  [width height attachment-count attachment-type attachment-format depth-stencil]
  {:width width
   :height height
   :attachment-count attachment-count
   :attachment-type attachment-type
   :attachment-format attachment-format
   :depth-stencil depth-stencil})

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

(defn- generate-gpu-texture [args request-id params unit]
  (texture/image-texture request-id nil))

(g/defnode RenderTargetNode
  (inherits resource-node/ResourceNode)

  (property width g/Num)
  (property height g/Num)
  (property attachment-count g/Num)
  (property attachment-type g/Keyword)
  (property attachment-format g/Keyword)
  (property depth-stencil g/Bool)

  (output size g/Any (g/fnk [width height]
                       {:width width
                        :height height}))
  (output pb-msg g/Any :cached produce-pb-msg)
  (output save-value g/Any (gu/passthrough pb-msg))
  (output form-data g/Any produce-form-data)
  (output gpu-texture-generator g/Any {:f generate-gpu-texture })
  (output build-targets g/Any :cached produce-build-targets))

(defn load-render-target [project self resource render-target]
  (g/set-property self
    :width (:width render-target)
    :height (:height render-target)
    :attachment-count (:attachment-count render-target)
    :attachment-type (:attachment-type render-target)
    :attachment-format (:attachment-format render-target)
    :depth-stencil (:depth-stencil render-target)))

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
