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
      (let [[node-id] (tx-nodes (g/make-node world SimpleDisposableNode))]
        (is (= "foo" (:v (g/node-value node-id :my-output))))
        (g/transact (g/set-property node-id :a-property "bar"))
        (is (= "bar" (:v (g/node-value node-id :my-output))))
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
      (let [[node-id] (tx-nodes (g/make-node world SimpleCachedDisposableNode))]
        (is (= "foo" (:v (g/node-value node-id :my-output))))
        (g/transact (g/set-property node-id :a-property "bar"))
        (g/dispose-pending!)
        (is (= 1 @dispose-counter))
        (is (= "bar" (:v (g/node-value node-id :my-output))))
        (g/transact (g/set-property node-id :a-property "baz"))
        (g/dispose-pending!)
        (is (= 2 @dispose-counter))
        (is (= "baz" (:v (g/node-value node-id :my-output))))))))

(g/defnode CachedPassthroughNode
  (input an-input g/Any)
  (output cached-output g/Any :cached (g/fnk [an-input] an-input)))

(deftest value-cached-in-multiple-outputs
  (testing "disposable values are only disposed once"
    (reset! dispose-counter 0)
    (with-clean-system
      (let [[source sink] (tx-nodes (g/make-node world SimpleCachedDisposableNode)
                                    (g/make-node world CachedPassthroughNode))]
        (g/connect! source :my-output sink :an-input)

        (g/node-value sink :cached-output)

        (g/invalidate! [[source :my-output]])

        (g/dispose-pending!)

        ;; TODO - this test demonstrates current behavior. The desired
        ;; behavior would be (= @dispose-counter 1)
        (is (= @dispose-counter 2))))))
