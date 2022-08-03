;; Copyright 2020-2022 The Defold Foundation
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

(ns internal.cache-test
  (:require [clojure.core.cache :as cc]
            [clojure.test :refer :all]
            [internal.cache :refer :all]
            [internal.graph.types :as gt]
            [support.test-support :refer :all]))

(defn- as-map [c] (select-keys c (keys c)))

(deftest items-in-and-out-test
  (testing "things put into cache appear in snapshot"
    (with-clean-system {:cache-size 1000}
      (let [snapshot (-> cache
                       (cache-encache {(gt/endpoint 1 :a) 1
                                       (gt/endpoint 1 :b) 2
                                       (gt/endpoint 1 :c) 3}))]
        (are [k v] (= v (get snapshot k))
          (gt/endpoint 1 :a) 1
          (gt/endpoint 1 :b) 2
          (gt/endpoint 1 :c) 3))))

  (testing "one item can be decached"
    (with-clean-system
      (is (= {(gt/endpoint 1 :a) 1
              (gt/endpoint 1 :c) 3}
             (-> cache
               (cache-encache {(gt/endpoint 1 :a) 1
                               (gt/endpoint 1 :b) 2
                               (gt/endpoint 1 :c) 3})
               (cache-invalidate [(gt/endpoint 1 :b)])
               (as-map))))))

  (testing "multiple items can be decached"
    (with-clean-system
      (is (empty? (-> cache
                    (cache-encache {(gt/endpoint 1 :a) 1
                                    (gt/endpoint 1 :b) 2
                                    (gt/endpoint 1 :c) 3})
                    (cache-invalidate [(gt/endpoint 1 :b) (gt/endpoint 1 :c)])
                    (cache-invalidate [(gt/endpoint 1 :a)])
                    (as-map)))))))

(deftest limits-test
  (letfn [(populate-cache [cache]
            (-> cache
              (cache-encache {(gt/endpoint 1 :a) 1
                              (gt/endpoint 1 :b) 2
                              (gt/endpoint 1 :c) 3})
              (cache-hit [(gt/endpoint 1 :a)
                          (gt/endpoint 1 :c)])
              (cache-encache {(gt/endpoint 1 :d) 4})))]

    (testing "positive cache limit"
      (with-clean-system {:cache-size 3}
        (let [snapshot (populate-cache cache)]
          (is (= 3 (count snapshot)))
          (are [k v] (= v (get snapshot k))
            (gt/endpoint 1 :a) 1
            (gt/endpoint 1 :c) 3
            (gt/endpoint 1 :d) 4))))

    (testing "zero cache limit (no caching)"
      (with-clean-system {:cache-size 0}
        (let [snapshot (populate-cache cache)]
          (is (empty? snapshot)))))

    (testing "unlimited cache"
      (with-clean-system {:cache-size -1}
        (let [snapshot (populate-cache cache)]
          (is (= 4 (count snapshot)))
          (are [k v] (= v (get snapshot k))
            (gt/endpoint 1 :a) 1
            (gt/endpoint 1 :b) 2
            (gt/endpoint 1 :c) 3
            (gt/endpoint 1 :d) 4))))))

(deftest clear-test
  (letfn [(populate-cache [cache]
            (cache-encache cache {(gt/endpoint 1 :a) 1
                                  (gt/endpoint 1 :b) 2
                                  (gt/endpoint 1 :c) 3
                                  (gt/endpoint 1 :d) 4}))]

    (testing "positive cache limit"
      (with-clean-system {:cache-size 3}
        (let [cache (populate-cache cache)]
          (is (= 3 (count cache)))
          (let [cache (cache-clear cache)]
            (is (= 0 (count cache)))
            (let [cache (populate-cache cache)]
              (is (= 3 (count cache))))))))

    (testing "zero cache limit (no caching)"
      (with-clean-system {:cache-size 0}
        (is (identical? null-cache cache))
        (let [cache (populate-cache cache)]
          (is (= 0 (count cache)))
          (let [cache (cache-clear cache)]
            (is (= 0 (count cache)))
            (is (identical? null-cache cache))
            (let [cache (populate-cache cache)]
              (is (= 0 (count cache))))))))

    (testing "unlimited cache"
      (with-clean-system {:cache-size -1}
        (is (unlimited-cache? cache))
        (let [cache (populate-cache cache)]
          (is (= 4 (count cache)))
          (let [cache (cache-clear cache)]
            (is (= 0 (count cache)))
            (is (unlimited-cache? cache))
            (let [cache (populate-cache cache)]
              (is (= 4 (count cache))))))))))

