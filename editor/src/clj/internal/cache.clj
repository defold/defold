(ns internal.cache
  (:require [clojure.core.cache :as cc]))

(set! *warn-on-reflection* true)

(def ^:dynamic *cache-debug* nil)

;; ----------------------------------------
;; Null cache for testing
;; ----------------------------------------
(cc/defcache NullCache [cache]
  cc/CacheProtocol
  (lookup [_ item])
  (lookup [_ item not-found] not-found)
  (has? [_ item] false)
  (hit [this item] this)
  (miss [this item result] this)
  (evict [this key] this)
  (seed [this base] base))

(defn- null-cache [] (NullCache. {}))

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

(defn make-cache
  ([]
    (make-cache default-cache-limit))
  ([limit]
    (if (zero? limit)
      (null-cache)
      (cc/lru-cache-factory {} :threshold limit))))

(defn cache-hit
  [cache ks]
  (hits cache ks))

(defn cache-encache
  [cache kvs]
  (encache cache kvs))

(defn cache-invalidate
  [cache ks]
  (evict cache ks))
