(ns dynamo.integration.value-disposal
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system tx-nodes]]))

(def dispose-counter (atom 0))

(defrecord DisposableThing [v]
  g/IDisposable
  (dispose [this] (swap! dispose-counter inc) v))

(g/defnode SimpleDisposableNode
  (property a-property g/Str (default "foo"))
  (output my-output g/Any (g/fnk [a-property] (DisposableThing. a-property))))

(deftest non-cached-value-disposing
  (testing "values that are not cached are not disposed"
    (reset! dispose-counter 0)
    (with-clean-system {:cache-size 3}
      (let [[node] (tx-nodes (g/make-node world SimpleDisposableNode))]
        (is (= "foo" (:v (g/node-value node :my-output))))
        (g/transact (g/set-property node :a-property "bar"))
        (is (= "bar" (:v (g/node-value node :my-output))))
        (g/dispose-pending!)
        (is (= 0 @dispose-counter))))))

(g/defnode SimpleCachedDisposableNode
  (property a-property g/Str (default "foo"))
  (output my-output g/Any :cached (g/fnk [a-property] (DisposableThing. a-property))))

(deftest cached-value-disposing
  (testing "disposable values that are cached and not invalidated are not disposed"
    (reset! dispose-counter 0)
    (with-clean-system {:cache-size 3}
      (let [[node] (tx-nodes (g/make-node world SimpleCachedDisposableNode))]
        (is (= "foo" (:v (g/node-value node :my-output))))
        (g/dispose-pending!)
        (is (= 0 @dispose-counter)))))

  (testing "disposable values that are cached and invalidated are disposed"
    (reset! dispose-counter 0)
    (with-clean-system {:cache-size 3}
      (let [[node] (tx-nodes (g/make-node world SimpleCachedDisposableNode))]
        (is (= "foo" (:v (g/node-value node :my-output))))
        (g/transact (g/set-property node :a-property "bar"))
        (g/dispose-pending!)
        (is (= 1 @dispose-counter))
        (is (= "bar" (:v (g/node-value node :my-output))))
        (g/transact (g/set-property node :a-property "baz"))
        (g/dispose-pending!)
        (is (= 2 @dispose-counter))
        (is (= "baz" (:v (g/node-value node :my-output))))))))
