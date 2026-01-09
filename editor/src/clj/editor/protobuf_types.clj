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

(ns editor.protobuf-types
  (:require [dynamo.graph :as g]
            [editor.build-target :as bt]
            [editor.localization :as localization]
            [editor.protobuf :as protobuf]
            [editor.protobuf-forms :as protobuf-forms]
            [editor.resource-node :as resource-node]
            [editor.workspace :as workspace])
  (:import [com.defold.extension.pipeline.texture TextureCompression TextureCompressorASTC TextureCompressorBasisU TextureCompressorUncompressed]
           [com.dynamo.gamesys.proto GameSystem$LightDesc]
           [com.dynamo.gamesys.proto Physics$ConvexShape]
           [com.dynamo.graphics.proto Graphics$TextureFormatAlternative$CompressionLevel Graphics$TextureImage$TextureFormat Graphics$TextureProfiles]
           [com.dynamo.input.proto Input$GamepadMaps Input$InputBinding]))

(set! *warn-on-reflection* true)

(defn- migrate-texture-profile-format [compression-type compression-level]
  (let [compression-level-pb (when compression-level
                               (protobuf/val->pb-enum Graphics$TextureFormatAlternative$CompressionLevel compression-level))]
   (case compression-type
    :compression-type-webp
    {:compressor TextureCompressorUncompressed/TextureCompressorName
     :compressor-preset (TextureCompressorUncompressed/GetMigratedCompressionPreset)}

    :compression-type-webp-lossy
    {:compressor TextureCompressorBasisU/TextureCompressorName
     :compressor-preset (TextureCompressorBasisU/GetMigratedCompressionPreset compression-level-pb)}

    :compression-type-basis-uastc
    {:compressor TextureCompressorBasisU/TextureCompressorName
     :compressor-preset (TextureCompressorBasisU/GetMigratedCompressionPreset compression-level-pb)}

    :compression-type-basis-etc1s
    {:compressor TextureCompressorBasisU/TextureCompressorName
     :compressor-preset (TextureCompressorBasisU/GetMigratedCompressionPreset compression-level-pb)}

    :compression-type-astc
    {:compressor TextureCompressorASTC/TextureCompressorName
     :compressor-preset (TextureCompressorASTC/GetMigratedCompressionPreset compression-level-pb)}

    {:compressor TextureCompressorUncompressed/TextureCompressorName
     :compressor-preset (TextureCompressorUncompressed/GetMigratedCompressionPreset)})))

(defn- sanitize-texture-profile-format [texture-format]
  (let [migrated-format (if (and (:compressor texture-format) (:compressor-preset texture-format))
                          texture-format
                          (merge texture-format (migrate-texture-profile-format (:compression-type texture-format) (:compression-level texture-format))))]
    ;; Remove deprecated fields
    (dissoc migrated-format :compression-level :compression-type)))

(defn- sanitize-texture-profiles [pb]
  (protobuf/sanitize-repeated
    pb :profiles
    (fn [profile]
      (protobuf/sanitize-repeated
        profile :platforms
        (fn [platform]
          (protobuf/sanitize-repeated
            platform :formats
            sanitize-texture-profile-format))))))

(defn- texture-profiles-errors-fn [node-id resource texture-profiles]
  (let [all-format-entries (->> (:profiles texture-profiles)
                                (mapcat :platforms)
                                (mapcat :formats)
                                (into []))]
    (mapv (fn [format]
            (let [texture-compressor (TextureCompression/getCompressor (:compressor format))
                  texture-compression-preset (TextureCompression/getPreset (:compressor-preset format))
                  texture-format (when (:format format)
                                   (protobuf/val->pb-enum Graphics$TextureImage$TextureFormat (:format format)))
                  format-is-supported? (.supportsTextureFormat texture-compressor texture-format)
                  preset-is-supported? (.supportsTextureCompressorPreset texture-compressor texture-compression-preset)]
              [(when-not format-is-supported? (g/->error node-id :pb :fatal resource "Texture format is not supported by the texture compressor"))
               (when-not preset-is-supported? (g/->error node-id :pb :fatal resource "Texture preset is not supported by the texture compressor"))]))
          all-format-entries)))

(def pb-defs [{:ext "input_binding"
               :icon "icons/32/Icons_35-Inputbinding.png"
               :icon-class :property
               :category (localization/message "resource.category.project_settings")
               :pb-class Input$InputBinding
               :label (localization/message "resource.type.input-binding")
               :view-types [:cljfx-form-view :text]}
              {:ext "light"
               :label (localization/message "resource.type.light")
               :icon "icons/32/Icons_21-Light.png"
               :pb-class GameSystem$LightDesc
               :tags #{:component}
               :tag-opts {:component {:transform-properties #{}}}}
              {:ext "gamepads"
               :label (localization/message "resource.type.gamepads")
               :icon "icons/32/Icons_34-Gamepad.png"
               :icon-class :property
               :category (localization/message "resource.category.project_settings")
               :pb-class Input$GamepadMaps
               :view-types [:cljfx-form-view :text]}
              {:ext "convexshape"
               :label (localization/message "resource.type.convexshape")
               ; TODO - missing icon
               :icon "icons/32/Icons_43-Tilesource-Collgroup.png"
               :pb-class Physics$ConvexShape}
              {:ext "texture_profiles"
               :label (localization/message "resource.type.texture-profiles")
               :view-types [:cljfx-form-view :text]
               :icon "icons/32/Icons_37-Texture-profile.png"
               :icon-class :property
               :category (localization/message "resource.category.project_settings")
               :pb-class Graphics$TextureProfiles
               :pb-errors-fn texture-profiles-errors-fn
               :sanitize-fn sanitize-texture-profiles}])

(defn- build-pb [resource dep-resources user-data]
  {:resource resource :content (protobuf/map->bytes (:pb-class user-data) (:pb user-data))})

(g/defnk produce-build-targets [_node-id resource pb def]
  (g/precluding-errors
    (when-some [pb-errors-fn (:pb-errors-fn def)]
      (pb-errors-fn _node-id resource pb))
    [(bt/with-content-hash
       {:node-id _node-id
        :resource (workspace/make-build-resource resource)
        :build-fn build-pb
        :user-data {:pb pb
                    :pb-class (:pb-class def)}})]))

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
    (g/set-properties self
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
        :load-fn (partial load-pb def)
        :icon (:icon def)
        :icon-class (:icon-class def)
        :category (:category def)
        :view-types (:view-types def)
        :view-opts (:view-opts def)
        :sanitize-fn (:sanitize-fn def)
        :tags (:tags def)
        :tag-opts (:tag-opts def)
        :template (:template def)))))

(defn register-resource-types [workspace]
  (for [def pb-defs]
    (register workspace def)))
