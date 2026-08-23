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

(ns editor.resource-watch
  (:require [clojure.java.io :as io]
            [clojure.set :as set]
            [clojure.string :as str]
            [dynamo.graph :as g]
            [editor.gltf :as gltf]
            [editor.library :as library]
            [editor.resource :as resource]
            [editor.system :as system]
            [service.log :as log]
            [util.coll :as coll :refer [pair]]
            [util.digest :as digest]
            [util.fn :as fn]
            [util.path :as path])
  (:import [com.dynamo.bob.fs GltfContainer GltfContainer$Asset GltfContainer$Extraction GltfContainer$ImageAsset GltfContainer$MaterialAsset GltfContainer$MeshMetadata GltfContainer$SamplerBinding GltfContainer$TextureMetadata]
           [com.dynamo.bob.util Library$Archive Library$Result]
           [java.io File]
           [java.util Map]))

(set! *warn-on-reflection* true)

(defn- resource-root-dir [resource]
  (when-let [path-splits (seq (rest (str/split (resource/proj-path resource) #"/")))] ; skip initial ""
    (if (= (count path-splits) 1)
      (when (= (resource/source-type resource) :folder)
        (first path-splits))
      (first path-splits))))

(defn- make-library-snapshot [workspace ^Library$Result lib-result mtime]
  (let [archive (.archive lib-result)
        base-dir (.baseDir archive)
        zip-resources (resource/load-zip-resources
                        workspace
                        (.toFile (.path archive))
                        (when-not (str/blank? base-dir) base-dir))
        include-dirs (set (.includeDirs archive))
        {:keys [tree crc]} (update zip-resources :tree coll/filterv-> #(include-dirs (resource-root-dir %)))]
    {:mtime mtime
     :resources tree
     :status-map (coll/into-> tree {}
                   resource/xform-recursive-resources
                   (map (fn [resource]
                          (let [proj-path (resource/proj-path resource)]
                            (pair proj-path
                                  {:version (str mtime ":" (crc proj-path))
                                   :source :library
                                   :library (.uri lib-result)})))))}))

(defn- update-library-snapshot-cache
  [library-snapshot-cache workspace lib-results]
  (into library-snapshot-cache
        (keep (fn [^Library$Result lib-result]
                (when-some [archive (.archive lib-result)]
                  (let [archive-path (.path archive)
                        mtime (path/last-modified-ms archive-path)
                        cached-snapshot (get library-snapshot-cache archive-path)]
                    (when (or (nil? cached-snapshot)
                              (not= mtime (:mtime cached-snapshot)))
                      (pair archive-path (make-library-snapshot workspace lib-result mtime)))))))
        lib-results))

(defn- make-library-snapshots [library-snapshot-cache lib-results]
  (into []
        (comp (keep Library$Result/.archive)
              (map Library$Archive/.path)
              (map library-snapshot-cache))
        lib-results))

(defn- make-builtins-snapshot-raw [workspace]
  (let [unpack-path (system/defold-unpack-path)
        builtins-zip-file (io/file unpack-path "builtins" "builtins.zip")
        resources (:tree (resource/load-zip-resources workspace builtins-zip-file))]
    {:resources resources
     :status-map (into {}
                       (comp resource/xform-recursive-resources
                             (map (fn [resource]
                                    (pair (resource/proj-path resource)
                                          {:version :constant
                                           :source :builtins}))))
                       resources)}))

(def make-builtins-snapshot (fn/memoize make-builtins-snapshot-raw))

(def reserved-proj-paths #{"/builtins" "/build" "/.internal" "/.git" "/.editor_settings"})

(defn reserved-proj-path? [^File root path]
  (or (reserved-proj-paths path)
      (resource/ignored-project-path? root path)))

(defn- file-resource-filter [^File root ^File f]
  (not (or (let [file-name (.getName f)]
             (= file-name ".DS_Store"))
           (reserved-proj-path? root (resource/file->proj-path root f)))))

(defn- make-file-tree
  ([workspace ^File file]
   (let [basis (g/now)
         project-directory (resource/project-directory basis workspace)
         editable-proj-path? (g/raw-property-value basis workspace :editable-proj-path?)
         unloaded-proj-path? (g/raw-property-value basis workspace :unloaded-proj-path?)]
     (make-file-tree workspace project-directory file editable-proj-path? unloaded-proj-path?)))
  ([workspace ^File root ^File file editable-proj-path? unloaded-proj-path?]
   (coll/ptree
     (fn file-tree-children [^File file]
       (when (.isDirectory file)
         (filterv #(file-resource-filter root %) (.listFiles file))))
     (fn file-tree-node [^File file children]
       (resource/make-file-resource workspace (.getPath root) file children editable-proj-path? unloaded-proj-path?))
     file)))

(defn- file-resource-status [resource]
  (assert (resource/file-resource? resource))
  {:version (str (.lastModified ^File (io/file resource)))
   :source :directory})

(defn file-resource-status-map-entry [resource]
  (pair (resource/proj-path resource)
        (file-resource-status resource)))

(defn file-resource-status-map-entry? [[proj-path {:keys [version source]}]]
  (and (string? proj-path)
       (str/starts-with? proj-path "/")
       (= :directory source)
       (try
         (Long/parseUnsignedLong version)
         true
         (catch NumberFormatException _
           false))))

(defn- make-directory-snapshot [workspace ^File root]
  (assert (and root (.isDirectory root)))
  (let [resources (resource/children (make-file-tree workspace root))]
    {:resources resources
     :status-map (into {}
                       (comp resource/xform-recursive-resources
                             (map file-resource-status-map-entry))
                       resources)}))

(def empty-snapshot
  {:resources []
   :status-map {}
   :errors []})

(defn map-intersection
  "Given 2 maps, return a vector of keys present in both maps"
  [m1 m2]
  (if (< (count m2) (count m1))
    (recur m2 m1)
    (persistent!
      (reduce-kv
        (fn [acc k _]
          (cond-> acc (contains? m2 k) (conj! k)))
        (transient [])
        m1))))

(defn- combine-snapshots [snapshots]
  (reduce
    (fn [result snapshot]
      (if-let [collisions (not-empty (map-intersection (:status-map result) (:status-map snapshot)))]
        (update result :errors conj {:type :collision :collisions (select-keys (:status-map snapshot) collisions)})
        (-> result
            (update :resources into (:resources snapshot))
            (update :status-map merge (:status-map snapshot)))))
    empty-snapshot
    snapshots))

(defn- make-debugger-snapshot [workspace]
  (let [base-path (if (system/defold-dev?)
                    ;; Use local debugger support files so we can see
                    ;; changes to them instantly without re-packing/restarting.
                    (.getAbsolutePath (io/file "bundle-resources"))
                    (system/defold-unpack-path))
        root (io/file base-path "_defold/debugger")
        ;; Supplying this mount-root derived from the base-path appears to
        ;; produce a strange file-resource-filter inside make-file-tree, which
        ;; won't include defignore patterns and so on. It probably won't matter
        ;; for this case, since this directory will not have any of those files
        ;; below it anyway.
        mount-root (io/file base-path)
        resources (resource/children (make-file-tree workspace mount-root root fn/constantly-false fn/constantly-false))]
    {:resources resources
     :status-map (into {}
                       (comp resource/xform-recursive-resources
                             (map file-resource-status-map-entry))
                       resources)}))

(defn update-snapshot-status [snapshot file-resource-status-map-entries]
  (assert (every? file-resource-status-map-entry? file-resource-status-map-entries))
  (update snapshot :status-map into file-resource-status-map-entries))

(defn make-resource-map [snapshot]
  (into {}
        (comp resource/xform-recursive-resources
              (coll/pair-map-by resource/proj-path))
        (:resources snapshot)))

(def ^:private gltf-snapshot-cache-key ::gltf-snapshot-cache)

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
       (contains? cache-entry :gltf-container-info)
       (= (:dependency-statuses cache-entry)
          (resource-statuses status-map (coll/keys (:dependency-statuses cache-entry))))))

(defn- make-gltf-data-resolver [resources-by-proj-path dependency-proj-paths]
  (gltf/make-data-resolver resources-by-proj-path #(swap! dependency-proj-paths conj %)))

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

(defn- gltf-container-info [^GltfContainer$Extraction extraction]
  {:meshes
   (mapv
     (fn [^GltfContainer$MeshMetadata mesh]
       (let [index (.getIndex mesh)
             name-generated (.isNameGenerated mesh)]
         {:index index
          :name (if name-generated
                  (format "Mesh %d" index)
                  (.getName mesh))
          :name-generated name-generated
          :primitive-count (.getPrimitiveCount mesh)
          :vertex-count (.getVertexCount mesh)}))
     (.getMeshes extraction))})

(defn- extract-gltf-cache-entry
  [workspace source-resource resources-by-proj-path status-map]
  (let [source-proj-path (resource/proj-path source-resource)
        dependency-proj-paths (atom #{source-proj-path})
        data-resolver (make-gltf-data-resolver resources-by-proj-path dependency-proj-paths)
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
            (assoc (make-gltf-children+status workspace source-resource extraction)
              :gltf-container-info (gltf-container-info extraction)))
          (catch Exception exception
            (log/warn :message (format "Failed to expose glTF resources from '%s'" source-proj-path)
                      :exception exception)
            {:children []
             :gltf-container-info {:meshes []}
             :status-map {}}))]
    (assoc extraction-data
      :dependency-statuses (resource-statuses status-map @dependency-proj-paths))))

(defn- attach-gltf-children [resource cache-entries]
  (if-let [cache-entry (cache-entries (resource/proj-path resource))]
    (assoc resource
      :children (:children cache-entry)
      :gltf-container-info (:gltf-container-info cache-entry))
    (if-let [children (resource/children resource)]
      (assoc resource :children (mapv #(attach-gltf-children % cache-entries) children))
      resource)))

(defn- add-gltf-resources [workspace snapshot snapshot-cache]
  (let [status-map (:status-map snapshot)
        resources-by-proj-path (make-resource-map snapshot)
        gltf-source-resources
        (into []
              (comp resource/xform-recursive-resources
                    (filter gltf-source-resource?))
              (:resources snapshot))
        old-cache (get snapshot-cache gltf-snapshot-cache-key {})
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
        virtual-status-map (reduce-kv
                             (fn [status-map _source-proj-path cache-entry]
                               (into status-map (:status-map cache-entry)))
                             {}
                             cache-entries)
        status-map-with-gltf-resource-paths
        (reduce-kv
          (fn [status-map source-proj-path cache-entry]
            (update status-map source-proj-path assoc
                    :gltf-resource-paths (into #{} (coll/keys (:status-map cache-entry)))))
          status-map
          cache-entries)
        resources (mapv #(attach-gltf-children % cache-entries) (:resources snapshot))]
    {:snapshot (assoc snapshot
                 :resources resources
                 :status-map (into status-map-with-gltf-resource-paths virtual-status-map))
     :snapshot-cache (assoc snapshot-cache gltf-snapshot-cache-key cache-entries)}))

(defn make-snapshot-info [workspace project-directory library-uris snapshot-cache]
  (resource/with-defignore-pred project-directory
    (let [lib-results (library/cached project-directory library-uris)
          new-library-snapshot-cache (update-library-snapshot-cache snapshot-cache workspace lib-results)
          snapshot (combine-snapshots (list* (make-builtins-snapshot workspace)
                                             (make-directory-snapshot workspace project-directory)
                                             (make-debugger-snapshot workspace)
                                             (make-library-snapshots new-library-snapshot-cache lib-results)))]
      (add-gltf-resources workspace snapshot new-library-snapshot-cache))))

(defn- resource-status [snapshot path]
  (get-in snapshot [:status-map path]))

(defn diff [old-snapshot new-snapshot]
  (let [old-map (make-resource-map old-snapshot)
        new-map (make-resource-map new-snapshot)
        old-paths (set (keys old-map))
        new-paths (set (keys new-map))
        common-paths (set/intersection new-paths old-paths)
        changed-paths (filterv #(not= (resource-status old-snapshot %)
                                      (resource-status new-snapshot %))
                               common-paths)

        {changed-from-folder-to-file :file
         changed-from-file-to-folder :folder}
        (coll/reduce->
          changed-paths
          {:file (sorted-set-by coll/descending-order)
           :folder (sorted-set-by coll/descending-order)}
          (fn [acc path]
            (let [old-source-type (resource/source-type (get old-map path))
                  new-source-type (resource/source-type (get new-map path))]
              (cond-> acc
                      (not= old-source-type new-source-type)
                      (update new-source-type conj path)))))

        added-paths (into changed-from-folder-to-file (set/difference new-paths old-paths))
        removed-paths (into changed-from-file-to-folder (set/difference old-paths new-paths))
        changed-paths (coll/into-> changed-paths (sorted-set-by coll/descending-order)
                        (remove changed-from-folder-to-file)
                        (remove changed-from-file-to-folder))
        added (mapv new-map added-paths)
        removed (mapv old-map removed-paths)
        changed (mapv new-map changed-paths)]

    (assert (empty? (set/intersection (set added) (set removed))))
    (assert (empty? (set/intersection (set added) (set changed))))
    (assert (empty? (set/intersection (set removed) (set changed))))
    {:added added
     :removed removed
     :changed changed}))

(defn empty-diff? [diff]
  (not (or (seq (:added diff))
           (seq (:removed diff))
           (seq (:changed diff)))))
