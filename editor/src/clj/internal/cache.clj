(ns internal.cache
  (:require [clojure.core.cache :as cc]))

(set! *warn-on-reflection* true)

(def ^:dynamic *cache-debug* nil)

;; ----------------------------------------
;; Null cache for testing
;; ----------------------------------------
(cc/defcache NullCache []
  cc/CacheProtocol
  (lookup [_ item] nil)
  (lookup [_ item not-found] not-found)
  (has?   [_ item] false)
  (hit    [this item] this)
  (miss   [this item result] this)
  (evict  [this key] this)
  (seed   [_ base] base))

(defn- null-cache [] (NullCache.))

;; ----------------------------------------
;; Faster LRU cache, (contains? cache key) rather than (= v ::miss) nonsense
;; ----------------------------------------

(defn- build-leastness-queue
  [base limit start-at]
  (into (clojure.data.priority-map/priority-map)
        (concat (take (- limit (count base)) (for [k (range (- limit) 0)] [k k]))
                (for [[k _] base] [k start-at]))))

(cc/defcache LRUCache [cache lru tick limit]
  cc/CacheProtocol
  (lookup [_ item]
    (get cache item))
  (lookup [_ item not-found]
    (get cache item not-found))
  (has? [_ item]
    (contains? cache item))
  (hit [_ item]
    (let [tick+ (inc tick)]
      (LRUCache. cache
                 (if (contains? cache item)
                   (assoc lru item tick+)
                   lru)
                 tick+
                 limit)))
  (miss [_ item result]
    (let [tick+ (inc tick)]
      (if (>= (count lru) limit)
        (let [k (if (contains? lru item)
                  item
                  (first (peek lru))) ;; minimum-key, maybe evict case
              c (-> cache (dissoc k) (assoc item result))
              l (-> lru (dissoc k) (assoc item tick+))]
          (LRUCache. c l tick+ limit))
        (LRUCache. (assoc cache item result)  ;; no change case
                   (assoc lru item tick+)
                   tick+
                   limit))))
  (evict [this key]
    (if (contains? cache key)
      (LRUCache. (dissoc cache key)
                 (dissoc lru key)
                 (inc tick)
                 limit)
      this))
  (seed [_ base]
    (LRUCache. base
               (build-leastness-queue base limit 0)
               0
               limit))
  Object
  (toString [_]
    (str cache \, \space lru \, \space tick \, \space limit)))

(defn lru-cache-factory
  "Returns an LRU cache with the cache and usage-table initialied to `base` --
   each entry is initialized with the same usage value.
   This function takes an optional `:threshold` argument that defines the maximum number
   of elements in the cache before the LRU semantics apply (default is 32)."
  [base & {threshold :threshold :or {threshold 32}}]
  {:pre [(number? threshold) (< 0 threshold)
         (map? base)]}
  (clojure.core.cache/seed (LRUCache. {} (clojure.data.priority-map/priority-map) 0 threshold) base))

;; ----------------------------------------
;; Mutators
;; ----------------------------------------
(defn- encache
  [c kvs]
  (if-let [kv (first kvs)]
    (recur (cc/miss c (first kv) (second kv)) (next kvs))
    c))

(defn- hits
  [c ks]
  (if-let [k (first ks)]
    (recur (cc/hit c k) (next ks))
    c))

(defn- evict
  [c ks]
  (if-let [k (first ks)]
    (recur (cc/evict c k) (next ks))
    c))

;; ----------------------------------------
;; Interface
;; ----------------------------------------
(def default-cache-limit 1000)

(defn cache-subsystem
  ([]
   (cache-subsystem default-cache-limit))
  ([limit]
   (atom
    (if (zero? limit)
      (null-cache)
      (lru-cache-factory {} :threshold limit)))))

(defn cache-snapshot
  [cache]
  @cache)

(defn cache-hit
  [cache ks]
  (swap! cache hits ks))

(defn cache-encache
  [cache kvs]
  (swap! cache encache kvs))

(defn cache-invalidate
  [cache ks]
  (swap! cache evict ks))
