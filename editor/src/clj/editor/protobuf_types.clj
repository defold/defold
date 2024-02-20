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

(ns editor.protobuf-types
  (:require [dynamo.graph :as g]
            [editor.build-target :as bt]
            [editor.protobuf :as protobuf]
            [editor.protobuf-forms :as protobuf-forms]
            [editor.resource-node :as resource-node]
            [editor.workspace :as workspace])
  (:import [com.dynamo.gamesys.proto GameSystem$LightDesc]
           [com.dynamo.gamesys.proto Physics$ConvexShape]
           [com.dynamo.graphics.proto Graphics$TextureProfiles]
           [com.dynamo.input.proto Input$GamepadMaps Input$InputBinding]))

(set! *warn-on-reflection* true)

(def pb-defs [{:ext "input_binding"
               :icon "icons/32/Icons_35-Inputbinding.png"
               :pb-class Input$InputBinding
               :label "Input Binding"
               :view-types [:cljfx-form-view :text]}
              {:ext "light"
               :label "Light"
               :icon "icons/32/Icons_21-Light.png"
               :pb-class GameSystem$LightDesc
               :tags #{:component}
               :tag-opts {:component {:transform-properties #{}}}}
              {:ext "gamepads"
               :label "Gamepads"
               :icon "icons/32/Icons_34-Gamepad.png"
               :pb-class Input$GamepadMaps
               :view-types [:cljfx-form-view :text]}
              {:ext "convexshape"
               :label "Convex Shape"
               ; TODO - missing icon
               :icon "icons/32/Icons_43-Tilesource-Collgroup.png"
               :pb-class Physics$ConvexShape}
              {:ext "texture_profiles"
               :label "Texture Profiles"
               :view-types [:cljfx-form-view :text]
               :icon "icons/32/Icons_37-Texture-profile.png"
               :pb-class Graphics$TextureProfiles}])

(defn- build-pb [resource dep-resources user-data]
  {:resource resource :content (protobuf/map->bytes (:pb-class user-data) (:pb user-data))})

(g/defnk produce-build-targets [_node-id resource pb def]
  [(bt/with-content-hash
     {:node-id _node-id
      :resource (workspace/make-build-resource resource)
      :build-fn build-pb
      :user-data {:pb pb
                  :pb-class (:pb-class def)}})])

(g/defnk produce-form-data [_node-id pb def]
  (protobuf-forms/produce-form-data _node-id pb def))

(g/defnk produce-save-value [def pb]
  (let [pb-class (:pb-class def)]
    (protobuf/clear-defaults pb-class pb)))

(g/defnode ProtobufNode
  (inherits resource-node/ResourceNode)

  (property pb g/Any ; Always assigned in load-fn.
            (dynamic visible (g/constantly false)))
  (property def g/Any ; Always assigned in load-fn.
            (dynamic visible (g/constantly false)))

  (output form-data g/Any :cached produce-form-data)
  (output save-value g/Any :cached produce-save-value)
  (output build-targets g/Any :cached produce-build-targets))

(defn load-pb [def _project self _resource pb-map-without-defaults]
  (let [pb-class (:pb-class def)
        pb-map-with-defaults (protobuf/inject-defaults pb-class pb-map-without-defaults)]
    (g/set-property self
      :def def
      :pb pb-map-with-defaults)))

(defn- register [workspace def]
  (let [ext (:ext def)
        exts (if (vector? ext) ext [ext])]
    (for [ext exts]
      (resource-node/register-ddf-resource-type workspace
        :ext ext
        :label (:label def)
        :build-ext (:build-ext def)
        :node-type ProtobufNode
        :ddf-type (:pb-class def)
        :read-defaults false
        :load-fn (partial load-pb def)
        :icon (:icon def)
        :view-types (:view-types def)
        :view-opts (:view-opts def)
        :tags (:tags def)
        :tag-opts (:tag-opts def)
        :template (:template def)))))

(defn register-resource-types [workspace]
  (for [def pb-defs]
    (register workspace def)))
