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

(defn- resolve-resource-paths
  [pb dep-resources resource-props]
  (reduce (fn [m [path resource]]
            (assoc-in m path (resource/proj-path (get dep-resources resource))))
          pb
          resource-props))

(defn- make-resource-props
  [dep-build-targets m resource-keys]
  (let [deps-by-source (into {}
                             (map (fn [build-target]
                                    (let [build-resource (:resource build-target)
                                          source-resource (:resource build-resource)]
                                      (when-not (and (satisfies? resource/Resource build-resource)
                                                     (satisfies? resource/Resource source-resource))
                                        (throw (ex-info "dep-build-targets contains an invalid build target"
                                                        {:build-target build-target
                                                         :build-resource build-resource
                                                         :source-resource source-resource})))
                                      [source-resource build-resource])))
                             dep-build-targets)]
    (letfn [(fill-from-path [x path path-index]
              (let [end (= path-index (count path))]
                (if end
                  (when x
                    (when-not (satisfies? resource/Resource x)
                      (throw (ex-info "value for resource path in m is not a Resource"
                                      {:path path
                                       :value x
                                       :resource-keys resource-keys})))
                    (if-let [build-resource (deps-by-source x)]
                      [(pair path build-resource)]
                      (throw (ex-info "deps-by-source is missing a referenced source-resource"
                                      {:path path
                                       :deps-by-source deps-by-source
                                       :source-resource x}))))
                  (let [k (path path-index)
                        v (x k)]
                    (if (vector? v)
                      (->Eduction
                        (mapcat (fn [i]
                                  (let [next-path-index (inc path-index)
                                        vec-path (-> path
                                                     (subvec 0 next-path-index)
                                                     (conj i)
                                                     (into (subvec path next-path-index)))]
                                    (fill-from-path v vec-path next-path-index))))
                        (range (count v)))
                      (fill-from-path v path (inc path-index)))))))]
      (into []
            (mapcat (fn [resource-key]
                      (let [path (if (vector? resource-key) resource-key [resource-key])]
                        (fill-from-path m path 0))))
            resource-keys))))

(defn- build-protobuf
  [resource dep-resources user-data]
  (let [{:keys [pb-class pb-msg resource-keys]} user-data
        pb-msg (resolve-resource-paths pb-msg dep-resources resource-keys)]
    {:resource resource
     :content (protobuf/map->bytes pb-class pb-msg)}))

(defn make-protobuf-build-target
  [resource dep-build-targets pb-class pb-msg pb-resource-fields]
  (let [dep-build-targets (flatten dep-build-targets)
        resource-keys (make-resource-props dep-build-targets pb-msg pb-resource-fields)]
    (bt/with-content-hash
      {:resource (workspace/make-build-resource resource)
       :build-fn build-protobuf
       :user-data {:pb-class pb-class
                   :pb-msg pb-msg
                   :resource-keys resource-keys}
       :deps dep-build-targets})))

;;--------------------------------------------------------------------

(defn- make-build-targets-by-content-hash
  [flat-build-targets]
  (coll/pair-map-by :content-hash flat-build-targets))

(defn- make-dep-resources
  [deps build-targets-by-content-hash]
  (into {}
        (map (fn [{:keys [content-hash resource] :as _build-target}]
               (assert (bt/content-hash? content-hash))
               [resource (:resource (get build-targets-by-content-hash content-hash))]))
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
  [flat-build-targets build-dir old-artifact-map render-progress!]
  (let [build-targets-by-content-hash (make-build-targets-by-content-hash flat-build-targets)
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
