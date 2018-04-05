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
            [service.log :as log])
  (:import [editor.types Rect Image]
           [java.awt Color]
           [java.awt.image BufferedImage]
           [javax.imageio ImageIO]))

(set! *warn-on-reflection* true)

(def exts ["jpg" "png"])

(defn- build-texture [resource dep-resources user-data]
  (let [{:keys [image texture-profile compress?]} user-data
        texture-image (tex-gen/make-texture-image image texture-profile compress?)]
    {:resource resource
     :content (protobuf/pb->bytes texture-image)}))

(defn make-texture-build-target
  [workspace node-id image texture-profile compress?]
  (let [texture-type     (workspace/get-resource-type workspace "texture")
        texture-resource (resource/make-memory-resource workspace texture-type (str (gensym)))]
    {:node-id   node-id
     :resource  (workspace/make-build-resource texture-resource)
     :build-fn  build-texture
     :user-data {:image image
                 :compress? compress?
                 :texture-profile texture-profile}}))

(g/defnk produce-build-targets [_node-id resource content texture-profile build-settings]
  [{:node-id _node-id
    :resource (workspace/make-build-resource resource)
    :build-fn build-texture
    :user-data {:image content
                :texture-profile texture-profile
                :compress? (:compress? build-settings false)}}])

(defn- generate-gpu-texture [{:keys [texture-image]} request-id params unit]
  (texture/texture-image->gpu-texture request-id texture-image params unit))

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
                               (try
                                 (or (image-util/read-size resource)
                                   (validation/invalid-content-error _node-id :size :fatal resource))
                                 (catch java.io.FileNotFoundException e
                                   (validation/file-not-found-error _node-id :size :fatal resource))
                                 (catch Exception _
                                   (validation/invalid-content-error _node-id :size :fatal resource)))))
  
  (output content BufferedImage (g/fnk [_node-id resource]
                                  (try
                                    (or (image-util/read-image resource)
                                        (validation/invalid-content-error _node-id :content :fatal resource))
                                    (catch java.io.FileNotFoundException e
                                      (validation/file-not-found-error _node-id :content :fatal resource))
                                    (catch Exception _
                                      (validation/invalid-content-error _node-id :content :fatal resource)))))

  (output texture-image g/Any (g/fnk [content texture-profile] (tex-gen/make-preview-texture-image content texture-profile)))

  (output gpu-texture-generator g/Any :cached (g/fnk [texture-image :as user-data]
                                                {:generate-fn generate-gpu-texture
                                                 :user-data user-data}))
  
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
