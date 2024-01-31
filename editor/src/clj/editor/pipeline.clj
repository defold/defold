;; Copyright 2020-2024 The Defold Foundation
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

(ns editor.pipeline
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.build-target :as bt]
            [editor.fs :as fs]
            [editor.progress :as progress]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.workspace :as workspace]
            [util.coll :as coll :refer [pair]]
            [util.digest :as digest])
  (:import [java.io File]))

(set! *warn-on-reflection* true)

(defn- replace-build-resources-with-fused-build-resource-paths
  [pb-map dep-resources build-resource-field-value-paths]
  (reduce (fn [pb-map [field-path build-resource]]
            (let [fused-build-resource
                  (when build-resource
                    (or (get dep-resources build-resource)
                        (throw (ex-info "dep-resources is missing a referenced build-resource."
                                        {:build-resource build-resource
                                         :dep-resources dep-resources}))))]
              (coll/assoc-in-ex pb-map field-path (resource/resource->proj-path fused-build-resource))))
          pb-map
          build-resource-field-value-paths))

(defn- make-build-resource-field-value-paths
  [^Class pb-class pb-map dep-build-targets]
  (let [resource-field-path-specs (protobuf/resource-field-path-specs pb-class)
        pb-map->resource-field-value-paths (protobuf/get-field-value-paths-fn resource-field-path-specs)
        resource-field-value-paths (pb-map->resource-field-value-paths pb-map)

        resource-field-value->build-resource
        (into {nil nil
               "" nil}
              (mapcat (fn [build-target]
                        (let [build-resource (:resource build-target)
                              source-resource (:resource build-resource)]
                          (when-not (and (satisfies? resource/Resource build-resource)
                                         (satisfies? resource/Resource source-resource))
                            (throw (ex-info "dep-build-targets contains an invalid build target."
                                            {:build-target build-target
                                             :build-resource build-resource
                                             :source-resource source-resource})))
                          [(pair build-resource build-resource)
                           (pair (resource/proj-path build-resource) build-resource)
                           (pair source-resource build-resource)
                           (pair (resource/proj-path source-resource) build-resource)])))
              dep-build-targets)]

    (mapv (fn [[field-path field-value]]
            (when-not (or (nil? field-value)
                          (string? field-value)
                          (satisfies? resource/Resource field-value))
              (throw (ex-info "value for resource field in pb-map is not a Resource or proj-path."
                              {:field-value field-value
                               :field-path field-path
                               :resource-field-path-specs resource-field-path-specs})))
            (if-let [build-resource (resource-field-value->build-resource field-value)]
              (pair field-path build-resource)
              (throw (ex-info "deps-by-source is missing a referenced source-resource. Ensure it is among the dep-build-targets."
                              {:field-value field-value
                               :field-path field-path
                               :deps-by-source resource-field-value->build-resource
                               :source-resource field-value}))))
          resource-field-value-paths)))

(defn- build-protobuf
  [resource dep-resources user-data]
  (let [{:keys [pb-class pb-map build-resource-field-value-paths]} user-data
        pb-map (replace-build-resources-with-fused-build-resource-paths pb-map dep-resources build-resource-field-value-paths)]
    {:resource resource
     :content (protobuf/map->bytes pb-class pb-map)}))

(defn make-protobuf-build-target
  ([node-id source-resource pb-class pb-map]
   (make-protobuf-build-target node-id source-resource pb-class pb-map nil))
  ([node-id source-resource pb-class pb-map dep-build-targets]
   {:pre [(g/node-id? node-id)
          (protobuf/pb-class? pb-class)
          (map? pb-map)]}
   (let [dep-build-targets (some-> dep-build-targets coll/not-empty flatten vec)
         build-resource-field-value-paths (make-build-resource-field-value-paths pb-class pb-map dep-build-targets)]
     (bt/with-content-hash
       (cond-> {:node-id node-id
                :resource (workspace/make-build-resource source-resource)
                :build-fn build-protobuf
                :user-data {:pb-class pb-class
                            :pb-map pb-map
                            :build-resource-field-value-paths build-resource-field-value-paths}}

               dep-build-targets
               (assoc :deps dep-build-targets))))))

;;--------------------------------------------------------------------

