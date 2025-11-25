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

(ns editor.image
  (:require [dynamo.graph :as g]
            [editor.build-target :as bt]
            [editor.gl :as gl]
            [editor.gl.texture :as texture]
            [editor.image-util :as image-util]
            [editor.localization :as localization]
            [editor.pipeline.tex-gen :as tex-gen]
            [editor.resource :as resource]
            [editor.resource-io :as resource-io]
            [editor.resource-node :as resource-node]
            [editor.workspace :as workspace])
  (:import [com.dynamo.bob.pipeline TextureGenerator$GenerateResult]
           [com.dynamo.bob.textureset TextureSetGenerator$UVTransform]
           [java.awt.image BufferedImage]))

(set! *warn-on-reflection* true)

(def exts ["jpg" "png"])

(defn image-resource?
  [resource]
  (boolean (some #{(resource/type-ext resource)} exts)))

(defn- build-texture [resource _dep-resources user-data]
  (let [{:keys [content-generator texture-profile compress?]} user-data
        image ((:f content-generator) (:args content-generator))]
    (g/precluding-errors
      [image]
      (let [texture-generator-result (tex-gen/make-texture-image image texture-profile compress?)]
        {:resource resource
         :write-content-fn tex-gen/write-texturec-content-fn
         :user-data {:texture-generator-result texture-generator-result}}))))

(defn make-texture-build-target
  [workspace node-id image-generator texture-profile compress?]
  (assert (contains? image-generator :sha1))
  (bt/with-content-hash
    {:node-id node-id
     :resource (workspace/make-placeholder-build-resource workspace "texture")
     :build-fn build-texture
     :user-data {:content-generator image-generator
                 :compress? compress?
                 :texture-profile texture-profile}}))

(defn- build-array-texture [resource _dep-resources user-data]
  (let [{:keys [content-generator texture-profile texture-page-count compress?]} user-data
        images ((:f content-generator) (:args content-generator))]
    (g/precluding-errors
      [images]
      (let [texture-generator-results (mapv #(tex-gen/make-texture-image % texture-profile compress?) images)
            ^TextureGenerator$GenerateResult combined-texture-image (tex-gen/assemble-texture-images texture-generator-results texture-page-count)]
        {:resource resource
         :write-content-fn tex-gen/write-texturec-content-fn
         :user-data {:texture-generator-result combined-texture-image}}))))

(defn make-array-texture-build-target
  [workspace node-id array-images-generator texture-profile texture-page-count compress?]
  (assert (contains? array-images-generator :sha1))
  (bt/with-content-hash
    {:node-id node-id
     :resource (workspace/make-placeholder-build-resource workspace "texture")
     :build-fn build-array-texture
     :user-data {:content-generator array-images-generator
                 :compress? compress?
                 :texture-page-count texture-page-count
                 :texture-profile texture-profile}}))

(g/defnk produce-build-targets [_node-id resource content-generator texture-profile build-settings]
  [(bt/with-content-hash
     {:node-id _node-id
      :resource (workspace/make-build-resource resource)
      :build-fn build-texture
      :user-data {:content-generator content-generator
                  :compress? (:compress-textures? build-settings false)
                  :texture-profile texture-profile}})])

(defn- generate-gpu-texture [{:keys [texture-image]} request-id params unit]
  (texture/texture-image->gpu-texture request-id texture-image params unit))

(defn- generate-content [{:keys [digest-ignored/error-node-id resource]}]
  (resource-io/with-error-translation resource error-node-id :resource
    (image-util/read-image resource)))

(g/defnode ImageNode
  (inherits resource-node/ResourceNode)

  (input build-settings g/Any)
  (input texture-profiles g/Any)

  (output texture-profile g/Any (g/fnk [texture-profiles resource]
                                  (tex-gen/match-texture-profile texture-profiles (resource/proj-path resource))))

  (output size g/Any :cached (g/fnk [_node-id resource]
                               (resource-io/with-error-translation resource _node-id :size
                                 (image-util/read-size resource))))

  (output content BufferedImage (g/fnk [content-generator]
                                  ((:f content-generator) (:args content-generator))))

  (output content-generator g/Any (g/fnk [_node-id resource :as args]
                                    {:f generate-content
                                     :args (-> args
                                               (dissoc :_node-id)
                                               (assoc :digest-ignored/error-node-id _node-id))
                                     :sha1 (resource/resource->path-inclusive-sha1-hex resource)}))

  (output texture-image g/Any (g/fnk [content texture-profile] (tex-gen/make-preview-texture-image content texture-profile)))

  ;; NOTE: The anim-data and gpu-texture outputs allow standalone images to be used in place of texture sets in legacy projects.
  (output anim-data g/Any (g/fnk [size]
                            {nil (assoc size
                                   :frames [{:tex-coords [[0 1] [0 0] [1 0] [1 1]]}]
                                   :uv-transforms [(TextureSetGenerator$UVTransform.)])}))

  (output texture-page-count g/Int (g/constantly texture/non-paged-page-count))

  (output gpu-texture g/Any :cached (g/fnk [_node-id texture-image]
                                      (texture/texture-image->gpu-texture _node-id
                                                                          texture-image
                                                                          {:min-filter gl/nearest
                                                                           :mag-filter gl/nearest})))

  (output gpu-texture-generator g/Any (g/fnk [texture-image :as args]
                                        {:f    generate-gpu-texture
                                         :args args}))

  (output build-targets g/Any :cached produce-build-targets))

(defn- load-image
  [project self _resource]
  (concat
    (g/connect project :build-settings self :build-settings)
    (g/connect project :texture-profiles self :texture-profiles)))

(defn register-resource-types [workspace]
  (concat
    (workspace/register-resource-type workspace
                                      :ext exts
                                      :label (localization/message "resource.type.image")
                                      :icon "icons/32/Icons_25-AT-Image.png"
                                      :build-ext "texturec"
                                      :node-type ImageNode
                                      :load-fn load-image
                                      :stateless? true
                                      :view-types [:default])
    (workspace/register-resource-type workspace :ext "texture")))