(deftest retain-test
  (letfn [(populate-cache [cache]
            (cache-encache cache {(gt/endpoint 1 :a) 1
                                  (gt/endpoint 1 :b) 2
                                  (gt/endpoint 1 :retain/a) -1
                                  (gt/endpoint 1 :retain/b) -2
                                  (gt/endpoint 1 :c) 3
                                  (gt/endpoint 1 :d) 4}))
          (cache-retain? [endpoint]
            (= "retain" (namespace (gt/endpoint-label endpoint))))]
    (with-clean-system {:cache-size 3
                        :cache-retain? cache-retain?}

      (testing "retained items do not count towards the cache limit"
        (let [cache (-> cache populate-cache)]
          (is (= 5 (count cache)))
          (is (contains? cache (gt/endpoint 1 :retain/a)))
          (is (contains? cache (gt/endpoint 1 :retain/b)))))

      (testing "retained items are invalidated"
        (let [invalidated-item (gt/endpoint 1 :retain/a)
              cache (-> cache populate-cache (cache-invalidate [invalidated-item]))]
          (is (= 4 (count cache)))
          (is (not (contains? cache invalidated-item)))))

      (testing "retained items are cleared out"
        (let [cache (-> cache populate-cache cache-clear)]
          (is (= 0 (count cache)))
          (is (not (contains? cache (gt/endpoint 1 :retain/a))))
          (is (not (contains? cache (gt/endpoint 1 :retain/b))))))

      (testing "retain rules remain after clear"
        (let [cache (-> cache populate-cache cache-clear populate-cache)]
          (is (= 5 (count cache)))
          (is (contains? cache (gt/endpoint 1 :retain/a)))
          (is (contains? cache (gt/endpoint 1 :retain/b)))))

      (testing "retain rules apply to seeded items"
        (let [cache (-> cache
                        (cc/seed {(gt/endpoint 2 :retain/a) -1
                                  (gt/endpoint 2 :retain/b) -2})
                        populate-cache
                        (cache-invalidate [(gt/endpoint 2 :retain/b)]))]
          (is (= 6 (count cache)))
          (is (contains? cache (gt/endpoint 1 :retain/a)))
          (is (contains? cache (gt/endpoint 1 :retain/b)))
          (is (contains? cache (gt/endpoint 2 :retain/a)))
          (is (not (contains? cache (gt/endpoint 2 :retain/b))))))

      (testing "retained items do not enter the LRU queue when hit"
        (let [cache (populate-cache cache)
              lru-before (.lru cache)]
          (is (= [(gt/endpoint 1 :b)
                  (gt/endpoint 1 :c)
                  (gt/endpoint 1 :d)]
                 (keys lru-before)))
          (let [cache (cc/hit cache (gt/endpoint 1 :retain/a))
                lru-after (.lru cache)]
            (is (= lru-before lru-after))))))))

(deftest limit-test
  (is (= -1 (limit (make-cache -1 nil))))
  (is (= 0 (limit (make-cache 0 nil))))
  (is (= 1 (limit (make-cache 1 nil))))
  (is (= 2 (limit (make-cache 2 keyword?)))))

(deftest retained-count-test
  (letfn [(populate-cache [cache]
            (cache-encache cache {(gt/endpoint 1 :a) 1
                                  (gt/endpoint 1 :b) 2
                                  (gt/endpoint 1 :retain/a) -1
                                  (gt/endpoint 1 :retain/b) -2
                                  (gt/endpoint 1 :c) 3
                                  (gt/endpoint 1 :d) 4}))
          (cache-retain? [endpoint]
            (= "retain" (namespace (gt/endpoint-label endpoint))))]
    (testing "positive cache limit with retain predicate"
      (with-clean-system {:cache-size 3
                          :cache-retain? cache-retain?}
        (let [cache (populate-cache cache)]
          (is (= 2 (retained-count cache))))))

    (testing "positive cache limit without retain predicate"
      (with-clean-system {:cache-size 3}
        (let [cache (populate-cache cache)]
          (is (= 0 (retained-count cache))))))

    (testing "zero cache limit (no caching)"
      (with-clean-system {:cache-size 0}
        (let [cache (populate-cache cache)]
          (is (= 0 (retained-count cache))))))

    (testing "unlimited cache"
      (with-clean-system {:cache-size -1}
        (let [cache (populate-cache cache)]
          (is (= 0 (retained-count cache))))))))

