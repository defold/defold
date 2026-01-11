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

(ns editor.render-target
  (:require [dynamo.graph :as g]
            [editor.build-target :as bt]
            [editor.graph-util :as gu]
            [editor.localization :as localization]
            [editor.protobuf :as protobuf]
            [editor.protobuf-forms :as protobuf-forms]
            [editor.protobuf-forms-util :as protobuf-forms-util]
            [editor.resource-node :as resource-node]
            [editor.texture-util :as texture-util]
            [editor.validation :as validation]
            [editor.workspace :as workspace]
            [util.fn :as fn])
  (:import [com.dynamo.graphics.proto Graphics$TextureImage$TextureFormat]
           [com.dynamo.render.proto RenderTarget$RenderTargetDesc RenderTarget$RenderTargetDesc$ColorAttachment RenderTarget$RenderTargetDesc$DepthStencilAttachment]))

(def ^:const texture-icon "icons/32/Icons_36-Texture.png")

; This must match 'MAX_BUFFER_COLOR_ATTACHMENTS' in engine/graphics/src/graphics.h
(def ^:private max-color-attachment-count 4)

(def form-data
  {:navigation false
   :sections
   [{:localization-key "render-target"
     :fields [{:path [:color-attachments]
               :localization-key "render-target.color-attachments"
               :type :table
               :columns [{:path [:width]
                          :localization-key "render-target.color-attachments.width"
                          :type :integer
                          :default 128}
                         {:path [:height]
                          :localization-key "render-target.color-attachments.height"
                          :type :integer
                          :default 128}
                         {:path [:format]
                          :localization-key "render-target.color-attachments.format"
                          :type :choicebox
                          :options (protobuf-forms/make-enum-options Graphics$TextureImage$TextureFormat)
                          :default :texture-format-rgba}]}
              {:path [:depth-stencil-attachment-width]
               :localization-key "render-target.depth-stencil-attachment-width"
               :type :integer}
              {:path [:depth-stencil-attachment-height]
               :localization-key "render-target.depth-stencil-attachment-height"
               :type :integer}
              {:path [:depth-stencil-attachment-texture-storage]
               :localization-key "render-target.depth-stencil-attachment-texture-storage"
               :type :boolean}]}]})

(g/defnk produce-form-data [_node-id color-attachments depth-stencil-attachment-width depth-stencil-attachment-height depth-stencil-attachment-texture-storage :as args]
  (let [values (select-keys args (mapcat :path (get-in form-data [:sections 0 :fields])))
        form-values (into {} (map (fn [[k v]] [[k] v]) values))]
    (-> form-data
        (assoc :values form-values)
        (assoc :form-ops {:user-data {:node-id _node-id}
                          :set protobuf-forms-util/set-form-op
                          :clear protobuf-forms-util/clear-form-op}))))

(g/defnk produce-save-value
  [color-attachments depth-stencil-attachment-width depth-stencil-attachment-height depth-stencil-attachment-format depth-stencil-attachment-texture-storage]
  (let [color-attachments
        (mapv #(protobuf/clear-defaults RenderTarget$RenderTargetDesc$ColorAttachment %)
              color-attachments)

        depth-stencil-attachment
        (protobuf/make-map-without-defaults RenderTarget$RenderTargetDesc$DepthStencilAttachment
          :width depth-stencil-attachment-width
          :height depth-stencil-attachment-height
          :format depth-stencil-attachment-format
          :texture-storage depth-stencil-attachment-texture-storage)]

    (protobuf/make-map-without-defaults RenderTarget$RenderTargetDesc
      :color-attachments color-attachments
      :depth-stencil-attachment depth-stencil-attachment)))

(defn build-render-target
  [resource _dep-resources user-data]
  {:resource resource
   :content (protobuf/map->bytes RenderTarget$RenderTargetDesc (:pb-msg user-data))})

(g/defnk produce-build-targets
  [_node-id resource save-value build-errors]
  (g/precluding-errors build-errors
    [(bt/with-content-hash
       {:node-id _node-id
        :resource (workspace/make-build-resource resource)
        :build-fn build-render-target
        :user-data {:pb-msg save-value}})]))

(defn- validate-color-attachment-count [v name]
  (when (> (count v) max-color-attachment-count)
    (format "'%s' render targets cannot have more than %d color attachments"
            name max-color-attachment-count)))

(defn- color-attachment->error-values [color-attachment-index {:keys [width height]} node-id label]
  (filterv some?
           [(when (< width 1)
              (g/->error node-id label :fatal width
                         (format "'Color attachment %d' must have a greater than zero width"
                                 color-attachment-index)))
            (when (< height 1)
              (g/->error node-id label :fatal height
                         (format "'Color attachment %d' must have a greater than zero height"
                                 color-attachment-index)))]))

