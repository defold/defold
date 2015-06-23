(ns internal.cache-test
  (:require [clojure.test :refer :all]
            [dynamo.graph.test-support :refer :all]
            [internal.cache :refer :all]
            [internal.graph.types :as gt]
            [internal.system :as is]))

(defn- mock-system [cache-size ch]
  (is/make-system {:cache-size cache-size :disposal-queue ch}))

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

(defrecord DisposableThing [v]
  gt/IDisposable
  (dispose [this] v))

(defn- thing [v] (DisposableThing. v))

(deftest value-disposal
  (testing "one value disposed when decached"
    (with-clean-system {:cache-size 3}
      (cache-encache cache [[:a (thing 1)] [:b (thing 2)] [:c (thing 3)]])
      (cache-invalidate cache [:a])
      (yield)
      (let [disposed (take-waiting-to-dispose system)]
        (is (= 1 (count disposed)))
        (is (= [1] (map gt/dispose disposed))))))

  (testing "multiple values disposed when decached"
    (with-clean-system {:cache-size 3}
      (cache-encache cache [[:a (thing 1)] [:b (thing 2)] [:c (thing 3)]])
      (cache-invalidate cache [:b :c])
      (yield)
      (let [disposed (take-waiting-to-dispose system)]
        (is (= 2 (count disposed)))
        (is (= [2 3] (map gt/dispose disposed))))))

  (testing "values that are pushed out also get disposed"
    (with-clean-system {:cache-size 1}
      (cache-encache cache [[:a (thing 1)]])
      (yield)
      (cache-encache cache [[:b (thing 2)] [:c (thing 3)]])
      (yield)
      (let [disposed (take-waiting-to-dispose system)]
        (is (= 2 (count disposed)))
        (is (= [1 2] (map gt/dispose disposed)))))))
