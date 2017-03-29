(ns editor.pipeline
  (:require
   [clojure.java.io :as io]
   [editor.resource :as resource]
   [editor.protobuf :as protobuf]
   [editor.util :as util]
   [editor.workspace :as workspace])
  (:import
   (java.nio.file Files)
   (java.nio.file.attribute FileAttribute)
   (java.util UUID)))

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
  [node-id basis resource dep-resources user-data]
  (let [{:keys [pb-class pb-msg resource-keys]} user-data
        pb-msg (resolve-resource-paths pb-msg dep-resources resource-keys)]
    {:resource resource
     :content (protobuf/map->bytes pb-class pb-msg)}))

(defn make-protobuf-build-target
  [_node-id resource dep-build-targets pb-class pb-msg pb-resource-fields]
  (let [dep-build-targets (flatten dep-build-targets)
        resource-keys     (make-resource-props dep-build-targets pb-msg pb-resource-fields)]
    {:node-id   _node-id
     :resource  (workspace/make-build-resource resource)
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

(defn- build-target
  [basis target build-targets-by-key]
  (let [{:keys [node-id resource deps build-fn user-data]} target
        dep-resources (make-dep-resources deps build-targets-by-key)]
    (build-fn node-id basis resource dep-resources user-data)))


(defn make-build-cache
  []
  {:dir     (doto (.toFile (Files/createTempDirectory "defold-build-cache" (make-array FileAttribute 0)))
              (util/delete-on-exit!))
   :entries (atom {})})

(defn cache!
  [build-cache resource key result]
  (let [id (str (UUID/randomUUID))
        content (:content result)
        cache-file (io/file (:dir build-cache) id)
        cache-value (-> result
                        (dissoc :content)
                        (assoc :id id :key key :cached true))]
    (io/copy content cache-file)
    (swap! (:entries build-cache) assoc resource cache-value)
    (assoc result :key key)))

(defn lookup
  [build-cache resource key]
  (when-let [entry (get @(:entries build-cache) resource)]
    (when (= key (:key entry))
      (let [file (io/file (:dir build-cache) (:id entry))]
        (-> entry
            (dissoc :id)
            (assoc :content file))))))

(defn- prune-build-cache!
  [{:keys [dir entries] :as build-cache} build-targets-by-key]
  (swap! entries #(reduce-kv (fn [ret resource {:keys [key] :as result}]
                               (if (contains? build-targets-by-key key)
                                 (assoc ret resource result)
                                 ret))
                             {}
                             %))
  build-cache)

(defn build-target-cached
  [basis target build-targets-by-key build-cache]
  (let [{:keys [resource key]} target]
    (or (lookup build-cache resource key)
        (let [result (build-target basis target build-targets-by-key)]
          (cache! build-cache resource key result)))))

(defn build
  ([basis build-targets build-cache]
   (build basis build-targets build-cache mapv))
  ([basis build-targets build-cache mapv-fn]
   (let [build-targets-by-key (make-build-targets-by-key build-targets)]
     (prune-build-cache! build-cache build-targets-by-key)
     (mapv-fn (fn [[key build-target]]
                (build-target-cached basis build-target build-targets-by-key build-cache))
              build-targets-by-key))))
