;; Copyright 2020-2026 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.

(ns editor.gltf
  (:require [editor.resource :as resource]
            [service.log :as log]
            [util.coll :as coll :refer [pair]]
            [util.digest :as digest])
  (:import [com.dynamo.bob.fs GltfContainer GltfContainer$Asset GltfContainer$Extraction GltfContainer$ImageAsset GltfContainer$MaterialAsset GltfContainer$MeshMetadata GltfContainer$SamplerBinding GltfContainer$TextureMetadata]
           [com.dynamo.bob.pipeline ModelImporterJni$DataResolver]
           [java.util Map]))

(set! *warn-on-reflection* true)

(defn- asset-resources [source-resource]
  (into []
        (comp resource/xform-recursive-resources
              (filter resource/gltf-resource?)
              (filter #(some? (resource/gltf-resource-asset-info %))))
        (resource/children source-resource)))

(defn material-binding-descriptors [source-resource material-indices]
  (let [asset-resources
        (asset-resources source-resource)

        resource-by-asset-path
        (into {}
              (map (fn [asset-resource]
                     [(:path (resource/gltf-resource-asset-info asset-resource))
                      asset-resource]))
              asset-resources)]
    (into []
          (comp
            (filter #(= :material (:kind (resource/gltf-resource-asset-info %))))
            (keep
              (fn [material-resource]
                (let [{:keys [index name sampler-bindings]} (resource/gltf-resource-asset-info material-resource)]
                  (when (or (nil? material-indices)
                            (contains? material-indices index))
                    {:name (or (coll/not-empty name) (str "gltf_material_" index))
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

(defn metadata-descriptors [source-resource]
  (let [asset-resources (asset-resources source-resource)
        texture-descriptors-by-index
        (reduce
          (fn [descriptors-by-index image-resource]
            (let [{:keys [mime-type name source-kind textures uri] :as image-info}
                  (resource/gltf-resource-asset-info image-resource)
                  image-index (:index image-info)]
              (reduce
                (fn [descriptors-by-index {:keys [basisu index] :as texture-info}]
                  (let [descriptor (assoc texture-info
                                     :image image-resource
                                     :image-index image-index
                                     :image-name (or (coll/not-empty name) (format "Image %d" image-index))
                                     :mime-type (or mime-type "")
                                     :name (or (coll/not-empty (:name texture-info)) (format "Texture %d" index))
                                     :source-kind (or source-kind "")
                                     :uri (or uri ""))
                        previous-descriptor (get descriptors-by-index index)]
                    (if (and previous-descriptor
                             (not basisu))
                      descriptors-by-index
                      (assoc descriptors-by-index index descriptor))))
                descriptors-by-index
                textures)))
          (sorted-map)
          (eduction
            (filter #(= :image (:kind (resource/gltf-resource-asset-info %))))
            asset-resources))
        texture-descriptors (into [] (map val) texture-descriptors-by-index)
        texture-name-by-index
        (into {}
              (map (juxt :index :name))
              texture-descriptors)
        material-descriptors
        (into []
              (comp
                (filter #(= :material (:kind (resource/gltf-resource-asset-info %))))
                (map
                  (fn [material-resource]
                    (let [{:keys [index name sampler-bindings] :as asset-info}
                          (resource/gltf-resource-asset-info material-resource)
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
                (map resource/gltf-resource-asset-info)
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
     :textures texture-descriptors}))

(defn uri->proj-path
  ^String [^String source-path ^String uri]
  (try
    (str "/" (GltfContainer/resolveExternalResourcePath source-path uri))
    (catch Exception _
      nil)))

(defn make-data-resolver
  ^ModelImporterJni$DataResolver
  [resource-by-proj-path resolved-proj-path!]
  (reify ModelImporterJni$DataResolver
    (getData [_this source-path uri]
      (try
        (when-let [proj-path (uri->proj-path source-path uri)]
          (when resolved-proj-path!
            (resolved-proj-path! proj-path))
          (when-let [external-resource (resource-by-proj-path proj-path)]
            (when (= :file (resource/source-type external-resource))
              (resource/resource->bytes external-resource))))
        (catch Exception _
          nil)))))

(defn- gltf-source-resource? [resource]
  (and (= :file (resource/source-type resource))
       (#{"gltf" "glb"} (resource/type-ext resource))
       (resource/loaded? resource)))

(defn- resource-statuses [status-map proj-paths]
  (into {}
        (map (fn [proj-path]
               (pair proj-path (get status-map proj-path))))
        proj-paths))

(defn- gltf-cache-entry-valid? [cache-entry status-map]
  (and cache-entry
       (= (:dependency-statuses cache-entry)
          (resource-statuses status-map (coll/keys (:dependency-statuses cache-entry))))))

(defn- gltf-asset-info [^GltfContainer$Asset asset]
  (let [common-info {:index (.getIndex asset)
                     :name (.getName asset)
                     :path (.getPath asset)}]
    (cond
      (instance? GltfContainer$MaterialAsset asset)
      (let [^GltfContainer$MaterialAsset material-asset asset
            ^Map sampler-bindings (.getSamplerBindings material-asset)]
        (assoc common-info
          :kind :material
          :sampler-bindings
          (mapv
            (fn [^GltfContainer$SamplerBinding sampler-binding]
              {:sampler (.getSamplerName sampler-binding)
               :material-index (.getMaterialIndex sampler-binding)
               :texture-index (.getTextureIndex sampler-binding)
               :image-index (.getImageIndex sampler-binding)
               :image-path (.getImagePath sampler-binding)})
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
              {:index (.getIndex texture)
               :name (.getName texture)
               :sampler-index (.getSamplerIndex texture)
               :min-filter (.getMinFilter texture)
               :mag-filter (.getMagFilter texture)
               :wrap-s (.getWrapS texture)
               :wrap-t (.getWrapT texture)
               :basisu (.isBasisu texture)})
            (.getTextures image-asset)))))))

(defn- make-gltf-children+status
  [workspace source-resource ^GltfContainer$Extraction extraction]
  (let [source-proj-path (resource/proj-path source-resource)
        editable (resource/editable? source-resource)
        loaded (resource/loaded? source-resource)
        {:keys [children-by-group status-map]}
        (reduce
          (fn [{:keys [children-by-group status-map]} ^GltfContainer$Asset asset]
            (let [asset-path (.getPath asset)
                  separator-index (.indexOf ^String asset-path "/")
                  group-name (subs asset-path 0 separator-index)
                  asset-proj-path (str source-proj-path "/" asset-path)
                  content (.getContent asset)
                  asset-info (gltf-asset-info asset)
                  asset-resource (resource/make-gltf-resource
                                   workspace asset-proj-path content nil editable loaded
                                   asset-info)]
              {:children-by-group (update children-by-group group-name (fnil conj []) asset-resource)
               :status-map (assoc status-map asset-proj-path
                                  {:version (if (= :mesh (:kind asset-info))
                                              asset-info
                                              (digest/sha1-hex content))
                                   :source :gltf
                                   :container source-proj-path})}))
          {:children-by-group (sorted-map)
           :status-map {}}
          (.getAssets extraction))]
    (reduce-kv
      (fn [{:keys [children status-map]} group-name group-children]
        (let [group-proj-path (str source-proj-path "/" group-name)
              group-resource (resource/make-gltf-resource
                               workspace group-proj-path nil group-children editable loaded nil)]
          {:children (conj children group-resource)
           :status-map (assoc status-map group-proj-path
                              {:version :constant
                               :source :gltf
                               :container source-proj-path})}))
      {:children []
       :status-map status-map}
      children-by-group)))

(defn- extract-gltf-cache-entry
  [workspace source-resource resources-by-proj-path status-map]
  (let [source-proj-path (resource/proj-path source-resource)
        dependency-proj-paths (atom #{source-proj-path})
        data-resolver (make-data-resolver resources-by-proj-path #(swap! dependency-proj-paths conj %))
        extraction-data
        (try
          (let [^bytes source-content (resource/resource->bytes source-resource)
                ^GltfContainer$Extraction extraction
                (GltfContainer/extract source-content (resource/path source-resource) data-resolver)]
            (run!
              (fn [diagnostic]
                (log/warn :message (format "Failed to expose part of glTF resource '%s': %s"
                                           source-proj-path diagnostic)))
              (.getDiagnostics extraction))
            (make-gltf-children+status workspace source-resource extraction))
          (catch Exception exception
            (log/warn :message (format "Failed to expose glTF resources from '%s'" source-proj-path)
                      :exception exception)
            {:children []
             :status-map {}}))]
    (assoc extraction-data
      :dependency-statuses (resource-statuses status-map @dependency-proj-paths))))

(defn- attach-gltf-children [resource cache-entries]
  (if-let [cache-entry (cache-entries (resource/proj-path resource))]
    (assoc resource :children (:children cache-entry))
    (if-let [children (resource/children resource)]
      (assoc resource :children (mapv #(attach-gltf-children % cache-entries) children))
      resource)))

(defn add-resources-to-snapshot
  "Adds glTF child resources and their statuses to a resource snapshot. Reuses
  cached extractions while their source and external dependency statuses match.
  Returns the updated :snapshot and :snapshot-cache."
  [workspace snapshot resources-by-proj-path snapshot-cache]
  (let [status-map (:status-map snapshot)
        gltf-source-resources
        (into []
              (comp resource/xform-recursive-resources
                    (filter gltf-source-resource?))
              (:resources snapshot))
        old-cache (get snapshot-cache ::snapshot-cache {})
        cache-entries
        (reduce
          (fn [cache source-resource]
            (let [source-proj-path (resource/proj-path source-resource)
                  old-cache-entry (old-cache source-proj-path)
                  cache-entry (if (gltf-cache-entry-valid? old-cache-entry status-map)
                                old-cache-entry
                                (extract-gltf-cache-entry workspace source-resource resources-by-proj-path status-map))]
              (assoc cache source-proj-path cache-entry)))
          {}
          gltf-source-resources)
        new-status-map
        (reduce-kv
          (fn [status-map source-proj-path cache-entry]
            (let [child-status-map (:status-map cache-entry)]
              (-> status-map
                  (into child-status-map)
                  (update source-proj-path assoc
                          :gltf-resource-paths (into #{} (coll/keys child-status-map))))))
          status-map
          cache-entries)
        resources (mapv #(attach-gltf-children % cache-entries) (:resources snapshot))]
    {:snapshot (assoc snapshot
                 :resources resources
                 :status-map new-status-map)
     :snapshot-cache (assoc snapshot-cache ::snapshot-cache cache-entries)}))

(defn expand-container-moves
  "Includes virtual child file moves when their glTF source file is moved."
  [moved-proj-paths old-map new-map]
  (reduce
    (fn [expanded-moved-proj-paths [source-proj-path target-proj-path :as moved-proj-path-pair]]
      (let [source-resource (old-map source-proj-path)
            expanded-moved-proj-paths (conj expanded-moved-proj-paths moved-proj-path-pair)]
        (if (and source-resource
                 (= :file (resource/source-type source-resource))
                 (#{"gltf" "glb"} (resource/type-ext source-resource)))
          (into expanded-moved-proj-paths
                (comp resource/xform-recursive-resources
                      (filter #(and (resource/gltf-resource? %)
                                    (= :file (resource/source-type %))))
                      (keep (fn [source-child]
                              (let [source-child-proj-path (resource/proj-path source-child)
                                    child-suffix (subs source-child-proj-path (count source-proj-path))
                                    target-child-proj-path (str target-proj-path child-suffix)
                                    target-child (new-map target-child-proj-path)]
                                (when (and (resource/gltf-resource? target-child)
                                           (= :file (resource/source-type target-child)))
                                  (pair source-child-proj-path target-child-proj-path))))))
                (resource/children source-resource))
          expanded-moved-proj-paths)))
    []
    moved-proj-paths))
