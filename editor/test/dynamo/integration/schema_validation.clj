(ns dynamo.integration.schema-validation
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system tx-nodes]]))

(g/defnode TestNode
  (input str-input g/Str)
  (input int-input g/Int)
  (input sub-str-input g/Str :substitute "String substitute")
  (input sub-int-input g/Int :substitute 2675)

  (output str-output g/Str (g/always "I am a string."))
  (output int-output g/Int (g/always 99))

  (output bad-str-output g/Str (g/always 133))
  (output bad-int-output g/Int (g/always "I should be an Int."))

  (output str-pass-through g/Str (g/fnk [str-input] str-input))
  (output int-pass-through g/Str (g/fnk [int-input] int-input))

  (output cascade-str-pass-through g/Str (g/fnk [str-pass-through] str-pass-through))
  (output cascade-int-pass-through g/Str (g/fnk [int-pass-through] int-pass-through))

  (output sub-str-pass-through g/Str (g/fnk [sub-str-input] sub-str-input))
  (output sub-int-pass-through g/Str (g/fnk [sub-int-input] sub-int-input))

  (output cascade-sub-str-pass-through g/Str (g/fnk [sub-str-pass-through] sub-str-pass-through))
  (output cascade-sub-int-pass-through g/Str (g/fnk [sub-int-pass-through] sub-int-pass-through)))

(deftest test-schema-validations-on-connect
  (testing "matching schemas connect"
    (with-clean-system
      (let [[node] (tx-nodes (g/make-node world TestNode))]
        (is (not (nil? (g/transact (g/connect node :str-output node :str-input)))))
        (is (not (nil? (g/transact (g/connect node :int-output node :int-input))))))))

  (testing "mismatched schemas do not connect"
    (with-clean-system
      (let [[node] (tx-nodes (g/make-node world TestNode))]
        (is (thrown? AssertionError (g/transact (g/connect node :int-output node :str-input))))
        (is (thrown? AssertionError (g/transact (g/connect node :str-output node :int-input))))))))

(deftest test-schema-validations-on-value-production
  (testing "values that match input schemas produce values"
    (with-clean-system
      (let [[node] (tx-nodes (g/make-node world TestNode))]
        (g/transact (g/connect node :str-output node :str-input))
        (is (= "I am a string." (g/node-value node :str-pass-through))))))

  (testing "values that do not match input schemas produce errors"
    (with-clean-system
      (with-redefs [internal.node/warn (constantly nil)]
        (let [[node] (tx-nodes (g/make-node world TestNode))]
          (g/transact [ (g/connect node :bad-str-output node :str-input)
                        (g/connect node :bad-int-output node :int-input)])
          (is (thrown-with-msg? Exception #"Error Value Found in Node." (g/node-value node :str-pass-through)))
          (is (thrown-with-msg? Exception #"Error Value Found in Node." (g/node-value node :int-pass-through)))))))

  (testing "disconnected mismatched values do not produce errors"
    (with-clean-system
      (let [[node] (tx-nodes (g/make-node world TestNode))]
        (is (nil? (g/node-value node :str-pass-through)))
        (is (nil? (g/node-value node :int-pass-through))))))

  (testing "values that do not match input schemas produce errors that cascade through"
    (with-clean-system
      (with-redefs [internal.node/warn (constantly nil)]
        (let [[node] (tx-nodes (g/make-node world TestNode))]
          (g/transact [ (g/connect node :bad-str-output node :str-input)
                        (g/connect node :bad-int-output node :int-input)])
          (is (thrown-with-msg? Exception #"Error Value Found in Node." (g/node-value node :cascade-str-pass-through)))
          (is (thrown-with-msg? Exception #"Error Value Found in Node." (g/node-value node :cascade-int-pass-through)))))))

  (testing "values that do not match input schemas with substitutions produce substitue values"
    (with-clean-system
      (with-redefs [internal.node/warn (constantly nil)]
        (let [[node] (tx-nodes (g/make-node world TestNode))]
          (g/transact [ (g/connect node :bad-str-output node :sub-str-input)
                        (g/connect node :bad-int-output node :sub-int-input)])
          (is (= "String substitute" (g/node-value node :sub-str-pass-through)))
          (is (= 2675 (g/node-value node :sub-int-pass-through)))))))

  (testing "values that do not match input schemas with substitutions produce substitue values that cascade through"
(with-clean-system
      (with-redefs [internal.node/warn (constantly nil)]
        (let [[node] (tx-nodes (g/make-node world TestNode))]
          (g/transact [ (g/connect node :bad-str-output node :sub-str-input)
                        (g/connect node :bad-int-output node :sub-int-input)])
          (is (= "String substitute" (g/node-value node :cascade-sub-str-pass-through)))
          (is (= 2675 (g/node-value node :cascade-sub-int-pass-through))))))))
