(ns editor.pipeline
  (:require
   [editor.resource :as resource]
   [editor.protobuf :as protobuf]
   [editor.workspace :as workspace]))

(defn- resolve-resource-paths
  [pb dep-resources resource-props]
  (reduce (fn [m [prop resource]]
            (assoc m prop (resource/proj-path (get dep-resources resource))))
          pb
          resource-props))

(defn- make-resource-props
  [dep-build-targets m resource-keys]
  (let [deps-by-source (into {} (map #(let [res (:resource %)] [(:resource res) res]) dep-build-targets))]
    (map (fn [prop]
           [prop (-> m prop deps-by-source)])
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
  ([]
   (make-build-cache {}))
  ([init]
   (atom init)))

(defn- prune-build-cache!
  [cache build-targets-by-key]
  (reset! cache (into {} (filter (fn [[resource result]] (contains? build-targets-by-key (:key result))) @cache))))

(defn build-target-cached
  [basis target build-targets-by-key build-cache]
  (let [{:keys [resource key]} target]
    (or (when-let [cached-result (get @build-cache resource)]
          (and (= key (:key cached-result)) cached-result))
        (let [result (-> (build-target basis target build-targets-by-key)
                         (assoc :key key))]
          (swap! build-cache assoc resource (assoc result :cached true))
          result))))

(defn build
  ([basis build-targets build-cache]
   (build basis build-targets build-cache mapv))
  ([basis build-targets build-cache mapv-fn]
   (let [build-targets-by-key (make-build-targets-by-key build-targets)]
     (prune-build-cache! build-cache build-targets-by-key)
     (mapv-fn (fn [[key build-target]]
                (build-target-cached basis build-target build-targets-by-key build-cache))
              build-targets-by-key))))