(g/defnode RenderTargetNode
  (inherits resource-node/ResourceNode)

  (property color-attachments g/Any ; Nil is valid default.
            (dynamic visible (g/constantly false)))
  (property depth-stencil-attachment-width g/Int ; Required protobuf field.
            (dynamic visible (g/constantly false)))
  (property depth-stencil-attachment-height g/Int ; Required protobuf field.
            (dynamic visible (g/constantly false)))
  (property depth-stencil-attachment-format g/Any (default (protobuf/default RenderTarget$RenderTargetDesc$DepthStencilAttachment :format))
            (dynamic visible (g/constantly false)))
  (property depth-stencil-attachment-texture-storage g/Bool (default (protobuf/default RenderTarget$RenderTargetDesc$DepthStencilAttachment :texture-storage))
            (dynamic visible (g/constantly false)))

  (output save-value g/Any :cached produce-save-value)
  (output form-data g/Any produce-form-data)
  (output gpu-texture-generator g/Any (g/constantly texture-util/placeholder-gpu-texture-generator))
  (output build-targets g/Any :cached produce-build-targets)
  (output build-errors g/Any (g/fnk [_node-id color-attachments depth-stencil-attachment-width depth-stencil-attachment-height]
                               (g/package-errors _node-id
                                                 (validation/prop-error :fatal _node-id :color-attachments validate-color-attachment-count color-attachments "Color Attachments")
                                                 (into [] (map-indexed
                                                            (fn [i color-attachment]
                                                              (color-attachment->error-values i color-attachment _node-id :color-attachments))
                                                            color-attachments))
                                                 (validation/prop-error :fatal _node-id :depth-stencil-attachment-width validation/prop-negative? depth-stencil-attachment-width "Depth/Stencil Width")
                                                 (validation/prop-error :fatal _node-id :depth-stencil-attachment-height validation/prop-negative? depth-stencil-attachment-height "Depth/Stencil Height")
                                                 (when (and (> depth-stencil-attachment-width 0) (= 0 depth-stencil-attachment-height))
                                                   (g/->error _node-id :depth-stencil-attachment-width :fatal depth-stencil-attachment-width
                                                              (format "Incorrect Depth/Stencil attachment: The width is greater than zero, but the height is zero")))
                                                 (when (and (> depth-stencil-attachment-height 0) (= 0 depth-stencil-attachment-width))
                                                   (g/->error _node-id :depth-stencil-attachment-height :fatal depth-stencil-attachment-height
                                                              (format "Incorrect Depth/Stencil attachment: The height is greater than zero, but the width is zero")))))))

(defn load-render-target [_project self _resource render-target-desc]
  {:pre [(map? render-target-desc)]} ; RenderTarget$RenderTargetDesc in map format.
  ;; Inject any missing defaults into the stripped pb-map for form-view editing.
  (let [render-target-desc (protobuf/inject-defaults RenderTarget$RenderTargetDesc render-target-desc)
        depth-stencil-attachment (:depth-stencil-attachment render-target-desc)]
    (concat
      (gu/set-properties-from-pb-map self RenderTarget$RenderTargetDesc render-target-desc
        color-attachments :color-attachments)
      (gu/set-properties-from-pb-map self RenderTarget$RenderTargetDesc$DepthStencilAttachment depth-stencil-attachment
        depth-stencil-attachment-width :width
        depth-stencil-attachment-height :height
        depth-stencil-attachment-format :format
        depth-stencil-attachment-texture-storage :texture-storage))))

(def ^:private default-pb-depth-stencil-attachment (protobuf/default-message RenderTarget$RenderTargetDesc$DepthStencilAttachment #{:required}))

(defn- sanitize-render-target [render-target-desc]
  {:pre [(map? render-target-desc)]} ; RenderTarget$RenderTargetDesc in map format.
  (-> render-target-desc
      (update :depth-stencil-attachment fn/or default-pb-depth-stencil-attachment)))

(defn register-resource-types
  [workspace]
  (resource-node/register-ddf-resource-type workspace
    :ext "render_target"
    :node-type RenderTargetNode
    :ddf-type RenderTarget$RenderTargetDesc
    :load-fn load-render-target
    :sanitize-fn sanitize-render-target
    :icon texture-icon
    :icon-class :design
    :view-types [:cljfx-form-view :text]
    :view-opts {}
    :label (localization/message "resource.type.render-target")))
