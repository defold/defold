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

(ns editor.volatile-cache
  (:refer-clojure :exclude [empty])
  (:require [clojure.core.cache :as cache]
            [util.coll :as coll])
  (:import [clojure.lang IMeta IObj]))

(set! *warn-on-reflection* true)

(cache/defcache VolatileCache [cache accessed metadata]
  cache/CacheProtocol
  (lookup [_ item]
          (get cache item))
  (lookup [_ item not-found]
          (get cache item not-found))
  (has? [_ item]
        (contains? cache item))
  (hit [this item]
       (VolatileCache. cache (conj accessed item) metadata))
  (miss [this item result]
        (VolatileCache. (assoc cache item result) (conj accessed item) metadata))
  (evict [_ key]
         (VolatileCache. (dissoc cache key) (disj accessed key) metadata))
  (seed [_ base]
        (VolatileCache. base #{} metadata))

  IMeta
  (meta [_this] metadata)

  IObj
  (withMeta [_this metadata]
            (VolatileCache. cache accessed metadata)))

(defn volatile-cache-factory [base]
  (VolatileCache. base #{} nil))

(defonce empty (volatile-cache-factory {}))

(defn prune
  ^VolatileCache [^VolatileCache volatile-cache]
  (let [accessed-key? (.accessed volatile-cache)
        metadata (.metadata volatile-cache)
        cache (.cache volatile-cache)
        pruned-cache (coll/transform-> cache
                       (filter (fn [entry]
                                 (accessed-key? (key entry)))))]
    (VolatileCache. pruned-cache #{} metadata)))
