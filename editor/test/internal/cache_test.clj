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
            [support.test-support :refer [with-clean-system]]))

(defn- as-map [c] (select-keys c (keys c)))

(deftest items-in-and-out-test
  (testing "things put into cache appear in snapshot"
    (with-clean-system {:cache-size 1000}
      (let [snapshot (-> cache
                         (cache-encache {(gt/endpoint 1 :a) 1
                                         (gt/endpoint 1 :b) 2
                                         (gt/endpoint 1 :c) 3}
                                        ::retain-arg))]
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
                                 (gt/endpoint 1 :c) 3}
                                ::retain-arg)
                 (cache-invalidate [(gt/endpoint 1 :b)])
                 (as-map))))))

  (testing "multiple items can be decached"
    (with-clean-system
      (is (empty? (-> cache
                      (cache-encache {(gt/endpoint 1 :a) 1
                                      (gt/endpoint 1 :b) 2
                                      (gt/endpoint 1 :c) 3}
                                     ::retain-arg)
                      (cache-invalidate [(gt/endpoint 1 :b) (gt/endpoint 1 :c)])
                      (cache-invalidate [(gt/endpoint 1 :a)])
                      (as-map)))))))

(deftest limits-test
  (letfn [(populate-cache [cache]
            (-> cache
                (cache-encache {(gt/endpoint 1 :a) 1
                                (gt/endpoint 1 :b) 2
                                (gt/endpoint 1 :c) 3}
                               ::retain-arg)
                (cache-hit [(gt/endpoint 1 :a)
                            (gt/endpoint 1 :c)])
                (cache-encache {(gt/endpoint 1 :d) 4}
                               ::retain-arg)))]

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
            (cache-encache cache
                           {(gt/endpoint 1 :a) 1
                            (gt/endpoint 1 :b) 2
                            (gt/endpoint 1 :c) 3
                            (gt/endpoint 1 :d) 4}
                           ::retain-arg))]

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
        (is (= -1 (limit cache)))
        (let [cache (populate-cache cache)]
          (is (= 4 (count cache)))
          (let [cache (cache-clear cache)]
            (is (= 0 (count cache)))
            (is (= -1 (limit cache)))
            (let [cache (populate-cache cache)]
              (is (= 4 (count cache))))))))))

(defn- make-helper-functions [base retain? expected-retain-arg]
  (letfn [(populate-cache [cache]
            (cache-encache cache base expected-retain-arg))
          (cache-retain?
            ([endpoint]
             (retain? endpoint))
            ([retain-arg endpoint]
             (assert (= expected-retain-arg retain-arg))
             (cache-retain? endpoint)))]
    {:populate-cache populate-cache
     :cache-retain? cache-retain?}))

(deftest retain-test
  (let [{:keys [populate-cache cache-retain?]}
        (make-helper-functions {(gt/endpoint 1 :a) 1
                                (gt/endpoint 1 :b) 2
                                (gt/endpoint 1 :retain/a) -1
                                (gt/endpoint 1 :retain/b) -2
                                (gt/endpoint 1 :c) 3
                                (gt/endpoint 1 :d) 4}
                               #(= "retain" (namespace (gt/endpoint-label %)))
                               ::retain-test-expected-retain-arg)]
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
  (let [{:keys [populate-cache cache-retain?]}
        (make-helper-functions {(gt/endpoint 1 :a) 1
                                (gt/endpoint 1 :b) 2
                                (gt/endpoint 1 :retain/a) -1
                                (gt/endpoint 1 :retain/b) -2
                                (gt/endpoint 1 :c) 3
                                (gt/endpoint 1 :d) 4}
                               #(= "retain" (namespace (gt/endpoint-label %)))
                               ::retained-count-test-expected-retain-arg)]

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

(deftest unmodified-identical-test
  (let [expected-retain-arg ::unmodified-identical-test-expected-retain-arg
        {:keys [populate-cache cache-retain?]}
        (make-helper-functions {(gt/endpoint 1 :a) 1
                                (gt/endpoint 1 :b) 2
                                (gt/endpoint 1 :retain/a) -1
                                (gt/endpoint 1 :retain/b) -2
                                (gt/endpoint 1 :c) 3
                                (gt/endpoint 1 :d) 4}
                               #(= "retain" (namespace (gt/endpoint-label %)))
                               expected-retain-arg)]

    (testing "positive cache limit with retain predicate"
      (with-clean-system {:cache-size 3
                          :cache-retain? cache-retain?}
        (let [cache (populate-cache cache)]
          (is (identical? cache (cache-hit cache [])))
          (is (identical? cache (cache-hit cache [(gt/endpoint 1 :missing)])))
          (is (identical? cache (cache-hit cache [(gt/endpoint 1 :retain/a)])))
          (is (identical? cache (cache-encache cache [] expected-retain-arg)))
          (is (identical? cache (cache-invalidate cache [])))
          (is (identical? cache (cache-invalidate cache [(gt/endpoint 1 :missing)]))))))

    (testing "positive cache limit without retain predicate"
      (with-clean-system {:cache-size 3}
        (let [cache (populate-cache cache)]
          (is (identical? cache (cache-hit cache [])))
          (is (identical? cache (cache-hit cache [(gt/endpoint 1 :missing)])))
          (is (identical? cache (cache-encache cache [] expected-retain-arg)))
          (is (identical? cache (cache-invalidate cache [])))
          (is (identical? cache (cache-invalidate cache [(gt/endpoint 1 :missing)]))))))

    (testing "zero cache limit (no caching)"
      (with-clean-system {:cache-size 0}
        (let [cache (populate-cache cache)]
          (is (identical? cache (cache-hit cache [])))
          (is (identical? cache (cache-hit cache [(gt/endpoint 1 :missing)])))
          (is (identical? cache (cache-hit cache [(gt/endpoint 1 :a)])))
          (is (identical? cache (cache-encache cache [] expected-retain-arg)))
          (is (identical? cache (cache-invalidate cache [])))
          (is (identical? cache (cache-invalidate cache [(gt/endpoint 1 :missing)]))))))

    (testing "unlimited cache"
      (with-clean-system {:cache-size -1}
        (let [cache (populate-cache cache)]
          (is (identical? cache (cache-hit cache [])))
          (is (identical? cache (cache-hit cache [(gt/endpoint 1 :missing)])))
          (is (identical? cache (cache-hit cache [(gt/endpoint 1 :a)])))
          (is (identical? cache (cache-encache cache [] expected-retain-arg)))
          (is (identical? cache (cache-invalidate cache [])))
          (is (identical? cache (cache-invalidate cache [(gt/endpoint 1 :missing)]))))))))