(deftest reconfigure-test
  (letfn [(populate-cache [cache]
            (cache-encache cache {(gt/endpoint 1 :a) 1
                                  (gt/endpoint 1 :b) 2
                                  (gt/endpoint 1 :retain/a) -1
                                  (gt/endpoint 1 :retain/b) -2
                                  (gt/endpoint 1 :c) 3
                                  (gt/endpoint 1 :d) 4}))
          (cache-retain? [endpoint]
            (= "retain" (namespace (gt/endpoint-label endpoint))))
          (different-cache-retain? [endpoint]
            (nil? (namespace (gt/endpoint-label endpoint))))]
    (testing "positive cache limit with retain predicate"
      (with-clean-system {:cache-size 3
                          :cache-retain? cache-retain?}
        (let [cache (populate-cache cache)]
          (is (= 3 (limit cache)))
          (is (= 5 (count cache)))
          (is (= 2 (retained-count cache)))
          (is (= [(gt/endpoint 1 :b)
                  (gt/endpoint 1 :c)
                  (gt/endpoint 1 :d)
                  (gt/endpoint 1 :retain/a)
                  (gt/endpoint 1 :retain/b)]
                 (sort (keys cache))))
          (let [same-cache (reconfigure cache 3 cache-retain?)]
            (is (identical? cache same-cache)))
          (let [different-cache (reconfigure cache 3 different-cache-retain?)]
            (is (= 3 (limit different-cache)))
            (is (= [(gt/endpoint 1 :b)
                    (gt/endpoint 1 :c)
                    (gt/endpoint 1 :d)
                    (gt/endpoint 1 :retain/a)
                    (gt/endpoint 1 :retain/b)]
                   (sort (keys different-cache))))
            (is (= 5 (count different-cache)))
            (is (= 3 (retained-count different-cache)))
            (let [different-cache-populated (populate-cache different-cache)]
              (is (= 3 (limit different-cache-populated)))
              (is (= 6 (count different-cache-populated)))
              (is (= 4 (retained-count different-cache-populated)))))
          (let [larger-cache (reconfigure cache 4 cache-retain?)]
            (is (= 4 (limit larger-cache)))
            (is (= 5 (count larger-cache)))
            (is (= 2 (retained-count larger-cache)))
            (let [larger-cache-populated (populate-cache larger-cache)]
              (is (= 4 (limit larger-cache-populated)))
              (is (= 6 (count larger-cache-populated)))
              (is (= 2 (retained-count larger-cache-populated)))))
          (let [smaller-cache (reconfigure cache 2 cache-retain?)]
            (is (= 2 (limit smaller-cache)))
            (is (= 4 (count smaller-cache)))
            (is (= 2 (retained-count smaller-cache))))
          (let [non-retaining-cache (reconfigure cache 3 nil)]
            (is (= 3 (limit non-retaining-cache)))
            (is (= 3 (count non-retaining-cache)))
            (is (= 0 (retained-count non-retaining-cache)))
            (is (= 0 (retained-count (populate-cache non-retaining-cache)))))
          (let [unlimited-cache (reconfigure cache -1 nil)]
            (is (= -1 (limit unlimited-cache)))
            (is (= 5 (count unlimited-cache)))
            (is (= 0 (retained-count unlimited-cache)))
            (let [unlimited-cache-populated (populate-cache unlimited-cache)]
              (is (= -1 (limit unlimited-cache-populated)))
              (is (= 6 (count unlimited-cache-populated)))
              (is (= 0 (retained-count unlimited-cache-populated)))))
          (let [no-cache (reconfigure cache 0 nil)]
            (is (= 0 (limit no-cache)))
            (is (= 0 (count no-cache)))
            (is (= 0 (retained-count no-cache)))))))

    (testing "positive cache limit without retain predicate"
      (with-clean-system {:cache-size 3}
        (let [cache (populate-cache cache)]
          (is (= 3 (limit cache)))
          (is (= 3 (count cache)))
          (is (= 0 (retained-count cache)))
          (let [same-cache (reconfigure cache 3 nil)]
            (is (identical? cache same-cache)))
          (let [larger-cache (reconfigure cache 4 nil)]
            (is (= 4 (limit larger-cache)))
            (is (= 3 (count larger-cache)))
            (is (= 0 (retained-count larger-cache)))
            (let [larger-cache-populated (populate-cache larger-cache)]
              (is (= 4 (limit larger-cache-populated)))
              (is (= 4 (count larger-cache-populated)))
              (is (= 0 (retained-count larger-cache-populated)))))
          (let [smaller-cache (reconfigure cache 2 nil)]
            (is (= 2 (limit smaller-cache)))
            (is (= 2 (count smaller-cache)))
            (is (= 0 (retained-count smaller-cache))))
          (let [retaining-cache (reconfigure cache 3 cache-retain?)]
            (is (= 3 (limit retaining-cache)))
            (is (= 3 (count retaining-cache)))
            (is (= [(gt/endpoint 1 :c)
                    (gt/endpoint 1 :d)
                    (gt/endpoint 1 :retain/b)]
                   (sort (keys cache))))
            (is (= 1 (retained-count retaining-cache)))
            (let [retaining-cache-populated (populate-cache retaining-cache)]
              (is (= 3 (limit retaining-cache-populated)))
              (is (= 5 (count retaining-cache-populated)))
              (is (= 2 (retained-count retaining-cache-populated)))))
          (let [unlimited-cache (reconfigure cache -1 nil)]
            (is (= -1 (limit unlimited-cache)))
            (is (= 3 (count unlimited-cache)))
            (is (= 0 (retained-count unlimited-cache)))
            (let [unlimited-cache-populated (populate-cache unlimited-cache)]
              (is (= -1 (limit unlimited-cache-populated)))
              (is (= 6 (count unlimited-cache-populated)))
              (is (= 0 (retained-count unlimited-cache-populated)))))
          (let [no-cache (reconfigure cache 0 nil)]
            (is (= 0 (limit no-cache)))
            (is (= 0 (count no-cache)))
            (is (= 0 (retained-count no-cache)))))))

    (testing "zero cache limit (no caching)"
      (with-clean-system {:cache-size 0}
        (let [cache (populate-cache cache)]
          (is (= 0 (limit cache)))
          (is (= 0 (count cache)))
          (is (= 0 (retained-count cache)))
          (let [same-cache (reconfigure cache 0 nil)]
            (is (identical? cache same-cache)))
          (let [limited-cache (reconfigure cache 4 nil)]
            (is (= 4 (limit limited-cache)))
            (is (= 0 (count limited-cache)))
            (is (= 0 (retained-count limited-cache)))
            (let [limited-cache-populated (populate-cache limited-cache)]
              (is (= 4 (limit limited-cache-populated)))
              (is (= 4 (count limited-cache-populated)))
              (is (= 0 (retained-count limited-cache-populated)))))
          (let [retaining-cache (reconfigure cache 3 cache-retain?)]
            (is (= 3 (limit retaining-cache)))
            (is (= 0 (count retaining-cache)))
            (is (= 0 (retained-count retaining-cache)))
            (let [retaining-cache-populated (populate-cache retaining-cache)]
              (is (= 3 (limit retaining-cache-populated)))
              (is (= 5 (count retaining-cache-populated)))
              (is (= 2 (retained-count retaining-cache-populated)))))
          (let [unlimited-cache (reconfigure cache -1 nil)]
            (is (= -1 (limit unlimited-cache)))
            (is (= 0 (count unlimited-cache)))
            (is (= 0 (retained-count unlimited-cache)))
            (let [unlimited-cache-populated (populate-cache unlimited-cache)]
              (is (= -1 (limit unlimited-cache-populated)))
              (is (= 6 (count unlimited-cache-populated)))
              (is (= 0 (retained-count unlimited-cache-populated))))))))

    (testing "unlimited cache"
      (with-clean-system {:cache-size -1}
        (let [cache (populate-cache cache)]
          (is (= -1 (limit cache)))
          (is (= 6 (count cache)))
          (is (= 0 (retained-count cache)))
          (let [same-cache (reconfigure cache -1 nil)]
            (is (identical? cache same-cache)))
          (let [limited-cache (reconfigure cache 4 nil)]
            (is (= 4 (limit limited-cache)))
            (is (= 4 (count limited-cache)))
            (is (= 0 (retained-count limited-cache))))
          (let [retaining-cache (reconfigure cache 6 cache-retain?)]
            (is (= 6 (limit retaining-cache)))
            (is (= 6 (count retaining-cache)))
            (is (= 2 (retained-count retaining-cache))))
          (let [unlimited-cache (reconfigure cache -1 nil)]
            (is (= -1 (limit unlimited-cache)))
            (is (= 6 (count unlimited-cache)))
            (is (= 0 (retained-count unlimited-cache)))))))))
