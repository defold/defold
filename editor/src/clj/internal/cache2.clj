(ns internal.cache2
  (:import [com.github.benmanes.caffeine.cache Caffeine Cache]))

(defn ^Cache make [limit]
  (-> (Caffeine/newBuilder)
      .recordStats
      (.maximumSize limit)
      .build))

(defn cache? [x]
  (instance? Cache x))

(defn lookup [^Cache cache key]
  (let [ret (.getIfPresent cache key)]
    (if (identical? ret ::nil)
      nil
      ret)))

(defn put! [^Cache cache key val]
  (let [val (if (nil? val) ::nil val)]
    (.put cache key val)
    cache))

(defn clear! [^Cache cache]
  (.invalidateAll cache)
  cache)

(comment (.stats ^Cache (:cache @dynamo.graph/*the-system*)))