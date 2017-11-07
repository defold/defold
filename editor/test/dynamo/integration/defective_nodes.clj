(ns dynamo.integration.defective-nodes
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system tx-nodes]]))

(defn- cache-peek [cache node-id label]
  (get cache [node-id label]))

(defn- cached? [cache node-id label]
  (not (nil? (cache-peek cache node-id label))))

(g/defnode OrdinaryNode
  (property a-property g/Str (default "a"))
  (property b-property g/Int (default 0))
  (extern   external-ref g/Str (default "/foo"))

  (output catted g/Str (g/fnk [a-property b-property] (str a-property b-property)))
  (output upper  g/Str :cached (g/fnk [a-property] (.toUpperCase a-property))))

(g/defnode StringConsumer
  (input a-string g/Str)
  (output duplicated g/Str :cached (g/fnk [a-string] (str a-string a-string))))

(deftest defective-node-overrides-outputs
  (with-clean-system
    (let [[node] (tx-nodes (g/make-node world OrdinaryNode))]
      (is (= "a0" (g/node-value node :catted)))
      (is (= "A"  (g/node-value node :upper)))

      (g/transact (g/mark-defective node :bad-value))

      (is (= :bad-value   (g/node-value node :catted)))
      (is (= :bad-value   (g/node-value node :upper))))))

(deftest defective-nodes-values-are-decached
  (with-clean-system
    (let [[node downstream prop-consumer] (tx-nodes (g/make-node world OrdinaryNode)
                                                    (g/make-node world StringConsumer)
                                                    (g/make-node world StringConsumer))]

      (g/transact [(g/connect node :upper downstream :a-string)
                   (g/connect node :a-property prop-consumer :a-string)])

      (is (= "A"  (g/node-value node :upper)))
      (is (= "AA" (g/node-value downstream :duplicated)))
      (is (= "aa" (g/node-value prop-consumer :duplicated)))

      (let [cache (g/cache)]
        (are [n l] (cached? cache n l)
          node          :upper
          downstream    :duplicated
          prop-consumer :duplicated))

      (g/transact (g/mark-defective node :bad-value))

      (let [cache (g/cache)]
        (are [n l] (not (cached? cache n l))
          node          :upper
          downstream    :duplicated
          prop-consumer :duplicated)))))

(deftest defective-node-excludes-externs
  (with-clean-system
    (let [[node] (tx-nodes (g/make-node world OrdinaryNode))]
      (is (= "/foo" (g/node-value node :external-ref)))

      (g/transact (g/mark-defective node :bad-value))

      (is (= "/foo" (g/node-value node :external-ref))))))

(deftest defective-node-excludes-intrinsics
  (with-clean-system
    (let [[node] (tx-nodes (g/make-node world OrdinaryNode))]

      (g/transact (g/mark-defective node :bad-value))

      (is (= node (g/node-value node :_node-id)))
      (is (contains? (g/node-value node :_output-jammers) :catted)))))
