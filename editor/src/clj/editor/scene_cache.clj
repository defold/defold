;; Copyright 2020-2026 The Defold Foundation
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
            [editor.graphics.types :as graphics.types]
            [editor.volatile-cache :as volatile-cache]
            [internal.util :as util]
            [util.coll :as coll :refer [pair]]
            [util.thread-util :as thread-util]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

;; Map of cache-id->cache-meta maps, where (:cache cache-meta) is a nested map
;; of context -> request-id -> [cached-object request-data].
(defonce ^:private object-caches-atom (atom {}))

(defn- make-deletion [destroyed-object+request-data-pairs destroy-batch-fn]
  {:pre [(ifn? destroy-batch-fn)]}
  (let [[destroyed-objects destroyed-request-datas]
        (util/into-multiple
          (pair [] [])
          (pair (map first) (map second))
          destroyed-object+request-data-pairs)]
    (assert (pos? (count destroyed-objects)))
    {:destroy-batch-fn destroy-batch-fn
     :destroyed-request-datas destroyed-request-datas
     :destroyed-objects destroyed-objects}))

(defn- perform-deletion! [context {:keys [destroy-batch-fn destroyed-request-datas destroyed-objects] :as _deletion}]
  (destroy-batch-fn context destroyed-objects destroyed-request-datas))

(defn- perform-deletions! [context deletions]
  (doseq [deletion deletions]
    (perform-deletion! context deletion)))

(defn- clear-cached-object+request-data-by-request-id [cached-object+request-data-by-request-id destroy-batch-fn]
  (if (coll/empty? cached-object+request-data-by-request-id)
    cached-object+request-data-by-request-id
    (let [deletion (make-deletion (vals cached-object+request-data-by-request-id) destroy-batch-fn)]
      (with-meta
        (empty cached-object+request-data-by-request-id)
        (update (meta cached-object+request-data-by-request-id)
                :pending-deletions
                coll/conj-vector
                deletion)))))

(defn- clear-caches-by-context [caches-by-context destroy-batch-fn]
  (if (coll/empty? caches-by-context)
    caches-by-context
    (coll/transfer caches-by-context (empty caches-by-context)
      (map (fn [[context cached-object+request-data-by-request-id :as entry]]
             (if (coll/empty? cached-object+request-data-by-request-id)
               entry
               (pair context
                     (clear-cached-object+request-data-by-request-id cached-object+request-data-by-request-id destroy-batch-fn))))))))

(defn- clear-cache-meta [cache-meta]
  (update cache-meta :caches clear-caches-by-context (:destroy-batch-fn cache-meta)))

(defn- clear-object-caches [object-caches]
  (coll/transfer object-caches (empty object-caches)
    (map (fn [[cache-id cache-meta]]
           (pair cache-id
                 (clear-cache-meta cache-meta))))))

(defn clear-all! []
  ;; Clear any cached objects from prior requests. Does not remove information
  ;; about the registered caches or contexts, just the requests. Useful during
  ;; development. It's the scene-cache equivalent of g/clear-system-cache!
  (swap! object-caches-atom clear-object-caches)
  nil)

(defn- consume-pending-deletions-in-caches-by-context [caches-by-context context]
  (let [pending-deletions
        (some-> caches-by-context
                (get context)
                (meta)
                (:pending-deletions))

        caches-by-context-without-pending-deletions
        (cond-> caches-by-context
                (coll/not-empty pending-deletions)
                (update context vary-meta dissoc :pending-deletions))]

    (pair caches-by-context-without-pending-deletions
          pending-deletions)))

(defn- consume-pending-deletions-in-cache-meta [cache-meta context]
  (let [[caches-by-context-without-pending-deletions pending-deletions]
        (consume-pending-deletions-in-caches-by-context (:caches cache-meta) context)

        cache-meta-without-pending-deletions
        (assoc cache-meta :caches caches-by-context-without-pending-deletions)]

    (pair cache-meta-without-pending-deletions
          pending-deletions)))

(defn- consume-pending-deletions-in-object-caches [object-caches context]
  (reduce-kv
    (fn [[object-caches pending-deletions] cache-id cache-meta]
      (let [[cache-meta-without-pending-deletions pending-deletions-from-cache]
            (consume-pending-deletions-in-cache-meta cache-meta context)]
        (pair (assoc object-caches cache-id cache-meta-without-pending-deletions)
              (into pending-deletions pending-deletions-from-cache))))
    (pair (empty object-caches)
          [])
    object-caches))

(defn process-pending-deletions! [context]
  ;; We can only operate on the contents of the cache during the time span while
  ;; the associated GLContext has been made current. This is called at the
  ;; beginning of each frame before requesting any objects from the cache. It
  ;; will consume and process any pending :pending-deletions that might have
  ;; been made in the interim.
  (let [[deletions] (thread-util/swap-rest! object-caches-atom consume-pending-deletions-in-object-caches context)]
    (perform-deletions! context deletions)))

(defn register-object-cache! [cache-id make-fn update-fn destroy-batch-fn]
  ;; This may be called multiple times when reloading Clojure files (typically
  ;; during editor development). Make sure to call the old destroy-batch-fn on
  ;; the contents of the cache before replacing it.
  (swap! object-caches-atom update cache-id
         (fn [cache-meta]
           (-> cache-meta
               (clear-cache-meta)
               (assoc
                 :make-fn make-fn
                 :update-fn update-fn
                 :destroy-batch-fn destroy-batch-fn))))
  nil)

