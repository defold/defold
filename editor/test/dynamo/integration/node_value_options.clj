(ns dynamo.integration.node-value-options
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [support.test-support :as ts]))

;;; ----------------------------------------
;;; :no-cache

(g/defnode CacheTestNode
  (input first-name String)
  (input last-name  String)
  (input operand    String)

  (property scalar g/Str)

  (output uncached-value  String         (g/fnk [scalar] scalar))
  (output expensive-value String :cached (g/fnk [] "this took a long time to produce"))
  (output nickname        String :cached (g/fnk [first-name] first-name))
  (output derived-value   String :cached (g/fnk [first-name last-name] (str first-name " " last-name)))
  (output another-value   String :cached (g/fnk [] "this is distinct from the other outputs"))
  (output nil-value       String :cached (g/fnk [this] nil)))

(defn- cached? [cache node label]
  (contains? @cache [node label]))

(defn- touch [node label] (g/node-value node label {:no-cache true}))

(defn- test-node [world] (first (ts/tx-nodes (g/make-node world CacheTestNode))))
(defn- connected-test-nodes
  [world]
  (ts/tx-nodes
   (g/make-nodes world
                 [node1 CacheTestNode
                  node2 CacheTestNode]
                 (g/connect node1 :derived-value node2 :first-name))))

(deftest node-value-allows-no-cache-option
  (testing ":no-cache options works on direct outputs"
    (ts/with-clean-system
      (let [node (test-node world)]
        (touch node :derived-value)
        (is (not (cached? cache node :derived-value))))))

  (testing ":no-cache option works on indirect outputs"
    (ts/with-clean-system
      (let [[node1 node2] (connected-test-nodes world)]
        (touch node2 :derived-value)
        (is (not (cached? cache node1 :derived-value)))
        (is (not (cached? cache node2 :derived-value)))))))


;;; ----------------------------------------
;;; :ignore-property-errors

(g/defnode PropertyNode
  (property pos-int g/Int
            (validate positive? (g/fnk [pos-int] (pos? pos-int))))

  (output   int-user g/Int
            (g/fnk [pos-int] (Math/log pos-int))))

(defn- property-test-node
  [world]
  (first
   (ts/tx-nodes
    (g/make-node world PropertyNode))))

(deftest node-value-allows-ignore-property-error-option
  (testing ":ignore-property-error works directly"
    (ts/with-clean-system
      (let [node (property-test-node world)]
        (g/set-property! node :pos-int -5)
        (is (= -5 (g/node-value node :pos-int)))
        (is (not (empty? (-> (g/node-value node :_properties) :properties :pos-int :validation-problems)))))))

  (testing "invalid properties may be consumed when :ignore-property-error is true"
    (ts/with-clean-system
      (let [node (property-test-node world)]
        (g/set-property! node :pos-int -5)
        (is (.isNaN (g/node-value node :int-user)))))))
