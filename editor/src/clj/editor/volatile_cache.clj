;; Copyright 2020 The Defold Foundation
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
  (:require [clojure.core.cache :as cache]))

(set! *warn-on-reflection* true)

(cache/defcache VolatileCache [cache accessed]
  cache/CacheProtocol
  (lookup [_ item]
          (get cache item))
  (lookup [_ item not-found]
          (get cache item not-found))
  (has? [_ item]
        (contains? cache item))
  (hit [this item]
       (VolatileCache. cache (conj accessed item)))
  (miss [this item result]
        (VolatileCache. (assoc cache item result) (conj accessed item)))
  (evict [_ key]
         (VolatileCache. (dissoc cache key) (disj accessed key)))
  (seed [_ base]
        (VolatileCache. base #{})))

(defn volatile-cache-factory [base]
  (VolatileCache. base #{}))

(defn prune [^VolatileCache volatile-cache]
  (let [accessed-fn (.accessed volatile-cache)]
    (VolatileCache. (into {} (filter (fn [[k v]] (accessed-fn k)) (.cache volatile-cache))) #{})))
