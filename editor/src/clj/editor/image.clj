(ns editor.image
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [schema.core :as s]
            [editor.image-util :as image-util]
            [editor.gl.texture :as texture]
            [editor.core :as core]
            [editor.geom :refer [clamper]]
            [editor.defold-project :as project]
            [editor.protobuf :as protobuf]
            [editor.types :as types]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.validation :as validation]
            [editor.workspace :as workspace]
            [editor.pipeline.tex-gen :as tex-gen]
            [service.log :as log]
            [util.digest :as digest])
  (:import [editor.types Rect Image]
           [java.awt Color]
           [java.awt.image BufferedImage]
           [javax.imageio ImageIO]))

(set! *warn-on-reflection* true)

(def exts ["jpg" "png"])

(defn- build-texture [resource dep-resources user-data]
  (let [{:keys [content-generator texture-profile compress?]} user-data
        image ((:f content-generator) (:args content-generator))]
    (if (g/error? image)
      image
      (let [texture-image (tex-gen/make-texture-image image texture-profile compress?)]
        {:resource resource
         :content  (protobuf/pb->bytes texture-image)}))))

(defn make-texture-build-target
  [workspace node-id image-generator texture-profile compress?]
  (assert (contains? image-generator :sha1))
  (let [texture-type     (workspace/get-resource-type workspace "texture")
        texture-resource (resource/make-memory-resource workspace texture-type (:sha1 image-generator))]
    {:node-id   node-id
     :resource  (workspace/make-build-resource texture-resource)
     :build-fn  build-texture
     :user-data {:content-generator image-generator
                 :compress?         compress?
                 :texture-profile   texture-profile}}))

(g/defnk produce-build-targets [_node-id resource content-generator texture-profile build-settings]
  [{:node-id   _node-id
    :resource  (workspace/make-build-resource resource)
    :build-fn  build-texture
    :user-data {:content-generator content-generator
                :compress?         (:compress? build-settings false)
                :texture-profile   texture-profile}}])

(defn- generate-gpu-texture [{:keys [texture-image]} request-id params unit]
  (texture/texture-image->gpu-texture request-id texture-image params unit))

(defn- generate-content [{:keys [_node-id resource]}]
  (validation/resource-io-with-errors
    #(or (image-util/read-image %)
         (validation/invalid-content-error _node-id :resource :fatal %))
    resource _node-id :resource))

(g/defnode ImageNode
  (inherits resource-node/ResourceNode)

  (input build-settings g/Any)
  (input texture-profiles g/Any)
  
  ;; we never modify ImageNode, save-data and source-value can be trivial and not cached
  (output save-data g/Any (g/constantly nil))
  (output source-value g/Any (g/constantly nil))

  (output texture-profile g/Any (g/fnk [texture-profiles resource]
                                  (tex-gen/match-texture-profile texture-profiles (resource/proj-path resource))))

  (output size g/Any :cached (g/fnk [_node-id resource]
                                    (validation/resource-io-with-errors
                                      #(or (image-util/read-size %)
                                           (validation/invalid-content-error _node-id :size :fatal %))
                                      resource _node-id :size)))

  (output content BufferedImage (g/fnk [content-generator]
                                  ((:f content-generator) (:args content-generator))))

  (output content-generator g/Any (g/fnk [_node-id resource :as args]
                                    {:f    generate-content
                                     :args args
                                     :sha1 (resource/resource->sha1-hex resource)}))

  (output texture-image g/Any (g/fnk [content texture-profile] (tex-gen/make-preview-texture-image content texture-profile)))

  (output gpu-texture-generator g/Any :cached (g/fnk [texture-image :as args]
                                                     {:f    generate-gpu-texture
                                                      :args args}))
  
  (output build-targets g/Any :cached produce-build-targets))

(defn- load-image
  [project self resource]
  (concat
    (g/connect project :build-settings self :build-settings)
    (g/connect project :texture-profiles self :texture-profiles)))

(defn register-resource-types [workspace]
  (concat
    (workspace/register-resource-type workspace
                                      :ext exts
                                      :label "Image"
                                      :icon "icons/32/Icons_25-AT-Image.png"
                                      :build-ext "texturec"
                                      :node-type ImageNode
                                      :load-fn load-image
                                      :stateless? true
                                      :view-types [:default])
    (workspace/register-resource-type workspace :ext "texture")))
