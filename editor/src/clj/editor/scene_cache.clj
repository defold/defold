;; Copyright 2020-2024 The Defold Foundation
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

(ns editor.scene-cache
  (:require [clojure.core.cache :as cache]
            [editor.volatile-cache :as vcache]
            [util.coll :refer [pair]]))

(set! *warn-on-reflection* true)

(defonce ^:private object-caches (atom {}))

(defn register-object-cache! [cache-id make-fn update-fn destroy-batch-fn]
  (swap! object-caches conj [cache-id {:caches {} :make-fn make-fn :update-fn update-fn :destroy-batch-fn destroy-batch-fn}])
  nil)

(defn- dump-cache [cache]
  (prn "Cache dump" (count cache))
  (doseq [entry cache]
    (prn entry)))

(defn request-object! [cache-id request-id context data]
  (let [cache-meta (get @object-caches cache-id)
        make-fn (:make-fn cache-meta)
        cache (or (get-in cache-meta [:caches context])
                  (vcache/volatile-cache-factory {}))
        new-cache (if (cache/has? cache request-id)
                    (let [[object old-data] (cache/lookup cache request-id)]
                      (if (not= data old-data)
                        (let [update-fn (:update-fn cache-meta)]
                          (cache/miss cache request-id [(update-fn context object data) data]))
                        (cache/hit cache request-id)))
                    (cache/miss cache request-id [(make-fn context data) data]))]
    (swap! object-caches update-in [cache-id :caches] assoc context new-cache)
    (first (cache/lookup new-cache request-id))))

(defn lookup-object [cache-id request-id context]
  (let [cache-meta (get @object-caches cache-id)]
    (when-let [cache (get-in cache-meta [:caches context])]
      (first (cache/lookup cache request-id)))))

(defn prune-context [caches context]
  (into {}
        (map (fn [[cache-id meta]]
               (let [destroy-batch-fn (:destroy-batch-fn meta)]
                 [cache-id (update-in meta [:caches context]
                                      (fn [cache]
                                        (when cache
                                          (let [pruned-cache (vcache/prune cache)
                                                dead-entries (filter (fn [[request-id _]] (not (contains? pruned-cache request-id))) cache)
                                                dead-objects (mapv (fn [[_ object]] object) dead-entries)]
                                            (when (seq dead-objects)
                                              (destroy-batch-fn context (map first dead-objects) (map second dead-objects)))
                                            pruned-cache))))])))
        caches))

(defn prune-context! [context]
  (swap! object-caches prune-context context))

;; This should is used for debugging purposes. In order to update objects in any cache,
;; we must preserve the cache keys so that re-creation of objects can be triggered
;; when needed (e.g when modifying preview rendering).
(defn- clear-all [caches]
  (into {}
        (map (fn [[cache-id meta]]
               (let [destroy-batch-fn (:destroy-batch-fn meta)]
                 [cache-id (update meta :caches
                                   (fn [caches-by-context]
                                     (into {}
                                           (map (fn [[context cache]]
                                                  [context (when (some? cache)
                                                             (let [evicted-keys (mapv first cache)
                                                                   dead-objects (mapv second cache)]
                                                               (when (seq dead-objects)
                                                                 (destroy-batch-fn context (map first dead-objects) (map second dead-objects)))
                                                               (reduce cache/evict cache evicted-keys)))]))
                                           caches-by-context)))])))
        caches))

(defn clear-all! []
  (swap! object-caches clear-all))

(defn- drop-context [caches context]
  (into {}
        (map (fn [[cache-id meta]]
               (let [destroy-batch-fn (:destroy-batch-fn meta)
                     cache (get-in meta [:caches context])]
                 [cache-id (if (nil? cache)
                             meta
                             (let [dead-objects (map second cache)]
                               (when (seq dead-objects)
                                 (destroy-batch-fn context (map first dead-objects) (map second dead-objects)))
                               (update meta :caches dissoc context)))])))
        caches))

(defn drop-context! [context]
  (swap! object-caches drop-context context))

(defn cache-stats
  "Returns a sorted map where the keys are pairs of [context-id cache-id],
  mapped to the number of entries in the cache for that context and cache-id
  combination. The number of entries represent the number of unique request-ids
  in the cache. The contexts are typically OpenGL contexts, so in order to group
  the results, we use a string representation of the identity hash code of the
  OpenGL context as the context-id."
  []
  (into (sorted-map)
        (mapcat (fn [[cache-id {caches-by-context :caches}]]
                  (keep (fn [[context cached-values-by-request-id]]
                          (let [entry-count (count cached-values-by-request-id)]
                            (when (pos? entry-count)
                              (let [context-ptr (or (some-> context (System/identityHashCode)) (int 0))
                                    context-id (format "0x%08x" context-ptr)
                                    stats-key (pair context-id cache-id)]
                                (pair stats-key entry-count)))))
                        caches-by-context)))
        @object-caches))
