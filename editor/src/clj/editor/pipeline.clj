(ns editor.pipeline
  (:require
   [clojure.java.io :as io]
   [editor.fs :as fs]
   [editor.resource :as resource]
   [editor.progress :as progress]
   [editor.protobuf :as protobuf]
   [editor.workspace :as workspace]
   [dynamo.graph :as g]
   [util.digest :as digest])
  (:import
   (java.io File)
   (java.util UUID)))

(set! *warn-on-reflection* true)

(defn- resolve-resource-paths
  [pb dep-resources resource-props]
  (reduce (fn [m [prop resource]]
            (assoc m prop (resource/proj-path (get dep-resources resource))))
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
    (keep (fn [resource-key]
            (when-let [source-resource (m resource-key)]
              (when-not (satisfies? resource/Resource source-resource)
                (throw (ex-info "value for resource key in m is not a Resource"
                                {:key resource-key
                                 :value source-resource
                                 :resource-keys resource-keys})))
              (if-let [build-resource (deps-by-source source-resource)]
                [resource-key build-resource]
                (throw (ex-info "deps-by-source is missing a referenced source-resource"
                                {:key resource-key
                                 :deps-by-source deps-by-source
                                 :source-resource source-resource})))))
          resource-keys)))

(defn- build-protobuf
  [resource dep-resources user-data]
  (let [{:keys [pb-class pb-msg resource-keys]} user-data
        pb-msg (resolve-resource-paths pb-msg dep-resources resource-keys)]
    {:resource resource
     :content (protobuf/map->bytes pb-class pb-msg)}))

(defn make-protobuf-build-target
  [resource dep-build-targets pb-class pb-msg pb-resource-fields]
  (let [dep-build-targets (flatten dep-build-targets)
        resource-keys     (make-resource-props dep-build-targets pb-msg pb-resource-fields)]
    {:resource  (workspace/make-build-resource resource)
     :build-fn  build-protobuf
     :user-data {:pb-class      pb-class
                 :pb-msg        (reduce dissoc pb-msg resource-keys)
                 :resource-keys resource-keys}
     :deps      dep-build-targets}))

;;--------------------------------------------------------------------

(defn- target-key
  [target]
  [(:resource (:resource target))
   (:build-fn target)
   (:user-data target)])

(defn- build-targets-by-key
  [build-targets]
  (into {} (map #(let [key (target-key %)] [key (assoc % :key key)])) build-targets))

(defn- flatten-build-targets
  "Breadth first traversal / collection of build-targets and their child :deps,
  skipping seen targets identified by target-key."
  [build-targets]
  (loop [targets build-targets
         queue []
         seen #{}
         result []]
    (if-let [t (first targets)]
      (let [key (target-key t)]
        (if (contains? seen key)
          (recur (rest targets) queue seen result)
          (recur (rest targets) (conj queue (flatten (:deps t))) (conj seen key) (conj result t))))
      (if-let [targets (first queue)]
        (recur targets (rest queue) seen result)
        result))))

(defn- make-build-targets-by-key
  [build-targets]
  (->> build-targets
       flatten-build-targets
       build-targets-by-key))

(defn- make-dep-resources
  [deps build-targets-by-key]
  (into {}
        (map (fn [{:keys [resource] :as build-target}]
               (let [key (target-key build-target)]
                 [resource (:resource (get build-targets-by-key key))])))
        (flatten deps)))

(defn prune-artifacts [artifacts build-targets-by-key]
  (into {}
        (filter (fn [[resource result]] (contains? build-targets-by-key (:key result))))
        artifacts))

(defn- valid? [artifact]
  (when-let [^File f (io/as-file (:resource artifact))]
    (let [{:keys [mtime size]} artifact]
      (and (.exists f) (= mtime (.lastModified f)) (= size (.length f))))))

(defn- to-disk! [artifact key]
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
          :key key
          :mtime mtime
          :size size
          :etag (digest/sha1-hex content))))))

(defn- prune-build-dir! [build-dir build-targets-by-key]
  (let [targets (into #{} (map (fn [[_ target]] (io/as-file (:resource target)))) build-targets-by-key)]
    (fs/create-directories! build-dir)
    (doseq [^File f (file-seq build-dir)]
      (when (and (not (.isDirectory f)) (not (contains? targets f)))
        (fs/delete! f)))))

(defn build!
  [build-targets build-dir old-artifact-map render-progress!]
  (let [build-targets-by-key (make-build-targets-by-key build-targets)
        pruned-old-artifacts (prune-artifacts old-artifact-map build-targets-by-key)
        progress (atom (progress/make "" (count build-targets-by-key)))]
    (prune-build-dir! build-dir build-targets-by-key)
    (let [results (into []
                        (map (fn [[key {:keys [node-id resource] :as build-target}]]
                               (let [cached-artifact (when-let [artifact (get pruned-old-artifacts resource)]
                                                       (and (valid? artifact) artifact))
                                     message (str "Building " (resource/proj-path resource))]
                                 (render-progress! (swap! progress
                                                          #(-> %
                                                               (progress/advance)
                                                               (progress/with-message message))))
                                 (or cached-artifact
                                     (let [{:keys [resource deps build-fn user-data]} build-target
                                           dep-resources (make-dep-resources deps build-targets-by-key)
                                           result (build-fn resource dep-resources user-data)]
                                       (if (g/error? result)
                                         (assoc result :_node-id node-id)
                                         (to-disk! result key)))))))
                        build-targets-by-key)
          {successful-results false error-results true} (group-by #(boolean (g/error? %)) results)
          new-artifact-map (into {} (map (fn [a] [(:resource a) a])) successful-results)
          etags (into {} (map (fn [a] [(resource/proj-path (:resource a)) (:etag a)])) successful-results)]
      (cond-> {:artifacts successful-results
               :artifact-map new-artifact-map
               :etags etags}
        (seq error-results)
        (assoc :error (g/error-aggregate error-results))))))
