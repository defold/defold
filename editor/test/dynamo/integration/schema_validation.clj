(ns dynamo.integration.schema-validation
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system tx-nodes]]))

(g/defnode BaseNode
  (input str-input g/Str)
  (input int-input g/Int)

  (input sub-str-input g/Str :substitute "String substitute")
  (input sub-int-input g/Int :substitute 2675)

  (output str-output g/Str (g/always "I am a string."))
  (output int-output g/Int (g/always 99))

  (output bad-str-output g/Str (g/always 133))
  (output bad-int-output g/Int (g/always "I should be an Int."))

  (output str-pass-through g/Str (g/fnk [str-input] str-input))
  (output int-pass-through g/Int (g/fnk [int-input] int-input))

  (output cascade-str-pass-through g/Str (g/fnk [str-pass-through] str-pass-through))
  (output cascade-int-pass-through g/Int (g/fnk [int-pass-through] int-pass-through))

  (output sub-str-pass-through g/Str (g/fnk [sub-str-input] sub-str-input))
  (output sub-int-pass-through g/Int (g/fnk [sub-int-input] sub-int-input))

  (output cascade-sub-str-pass-through g/Str (g/fnk [sub-str-pass-through] sub-str-pass-through))
  (output cascade-sub-int-pass-through g/Int (g/fnk [sub-int-pass-through] sub-int-pass-through)))

(g/defnode  ArrayNode
  (input str-array-input g/Str :array)
  (input int-array-input g/Int :array)

  (input sub-str-array-input g/Str :array :substitute "String array substitute")
  (input sub-int-array-input g/Int :array  :substitute 1)

  (output str-array-pass-through g/Str (g/fnk [str-array-input] str-array-input))
  (output int-array-pass-through g/Int (g/fnk [int-array-input] int-array-input))

  (output cascade-str-array-pass-through g/Str (g/fnk [str-array-pass-through] str-array-pass-through))
  (output cascade-int-array-pass-through g/Int (g/fnk [int-array-pass-through] int-array-pass-through))

  (output sub-str-array-pass-through [g/Str] (g/fnk [sub-str-array-input] sub-str-array-input))
  (output sub-int-array-pass-through [g/Int] (g/fnk [sub-int-array-input] sub-int-array-input))

  (output cascade-sub-str-array-pass-through [g/Str] (g/fnk [sub-str-array-pass-through] sub-str-array-pass-through))
  (output cascade-sub-int-array-pass-through [g/Int] (g/fnk [sub-int-array-pass-through] sub-int-array-pass-through)))

(deftest test-schema-validations-on-connect
  (testing "matching schemas connect"
    (with-clean-system
      (let [[b-node a-node] (tx-nodes (g/make-node world BaseNode)
                                      (g/make-node world ArrayNode))]
        (is (not (nil? (g/transact (g/connect b-node :str-output b-node :str-input)))))
        (is (not (nil? (g/transact (g/connect b-node :int-output b-node :int-input)))))
        (is (not (nil? (g/transact (g/connect b-node :str-output a-node :str-array-input)))))
        (is (not (nil? (g/transact (g/connect b-node :int-output a-node :int-array-input))))))))

  (testing "mismatched schemas do not connect"
    (with-clean-system
      (let [[b-node a-node] (tx-nodes (g/make-node world BaseNode)
                                      (g/make-node world ArrayNode))]
        (is (thrown? AssertionError (g/transact (g/connect b-node :int-output b-node :str-input))))
        (is (thrown? AssertionError (g/transact (g/connect b-node :str-output b-node :int-input))))
        (is (thrown? AssertionError (g/transact (g/connect b-node :int-output a-node :str-array-input))))
        (is (thrown? AssertionError (g/transact (g/connect b-node :str-output a-node :int-array-input))))))))

