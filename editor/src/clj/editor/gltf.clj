;; Copyright 2020-2026 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.

(ns editor.gltf
  (:require [editor.resource :as resource]
            [util.coll :as coll])
  (:import [com.dynamo.bob.fs GltfContainer]
           [com.dynamo.bob.pipeline ModelImporterJni$DataResolver]))

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
              asset-resources)

        material-resources
        (into []
              (filter #(= :material (:kind (resource/gltf-resource-asset-info %))))
              asset-resources)]
    (into []
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
                         sampler-bindings)}))))
          material-resources)))

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
          (into []
                (filter #(= :image (:kind (resource/gltf-resource-asset-info %))))
                asset-resources))
        texture-descriptors (into [] (map val) texture-descriptors-by-index)
        texture-name-by-index
        (into {}
              (map (fn [{:keys [index name]}]
                     [index (or (coll/not-empty name) (format "Texture %d" index))]))
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
              asset-resources)]
    {:materials material-descriptors
     :meshes (or (:meshes (resource/gltf-container-info source-resource)) [])
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
