(ns editor.resource-types
  (:require [dynamo.graph :as g]
            [editor.animation-set :as animation-set]
            [editor.atlas :as atlas]
            [editor.camera-editor :as camera]
            [editor.code.json :as code-json]
            [editor.code.script :as code-script]
            [editor.code.shader :as code-shader]
            [editor.code.text-file :as code-text-file]
            [editor.collada-scene :as collada-scene]
            [editor.collection :as collection]
            [editor.collection-proxy :as collection-proxy]
            [editor.collision-object :as collision-object]
            [editor.cubemap :as cubemap]
            [editor.display-profiles :as display-profiles]
            [editor.factory :as factory]
            [editor.font :as font]
            [editor.game-object :as game-object]
            [editor.game-project :as game-project]
            [editor.gl.shader :as shader]
            [editor.gui :as gui]
            [editor.html :as html]
            [editor.image :as image]
            [editor.json :as json]
            [editor.label :as label]
            [editor.live-update-settings :as live-update-settings]
            [editor.markdown :as markdown]
            [editor.material :as material]
            [editor.model :as model]
            [editor.particlefx :as particlefx]
            [editor.protobuf-types :as protobuf-types]
            [editor.render-pb :as render-pb]
            [editor.rig :as rig]
            [editor.script :as script]
            [editor.sound :as sound]
            [editor.spine :as spine]
            [editor.sprite :as sprite]
            [editor.text-file :as text-file]
            [editor.tile-map :as tile-map]
            [editor.tile-source :as tile-source]))

(defn register-resource-types! [workspace use-new-code-editor?]
  (g/transact
    (concat
      (animation-set/register-resource-types workspace)
      (atlas/register-resource-types workspace)
      (camera/register-resource-types workspace)
      (collada-scene/register-resource-types workspace)
      (collection/register-resource-types workspace)
      (collection-proxy/register-resource-types workspace)
      (collision-object/register-resource-types workspace)
      (cubemap/register-resource-types workspace)
      (display-profiles/register-resource-types workspace)
      (factory/register-resource-types workspace)
      (font/register-resource-types workspace)
      (game-object/register-resource-types workspace)
      (game-project/register-resource-types workspace)
      (gui/register-resource-types workspace)
      (html/register-resource-types workspace)
      (image/register-resource-types workspace)
      (json/register-resource-types workspace)
      (label/register-resource-types workspace)
      (live-update-settings/register-resource-types workspace)
      (markdown/register-resource-types workspace)
      (material/register-resource-types workspace)
      (model/register-resource-types workspace)
      (particlefx/register-resource-types workspace)
      (protobuf-types/register-resource-types workspace)
      (render-pb/register-resource-types workspace)
      (rig/register-resource-types workspace)
      (sound/register-resource-types workspace)
      (spine/register-resource-types workspace)
      (sprite/register-resource-types workspace)
      (tile-map/register-resource-types workspace)
      (tile-source/register-resource-types workspace)
      (if use-new-code-editor?
        (concat
          ;; TODO: Disabled due to conflict with spine .json files.
          #_(code-json/register-resource-types workspace)
          (code-script/register-resource-types workspace)
          (code-shader/register-resource-types workspace)
          (code-text-file/register-resource-types workspace))
        (concat
          (script/register-resource-types workspace)
          (shader/register-resource-types workspace)
          (text-file/register-resource-types workspace))))))