(deftest test-schema-validations-on-value-production
  (testing "values that match input schemas produce values"
    (with-clean-system
      (let [[b-node a-node] (tx-nodes (g/make-node world BaseNode)
                                      (g/make-node world ArrayNode))]
        (g/transact [ (g/connect b-node :str-output b-node :str-input)
                      (g/connect b-node :int-output b-node :int-input)
                      (g/connect b-node :str-output a-node :str-array-input)
                      (g/connect b-node :int-output a-node :int-array-input)])
        (is (= "I am a string." (g/node-value b-node :str-pass-through)))
        (is (= 99 (g/node-value b-node :int-pass-through)))
        (is (= ["I am a string."] (g/node-value a-node :str-array-pass-through)))
        (is (= [99] (g/node-value a-node :int-array-pass-through))))))

  (testing "values that do not match input schemas produce errors"
    (with-clean-system
      (with-redefs [internal.node/warn (constantly nil)]
        (let [[b-node a-node] (tx-nodes (g/make-node world BaseNode)
                                        (g/make-node world ArrayNode))]
          (g/transact [ (g/connect b-node :bad-str-output b-node :str-input)
                        (g/connect b-node :bad-int-output b-node :int-input)
                        (g/connect b-node :bad-str-output a-node :str-array-input)
                        (g/connect b-node :bad-int-output a-node :int-array-input)])

          (is (thrown-with-msg? Exception #"SCHEMA-VALIDATION" (g/node-value b-node :str-pass-through)))
          (is (thrown-with-msg? Exception #"SCHEMA-VALIDATION" (g/node-value b-node :int-pass-through)))
          (is (thrown-with-msg? Exception #"SCHEMA-VALIDATION" (g/node-value a-node :str-array-pass-through)))
          (is (thrown-with-msg? Exception #"SCHEMA-VALIDATION" (g/node-value a-node :int-array-pass-through)))))))

  (testing "disconnected mismatched values do not produce errors"
    (with-clean-system
      (let [[b-node a-node] (tx-nodes (g/make-node world BaseNode)
                                      (g/make-node world ArrayNode))]
        (is (nil? (g/node-value b-node :str-pass-through)))
        (is (nil? (g/node-value b-node :int-pass-through)))
        (is (= [] (g/node-value a-node :str-array-pass-through)))
        (is (= [] (g/node-value a-node :int-array-pass-through))))))

  (testing "values that do not match input schemas produce errors with cascades"
    (with-clean-system
      (with-redefs [internal.node/warn (constantly nil)]
        (let [[b-node a-node] (tx-nodes (g/make-node world BaseNode)
                                        (g/make-node world ArrayNode))]
          (g/transact [ (g/connect b-node :bad-str-output b-node :str-input)
                        (g/connect b-node :bad-int-output b-node :int-input)
                        (g/connect b-node :bad-str-output a-node :str-array-input)
                        (g/connect b-node :bad-int-output a-node :int-array-input)])
          (is (thrown-with-msg? Exception #"SCHEMA-VALIDATION" (g/node-value b-node :cascade-str-pass-through)))
          (is (thrown-with-msg? Exception #"SCHEMA-VALIDATION" (g/node-value b-node :cascade-int-pass-through)))
          (is (thrown-with-msg? Exception #"SCHEMA-VALIDATION" (g/node-value a-node :cascade-str-array-pass-through)))
          (is (thrown-with-msg? Exception #"SCHEMA-VALIDATION" (g/node-value a-node :cascade-int-array-pass-through)))))))

  (testing "values that do not match input schemas with substitutions produce errors not substitutes"
    (with-clean-system
      (with-redefs [internal.node/warn (constantly nil)]
        (let [[b-node a-node] (tx-nodes (g/make-node world BaseNode)
                                        (g/make-node world ArrayNode))]
          (g/transact [ (g/connect b-node :bad-str-output b-node :sub-str-input)
                        (g/connect b-node :bad-int-output b-node :sub-int-input)
                        (g/connect b-node :bad-str-output a-node :sub-str-array-input)
                        (g/connect b-node :bad-int-output a-node :sub-int-array-input)])
          (is (thrown-with-msg? Exception #"SCHEMA-VALIDATION" (g/node-value b-node :sub-str-pass-through)))
          (is (thrown-with-msg? Exception #"SCHEMA-VALIDATION" (g/node-value b-node :sub-int-pass-through)))
          (is (thrown-with-msg? Exception #"SCHEMA-VALIDATION" (g/node-value a-node :sub-str-array-pass-through)))
          (is (thrown-with-msg? Exception #"SCHEMA-VALIDATION" (g/node-value a-node :sub-int-array-pass-through)))))))

  (testing "values that do not match input schemas with substitutions produce substitue values with cascades"
    (with-clean-system
      (with-redefs [internal.node/warn (constantly nil)]
        (let [[b-node a-node] (tx-nodes (g/make-node world BaseNode)
                                        (g/make-node world ArrayNode))]
          (g/transact [ (g/connect b-node :bad-str-output b-node :sub-str-input)
                        (g/connect b-node :bad-int-output b-node :sub-int-input)
                        (g/connect b-node :bad-str-output a-node :sub-str-array-input)
                        (g/connect b-node :bad-int-output a-node :sub-int-array-input)])
          (is (thrown-with-msg? Exception #"SCHEMA-VALIDATION" (g/node-value b-node :cascade-sub-str-pass-through)))
          (is (thrown-with-msg? Exception #"SCHEMA-VALIDATION" (g/node-value b-node :cascade-sub-int-pass-through)))
          (is (thrown-with-msg? Exception #"SCHEMA-VALIDATION" (g/node-value a-node :cascade-sub-str-array-pass-through)))
          (is (thrown-with-msg? Exception #"SCHEMA-VALIDATION" (g/node-value a-node :cascade-sub-int-array-pass-through))))))))
