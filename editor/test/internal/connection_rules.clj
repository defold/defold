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
        (is (g/connected? (g/now) (g/node-id output) :string-scalar (g/node-id input) :string-scalar)))))

  (testing "incompatible single value connections"
    (with-clean-system
      (let [[output input] (tx-nodes (g/make-node world OutputNode)
                                     (g/make-node world InputNode))]
        (is (thrown? AssertionError
                     (g/connect! output :int-scalar input :string-scalar)))
        (is (not (g/connected? (g/now) (g/node-id output) :string-scalar (g/node-id input) :string-scalar)))
        ))))
