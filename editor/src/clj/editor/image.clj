(ns editor.image
  (:require [dynamo.graph :as g]
            [editor.image-util :as image-util]
            [editor.gl.texture :as texture]
            [editor.geom :refer [clamper]]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.validation :as validation]
            [editor.workspace :as workspace]
            [editor.pipeline.tex-gen :as tex-gen])
  (:import [editor.gl.texture TextureLifecycle]
           [java.awt.image BufferedImage]))

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

(g/defnk produce-texture-build-target [_node-id resource content texture-profile build-settings]
  {:node-id _node-id
   :resource (workspace/make-build-resource resource)
   :build-fn build-texture
   :user-data {:image content
               :texture-profile texture-profile
               :compress? (:compress? build-settings false)}})

(defn- generate-variant-gpu-texture [user-data texture-params unit-index]
  (-> (:gpu-texture user-data)
      (texture/set-params texture-params)
      (texture/set-unit-index unit-index)))

(defn make-variant-gpu-texture-generator [gpu-texture]
  (assert (instance? TextureLifecycle gpu-texture))
  {:generate-fn generate-variant-gpu-texture
   :user-data {:gpu-texture gpu-texture}})

(g/defnode ImageNode
  (inherits resource-node/ResourceNode)

  (input build-settings g/Any)
  (input texture-profiles g/Any)
  
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

  (output gpu-texture g/Any :cached (g/fnk [_node-id content texture-profile]
                                      (let [texture-data (texture/make-preview-texture-data content texture-profile)]
                                        (texture/texture-data->gpu-texture _node-id texture-data))))

  (output gpu-texture-generator g/Any (g/fnk [gpu-texture] (make-variant-gpu-texture-generator gpu-texture)))

  (output texture-build-target g/Any :cached produce-texture-build-target)
  (output build-targets g/Any (g/fnk [texture-build-target] [texture-build-target])))

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
