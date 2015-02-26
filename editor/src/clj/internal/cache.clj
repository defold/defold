(ns internal.cache
  (:require [clojure.core.async :as a]
            [clojure.core.cache :as cc]
            [com.stuartsierra.component :as component]))

(defn make-cache
  []
  (cc/lru-cache-factory {} :threshold 1000))

(defrecord Cache [size dispose-ch state]
  component/Lifecycle
  (start [this]
    (if state
      this
      (assoc this :state (atom (cc/lru-cache-factory {} :threshold size)))))

  (stop [this]
    (if state
      (assoc this :state nil)
      this))

  clojure.lang.IDeref
  (deref [this]
    (when state @state)))

(defn- encache
  [c kvs]
  (reduce
   (fn [c [k v]]
     (cc/miss c k v))
   c kvs))

(defn- dispose
  [ch vs]
  (when ch
    (a/onto-chan ch vs false)))

(defn- decache
  [cache dispose-ch ks]
  (loop [cache   cache
         ks      (seq ks)
         evicted []]
    (if-let [k (first ks)]
      (if-let [v (get cache k)]
        (recur (cc/evict cache k) (next ks) (conj evicted v))
        (recur cache (next ks) evicted))
      (do
        (dispose dispose-ch evicted)
        cache))))

;; ----------------------------------------
;; Interface
;; ----------------------------------------

(defn make-cache-component
  [size dispose-ch]
  (Cache. size dispose-ch nil))

(defn cache-snapshot
  [ccomp]
  @ccomp)

(defn cache-hit
  [ccomp kvs]
  (swap! (:state ccomp) encache kvs))

(defn cache-invalidate
  [ccomp ks]
  (swap! (:state ccomp) decache (:dispose-ch ccomp) ks))
