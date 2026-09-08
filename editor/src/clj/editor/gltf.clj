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
            [clojure.string :as string]
            [editor.resource :as resource]
            [service.log :as log]
            [util.coll :as coll :refer [pair]])
  (:import [com.dynamo.bob.fs GltfContainer GltfContainer$Asset GltfContainer$Extraction GltfContainer$ImageAsset GltfContainer$ImageLocation GltfContainer$MaterialAsset GltfContainer$MeshMetadata GltfContainer$SamplerBinding GltfContainer$TextureMetadata]
           [com.dynamo.bob.pipeline ModelImporterJni$DataResolver]
           [com.google.protobuf ByteString]
           [java.util Map]
           [org.apache.commons.io FilenameUtils]))

(set! *warn-on-reflection* true)

(defn asset-info
  "Returns glTF metadata attached to an embedded resource, or nil."
  [resource]
  (get-in resource [:data ::asset]))

(defn- asset-resources
  "Returns virtual assets beneath a glTF source, excluding grouping folders."
  [source-resource]
  (into []
        (comp resource/xform-recursive-resources
              (filter #(some? (asset-info %))))
        (resource/children source-resource)))

(defn material-binding-descriptors
  "Builds material and texture bindings for the given index set; nil selects all materials."
  [source-resource material-indices]
  (let [asset-resources
        (asset-resources source-resource)

        resource-by-asset-path
        (into {}
              (map (fn [asset-resource]
                     [(:path (asset-info asset-resource))
                      asset-resource]))
              asset-resources)]
    (into []
          (comp
            (filter #(= :material (:kind (asset-info %))))
            (keep
              (fn [material-resource]
                (let [{:keys [index material-name sampler-bindings]} (asset-info material-resource)]
                  (when (or (nil? material-indices)
                            (contains? material-indices index))
                    {:name material-name
                     :material material-resource
                     :material-index index
                     :textures
                     (into []
                           (keep (fn [{:keys [sampler image-path]}]
                                   (when-let [texture-resource (resource-by-asset-path image-path)]
                                     {:sampler sampler
                                      :texture texture-resource})))
                           sampler-bindings)})))))
          asset-resources)))

(defn metadata-descriptors
  "Builds mesh, material and texture descriptors for the read-only glTF outline."
  [source-resource]
  (let [asset-resources (asset-resources source-resource)
        texture-descriptors
        (into []
              (comp
                (filter #(= :image (:kind (asset-info %))))
                (mapcat
                  (fn [image-resource]
                    (let [{:keys [mime-type name source-kind textures uri] :as image-info}
                          (asset-info image-resource)
                          image-index (:index image-info)]
                      (eduction
                        (map (fn [{:keys [index] :as texture-info}]
                               (assoc texture-info
                                 :image image-resource
                                 :image-index image-index
                                 :image-name (or (coll/not-empty name) (format "Image %d" image-index))
                                 :mime-type (or mime-type "")
                                 :name (or (coll/not-empty (:name texture-info)) (format "Texture %d" index))
                                 :source-kind (or source-kind "")
                                 :uri (or uri ""))))
                        textures)))))
              asset-resources)
        texture-name-by-index
        (into {}
              (map (juxt :index :name))
              texture-descriptors)
        material-descriptors
        (into []
              (comp
                (filter #(= :material (:kind (asset-info %))))
                (map
                  (fn [material-resource]
                    (let [{:keys [index name sampler-bindings] :as asset-info}
                          (asset-info material-resource)
                          sampler-descriptions
                          (into []
                                (map
                                  (fn [{:keys [sampler texture-index]}]
                                    (format "%s → %s"
                                            sampler
                                            (get texture-name-by-index texture-index
                                                 (format "Texture %d" texture-index)))))
                                sampler-bindings)]
                      (assoc asset-info
                        :material material-resource
                        :name (or (coll/not-empty name) (format "Material %d" index))
                        :samplers (coll/join-to-string ", " sampler-descriptions))))))
              asset-resources)
        mesh-descriptors
        (into []
              (comp
                (map asset-info)
                (filter #(= :mesh (:kind %)))
                (map (fn [{:keys [index name name-generated primitive-count vertex-count]}]
                       {:index index
                        :name (if name-generated (format "Mesh %d" index) name)
                        :name-generated name-generated
                        :primitive-count primitive-count
                        :vertex-count vertex-count})))
              asset-resources)]
    {:materials material-descriptors
     :meshes (vec (sort-by :index mesh-descriptors))
     :textures (vec (sort-by :index texture-descriptors))}))

(defn uri->proj-path
  "Resolves an external URI against a glTF source path, returning nil for unsupported paths."
  ^String [^String source-path ^String uri]
  (try
    (str "/" (GltfContainer/resolveExternalResourcePath source-path uri))
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

(defn- gltf-asset-info
  "Converts extracted asset metadata to the map stored on its virtual resource."
  [^GltfContainer$Asset asset]
  (let [common-info {:index (.getIndex asset)
                     :name (.getName asset)
                     :path (.getPath asset)}]
    (cond
      (instance? GltfContainer$MaterialAsset asset)
      (let [^GltfContainer$MaterialAsset material-asset asset
            ^Map sampler-bindings (.getSamplerBindings material-asset)]
        (assoc common-info
          :kind :material
          :material-name (-> material-asset .getMaterialDesc .getName)
          :sampler-bindings
          (mapv
            (fn [^GltfContainer$SamplerBinding sampler-binding]
              {:sampler (.samplerName sampler-binding)
               :material-index (.materialIndex sampler-binding)
               :texture-index (.textureIndex sampler-binding)
               :image-index (.imageIndex sampler-binding)
               :image-path (.imagePath sampler-binding)})
            (.values sampler-bindings))))

      (instance? GltfContainer$MeshMetadata asset)
      (let [^GltfContainer$MeshMetadata mesh asset]
        (assoc common-info
          :kind :mesh
          :name-generated (.isNameGenerated mesh)
          :primitive-count (.getPrimitiveCount mesh)
          :vertex-count (.getVertexCount mesh)))

      :else
      (let [^GltfContainer$ImageAsset image-asset asset
            source-kind (.getSourceKind image-asset)]
        (assoc common-info
          :kind :image
          :uri (when-not (= "data-uri" source-kind)
                 (.getUri image-asset))
          :mime-type (.getMimeType image-asset)
          :source-kind source-kind
          :textures
          (mapv
            (fn [^GltfContainer$TextureMetadata texture]
              {:index (.index texture)
               :name (.name texture)
               :sampler-index (.samplerIndex texture)
               :min-filter (.minFilter texture)
               :mag-filter (.magFilter texture)
               :wrap-s (.wrapS texture)
               :wrap-t (.wrapT texture)
               :basisu (.basisu texture)})
            (.getTextures image-asset)))))))

(defn expand-resource
  "Adapts shared asset metadata, resolving headers only when image format is unspecified."
  [source resolve-resource]
  (when (#{"gltf" "glb"} (resource/type-ext source))
    (try
      (with-open [stream (io/input-stream source)]
        (let [^GltfContainer$Extraction extraction (GltfContainer/inspect
                                                     stream (resource/path source)
                                                     (reify ModelImporterJni$DataResolver
                                                       (getData [_this source-path uri]
                                                         (when-let [path (uri->proj-path source-path uri)]
                                                           (when-let [resource (resolve-resource path)]
                                                             (with-open [stream (io/input-stream resource)]
                                                               (.readNBytes stream 8)))))))
              diagnostics (into []
                                ;; Keep unsupported KTX2 images quiet in the editor for now.
                                (remove #(re-matches #"Image \d+: unsupported image MIME type 'image/ktx2'" %))
                                (.diagnostics extraction))
              children-by-group
              (reduce
                (fn [groups ^GltfContainer$Asset asset]
                  (let [path (.getPath asset)
                        group (subs path 0 (.indexOf ^String path "/"))
                        {:keys [index kind name] :as info} (gltf-asset-info asset)
                        ^GltfContainer$ImageLocation location (when (instance? GltfContainer$ImageAsset asset)
                                                                (.getLocation ^GltfContainer$ImageAsset asset))
                        content (cond
                                  location
                                  {:path (str "/" (.path location))
                                   :offset (.offset location)
                                   :length (.length location)}

                                  (= :mesh kind)
                                  nil

                                  :else
                                  (ByteString/copyFrom (.getContent asset)))
                        resource-name (if (= :mesh kind)
                                        (str (resource/resource-name source) " : " (FilenameUtils/getName path))
                                        (format "%s [%d].%s" name index (FilenameUtils/getExtension path)))
                        data (cond-> {::asset info}
                               (= :material kind)
                               (assoc ::resource/export-name (format "%s [%d].material"
                                                                    (-> name
                                                                        (string/replace #"[\\/:*?\"<>|\p{Cntrl}]" "_")
                                                                        string/trim)
                                                                    index)))
                        child (resource/make-resource-entry source
                                                            {:path path
                                                             :name resource-name
                                                             :ext (when (= :mesh kind) "gltf-mesh")
                                                             :content content
                                                             :data data})]
                    (update groups group (fnil conj []) child)))
                (sorted-map)
                (.assets extraction))]
          {:data {::diagnostics diagnostics}
           :children (into []
                           (map (fn [[group children]]
                                  (resource/make-resource-entry source {:path group :children children})))
                           children-by-group)}))
      (catch Exception exception
        (log/warn :message (format "Failed to expose glTF resources from '%s'" (resource/proj-path source))
                  :exception exception)
        {:data {::diagnostics [(ex-message exception)]}
         :children []}))))

(defn diagnostics
  "Returns extraction diagnostics by source path for display in the editor."
  [resources]
  (into {}
        (keep (fn [source]
                (when-let [diagnostics (coll/not-empty (get-in source [:data ::diagnostics]))]
                  (pair (resource/proj-path source) diagnostics))))
        resources))
