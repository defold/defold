(ns internal.cache-test
  (:require [clojure.test :refer :all]
            [com.stuartsierra.component :as component]
            [internal.cache :refer :all]))

(defn- as-map    [c]       (select-keys c (keys c)))

(defn cache-with-stuff
  []
  (let [cache-component (component/start (make-cache-component 1000 nil))]
    (cache-hit cache-component [[:a 1] [:b 2] [:c 3]])
    cache-component))

(deftest cache-lifecycle-tests
  (testing "has an embedded cache"
    (let [cache-component (component/start (make-cache-component 1000 nil))]
      (is (not (nil? (cache-snapshot cache-component))))))

  (testing "things put into cache appear in snapshot"
    (let [cache-component (component/start (make-cache-component 1000 nil))]
      (cache-hit cache-component [[:a 1] [:b 2] [:c 3]])
      (let [snapshot (cache-snapshot cache-component)]
        (are [k v] (= v (get snapshot k))
             :a 1
             :b 2
             :c 3))))

  (testing "multiple starts are idempotent"
    (let [cache-component (component/start (make-cache-component 1000 nil))]
      (cache-hit cache-component [[:a 1] [:b 2] [:c 3]])
      (let [snapshot (cache-snapshot cache-component)
            cc2      (component/start cache-component)]
        (is (= snapshot (cache-snapshot cc2))))))

  (testing "after stop, is empty"
    (let [cache-component (component/start (make-cache-component 1000 nil))]
      (cache-hit cache-component [[:a 1] [:b 2] [:c 3]])
      (let [cc2 (component/stop cache-component)]
        (is (empty? (cache-snapshot cc2))))))

  (testing "multiple stops are idempotent"
    (let [cache-component (component/start (make-cache-component 1000 nil))]
      (cache-hit cache-component [[:a 1] [:b 2] [:c 3]])
      (let [cc2 (component/stop cache-component)
            cc3 (component/stop cc2)]
        (is (= cc2 cc3))))))

(deftest item-removal
  (testing "one item can be decached"
    (let [primed-cache (cache-with-stuff)]
      (cache-invalidate primed-cache [:b])
      (is (= {:a 1 :c 3} (as-map @primed-cache)))))

  (testing "multiple items can be decached"
    (let [primed-cache (cache-with-stuff)]
      (cache-invalidate primed-cache [:b :c])
      (cache-invalidate primed-cache [:a])
      (is (empty? (as-map @primed-cache))))))

(run-tests)
