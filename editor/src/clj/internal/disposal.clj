(ns internal.disposal
  (:require [internal.async :as ia]
            [internal.graph :as ig]
            [internal.graph.types :as gt]
            [internal.system :as is]
            [service.log :as log :refer [logging-exceptions]]))

(defn dispose-one [value]
  (when (gt/disposable? value) (gt/dispose value)))

(defn- dispose-deleted-one [system value]
  (logging-exceptions "disposal-deleted-loop"
    (let [node-now (ig/node-by-id-at (is/basis system) (gt/node-id value))]
      (when (nil? node-now)
        (dispose-one value)))))

(defn- dispose-cached-one [value]
  (logging-exceptions "disposal-cached-loop" (dispose-one value)))

(defn dispose-pending
  [system]
  (let [cache-queue (is/cache-disposal-queue @system)
        deleted-queue (is/deleted-disposal-queue @system)]
    (logging-exceptions "disposal-cached"
                        (doseq [v (ia/take-all cache-queue)]
                          (dispose-cached-one v)))
    (logging-exceptions "disposal-deleted"
                        (doseq [v (ia/take-all deleted-queue)]
                          (dispose-deleted-one @system v)))))

(defn disposal-message [v] v)
