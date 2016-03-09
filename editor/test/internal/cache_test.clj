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
     (cache-encache cache [[:a 1] [:b 2] [:c 3]])
     (let [snapshot (cache-snapshot cache)]
       (are [k v] (= v (get snapshot k))
            :a 1
            :b 2
            :c 3))))

  (testing "one item can be decached"
    (with-clean-system
      (cache-encache cache [[:a 1] [:b 2] [:c 3]])
      (cache-invalidate cache [:b])
      (is (= {:a 1 :c 3} (as-map @cache)))))

  (testing "multiple items can be decached"
    (with-clean-system
      (cache-encache cache [[:a 1] [:b 2] [:c 3]])
      (cache-invalidate cache [:b :c])
      (cache-invalidate cache [:a])
      (is (empty? (as-map @cache))))))

(deftest limits
  (with-clean-system {:cache-size 3}
     (cache-encache cache [[[:a :a] 1] [[:b :b] 2] [[:c :c] 3]])
     (cache-hit cache [[:a :a] [:c :c]])
     (cache-encache cache [[[:d :d] 4]])
     (let [snapshot (cache-snapshot cache)]
       (are [k v] (= v (get snapshot k))
            [:a :a] 1
            [:c :c] 3
            [:d :d] 4))))
