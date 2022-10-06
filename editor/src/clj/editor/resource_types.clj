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

(ns editor.resource-types
  (:require [dynamo.graph :as g]
            [editor.animation-set :as animation-set]
            [editor.app-manifest :as app-manifest]
            [editor.atlas :as atlas]
            [editor.buffer :as buffer]
            [editor.camera-editor :as camera]
            [editor.code.script :as code-script]
            [editor.code.shader :as code-shader]
            [editor.code.text-file :as code-text-file]
            [editor.model-scene :as model-scene]
            [editor.collection :as collection]
            [editor.collection-proxy :as collection-proxy]
            [editor.collision-object :as collision-object]
            [editor.cubemap :as cubemap]
            [editor.display-profiles :as display-profiles]
            [editor.editor-script :as editor-script]
            [editor.factory :as factory]
            [editor.font :as font]
            [editor.game-object :as game-object]
            [editor.game-project :as game-project]
            [editor.gui :as gui]
            [editor.html :as html]
            [editor.image :as image]
            [editor.label :as label]
            [editor.live-update-settings :as live-update-settings]
            [editor.markdown :as markdown]
            [editor.material :as material]
            [editor.mesh :as mesh]
            [editor.model :as model]
            [editor.particlefx :as particlefx]
            [editor.protobuf-types :as protobuf-types]
            [editor.render-pb :as render-pb]
            [editor.rig :as rig]
            [editor.script-api :as script-api]
            [editor.shared-editor-settings :as shared-editor-settings]
            [editor.sound :as sound]
            [editor.sprite :as sprite]
            [editor.tile-map :as tile-map]
            [editor.tile-source :as tile-source]))

(defn register-resource-types! [workspace]
  (g/transact
    (concat
      (animation-set/register-resource-types workspace)
      (app-manifest/register-resource-types workspace)
      (atlas/register-resource-types workspace)
      (buffer/register-resource-types workspace)
      (camera/register-resource-types workspace)
      (model-scene/register-resource-types workspace)
      (collection/register-resource-types workspace)
      (collection-proxy/register-resource-types workspace)
      (collision-object/register-resource-types workspace)
      (cubemap/register-resource-types workspace)
      (display-profiles/register-resource-types workspace)
      (editor-script/register-resource-types workspace)
      (factory/register-resource-types workspace)
      (font/register-resource-types workspace)
      (game-object/register-resource-types workspace)
      (game-project/register-resource-types workspace)
      (gui/register-resource-types workspace)
      (html/register-resource-types workspace)
      (image/register-resource-types workspace)
      (label/register-resource-types workspace)
      (live-update-settings/register-resource-types workspace)
      (markdown/register-resource-types workspace)
      (material/register-resource-types workspace)
      (mesh/register-resource-types workspace)
      (model/register-resource-types workspace)
      (particlefx/register-resource-types workspace)
      (protobuf-types/register-resource-types workspace)
      (render-pb/register-resource-types workspace)
      (rig/register-resource-types workspace)
      (shared-editor-settings/register-resource-types workspace)
      (sound/register-resource-types workspace)
      (sprite/register-resource-types workspace)
      (tile-map/register-resource-types workspace)
      (tile-source/register-resource-types workspace)
      (code-script/register-resource-types workspace)
      (code-shader/register-resource-types workspace)
      (code-text-file/register-resource-types workspace)
      (script-api/register-resource-types workspace))))
