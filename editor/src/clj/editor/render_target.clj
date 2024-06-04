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
  (:require [dynamo.graph :as g]
            [editor.build-target :as bt]
            [editor.gl.texture :as texture]
            [editor.graph-util :as gu]
            [editor.protobuf :as protobuf]
            [editor.protobuf-forms :as protobuf-forms]
            [editor.resource-node :as resource-node]
            [editor.validation :as validation]
            [editor.workspace :as workspace])
  (:import [com.dynamo.graphics.proto Graphics$TextureImage$TextureFormat]
           [com.dynamo.render.proto RenderTarget$RenderTargetDesc]))

(def ^:const texture-icon "icons/32/Icons_36-Texture.png")

; This must match 'MAX_BUFFER_COLOR_ATTACHMENTS' in engine/graphics/src/graphics.h
(def ^:private max-color-attachment-count 4)

(defn- set-form-op [{:keys [node-id]} [property] value]
  (g/set-property! node-id property value))

(defn- clear-form-op [{:keys [node-id]} [property]]
  (g/clear-property! node-id property))

(def form-data
  {:navigation false
   :sections
   [{:title "Render Target"
     :fields [{:path [:color-attachments]
               :label "Color Attachments"
               :type :table
               :columns [{:path [:width]
                          :label "width"
                          :type :integer
                          :default 128}
                         {:path [:height]
                          :label "height"
                          :type :integer
                          :default 128}
                         {:path [:format]
                          :label "format"
                          :type :choicebox
                          :options (protobuf-forms/make-options (protobuf/enum-values Graphics$TextureImage$TextureFormat))
                          :default :texture-format-rgba}]}
              {:path [:depth-stencil-attachment-width]
               :label "Depth/Stencil Width"
               :type :integer}
              {:path [:depth-stencil-attachment-height]
               :label "Depth/Stencil Height"
               :type :integer}
              {:path [:depth-stencil-attachment-texture-storage]
               :label "Depth Texture Storage"
               :type :boolean}]}]})
(g/defnk produce-form-data [_node-id color-attachments depth-stencil-attachment-width depth-stencil-attachment-height depth-stencil-attachment-texture-storage :as args]
  (let [values (select-keys args (mapcat :path (get-in form-data [:sections 0 :fields])))
        form-values (into {} (map (fn [[k v]] [[k] v]) values))]
    (-> form-data
        (assoc :values form-values)
        (assoc :form-ops {:user-data {:node-id _node-id}
                          :set set-form-op
                          :clear clear-form-op}))))

(g/defnk produce-pb-msg
  [color-attachments depth-stencil-attachment-width depth-stencil-attachment-height depth-stencil-attachment-format depth-stencil-attachment-texture-storage]
  {:color-attachments color-attachments
   :depth-stencil-attachment {:width depth-stencil-attachment-width
                              :height depth-stencil-attachment-height
                              :format depth-stencil-attachment-format
                              :texture-storage depth-stencil-attachment-texture-storage}})

(defn build-render-target
  [resource dep-resources user-data]
  {:resource resource
   :content (protobuf/map->bytes RenderTarget$RenderTargetDesc (:pb-msg user-data))})

(g/defnk produce-build-targets
  [_node-id resource pb-msg build-errors]
  (g/precluding-errors build-errors
    [(bt/with-content-hash
       {:node-id _node-id
        :resource (workspace/make-build-resource resource)
        :build-fn build-render-target
        :user-data {:pb-msg pb-msg}})]))

(defn- generate-gpu-texture [args request-id params unit]
  (texture/image-texture request-id nil))

(defn- validate-color-attachment-count [v name]
  (when (> (count v) max-color-attachment-count)
    (format "'%s' render targets cannot have more than %d color attachments"
            name max-color-attachment-count)))

(defn- color-attachments->error-values [color-attachment-index {:keys [width height]} node-id label]
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

  (property color-attachments g/Any (dynamic visible (g/constantly false)))
  (property depth-stencil-attachment-width g/Int (dynamic visible (g/constantly false)))
  (property depth-stencil-attachment-height g/Int (dynamic visible (g/constantly false)))
  (property depth-stencil-attachment-format g/Any (dynamic visible (g/constantly false)))
  (property depth-stencil-attachment-texture-storage g/Bool (dynamic visible (g/constantly false)))

  (output pb-msg g/Any :cached produce-pb-msg)
  (output save-value g/Any (gu/passthrough pb-msg))
  (output form-data g/Any produce-form-data)
  (output gpu-texture-generator g/Any {:f generate-gpu-texture})
  (output build-targets g/Any :cached produce-build-targets)
  (output build-errors g/Any (g/fnk [_node-id color-attachments depth-stencil-attachment-width depth-stencil-attachment-height]
                               (g/package-errors _node-id
                                                 (validation/prop-error :fatal _node-id :color-attachments validate-color-attachment-count color-attachments "Color Attachments")
                                                 (into [] (map-indexed
                                                            (fn [i color-attachment]
                                                              (color-attachments->error-values i color-attachment _node-id :color-attachments))
                                                            color-attachments))
                                                 (validation/prop-error :fatal _node-id :depth-stencil-attachment-width validation/prop-negative? depth-stencil-attachment-width "Depth/Stencil Width")
                                                 (validation/prop-error :fatal _node-id :depth-stencil-attachment-height validation/prop-negative? depth-stencil-attachment-height "Depth/Stencil Height")
                                                 (when (and (> depth-stencil-attachment-width 0) (= 0 depth-stencil-attachment-height))
                                                   (g/->error _node-id :depth-stencil-attachment-width :fatal depth-stencil-attachment-width
                                                              (format "Incorrect Depth/Stencil attachment: The width is greater than zero, but the height is zero")))
                                                 (when (and (> depth-stencil-attachment-height 0) (= 0 depth-stencil-attachment-width))
                                                   (g/->error _node-id :depth-stencil-attachment-height :fatal depth-stencil-attachment-height
                                                              (format "Incorrect Depth/Stencil attachment: The height is greater than zero, but the width is zero")))))))

(defn load-render-target [project self resource render-target]
  (let [depth-stencil-attachment (:depth-stencil-attachment render-target)]
    (concat
      (g/set-property self :color-attachments (:color-attachments render-target))
      (g/set-property self :depth-stencil-attachment-width (:width depth-stencil-attachment))
      (g/set-property self :depth-stencil-attachment-height (:height depth-stencil-attachment))
      (g/set-property self :depth-stencil-attachment-format (:format depth-stencil-attachment))
      (g/set-property self :depth-stencil-attachment-texture-storage (:texture-storage depth-stencil-attachment)))))

(defn register-resource-types
  [workspace]
  (resource-node/register-ddf-resource-type workspace
    :ext "render_target"
    :node-type RenderTargetNode
    :ddf-type RenderTarget$RenderTargetDesc
    :load-fn load-render-target
    :icon texture-icon
    :icon-class :design
    :view-types [:cljfx-form-view :text]
    :view-opts {}
    :label "Render Target"))
