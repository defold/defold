(ns internal.cache
  (:require [clojure.core.cache :as cc]
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

(defn- encache [c kvs] (reduce (fn [c [k v]] (cc/miss c k v)) c kvs))
(defn- decache [c ks]  (reduce cc/evict c ks))

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
  (swap! (:state ccomp) decache ks))
