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

(ns editor.gltf
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.core :as core]
            [editor.defold-project :as project]
            [editor.image :as image]
            [editor.image-util :as image-util]
            [editor.resource :as resource]
            [editor.resource-io :as resource-io]
            [editor.texture-util :as texture-util]
            [editor.workspace :as workspace]
            [internal.util :as util]
            [util.coll :as coll]
            [util.defonce :as defonce]
            [util.fn :as fn]
            [util.http-server :as http-server]
            [util.path :as path])
  (:import [com.dynamo.bob.fs GltfContainer GltfContainer$Asset GltfContainer$Extraction GltfContainer$ImageAsset GltfContainer$ImageLocation GltfContainer$ImageReference GltfContainer$MaterialAsset GltfContainer$MeshMetadata GltfContainer$SamplerBinding GltfContainer$TextureMetadata]
           [com.dynamo.bob.pipeline ModelImporterJni$DataResolver]
           [java.io FileNotFoundException IOException]
           [java.util Map]
           [org.apache.commons.io.input BoundedInputStream]))

(set! *warn-on-reflection* true)

(declare EmbeddedImageNode load-embedded-image embedded-image-dependencies)

(def ^:private embedded-image-resource-type
  (fn/memoize
    {:limit 8}
    (fn embedded-image-resource-type [resource-type]
      (assoc resource-type
        :node-type EmbeddedImageNode
        :load-fn load-embedded-image
        :dependencies-fn embedded-image-dependencies))))

;; An image whose encoded bytes occupy a range in a GLB or external glTF buffer.
;; All fields are non-nil except backing-resource.
;; entry: resource/Resource for the virtual image; its :source is the glTF/GLB.
;; buffer-proj-path: String; proj-path of the resource holding the image bytes.
;; offset: non-negative long; byte offset into that resource.
;; length: non-negative long; number of encoded image bytes.
;; backing-resource: resource/Resource or nil. Initially nil; EmbeddedImageNode
;;   supplies the connected buffer resource from the graph's evaluation context.
(defonce/record EmbeddedImageResource [entry buffer-proj-path offset length backing-resource]
  resource/Resource
  (children [_this] nil)
  (ext [_this] (resource/ext entry))
  (lookup-resource-type [_this editable->type-ext->resource-type]
    (embedded-image-resource-type
      (resource/lookup-resource-type entry editable->type-ext->resource-type)))
  (source-type [_this] :file)
  (exists? [_this] (resource/exists? entry))
  (read-only? [_this] true)
  (symlink? [_this] false)
  (path [_this] (resource/path entry))
  (abs-path [_this] nil)
  (proj-path [_this] (resource/proj-path entry))
  (resource-name [_this] (resource/resource-name entry))
  (workspace [_this] (resource/workspace entry))
  (resource-hash [_this] (resource/resource-hash entry))
  (openable? [_this] (resource/openable? entry))
  (editable? [_this] (resource/editable? entry))
  (loaded? [_this] (resource/loaded? entry))

  io/IOFactory
  (make-input-stream [_this _opts]
    (let [source (:source entry)
          ;; This lookup is safe for graph invalidation: EmbeddedImageNode
          ;; explicitly connects to the backing resource node, so buffer changes
          ;; invalidate its image outputs. Direct readers (e.g. copy/extract)
          ;; have no backing-resource supplied by the graph and resolve the
          ;; buffer from the workspace here.
          buffer (or backing-resource
                     (if (= buffer-proj-path (resource/proj-path source))
                       source
                       (workspace/find-resource (g/unsafe-basis) (resource/workspace source) buffer-proj-path)))]
      (when-not buffer
        (throw (FileNotFoundException. buffer-proj-path)))
      (let [stream (io/input-stream buffer)]
        (try
          (.skipNBytes stream (long offset))
          (BoundedInputStream. stream (long length))
          (catch Throwable error
            (.close stream)
            (throw error))))))
  (make-reader [this opts] (io/make-reader (io/make-input-stream this opts) opts))
  (make-output-stream [_this _opts] (throw (IOException. "Embedded images are read-only")))
  (make-writer [_this _opts] (throw (IOException. "Embedded images are read-only")))

  io/Coercions
  (as-file [_this] (io/as-file entry))
  (as-url [_this] (throw (IllegalArgumentException. "Embedded images have no URL")))

  path/Coercions
  (as-path [_this] (path/as-path entry))

  http-server/ContentType
  (content-type [_this] (http-server/content-type entry))

  http-server/->Connection
  (->connection [this] (io/input-stream this)))

