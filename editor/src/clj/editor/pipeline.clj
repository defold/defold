(ns editor.pipeline
  (:require
   [clojure.java.io :as io]
   [dynamo.graph :as g]
   [editor.fs :as fs]
   [editor.resource :as resource]
   [editor.progress :as progress]
   [editor.protobuf :as protobuf]
   [editor.workspace :as workspace]
   [util.digest :as digest])
  (:import
   (clojure.lang ExceptionInfo)
   (com.google.protobuf Message)
   (java.io File)))

(set! *warn-on-reflection* true)

(defn- build-pb-map
  "A build-fn that produces a binary protobuf resource from a map.
  The pb-msg map is expected to specify build resources for resource fields.
  These will be resolved into the fused build resources from dep-resources,
  which maps original build resources to fused build resources."
  [resource dep-resources {:keys [^Class pb-class pb-msg pb-resource-paths] :as _user-data}]
  (let [build-resource->fused-build-resource-path (fn [build-resource]
                                                    (when (some? build-resource)
                                                      (if-some [fused-build-resource (get dep-resources build-resource)]
                                                        (resource/proj-path fused-build-resource)
                                                        (throw (ex-info (format "error building %s %s - unable to resolve fused build resource from referenced %s"
                                                                                (.getName pb-class)
                                                                                (pr-str resource)
                                                                                (pr-str build-resource))
                                                                        {:resource-reference build-resource
                                                                         :referencing-resource resource
                                                                         :referencing-pb-class-name (.getName pb-class)})))))
        pb-msg-with-fused-build-resource-paths (protobuf/update-values-at-paths pb-msg pb-resource-paths build-resource->fused-build-resource-path)]
    {:resource resource
     :content (protobuf/map->bytes pb-class pb-msg-with-fused-build-resource-paths)}))

(defn- resource-reference->build-resource
  "Resolves a resource reference into a build resource. Resource references
  can be either source resources, build resources, or string paths."
  [built-resource? source-resource->build-resource proj-path->build-resource resource-reference]
  (cond
    ;; Empty string or nil.
    (or (nil? resource-reference) (= "" resource-reference))
    nil

    ;; Build resource.
    ;; If this fails, it is likely because dep-build-targets does not contain a referenced resource.
    (workspace/build-resource? resource-reference)
    (if (built-resource? resource-reference)
      resource-reference
      (throw (ex-info (str "referenced build resource not found among deps " (pr-str resource-reference))
                      {:resource-reference resource-reference})))

    ;; Source resource.
    ;; If this fails, it is likely because dep-build-targets does not contain a referenced resource.
    (resource/resource? resource-reference)
    (or (source-resource->build-resource resource-reference)
        (throw (ex-info (str "unable to resolve build resource from value " (pr-str resource-reference))
                        {:resource-reference resource-reference})))

    ;; Non-empty string, presumably a proj-path.
    ;; If this fails, it is likely because dep-build-targets does not contain a referenced resource.
    (string? resource-reference)
    (or (proj-path->build-resource resource-reference)
        (throw (ex-info (str "unable to resolve build resource from value " (pr-str resource-reference))
                        {:resource-reference resource-reference})))

    ;; Unsupported resource reference value.
    ;; This is likely due to an invalid value for a resource field in the pb-msg.
    :else
    (throw (ex-info (str "unsupported resource reference value " (pr-str resource-reference))
                    {:resource-reference resource-reference}))))

(defn- flatten-and-validate-dep-build-targets
  "Flattens dep-build-targets and ensures all build targets are well-formed."
  [dep-build-targets]
  (into []
        (map (fn [build-target]
               (let [build-resource (:resource build-target)
                     source-resource (:resource build-resource)]
                 (if (and (workspace/build-resource? build-resource)
                          (resource/resource? source-resource))
                   build-target
                   (throw (ex-info "dep-build-targets contains a malformed build target"
                                   {:build-target build-target}))))))
        (flatten dep-build-targets)))

