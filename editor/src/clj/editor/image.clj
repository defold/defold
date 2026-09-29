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

(ns editor.image
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.build-target :as bt]
            [editor.gl :as gl]
            [editor.gl.pass :as pass]
            [editor.gl.texture :as texture]
            [editor.image-util :as image-util]
            [editor.localization :as localization]
            [editor.outline :as outline]
            [editor.pipeline.tex-gen :as tex-gen]
            [editor.pose :as pose]
            [editor.properties :as properties]
            [editor.render-util :as render-util]
            [editor.resource :as resource]
            [editor.resource-io :as resource-io]
            [editor.resource-node :as resource-node]
            [editor.scene-picking :as scene-picking]
            [editor.texture-util :as texture-util]
            [editor.types :as types]
            [editor.workspace :as workspace])
  (:import [com.dynamo.bob.pipeline TexcLibraryJni TextureGenerator TextureGenerator$GenerateResult]
           [com.dynamo.bob.textureset TextureSetGenerator$UVTransform]))

(set! *warn-on-reflection* true)

(def exts ["jpg" "jpeg" "png"])

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

(g/defnk produce-scene [_node-id size gpu-texture texture-profile]
  (g/precluding-errors
    [size gpu-texture]
    (let [{:keys [width height]} size]
      (assoc (render-util/make-outlined-textured-quad-scene #{:image} pose/default width height gpu-texture 0)
        :node-id _node-id
        :info-text (format "%d x %d (%s profile)" width height (:name texture-profile))))))

(g/defnode ImageNode
  (inherits resource-node/ResourceNode)

  (input build-settings g/Any)
  (input texture-profiles g/Any)

  (output texture-profile g/Any (g/fnk [texture-profiles resource]
                                  (tex-gen/match-texture-profile texture-profiles (resource/proj-path resource))))

  (output size g/Any :cached (g/fnk [_node-id resource]
                               (resource-io/with-error-translation resource _node-id :size
                                 (image-util/read-size resource))))

  (output content-generator g/Any :cached
          (g/fnk [_node-id resource]
            (texture-util/make-buffered-image-generator resource _node-id :content-generator)))

  (output gpu-texture-generator g/Any :cached
          (g/fnk [_node-id content-generator texture-profile]
            (texture-util/make-gpu-texture-generator _node-id content-generator texture-profile)))

  (output gpu-texture g/Any :cached
          (g/fnk [gpu-texture-generator]
            (-> (texture-util/generate-gpu-texture gpu-texture-generator)
                (texture/set-params {:min-filter gl/nearest
                                     :mag-filter gl/nearest}))))

  ;; NOTE: The anim-data and gpu-texture outputs allow standalone images to be used in place of texture sets in legacy projects.
  (output anim-data g/Any (g/fnk [size]
                            {nil (assoc size
                                   :frames [{:tex-coords [[0 1] [0 0] [1 0] [1 1]]
                                             :tex-coords-raw [[0.0 0.0] [0.0 1.0] [1.0 1.0] [1.0 0.0]]
                                             :atlas-rotated false}]
                                   :uv-transforms [(TextureSetGenerator$UVTransform.)])}))

  (output texture-page-count g/Int (g/constantly texture/non-paged-page-count))
  (output scene g/Any :cached produce-scene)
  (output build-targets g/Any :cached produce-build-targets))

(defn- ktx2-content-gen-fn
  [{:keys [resource digest-ignored/error-node-id]}]
  (resource-io/with-error-translation resource error-node-id :content-generator
    (resource/resource->bytes resource)))

(defn- generate-ktx2-texture
  [content-generator texture-profile compress preview]
  (let [{:keys [resource digest-ignored/error-node-id]} (:args content-generator)]
    (resource-io/with-error-translation resource error-node-id :build-targets
      (let [content (texture-util/call-generator content-generator)]
        (g/precluding-errors [content]
          (tex-gen/make-ktx2-texture-image content texture-profile compress preview))))))

(defn- build-ktx2-texture
  [resource _dep-resources {:keys [content-generator texture-profile compress]}]
  (let [result (generate-ktx2-texture content-generator texture-profile compress false)]
    (g/precluding-errors [result]
      {:resource resource
       :write-content-fn tex-gen/write-texturec-content-fn
       :user-data {:texture-generator-result result}})))

(defn- ktx2-gpu-texture-gen-fn
  [{:keys [request-id texture-request-datas-delay]}]
  (texture-util/make-gpu-texture request-id (force texture-request-datas-delay)))

(defn- read-ktx2-info
  [_read-opts _owner-resource readable]
  (with-open [image (TexcLibraryJni/LoadKtx2 (with-open [stream (io/input-stream readable)] (.readAllBytes stream)))]
    {:dimensions [(.-width image) (.-height image)]
     :format (case (.-vkFormat image)
               0 (if (= 1 (.-supercompression image)) "ETC1S" "UASTC")
               (9 15) "R8"
               (16 22) "RG8"
               (23 29) "RGB8"
               (37 43) "RGBA8"
               (145 146) "BC7")
     :supercompression (case (.-supercompression image)
                         0 "—"
                         1 "BasisLZ"
                         2 "Zstd"
                         3 "Zlib")
     :mip-count (.-levelCount image)
     :channels (.-channels image)
     :color-space (if (.-srgb image) "sRGB" "Linear")
     :premultiplied-alpha (.-premultiplied image)
     :orientation (str (if (.-flipX image) "l" "r") (if (.-flipY image) "u" "d"))}))

(defn- render-ktx2-mip-selection
  [gl render-args renderables renderable-count]
  (assert (= pass/selection (:pass render-args)))
  (assert (= 1 renderable-count))
  (let [renderable (first renderables)
        [width height] (:dimensions (:user-data renderable))]
    (render-util/render-color-quad! gl render-args ::ktx2-mip-selection
                                  (scene-picking/renderable-picking-id-uniform renderable)
                                  [[0 0] [0 height] [width height] [width 0]])))

(g/defnode Ktx2MipNode
  (inherits outline/OutlineNode)

  (property level g/Int
            (dynamic label (properties/label-dynamic :ktx2 :level))
            (dynamic read-only? (g/constantly true)))
  (property dimensions types/Vec2
            (dynamic label (properties/label-dynamic :ktx2 :dimensions))
            (dynamic read-only? (g/constantly true)))
  (property offset-x g/Num
            (dynamic visible (g/constantly false)))

  (display-order [:level :dimensions])

  (input texture-request-datas g/Any)

  (output gpu-texture g/Any :cached
          (g/fnk [_node-id level texture-request-datas]
            (-> (texture-util/make-gpu-texture _node-id [(nth texture-request-datas level)])
                (texture/set-params {:min-filter gl/nearest
                                     :mag-filter gl/nearest}))))

  (output node-outline outline/OutlineData :cached
          (g/fnk [_node-id level dimensions]
            {:node-id _node-id
             :node-outline-key (str "mip-" level)
             :label (localization/message "outline.ktx2.mip" {"level" level "width" (first dimensions) "height" (second dimensions)})
             :icon "icons/32/Icons_25-AT-Image.png"
             :order level
             :read-only true}))

  (output scene g/Any :cached
          (g/fnk [_node-id level dimensions offset-x gpu-texture]
            (let [[width height] dimensions
                  scene (render-util/make-outlined-textured-quad-scene #{:image} (pose/translation-pose offset-x 0.0 0.0) width height gpu-texture 0)]
              (-> scene
                  (assoc :node-id _node-id
                         :order level
                         :node-outline-key (str "mip-" level))
                  (update :children conj
                          {:node-id _node-id
                           :aabb (:aabb scene)
                           :renderable {:render-fn render-ktx2-mip-selection
                                        :tags #{:image}
                                        :user-data {:dimensions dimensions}
                                        :passes [pass/selection]}}))))))

(g/defnode Ktx2ImageNode
  (inherits ImageNode)

  (property dimensions types/Vec2
            (dynamic label (properties/label-dynamic :ktx2 :dimensions))
            (dynamic read-only? (g/constantly true)))
  (property format g/Str
            (dynamic label (properties/label-dynamic :ktx2 :format))
            (dynamic read-only? (g/constantly true)))
  (property supercompression g/Str
            (dynamic label (properties/label-dynamic :ktx2 :supercompression))
            (dynamic read-only? (g/constantly true)))
  (property mip-count g/Int
            (dynamic label (properties/label-dynamic :ktx2 :mip-count))
            (dynamic read-only? (g/constantly true)))
  (property channels g/Int
            (dynamic label (properties/label-dynamic :ktx2 :channels))
            (dynamic read-only? (g/constantly true)))
  (property color-space g/Str
            (dynamic label (properties/label-dynamic :ktx2 :color-space))
            (dynamic read-only? (g/constantly true)))
  (property premultiplied-alpha g/Bool
            (dynamic label (properties/label-dynamic :ktx2 :premultiplied-alpha))
            (dynamic read-only? (g/constantly true)))
  (property orientation g/Str
            (dynamic label (properties/label-dynamic :ktx2 :orientation))
            (dynamic tooltip (properties/tooltip-dynamic :ktx2 :orientation))
            (dynamic read-only? (g/constantly true)))

  (display-order [:dimensions :format :supercompression :mip-count :channels :color-space :premultiplied-alpha :orientation])

  (input child-scenes g/Any :array)

  (output node-outline outline/OutlineData :cached
          (g/fnk [_node-id child-outlines]
            {:node-id _node-id
             :node-outline-key "ktx2"
             :label (localization/message "outline.ktx2")
             :icon "icons/32/Icons_13-Atlas.png"
             :read-only true
             :children (vec (sort-by :order child-outlines))}))

  (output content-generator g/Any :cached
          (g/fnk [_node-id resource]
            (resource-io/with-error-translation resource _node-id :content-generator
              {:f ktx2-content-gen-fn
               :sha1 (resource/resource->path-inclusive-sha1-hex resource)
               :args {:resource resource
                      :digest-ignored/error-node-id _node-id}})))

  (output size g/Any
          (g/fnk [dimensions]
            {:width (first dimensions)
             :height (second dimensions)}))

  (output mip-texture-request-datas g/Any :cached
          (g/fnk [_node-id resource content-generator]
            (resource-io/with-error-translation resource _node-id :mip-texture-request-datas
              (let [content (texture-util/call-generator content-generator)]
                (g/precluding-errors [content]
                  (with-open [image (TexcLibraryJni/LoadKtx2 content)]
                    (mapv (fn [level]
                            (texture/texture-image->texture-request-data
                              (TextureGenerator/generateKtx2MipPreview image level)
                              (hash [(:sha1 content-generator) level])))
                          (range (.-levelCount image)))))))))

  (output scene g/Any :cached
          (g/fnk [_node-id dimensions child-scenes texture-profile]
            {:node-id _node-id
             :children (vec (sort-by :order child-scenes))
             :info-text (format "%d x %d (%s profile)" (first dimensions) (second dimensions) (:name texture-profile))}))

  (output gpu-texture-generator g/Any :cached
          (g/fnk [_node-id content-generator texture-profile]
            {:f ktx2-gpu-texture-gen-fn
             :args {:request-id _node-id
                    :texture-request-datas-delay
                    (delay
                      (let [result (generate-ktx2-texture content-generator texture-profile false true)]
                        (g/precluding-errors [result]
                          [(texture/texture-image->texture-request-data result (hash [(:sha1 content-generator) texture-profile]))])))}}))

  (output build-targets g/Any :cached
          (g/fnk [_node-id resource content-generator texture-profile build-settings]
            [(bt/with-content-hash
               {:node-id _node-id
                :resource (workspace/make-build-resource resource)
                :build-fn build-ktx2-texture
                :user-data {:content-generator content-generator
                            :texture-profile texture-profile
                            :compress (:compress-textures? build-settings false)}})])))

(defn- load-image
  [{:keys [project]} {self :node-id}]
  (concat
    (g/connect project :build-settings self :build-settings)
    (g/connect project :texture-profiles self :texture-profiles)))

(defn- load-ktx2-image
  [load-opts {self :node-id texture-info :source-value :as node-load-info}]
  (loop [level 0
         offset-x 0
         tx-data [(load-image load-opts node-load-info)
                  (apply g/set-properties self (into [] cat texture-info))]]
    (if (= level (:mip-count texture-info))
      tx-data
      (let [[width :as dimensions] (mapv #(max 1 (bit-shift-right % level)) (:dimensions texture-info))]
        (recur (inc level)
               (+ offset-x (long width) 16)
               (conj tx-data
                     (g/make-nodes [mip-node [Ktx2MipNode :level level :dimensions dimensions :offset-x offset-x]]
                       (g/connect mip-node :_node-id self :nodes)
                       (g/connect mip-node :node-outline self :child-outlines)
                       (g/connect mip-node :scene self :child-scenes)
                       (g/connect self :mip-texture-request-datas mip-node :texture-request-datas))))))))

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
                                      :view-types [:scene :default])
    (workspace/register-resource-type workspace
                                      :ext "ktx2"
                                      :label (localization/message "resource.type.ktx2")
                                      :icon "icons/32/Icons_25-AT-Image.png"
                                      :build-ext "texturec"
                                      :node-type Ktx2ImageNode
                                      :load-fn load-ktx2-image
                                      :read-fn read-ktx2-info
                                      :view-types [:scene :default])
    (workspace/register-resource-type workspace :ext "texture")))