(defn request-object! [cache-id request-id context request-data]
  {:pre [(graphics.types/request-id? request-id)]}
  (let [cache-meta (get @object-caches-atom cache-id)]
    (if (nil? cache-meta)
      (throw (ex-info (str "Unknown scene cache id: " cache-id)
                      {:cache-id cache-id
                       :registered-cache-ids (into (sorted-set)
                                                   (map key)
                                                   @object-caches-atom)}))
      (if-some [[old-object old-request-data]
                (some-> (get-in cache-meta [:caches context])
                        (cache/lookup request-id))]
        (if (= old-request-data request-data)
          (do
            (swap!
              object-caches-atom update-in [cache-id :caches context]
              #(cache/hit (or % volatile-cache/empty) request-id))
            old-object)
          (let [update-fn (:update-fn cache-meta)
                updated-object (update-fn context old-object request-data)]
            (swap!
              object-caches-atom update-in [cache-id :caches context]
              #(cache/miss (or % volatile-cache/empty) request-id (pair updated-object request-data)))
            updated-object))
        (let [make-fn (:make-fn cache-meta)
              added-object (make-fn context request-data)

              [old-object-caches]
              (swap-vals!
                object-caches-atom update-in [cache-id :caches context]
                #(cache/miss (or % volatile-cache/empty) request-id (pair added-object request-data)))]

          ;; It's still possible we replaced an existing object, since we based
          ;; our decision on a snapshot. In that case, we should destroy the old
          ;; one using the :destroy-batch-fn in use at the time.
          (when-some [old-cache-meta (get old-object-caches cache-id)]
            (when-some [deletion
                        (some-> (get-in old-cache-meta [:caches context])
                                (cache/lookup request-id) ; [old-cached-object old-request-data] pair.
                                (vector)
                                (make-deletion (:destroy-batch-fn old-cache-meta)))]
              (perform-deletion! context deletion)))
          added-object)))))

(defn lookup-object [cache-id request-id context]
  (some-> (get-in @object-caches-atom [cache-id :caches context])
          (cache/lookup request-id) ; [cached-object request-data] pair.
          (first)))

(defn- prune-context-in-caches-by-context [caches-by-context context destroy-batch-fn]
  (let [cached-object+request-data-by-request-id
        (get caches-by-context context)

        pruned-cached-object+request-data-by-request-id
        (some-> cached-object+request-data-by-request-id
                (volatile-cache/prune))

        deletions
        (when (not= (count cached-object+request-data-by-request-id)
                    (count pruned-cached-object+request-data-by-request-id))
          (some-> (keep (fn [[request-id cached-object+request-data]]
                          (when (not (cache/has? pruned-cached-object+request-data-by-request-id request-id))
                            cached-object+request-data))
                        cached-object+request-data-by-request-id)
                  (coll/not-empty)
                  (make-deletion destroy-batch-fn)
                  (vector)))

        pruned-caches-by-context
        (assoc caches-by-context context pruned-cached-object+request-data-by-request-id)]

    (pair pruned-caches-by-context
          deletions)))

(defn- prune-context-in-cache-meta [cache-meta context]
  (let [[pruned-caches-by-context deletions]
        (prune-context-in-caches-by-context (:caches cache-meta) context (:destroy-batch-fn cache-meta))

        pruned-cache-meta
        (assoc cache-meta :caches pruned-caches-by-context)]

    (pair pruned-cache-meta
          deletions)))

(defn- prune-context-in-object-caches [object-caches context]
  (reduce-kv
    (fn [[object-caches deletions] cache-id cache-meta]
      (let [[pruned-cache-meta deletions-from-cache]
            (prune-context-in-cache-meta cache-meta context)]
        (pair (assoc object-caches cache-id pruned-cache-meta)
              (into deletions deletions-from-cache))))
    (pair (empty object-caches)
          [])
    object-caches))

(defn prune-context! [context]
  (let [[deletions] (thread-util/swap-rest! object-caches-atom prune-context-in-object-caches context)]
    (perform-deletions! context deletions)))

(defn- drop-context-from-caches-by-context [caches-by-context context destroy-batch-fn]
  (let [deletions
        (some-> caches-by-context
                (get context)
                (clear-cached-object+request-data-by-request-id destroy-batch-fn)
                (meta)
                (:pending-deletions))

        caches-by-context-without-context
        (some-> caches-by-context
                (dissoc context))]

    (pair caches-by-context-without-context
          deletions)))

(defn- drop-context-from-cache-meta [cache-meta context]
  (let [[caches-by-context-without-context deletions]
        (drop-context-from-caches-by-context (:caches cache-meta) context (:destroy-batch-fn cache-meta))

        cache-meta-without-context
        (assoc cache-meta :caches caches-by-context-without-context)]

    (pair cache-meta-without-context
          deletions)))

(defn- drop-context-from-object-caches [object-caches context]
  (reduce-kv
    (fn [[object-caches deletions] cache-id cache-meta]
      (let [[cache-meta-without-context deletions-from-cache]
            (drop-context-from-cache-meta cache-meta context)]
        (pair (assoc object-caches cache-id cache-meta-without-context)
              (into deletions deletions-from-cache))))
    (pair (empty object-caches)
          [])
    object-caches))

(defn drop-context! [context]
  (let [[deletions] (thread-util/swap-rest! object-caches-atom drop-context-from-object-caches context)]
    (perform-deletions! context deletions)))

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
        @object-caches-atom))
