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
            [internal.graph.types :as gt]
            [util.coll :refer [pair]])
  (:import [clojure.core.cache BasicCache]))

(set! *warn-on-reflection* true)

(def ^:dynamic *cache-debug* nil)

(defprotocol CacheExtensions
  (seed [this base retain-arg] "Replace the contents of the cache with base. Potentially faster than calling one-by-one. The retain-arg will be supplied as the first argument to a present retain predicate for this run.")
  (limit [this] "Returns the cache capacity, or -1 if unlimited.")
  (retained-count [this] "Returns the number of retained entries in the cache.")
  (hit-many [this ks] "Hits all the supplied keys. Potentially faster than calling one-by-one.")
  (miss-many [this kvs retain-arg] "Encaches all the supplied keys. Potentially faster than calling one-by-one. The retain-arg will be supplied as the first argument to a present retain predicate for this run.")
  (evict-many [this ks] "Evicts all the supplied keys. Potentially faster than calling one-by-one."))

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
  (seed [this base retain-arg] this)
  (limit [_this] 0)
  (retained-count [_this] 0)
  (hit-many [this _ks] this)
  (miss-many [this _kvs _retain-arg] this)
  (evict-many [this _ks] this))

(def null-cache (NullCache. {}))

;; ----------------------------------------
;; Basic unlimited cache implementation
;; ----------------------------------------

(defn- miss-many-base [base kvs]
  (if (empty? kvs)
    base
    (persistent!
      (reduce (fn [base [added-key added-value]]
                (assert (gt/endpoint? added-key))
                (assoc! base added-key added-value))
              (transient base)
              kvs))))

(defn- evict-many-base [base ks]
  (if (empty? ks)
    base
    (let [did-change (volatile! false)
          will-change #(or %1 (contains? base %2))
          base-after (reduce (fn [base removed-key]
                               (assert (gt/endpoint? removed-key))
                               (vswap! did-change will-change removed-key)
                               (dissoc! base removed-key))
                             (transient base)
                             ks)]
      (if @did-change
        (persistent! base-after)
        base))))

(defn- update-basic-cache [^BasicCache basic-cache update-fn arg]
  (let [base (.cache basic-cache)
        base-after (update-fn base arg)]
    (if (identical? base base-after)
      basic-cache
      (BasicCache. base-after))))

(extend-protocol CacheExtensions
  BasicCache
  (seed [this base _retain-arg] (cc/seed this base))
  (limit [_this] -1)
  (retained-count [_this] 0)
  (hit-many [this _ks] this)
  (miss-many [this kvs _retain-arg] (update-basic-cache this miss-many-base kvs))
  (evict-many [this ks] (update-basic-cache this evict-many-base ks)))

;; ----------------------------------------
;; Application-specialized LRU cache with the ability to selectively exclude
;; entries from the cache limit.
;; ----------------------------------------

(defn- occupied-lru-entry? [entry]
  (not (neg? (val entry))))

(defn- hit-many-lru [lru tick ks]
  (let [[lru-after tick-after]
        (reduce (fn [result k]
                  (assert (gt/endpoint? k))
                  (if (contains? lru k)
                    (let [tick (inc (second result))]
                      (pair (assoc (first result) k tick)
                            tick))
                    result))
                (pair lru
                      tick)
                ks)]
    (if (= tick tick-after)
      (pair lru
            tick)
      (pair lru-after
            tick-after))))

(defn- miss-update-lru [lru tick limit retain? added-key]
  (assert (gt/endpoint? added-key))
  (let [tick (inc tick)
        retain (and retain? (retain? added-key))]
    (if (and (not retain)
             (>= (count lru) limit))
      ;; We're at or over the limit.
      (let [replaced-key (if (contains? lru added-key)
                           added-key
                           (first (peek lru))) ; The key with the lowest tick.
            lru (cond-> lru
                        (not= added-key replaced-key) (dissoc replaced-key)
                        :always (assoc added-key tick))]
        [lru tick replaced-key])

      ;; We're below the limit.
      (let [lru (if retain
                  lru ; Retained keys do not enter the LRU.
                  (assoc lru added-key tick))]
        [lru tick nil]))))

(defn- miss-many-lru [base lru tick limit retain? kvs]
  (if (empty? kvs)
    [base lru tick]
    (let [[base-after lru-after tick-after]
          (reduce (fn [result [added-key added-value]]
                    (let [[base lru tick] result
                          [lru tick replaced-key] (miss-update-lru lru tick limit retain? added-key)
                          base (if (nil? replaced-key)
                                 (assoc! base added-key added-value)
                                 (cond-> base
                                         (not= added-key replaced-key) (dissoc! replaced-key)
                                         :always (assoc! added-key added-value)))]
                      [base lru tick]))
                  [(transient base) lru tick]
                  kvs)]
      [(persistent! base-after) lru-after tick-after])))

