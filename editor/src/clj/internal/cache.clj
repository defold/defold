;; Copyright 2020-2022 The Defold Foundation
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

(ns internal.cache
  (:require [clojure.core.cache :as cc]
            [internal.graph.types :as gt])
  (:import [clojure.core.cache BasicCache LRUCache]))

(set! *warn-on-reflection* true)

(def ^:dynamic *cache-debug* nil)

(defprotocol CacheExtensions
  (limit [this] "Returns the cache capacity, or -1 if unlimited.")
  (retained-count [this] "Returns the number of retained entries in the cache."))

(extend-protocol CacheExtensions
  BasicCache
  (limit [_this] -1)
  (retained-count [_this] 0)

  LRUCache
  (limit [this] (.limit ^LRUCache this))
  (retained-count [_this] 0))

;; ----------------------------------------
;; Null cache for testing / uncached queries
;; ----------------------------------------
(cc/defcache NullCache [cache]
  cc/CacheProtocol
  (lookup [_ item])
  (lookup [_ item not-found] not-found)
  (has? [_ item] false)
  (hit [this item] this)
  (miss [this item result] this)
  (evict [this key] this)
  (seed [this base] this)

  CacheExtensions
  (limit [_this] 0)
  (retained-count [_this] 0))

(def null-cache (NullCache. {}))

;; ----------------------------------------
;; Application-specialized LRU cache with the ability to selectively exclude
;; entries from the cache limit.
;; ----------------------------------------
(defn- occupied-lru-entry? [entry]
  (not (neg? (val entry))))

(cc/defcache RetainingLRUCache [cache lru tick limit retain?]
  cc/CacheProtocol
  (lookup [_ item]
    (get cache item))
  (lookup [_ item not-found]
    (get cache item not-found))
  (has? [_ item]
    (contains? cache item))
  (hit [this item]
    (if (or (not (contains? cache item))
            (retain? item))
      this
      (let [tick+ (inc tick)]
        (RetainingLRUCache. cache
                            (assoc lru item tick+)
                            tick+
                            limit
                            retain?))))
  (miss [_ item result]
    (let [tick+ (inc tick)
          retain (retain? item)]
      (if (and (not retain)
               (>= (count lru) limit))
        (let [k (if (contains? lru item)
                  item
                  (first (peek lru))) ; minimum-key, maybe evict case
              c (-> cache (dissoc k) (assoc item result))
              l (-> lru (dissoc k) (assoc item tick+))]
          (RetainingLRUCache. c l tick+ limit retain?))
        (RetainingLRUCache. (assoc cache item result) ; no change case
                            (if retain
                              lru
                              (assoc lru item tick+))
                            tick+
                            limit
                            retain?))))
  (evict [this item]
    (let [cache-without-item (dissoc cache item)]
      (if (identical? cache cache-without-item)
        this
        (RetainingLRUCache. cache-without-item
                            (dissoc lru item)
                            (inc tick)
                            limit
                            retain?))))
  (seed [_ base]
    (let [unretained-base (into {}
                                (remove (comp retain? key))
                                base)
          lru (#'cc/build-leastness-queue unretained-base limit 0)]
      (RetainingLRUCache. base
                          lru
                          0
                          limit
                          retain?)))

  CacheExtensions
  (limit [_this] limit)
  (retained-count [this]
    ;; Note: Likely slow, don't call excessively.
    (- (count this)
       (count (filter occupied-lru-entry? lru))))

  Object
  (toString [_]
    (str cache \, \space lru \, \space tick \, \space limit)))

(defn retaining-lru-cache-factory
  [base & {threshold :threshold retain? :retain?}]
  {:pre [(map? base)
         (number? threshold) (< 0 threshold)
         (ifn? retain?)]}
  (cc/seed (RetainingLRUCache. nil nil 0 threshold retain?) base))

;; ----------------------------------------
;; Mutators
;; ----------------------------------------
(defn- clear
  [cache]
  (cc/seed cache {}))

(defn- encache
  [cache kvs]
  (if-let [kv (first kvs)]
    (do (assert (gt/endpoint? (first kv)))
        (recur (cc/miss cache (first kv) (second kv)) (next kvs)))
    cache))

(defn- hits
  [cache ks]
  (if-let [k (first ks)]
    (do (assert (gt/endpoint? k))
        (recur (cc/hit cache k) (next ks)))
    cache))

(defn- evict
  [cache ks]
  (if-let [k (first ks)]
    (do (assert (gt/endpoint? k))
        (recur (cc/evict cache k) (next ks)))
    cache))

;; ----------------------------------------
;; Interface
;; ----------------------------------------
(defn cache? [value]
  (satisfies? cc/CacheProtocol value))

(defn unlimited-cache? [value]
  (instance? BasicCache value))

(defn make-cache [limit retain?]
  {:pre [(number? limit) (<= -1 limit)
         (or (nil? retain?) (ifn? retain?))]}
  (cond
    (= -1 limit) ; Unlimited cache.
    (cc/basic-cache-factory {})

    (zero? limit) ; No cache.
    null-cache

    (pos? limit) ; Limited cache that evicts the least recently used entry when full.
    (if (some? retain?)
      (retaining-lru-cache-factory {} :threshold limit :retain? retain?)
      (cc/lru-cache-factory {} :threshold limit))

    :else
    (throw (ex-info (str "Invalid limit: " limit)
                    {:limit limit}))))

(defn cache-clear
  [cache]
  (clear cache))

(defn cache-hit
  [cache ks]
  (hits cache ks))

(defn cache-encache
  [cache kvs]
  (encache cache kvs))

(defn cache-invalidate
  [cache ks]
  (evict cache ks))
