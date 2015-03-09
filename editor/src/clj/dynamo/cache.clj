(ns dynamo.cache
  "High level interface to the system cache."
  (:require [internal.cache :as c]
            [internal.system :as is]))

(defn cache-invalidate
  "Uses the system’s Cache component.

  Atomic action to invalidate the given collection of [node-id label]
  pairs. If nothing is cached for a pair, it is ignored."
  [pairs]
  (c/cache-invalidate (is/system-cache) pairs))

(defn cache-encache
  "Uses the system’s Cache component.

  Atomic action to record one or more items in cache.

  The collection must contain tuples of [[node-id label] value]."
  [coll]
  (c/cache-encache (is/system-cache) coll))

(defn cache-hit
  "Uses the system’s Cache component.

  Atomic action to record hits on cached items

  The collection must contain tuples of [node-id label] pairs."
  [coll]
  (c/cache-hit (is/system-cache) coll))

(defn cache-snapshot
  "Get a value of the cache at a point in time."
  []
  (c/cache-snapshot (is/system-cache)))