(defn- evict-many-lru [base lru ks]
  (let [did-change (volatile! false)
        [base-after lru-after]
        (reduce (fn [result k]
                  (assert (gt/endpoint? k))
                  (if (contains? base k) ; Assumes ks are distinct. Lots faster than checking existence in transient result.
                    (do
                      (vreset! did-change true)
                      (pair (dissoc! (first result) k)
                            (dissoc (second result) k)))
                    result))
                (pair (transient base)
                      lru)
                ks)]
    (if @did-change
      (pair (persistent! base-after)
            lru-after)
      (pair base
            lru))))

(defn- seed-lru [base limit retain?]
  (let [unretained-base (if (nil? retain?)
                          base
                          (into (empty base)
                                (remove (comp retain? key))
                                base))]
    (#'cc/build-leastness-queue unretained-base limit 0)))

(cc/defcache RetainingLRUCache [cache lru tick limit retain?]
  cc/CacheProtocol
  (lookup [_ item]
    (get cache item))
  (lookup [_ item not-found]
    (get cache item not-found))
  (has? [_ item]
    (contains? cache item))
  (hit [this item]
    (if (not (contains? lru item))
      this
      (let [tick (inc tick)]
        (assert (contains? cache item))
        (RetainingLRUCache. cache
                            (assoc lru item tick)
                            tick
                            limit
                            retain?))))
  (miss [_ added-key added-value]
    (let [[lru tick replaced-key] (miss-update-lru lru tick limit retain? added-key)
          base (if (nil? replaced-key)
                 (assoc cache added-key added-value)
                 (cond-> cache
                         (not= added-key replaced-key) (dissoc replaced-key)
                         :always (assoc added-key added-value)))]
      (RetainingLRUCache. base lru tick limit retain?)))
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
    (let [lru (seed-lru base limit retain?)]
      (RetainingLRUCache. base
                          lru
                          0
                          limit
                          retain?)))

  CacheExtensions
  (seed [_ base retain-arg]
    (let [fast-retain? (some-> retain? (partial retain-arg))
          lru (seed-lru base limit fast-retain?)]
      (RetainingLRUCache. base
                          lru
                          0
                          limit
                          retain?)))
  (limit [_this] limit)
  (retained-count [this]
    ;; Note: Likely slow, don't call excessively.
    (- (count this)
       (count (filter occupied-lru-entry? lru))))
  (hit-many [this ks]
    (let [^RetainingLRUCache this this
          [lru-after tick-after] (hit-many-lru lru tick ks)]
      (if (= tick tick-after)
        this
        (RetainingLRUCache. cache
                            lru-after
                            tick-after
                            limit
                            retain?))))
  (miss-many [this kvs retain-arg]
    (if (empty? kvs)
      this
      (let [fast-retain? (some-> retain? (partial retain-arg))
            [base-after lru-after tick-after] (miss-many-lru cache lru tick limit fast-retain? kvs)]
        (RetainingLRUCache. base-after
                            lru-after
                            tick-after
                            limit
                            retain?))))
  (evict-many [this ks]
    (let [[base-after lru-after] (evict-many-lru cache lru ks)]
      (if (identical? cache base-after)
        this
        (RetainingLRUCache. base-after
                            lru-after
                            (inc tick)
                            limit
                            retain?))))

  Object
  (toString [_]
    (str cache \, \space lru \, \space tick \, \space limit)))

(defn- retaining-lru-cache-factory
  [base & {threshold :threshold retain? :retain?}]
  {:pre [(map? base)
         (number? threshold) (< 0 threshold)
         (or (nil? retain?) (ifn? retain?))]}
  (cc/seed (RetainingLRUCache. nil nil 0 threshold retain?) base))

;; ----------------------------------------
;; Interface
;; ----------------------------------------

(defn cache? [value]
  (satisfies? cc/CacheProtocol value))

(defn make-cache [limit retain?]
  {:pre [(number? limit) (<= -1 limit)
         (or (nil? retain?) (ifn? retain?))]}
  (cond
    (= -1 limit) ; Unlimited cache.
    (cc/basic-cache-factory {})

    (zero? limit) ; No cache.
    null-cache

    (pos? limit) ; Limited cache that evicts the least recently used entry when full.
    (retaining-lru-cache-factory {} :threshold limit :retain? retain?)

    :else
    (throw (ex-info (str "Invalid limit: " limit)
                    {:limit limit}))))

(defn cache-clear
  [cache]
  (cc/seed cache {}))

(defn cache-hit
  [cache ks]
  (hit-many cache ks))

(defn cache-encache
  [cache kvs retain-arg]
  (miss-many cache kvs retain-arg))

(defn cache-invalidate
  [cache ks]
  (evict-many cache ks))

(defn cache-info
  "Return a map detailing cache utilization."
  [cache]
  (let [cached-count (count cache)
        retained-count (retained-count cache)
        unretained-count (- cached-count retained-count)
        limit (limit cache)]
    {:total cached-count
     :retained retained-count
     :unretained unretained-count
     :limit limit}))