(defn flatten-build-targets
  "Breadth first traversal / collection of build-targets and their child :deps,
  skipping seen targets identified by the :content-hash of each build-target."
  ([build-targets]
   (flatten-build-targets build-targets #{}))
  ([build-targets seen-content-hashes]
   (assert (set? seen-content-hashes))
   (loop [targets build-targets
          queue []
          seen seen-content-hashes
          result (transient [])]
     (if-some [target (first targets)]
       (let [content-hash (:content-hash target)]
         (assert (bt/content-hash? content-hash)
                 (str "Build target has invalid content-hash: "
                      (resource/resource->proj-path (:resource target))))
         (if (contains? seen content-hash)
           (recur (rest targets)
                  queue
                  seen
                  result)
           (recur (rest targets)
                  (conj queue (flatten (:deps target)))
                  (conj seen content-hash)
                  (conj! result target))))
       (if-some [targets (first queue)]
         (recur targets
                (rest queue)
                seen
                result)
         (persistent! result))))))

(defn- make-build-targets-by-content-hash
  [build-targets]
  (coll/pair-map-by :content-hash (flatten-build-targets build-targets)))

(defn- make-dep-resources
  [deps build-targets-by-content-hash]
  (into {}
        (map (fn [build-target]
               (let [content-hash (:content-hash build-target)
                     build-resource (:resource build-target)
                     fused-build-resource (:resource (get build-targets-by-content-hash content-hash))]
                 (assert (bt/content-hash? content-hash))
                 (assert (workspace/build-resource? build-resource))
                 (assert (workspace/build-resource? fused-build-resource))
                 (pair build-resource fused-build-resource))))
        (flatten deps)))

(defn prune-artifact-map [artifact-map build-targets-by-content-hash]
  (into {}
        (filter (fn [[_resource-path result]]
                  (contains? build-targets-by-content-hash
                             (:content-hash result))))
        artifact-map))

(defn- valid? [resource artifact]
  (when-some [^File f (io/as-file resource)]
    (let [{:keys [mtime size]} artifact]
      (and (.exists f) (= mtime (.lastModified f)) (= size (.length f))))))

(defn- to-disk! [artifact content-hash]
  (assert (some? (:content artifact)))
  (fs/create-parent-directories! (io/as-file (:resource artifact)))
  (let [^bytes content (:content artifact)]
    (with-open [out (io/output-stream (:resource artifact))]
      (.write out content))
    (let [^File target-f (io/as-file (:resource artifact))
          mtime (.lastModified target-f)
          size (.length target-f)]
      (-> artifact
          (dissoc :content)
          (assoc
            :content-hash content-hash
            :mtime mtime
            :size size
            :etag (digest/sha1-hex content))))))

(defn- prune-build-dir! [build-dir build-targets-by-content-hash]
  (let [targets (into #{}
                      (map (fn [[_ target]]
                             (io/as-file (:resource target))))
                      build-targets-by-content-hash)]
    (fs/create-directories! build-dir)
    (doseq [^File f (file-seq build-dir)]
      (when (and (not (.isDirectory f)) (not (contains? targets f)))
        (fs/delete! f)))))

(defn- expensive? [build-target]
  (contains? #{"fontc"} (resource/ext (:resource build-target))))

(defn- batched-pmap [f batches]
  (->> batches
       (pmap (fn [batch] (doall (map f batch))))
       (reduce concat)
       doall))

(def ^:private cheap-batch-size 500)
(def ^:private expensive-batch-size 5)

(defn build!
  [build-targets build-dir old-artifact-map render-progress!]
  (let [build-targets-by-content-hash (make-build-targets-by-content-hash build-targets)
        pruned-old-artifact-map (prune-artifact-map old-artifact-map build-targets-by-content-hash)
        progress (atom (progress/make "" (count build-targets-by-content-hash)))]
    (prune-build-dir! build-dir build-targets-by-content-hash)
    (let [{cheap-build-targets false expensive-build-targets true} (group-by expensive? (vals build-targets-by-content-hash))
          build-target-batches (into (partition-all cheap-batch-size cheap-build-targets)
                                     (partition-all expensive-batch-size expensive-build-targets))
          results (batched-pmap
                    (fn [build-target]
                      (let [{:keys [content-hash node-id resource deps build-fn user-data]} build-target
                            resource-path (resource/proj-path resource)
                            cached-artifact (when-some [artifact (get pruned-old-artifact-map resource-path)]
                                              (when (valid? resource artifact)
                                                (assoc artifact :resource resource)))
                            message (str "Building " (resource/proj-path resource))]
                        (render-progress! (swap! progress progress/with-message message))
                        (let [result (or cached-artifact
                                         (let [dep-resources (make-dep-resources deps build-targets-by-content-hash)
                                               build-result (try
                                                              (build-fn resource dep-resources user-data)
                                                              (catch OutOfMemoryError error
                                                                (g/error-aggregate
                                                                  [(g/error-fatal
                                                                     (format "Failed to allocate memory while building '%s': %s."
                                                                             (resource/proj-path resource)
                                                                             (.getMessage error)))])))]
                                           ;; Error results are assumed to be error-aggregates.
                                           ;; We need to inject the node-id of the source build
                                           ;; target into the causes, since the build-fn will
                                           ;; not have access to the node-id.
                                           (if (g/error? build-result)
                                             (update build-result :causes (partial mapv #(assoc % :_node-id node-id)))
                                             (to-disk! build-result content-hash))))]
                          (render-progress! (swap! progress progress/advance))
                          result)))
                    build-target-batches)
          {successful-results false error-results true} (group-by #(boolean (g/error? %)) results)
          new-artifact-map (into {}
                                 (map (fn [artifact]
                                        [(resource/proj-path (:resource artifact))
                                         (dissoc artifact :resource)]))
                                 successful-results)
          etags (workspace/artifact-map->etags new-artifact-map)]
      (cond-> {:artifacts successful-results
               :artifact-map new-artifact-map
               :etags etags}
        (seq error-results)
        (assoc :error (g/error-aggregate error-results))))))
