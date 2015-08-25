(ns dynamo.integration.dependencies
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system tx-nodes]]))

(g/defnode ChainedOutputNode
  (property foo g/Str (default "c"))
  (output a-output g/Str (g/fnk [foo] (str foo "a")))
  (output b-output g/Str (g/fnk [a-output] (str a-output "k")))
  (output c-output g/Str (g/fnk [b-output] (str b-output "e"))))

(g/defnode GladosNode
  (input  encouragement g/Str)
  (output cake-output g/Str (g/fnk [encouragement] (str "The " encouragement " is a lie"))))

(deftest test-dependencies-within-same-node
  (testing "dependencies from one output to another"
    (with-clean-system
      (let [[node-id] (tx-nodes (g/make-node world ChainedOutputNode))]
        (is (= "cake" (g/node-value node-id :c-output)))
        (are [label expected-dependencies] (= (set (map (fn [l] [node-id l]) expected-dependencies))
                                              (set (g/dependencies (g/now) [[node-id label]])))
          :c-output  [:c-output]
          :b-output  [:b-output :c-output]
          :a-output  [:a-output :b-output :c-output]
          :foo       [:a-output :b-output :c-output :foo :_declared-properties :_properties]))))

  (testing "dependencies from one output to another, across nodes"
    (with-clean-system
      (let [[anode-id gnode-id] (tx-nodes (g/make-node world ChainedOutputNode)
                                          (g/make-node world GladosNode))]
        (g/transact
         (g/connect anode-id :c-output gnode-id :encouragement))
        (is (= #{[anode-id :c-output]
                 [anode-id :b-output]
                 [gnode-id :cake-output]}
               (set (g/dependencies (g/now) [[anode-id :b-output]]))))))))