(core/register-record-type! EmbeddedImageResource)

(defn asset-info
  "Returns glTF metadata attached to an embedded resource, or nil."
  [resource]
  (get-in (if (instance? EmbeddedImageResource resource)
            (:entry resource)
            resource)
          [:data :asset]))

(defn- asset-resources
  "Returns virtual assets beneath a glTF source, excluding grouping folders."
  [source-resource]
  (coll/into-> (resource/children source-resource) []
    resource/xform-recursive-resources
    (filter asset-info)))

(defn material-binding-descriptors
  "Builds material and texture bindings for the given index set; nil selects all
  materials."
  [source-resource material-indices resolve-resource]
  (let [asset-resources (asset-resources source-resource)
        resource-by-proj-path (coll/pair-map-by resource/proj-path asset-resources)]
    (coll/into-> asset-resources []
      (keep
        (fn [material-resource]
          (let [{:keys [index kind material-name sampler-bindings]} (asset-info material-resource)]
            (when (and (= :material kind)
                       (or (nil? material-indices)
                           (contains? material-indices index)))
              {:name material-name
               :material material-resource
               :material-index index
               :textures
               (coll/into-> sampler-bindings []
                 (keep
                   (fn [{:keys [sampler image-proj-path]}]
                     (when-let [texture-resource (or (resource-by-proj-path image-proj-path)
                                                     (resolve-resource image-proj-path))]
                       {:sampler sampler
                        :texture texture-resource}))))})))))))

