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

(ns util.id-vec)

(defrecord IdVector [next-id v])

(defn- next! [iv]
  (swap! (:next-id iv) inc))

(defn iv-conj [iv e]
  (update iv :v conj [(next! iv) e]))

(defn iv-into [iv c]
  (reduce (fn [iv e]
            (iv-conj iv e))
          iv c))

(defn iv-vec [c]
  (-> (IdVector. (atom 0) [])
    (iv-into c)))

(defn iv-ids [iv]
  (mapv first (:v iv)))

(defn iv-vals [iv]
  (mapv second (:v iv)))

(defn iv-mapv [f iv]
  (mapv f (:v iv)))

(defn iv-filter-ids [iv ids]
  (if ids
    (let [ids (set ids)]
      (update iv :v (fn [v] (->> v
                              (filter (comp ids first))
                              vec))))
    iv))

(defn iv-update-ids [f iv ids]
  (if ids
    (let [ids (set ids)]
      (update iv :v (fn [v] (mapv (fn [[id v]] [id (if (ids id) (f v) v)]) v))))
    iv))

(defn iv-remove-ids [iv ids]
  (let [ids (set ids)]
    (update iv :v (fn [v] (->> v
                            (filter (comp not ids first))
                            vec)))))