(defn make-pb-map-build-target
  "Creates a build target that produces a binary protobuf resource from a map.
  Automatically resolves resource references into build resources. The resource
  fields are queried from the supplied pb-class using reflection. Resource
  references in the pb-msg map can be either source resources, build resources,
  or proj-path strings. Throws an exception if dep-build-targets is missing a
  referenced resource."
  [node-id source-resource dep-build-targets ^Class pb-class pb-msg]
  (assert (or (nil? node-id) (integer? node-id)))
  (assert (resource/resource? source-resource))
  (assert (or (nil? dep-build-targets) (vector? dep-build-targets)))
  (assert (and (class? pb-class) (.isAssignableFrom Message pb-class)))
  (assert (and (map? pb-msg) (every? keyword? (keys pb-msg))))
  (let [pb-resource-paths (protobuf/resource-field-paths pb-class)
        dep-build-targets (flatten-and-validate-dep-build-targets dep-build-targets)
        built-resource? (into #{} (map :resource) dep-build-targets)
        source-resource->build-resource (into {} (map (juxt (comp :resource :resource) :resource)) dep-build-targets)
        proj-path->build-resource (into {} (mapcat #(map vector (keep resource/proj-path %) (repeat (val %)))) source-resource->build-resource)
        resource-reference->build-resource (partial resource-reference->build-resource built-resource? source-resource->build-resource proj-path->build-resource)]
    (try
      (let [pb-msg-with-build-resources (protobuf/update-values-at-paths pb-msg pb-resource-paths resource-reference->build-resource)]
        (cond-> {:resource (workspace/make-build-resource source-resource)
                 :build-fn build-pb-map
                 :user-data {:pb-class pb-class
                             :pb-msg pb-msg-with-build-resources
                             :pb-resource-paths pb-resource-paths}}

                (some? node-id) (assoc :node-id node-id)
                (seq dep-build-targets) (assoc :deps dep-build-targets)))
      (catch ExceptionInfo e
        ;; Decorate and rethrow.
        (throw (ex-info (format "error producing build target %s %s - %s"
                                (.getName pb-class)
                                (pr-str source-resource)
                                (.getMessage e))
                        (merge (ex-data e)
                               {:referencing-resource source-resource
                                :referencing-pb-class-name (.getName pb-class)})))))))

;;--------------------------------------------------------------------

(defn- target-key
  [target]
  [(:resource (:resource target))
   (:build-fn target)
   (:user-data target)])

(defn- build-targets-by-key
  [build-targets]
  (into {} (map #(let [key (target-key %)] [key (assoc % :key key)])) build-targets))

(defn- build-targets-deep [build-targets]
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
       build-targets-deep
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

(defn- workspace->artifacts [workspace]
  (or (g/user-data workspace ::artifacts) {}))

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
          :etag (digest/sha1->hex content))))))

(defn- prune-build-dir! [workspace build-targets-by-key]
  (let [targets (into #{} (map (fn [[_ target]] (io/as-file (:resource target)))) build-targets-by-key)
        dir (io/as-file (workspace/build-path workspace))]
    (fs/create-directories! dir)
    (doseq [^File f (file-seq dir)]
      (when (and (not (.isDirectory f)) (not (contains? targets f)))
        (fs/delete! f)))))

(defn build!
  ([workspace build-targets]
   (build! workspace build-targets progress/null-render-progress!))
  ([workspace build-targets render-progress!]
   (let [build-targets-by-key (make-build-targets-by-key build-targets)
         artifacts (-> (workspace->artifacts workspace)
                       (prune-artifacts build-targets-by-key))
         progress  (atom (progress/make "" (count build-targets-by-key)))]
     (prune-build-dir! workspace build-targets-by-key)
     (let [results (into []
                         (map (fn [[key {:keys [resource] :as build-target}]]
                                (swap! progress #(-> %
                                                     (progress/advance)
                                                     (progress/with-message (str "Writing " (resource/proj-path resource)))))
                                (or (when-let [artifact (get artifacts resource)]
                                      (and (valid? artifact) artifact))
                                    (render-progress! @progress)
                                    (let [{:keys [resource deps build-fn user-data]} build-target
                                          dep-resources (make-dep-resources deps build-targets-by-key)
                                          result (-> (build-fn resource dep-resources user-data)
                                                     (to-disk! key))]
                                      result))))
                         build-targets-by-key)
           new-artifacts (into {} (map (fn [a] [(:resource a) a])) results)
           etags (into {} (map (fn [a] [(resource/proj-path (:resource a)) (:etag a)])) results)]
       (g/user-data! workspace ::artifacts new-artifacts)
       (g/user-data! workspace ::etags etags)
       results))))

(defn reset-cache! [workspace]
  (g/user-data! workspace ::artifacts nil)
  (g/user-data! workspace ::etags nil))

(defn etags [workspace]
  (g/user-data workspace ::etags))

(defn etag [workspace path]
  (get (etags workspace) path))