(defn metadata-descriptors
  "Builds mesh, material and texture descriptors for the read-only glTF outline."
  [source-resource resolve-resource]
  (let [asset-resources (asset-resources source-resource)

        image-descriptors
        (into (mapv #(assoc % :image (resolve-resource (:proj-path %)))
                    (get-in source-resource [:data :external-images]))
              (comp (filter #(= :image (:kind (asset-info %))))
                    (map #(assoc (asset-info %) :image %)))
              asset-resources)

        texture-descriptors
        (coll/into-> image-descriptors []
          (mapcat
            (fn [{:keys [image index mime-type name source-kind textures uri]}]
              (coll/into-> textures :eduction
                (map (fn [texture-info]
                       (assoc texture-info
                         :image image
                         :image-index index
                         :image-name (or (coll/not-empty name) (format "Image %d" index))
                         :mime-type (or mime-type "")
                         :name (or (coll/not-empty (:name texture-info)) (format "Texture %d" (:index texture-info)))
                         :source-kind source-kind
                         :uri (or uri ""))))))))

        texture-name-by-index
        (coll/pair-map-by :index :name texture-descriptors)

        material-descriptors
        (coll/into-> asset-resources []
          (keep (fn [material-resource]
                  (let [asset-info (asset-info material-resource)]
                    (when (= :material (:kind asset-info))
                      (let [{:keys [index name sampler-bindings]} asset-info

                            sampler-descriptions
                            (coll/into-> sampler-bindings []
                              (map (fn [{:keys [sampler texture-index]}]
                                     (format "%s → %s"
                                             sampler
                                             (get texture-name-by-index texture-index
                                                  (format "Texture %d" texture-index))))))]
                        (assoc asset-info
                          :material material-resource
                          :name (or (coll/not-empty name) (format "Material %d" index))
                          :samplers (coll/join-to-string ", " sampler-descriptions))))))))

        mesh-descriptors
        (coll/into-> asset-resources []
          (map asset-info)
          (filter #(= :mesh (:kind %)))
          (map (fn [{:keys [index name name-generated primitive-count vertex-count]}]
                 {:index index
                  :name (if name-generated (format "Mesh %d" index) name)
                  :name-generated name-generated
                  :primitive-count primitive-count
                  :vertex-count vertex-count})))]

    {:materials material-descriptors
     :meshes (vec (sort-by :index mesh-descriptors))
     :textures (vec (sort-by :index texture-descriptors))}))

(defn external-image-proj-paths [source-resource]
  (mapv :proj-path (get-in source-resource [:data :external-images])))

(defn uri->proj-path
  "Resolves an external URI against a glTF source path, returning nil for
  unsupported paths."
  ^String [^String source-path ^String uri]
  (try
    (GltfContainer/resolveExternalResourcePath source-path uri)
    (catch Exception _
      nil)))

(defn make-data-resolver
  "Creates an importer resolver backed by workspace resources."
  ^ModelImporterJni$DataResolver [resource-by-proj-path]
  (reify ModelImporterJni$DataResolver
    (getData [_this source-path uri]
      (try
        (when-let [proj-path (uri->proj-path source-path uri)]
          (when-let [external-resource (resource-by-proj-path proj-path)]
            (when (= :file (resource/source-type external-resource))
              (resource/resource->bytes external-resource))))
        (catch Exception _
          nil)))))

(defn- texture-metadata [textures]
  (coll/into-> textures []
    (map (fn [^GltfContainer$TextureMetadata texture]
           (let [min-filter (.minFilter texture)
                 mag-filter (.magFilter texture)
                 wrap-s (.wrapS texture)
                 wrap-t (.wrapT texture)]
             {:index (.index texture)
              :name (.name texture)
              :sampler-index (.samplerIndex texture)
              :min-filter (case min-filter
                            0 "Linear"
                            9728 "Nearest"
                            9729 "Linear"
                            9984 "Nearest Mipmap Nearest"
                            9985 "Linear Mipmap Nearest"
                            9986 "Nearest Mipmap Linear"
                            9987 "Linear Mipmap Linear"
                            (format "Unknown (%d)" min-filter))
              :mag-filter (case mag-filter
                            0 "Linear"
                            9728 "Nearest"
                            9729 "Linear"
                            (format "Unknown (%d)" mag-filter))
              :wrap-s (case wrap-s
                        10497 "Repeat"
                        33071 "Clamp to Edge"
                        33648 "Mirrored Repeat"
                        (format "Unknown (%d)" wrap-s))
              :wrap-t (case wrap-t
                        10497 "Repeat"
                        33071 "Clamp to Edge"
                        33648 "Mirrored Repeat"
                        (format "Unknown (%d)" wrap-t))
              :basisu (.basisu texture)})))))

(defn- gltf-asset-info
  "Converts extracted asset metadata to the map stored on its virtual resource."
  [^GltfContainer$Asset asset]
  (let [common-info {:index (.index asset)
                     :name (.name asset)
                     :proj-path (.resourcePath asset)}]
    (cond
      (instance? GltfContainer$MaterialAsset asset)
      (let [^GltfContainer$MaterialAsset material-asset asset
            ^Map sampler-bindings (.samplerBindings material-asset)]
        (assoc common-info
          :kind :material
          :material-name (-> material-asset .materialDesc .getName)
          :sampler-bindings
          (coll/into-> (.values sampler-bindings) []
            (map (fn [^GltfContainer$SamplerBinding sampler-binding]
                   {:sampler (.samplerName sampler-binding)
                    :material-index (.materialIndex sampler-binding)
                    :texture-index (.textureIndex sampler-binding)
                    :image-index (.imageIndex sampler-binding)
                    :image-proj-path (.imageResourcePath sampler-binding)})))))

      (instance? GltfContainer$MeshMetadata asset)
      (let [^GltfContainer$MeshMetadata mesh asset]
        (assoc common-info
          :kind :mesh
          :name-generated (.nameGenerated mesh)
          :primitive-count (.primitiveCount mesh)
          :vertex-count (.vertexCount mesh)))

      :else
      (let [^GltfContainer$ImageAsset image-asset asset
            source-kind (.sourceKind image-asset)]
        (assoc common-info
          :kind :image
          :uri (when-not (= "data-uri" source-kind)
                 (.uri image-asset))
          :mime-type (.mimeType image-asset)
          :source-kind source-kind
          :textures (texture-metadata (.textures image-asset)))))))

(g/defnode EmbeddedImageNode
  (inherits image/ImageNode)

  (input backing-resource resource/Resource)

  (output image-resource g/Any
          (g/fnk [resource ^:try backing-resource]
            (if (resource/resource? backing-resource)
              (assoc resource :backing-resource backing-resource)
              resource)))

  (output size g/Any :cached
          (g/fnk [_node-id image-resource]
            (resource-io/with-error-translation image-resource _node-id :size
              (image-util/read-size image-resource))))

  (output content-generator g/Any :cached
          (g/fnk [_node-id image-resource]
            (texture-util/make-buffered-image-generator image-resource _node-id :content-generator)))

  (output sha256 g/Str :cached
          (g/fnk [_node-id image-resource]
            (resource-io/with-error-translation image-resource _node-id :sha256
              (resource/resource->sha256-hex image-resource)))))

(defn- load-embedded-image
  [{:keys [project resolve-resource-fn editable->type-ext->resource-type] :as load-opts}
   {:keys [node-id resource] :as node-load-info}]
  (let [image-resource-type (resource/lookup-resource-type (:entry resource) editable->type-ext->resource-type)
        image-load-fn (:load-fn image-resource-type)]
    (into (vec (image-load-fn load-opts node-load-info))
          (g/expand-ec
            (fn [evaluation-context]
              (let [image-resource (resolve-resource-fn resource (:buffer-proj-path resource))
                    connections [[:resource :backing-resource]]]
                (:tx-data (project/connect-resource-node evaluation-context project image-resource node-id connections))))))))

(defn- embedded-image-dependencies [_read-opts resource _source-value]
  [(:buffer-proj-path resource)])

(defn- expand
  "Discovers embedded assets and external references using only the container bytes."
  [source stream]
  (let [^GltfContainer$Extraction extraction (GltfContainer/inspect stream (resource/proj-path source))
        children-by-group-proj-path
        (util/group-into
          (sorted-map) []
          #(resource/parent-proj-path (.resourcePath ^GltfContainer$Asset %))
          (fn [^GltfContainer$Asset asset]
            (let [{:keys [kind] :as info} (gltf-asset-info asset)
                  ^GltfContainer$ImageLocation location (when (instance? GltfContainer$ImageAsset asset)
                                                          (.location ^GltfContainer$ImageAsset asset))
                  child (resource/make-resource-entry source (.resourcePath asset)
                                                      :ext (when (= :mesh kind) "gltf-mesh")
                                                      :content (when-not location (.content asset))
                                                      :data {:asset info})]
              (if-not location
                child
                (->EmbeddedImageResource child (.resourcePath location) (.offset location) (.length location) nil))))
          (.assets extraction))]
    (assoc source
      :data {:external-images
             (mapv (fn [^GltfContainer$ImageReference image]
                     {:index (.index image)
                      :name (.name image)
                      :proj-path (.resourcePath image)
                      :uri (.uri image)
                      :mime-type (.mimeType image)
                      :source-kind "external-uri"
                      :textures (texture-metadata (.textures image))})
                   (.externalImages extraction))}
      :children (coll/into-> children-by-group-proj-path []
                  (map (fn [[group-proj-path children]]
                         (resource/make-resource-entry source group-proj-path :children children)))))))

(defmethod resource/expand "gltf" [source stream]
  (expand source stream))

(defmethod resource/expand "glb" [source stream]
  (expand source stream))
