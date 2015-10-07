(ns internal.connection-rules
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [support.test-support :refer :all]))

(g/defnode InputNode
  (input string-scalar String))

(g/defnode OutputNode
  (property string-scalar String)
  (property int-scalar Integer))

(deftest connection-rules
  (testing "compatible single value connections"
    (with-clean-system
      (let [[output input] (tx-nodes (g/make-node world OutputNode)
                                     (g/make-node world InputNode))]
        (g/connect! output :string-scalar input :string-scalar)
        (is (g/connected? (g/now) output :string-scalar input :string-scalar)))))

  (testing "incompatible single value connections"
    (with-clean-system
      (let [[output input] (tx-nodes (g/make-node world OutputNode)
                                     (g/make-node world InputNode))]
        (is (thrown? AssertionError
                     (g/connect! output :int-scalar input :string-scalar)))
        (is (not (g/connected? (g/now) output :string-scalar input :string-scalar)))))))

(deftest uni-connection-replaced
  (testing "single transaction"
    (with-clean-system
      (let [[out in next-out] (tx-nodes
                                (g/make-nodes world [out [OutputNode :string-scalar "first-val"]
                                                     in InputNode
                                                     next-out [OutputNode :string-scalar "second-val"]]
                                  (g/connect out :string-scalar in :string-scalar)
                                  (g/connect next-out :string-scalar in :string-scalar)))]
        (is (= "second-val" (g/node-value in :string-scalar))))))
  (testing "separate transactions"
    (with-clean-system
      (let [[out in next-out] (tx-nodes
                                (g/make-nodes world [out [OutputNode :string-scalar "first-val"]
                                                     in InputNode
                                                     next-out [OutputNode :string-scalar "second-val"]]
                                  (g/connect out :string-scalar in :string-scalar)))]
        (g/connect! next-out :string-scalar in :string-scalar)
        (is (= "second-val" (g/node-value in :string-scalar)))))))
