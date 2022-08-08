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
  (:import [clojure.core.cache BasicCache LRUCache]))

(set! *warn-on-reflection* true)

(def ^:dynamic *cache-debug* nil)

(defprotocol CacheExtensions
  (limit [this] "Returns the cache capacity, or -1 if unlimited.")
  (retained-count [this] "Returns the number of retained entries in the cache.")
  (hit-many [this ks] "Hits all the supplied keys. Potentially faster than calling one-by-one.")
  (miss-many [this kvs] "Encaches all the supplied keys. Potentially faster than calling one-by-one.")
  (evict-many [this ks] "Evicts all the supplied keys. Potentially faster than calling one-by-one."))

(defn- hit-many-lru [base lru tick ks]
  (let [[lru-after tick-after]
        (reduce (fn [result k]
                  (assert (gt/endpoint? k))
                  (if (contains? base k)
                    (let [new-tick (inc (second result))]
                      (pair (assoc (first result) k new-tick)
                            new-tick))
                    result))
                (pair lru
                      tick)
                ks)]
    (if (= tick tick-after)
      (pair lru
            tick)
      (pair lru-after
            tick-after))))

(defn- miss-many-lru [base lru tick limit retain? kvs]
  (let [[base-after lru-after tick-after]
        (reduce (fn [result [added-key added-value]]
                  (assert (gt/endpoint? added-key))
                  (let [[base lru tick] result
                        tick (inc tick)
                        retain (and retain? (retain? added-key))]
                    (if (and (not retain)
                             (>= (count lru) limit))
                      ;; We're at or over the limit.
                      (let [replaced-key (if (contains? lru added-key)
                                           added-key
                                           (first (peek lru))) ; The key with the lowest tick.
                            base (-> base (dissoc! replaced-key) (assoc! added-key added-value))
                            lru (-> lru (dissoc replaced-key) (assoc added-key tick))]
                        [base lru tick])

                      ;; We're below the limit.
                      (let [base (assoc! base added-key added-value)
                            lru (if retain
                                  lru ; Retained keys do not enter the LRU.
                                  (assoc lru added-key tick))]
                        [base lru tick]))))
                [(transient base) lru tick]
                kvs)]
    (if (= tick tick-after)
      [base lru tick]
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

(defn- basic-cache-reduce
  ^BasicCache [f ^BasicCache basic-cache coll]
  (BasicCache. (persistent! (reduce f
                                    (transient (.cache basic-cache))
                                    coll))))

(extend-protocol CacheExtensions
  BasicCache
  (limit [_this] -1)
  (retained-count [_this] 0)
  (hit-many [this _ks] this)
  (miss-many [this kvs] (basic-cache-reduce conj! this kvs))
  (evict-many [this ks] (basic-cache-reduce dissoc! this ks))

  LRUCache
  (limit [this] (.limit ^LRUCache this))
  (retained-count [_this] 0)
  (hit-many [this ks]
    (let [^LRUCache this this
          tick (.tick this)
          base (.cache this)
          [lru-after tick-after] (hit-many-lru base (.lru this) tick ks)]
      (if (= tick tick-after)
        this
        (LRUCache. base
                   lru-after
                   tick-after
                   (.limit this)))))
  (miss-many [this kvs]
    (let [^LRUCache this this
          tick (.tick this)
          limit (.limit this)
          [base-after lru-after tick-after] (miss-many-lru (.cache this) (.lru this) tick limit nil kvs)]
      (if (= tick tick-after)
        this
        (LRUCache. base-after
                   lru-after
                   tick-after
                   limit))))
  (evict-many [this ks]
    (let [^LRUCache this this
          base (.cache this)
          [base-after lru-after] (evict-many-lru base (.lru this) ks)]
      (if (identical? base base-after)
        this
        (LRUCache. base-after
                   lru-after
                   (inc (.tick this))
                   (.limit this))))))

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
  (retained-count [_this] 0)
  (hit-many [this _ks] this)
  (miss-many [this _kvs] this)
  (evict-many [this _ks] this))

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
    (let [tick (inc tick)
          retain (retain? item)]
      (if (and (not retain)
               (>= (count lru) limit))
        ;; We're at or over the limit.
        (let [replaced-key (if (contains? lru item)
                             item
                             (first (peek lru))) ; The key with the lowest tick.
              base (-> cache (dissoc replaced-key) (assoc item result))
              lru (-> lru (dissoc replaced-key) (assoc item tick))]
          (RetainingLRUCache. base lru tick limit retain?))

        ;; We're below the limit.
        (RetainingLRUCache. (assoc cache item result)
                            (if retain
                              lru ; Retained keys do not enter the LRU.
                              (assoc lru item tick))
                            tick
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
  (hit-many [this ks]
    (let [^RetainingLRUCache this this
          [lru-after tick-after] (hit-many-lru cache lru tick ks)]
      (if (= tick tick-after)
        this
        (RetainingLRUCache. cache
                            lru-after
                            tick-after
                            limit
                            retain?))))
  (miss-many [this kvs]
    (let [[base-after lru-after tick-after] (miss-many-lru cache lru tick limit retain? kvs)]
      (if (= tick tick-after)
        this
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
         (ifn? retain?)]}
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
    (if (some? retain?)
      (retaining-lru-cache-factory {} :threshold limit :retain? retain?)
      (cc/lru-cache-factory {} :threshold limit))

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
  [cache kvs]
  (miss-many cache kvs))

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
