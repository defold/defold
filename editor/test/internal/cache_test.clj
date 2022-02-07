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

(ns internal.cache-test
  (:require [clojure.test :refer :all]
            [support.test-support :refer :all]
            [internal.cache :refer :all]
            [internal.graph.types :as gt]
            [internal.system :as is]))

(defn- mock-system [cache-size ch]
  (is/make-system {:cache-size cache-size}))

(defn- as-map [c] (select-keys c (keys c)))

(deftest items-in-and-out
  (testing "things put into cache appear in snapshot"
    (with-clean-system {:cache-size 1000}
      (let [snapshot (-> cache
                       (cache-encache [[:a 1] [:b 2] [:c 3]]))]
        (are [k v] (= v (get snapshot k))
          :a 1
          :b 2
          :c 3))))

  (testing "one item can be decached"
    (with-clean-system
      (is (= {:a 1 :c 3} (-> cache
                           (cache-encache [[:a 1] [:b 2] [:c 3]])
                           (cache-invalidate [:b])
                           (as-map))))))

  (testing "multiple items can be decached"
    (with-clean-system
      (is (empty? (-> cache
                    (cache-encache [[:a 1] [:b 2] [:c 3]])
                    (cache-invalidate [:b :c])
                    (cache-invalidate [:a])
                    (as-map)))))))

(deftest limits
  (with-clean-system {:cache-size 3}
     (let [snapshot (-> cache
                      (cache-encache [[[:a :a] 1] [[:b :b] 2] [[:c :c] 3]])
                      (cache-hit [[:a :a] [:c :c]])
                      (cache-encache [[[:d :d] 4]]))]
       (are [k v] (= v (get snapshot k))
            [:a :a] 1
            [:c :c] 3
            [:d :d] 4))))
