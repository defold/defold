(ns internal.cache-test
  (:require [clojure.core.async :as a]
            [clojure.test :refer :all]
            [com.stuartsierra.component :as component]
            [dynamo.system.test-support :refer :all]
            [dynamo.types :as t]
            [internal.async :as ia]
            [internal.cache :refer :all]))

(def null-chan (a/chan (a/sliding-buffer 1)))

(defn- mock-system [cache-size ch]
  {:world {:state (ref {})}
   :cache (component/using (cache-subsystem cache-size ch) [:world])})

(defn resize-cache
  [system new-cache-size]
  (let [oldc (component/stop (:cache system))
        newc (assoc (cache-subsystem new-cache-size (:dispose-ch oldc))
                    :world (:world system))]
    (assoc system :cache (component/start newc))))

(defn- as-map [c] (select-keys c (keys c)))

(defn cache-with-stuff
  []
  (let [system          (component/start-system (mock-system 1000 null-chan))
        cache-component (:cache system)]
    (cache-encache cache-component [[:a 1] [:b 2] [:c 3]])
    cache-component))

(deftest cache-lifecycle-tests
  (testing "has an embedded cache"
    (let [system          (component/start-system (mock-system 1000 nil))
          cache-component (:cache system)]
      (is (not (nil? (cache-snapshot cache-component))))))

  (testing "things put into cache appear in snapshot"
    (let [system          (component/start-system (mock-system 1000 nil))
          cache-component (:cache system)]
      (cache-encache cache-component [[:a 1] [:b 2] [:c 3]])
      (let [snapshot (cache-snapshot cache-component)]
        (are [k v] (= v (get snapshot k))
             :a 1
             :b 2
             :c 3))))

  (testing "multiple starts are idempotent"
    (let [system          (component/start-system (mock-system 1000 nil))
          cache-component (:cache system)]
      (cache-encache cache-component [[:a 1] [:b 2] [:c 3]])
      (let [snapshot (cache-snapshot cache-component)
            cc2      (component/start cache-component)]
        (is (= snapshot (cache-snapshot cc2))))))

  (testing "after stop, is empty"
    (let [system          (component/start-system (mock-system 1000 nil))
          cache-component (:cache system)]
      (cache-encache cache-component [[:a 1] [:b 2] [:c 3]])
      (let [cc2 (component/stop cache-component)]
        (is (empty? (cache-snapshot cc2))))))

  (testing "multiple stops are idempotent"
    (let [system          (component/start-system (mock-system 1000 nil))
          cache-component (:cache system)]
      (cache-encache cache-component [[:a 1] [:b 2] [:c 3]])
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

(defrecord DisposableThing [v]
  t/IDisposable
  (dispose [this] v))

(defn- thing [v] (DisposableThing. v))

(deftest value-disposal
  (testing "one value disposed when decached"
    (with-clean-system
      (let [system (resize-cache system 3)
            cache  (:cache system)]
        (cache-encache cache [[:a (thing 1)] [:b (thing 2)] [:c (thing 3)]])
        (cache-invalidate cache [:a])
        (yield)
        (let [disposed (take-waiting-to-dispose system)]
          (is (= 1 (count disposed)))
          (is (= [1] (map t/dispose disposed)))))))

  (testing "multiple values disposed when decached"
    (with-clean-system
      (let [system (resize-cache system 3)
            cache  (:cache system)]
        (cache-encache cache [[:a (thing 1)] [:b (thing 2)] [:c (thing 3)]])
        (cache-invalidate cache [:b :c])
        (yield)
        (let [disposed (take-waiting-to-dispose system)]
          (is (= 2 (count disposed)))
          (is (= [2 3] (map t/dispose disposed)))))))

  (testing "values that are pushed out also get disposed"
    (with-clean-system
      (let [system (resize-cache system 1)
            cache  (:cache system)]
        (def c* cache)
        (cache-encache cache [[:a (thing 1)]])
        (yield)
        (cache-encache cache [[:b (thing 2)] [:c (thing 3)]])
        (yield)
        (let [disposed (take-waiting-to-dispose system)]
          (is (= 2 (count disposed)))
          (is (= [1 2] (map t/dispose disposed))))))))
