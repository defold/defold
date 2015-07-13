(ns dynamo.integration.error-substitute-values
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [internal.node :as in]
            [support.test-support :refer [with-clean-system tx-nodes]]))


(g/defnode SimpleOutputNode
  (output my-output g/Str (g/always "scones")))

(g/defnode SimpleTestNode
  (input my-input g/Str)
  (output passthrough g/Str (g/fnk [my-input] my-input)))

(g/defnode SimpleArrayTestNode
  (input my-input g/Str :array)
  (output passthrough [g/Str] (g/fnk [my-input] my-input)))

(deftest test-producing-values-without-substitutes
  (testing "values with no errors"
    (with-clean-system
      (let [[onode tnode] (tx-nodes (g/make-node world SimpleOutputNode)
                                           (g/make-node world SimpleTestNode))]
        (g/transact (g/connect onode :my-output tnode :my-input))
        (is (= "scones" (g/node-value tnode :passthrough))))))

  (testing "array values with no errors"
    (with-clean-system
      (let [[onode atnode] (tx-nodes (g/make-node world SimpleOutputNode)
                                           (g/make-node world SimpleArrayTestNode))]
        (g/transact (g/connect onode :my-output atnode :my-input))
        (is (= ["scones"] (g/node-value atnode :passthrough))))))

  (testing "chained values with no errors"
    (with-clean-system
      (let [[onode tnode1 tnode2] (tx-nodes (g/make-node world SimpleOutputNode)
                                           (g/make-node world SimpleTestNode)
                                           (g/make-node world SimpleTestNode))]
        (g/transact (g/connect onode :my-output tnode1 :my-input))
        (g/transact (g/connect tnode1 :passthrough tnode2 :my-input))
        (is (= "scones" (g/node-value tnode2 :passthrough))))))

  (testing "chained array values with no errors"
    (with-clean-system
      (let [[onode tnode1 atnode2] (tx-nodes (g/make-node world SimpleOutputNode)
                                           (g/make-node world SimpleTestNode)
                                           (g/make-node world SimpleArrayTestNode))]
        (g/transact (g/connect onode :my-output tnode1 :my-input))
        (g/transact (g/connect tnode1 :passthrough atnode2 :my-input))
        (is (= ["scones"] (g/node-value atnode2 :passthrough)))))))

(g/defnode NilOutputNode
  (output my-output g/Str (g/always nil)))

(deftest test-producing-values-with-nils
  (testing "values with nils"
    (with-clean-system
      (let [[onode tnode] (tx-nodes (g/make-node world NilOutputNode)
                                    (g/make-node world SimpleTestNode))]
        (g/transact (g/connect onode :my-output tnode :my-input))
        (is (nil? (g/node-value tnode :passthrough))))))

  (testing "array values with nils"
    (with-clean-system
      (let [[onode atnode] (tx-nodes (g/make-node world NilOutputNode)
                                           (g/make-node world SimpleArrayTestNode))]
        (g/transact (g/connect onode :my-output atnode :my-input))
        (is (= [nil] (g/node-value atnode :passthrough))))))

  (testing "chained values with nils"
    (with-clean-system
      (let [[onode tnode1 tnode2] (tx-nodes (g/make-node world NilOutputNode)
                                           (g/make-node world SimpleTestNode)
                                           (g/make-node world SimpleTestNode))]
        (g/transact (g/connect onode :my-output tnode1 :my-input))
        (g/transact (g/connect tnode1 :passthrough tnode2 :my-input))
        (is (nil?  (g/node-value tnode2 :passthrough))))))

  (testing "chained array values with nils"
    (with-clean-system
      (let [[onode tnode1 atnode2] (tx-nodes (g/make-node world NilOutputNode)
                                           (g/make-node world SimpleTestNode)
                                           (g/make-node world SimpleArrayTestNode))]
        (g/transact (g/connect onode :my-output tnode1 :my-input))
        (g/transact (g/connect tnode1 :passthrough atnode2 :my-input))
        (is (= [nil] (g/node-value atnode2 :passthrough)))))))

(g/defnode ErrorOutputNode
  (output my-output g/Str (g/always (g/error "I am an error!"))))

(defn thrown-for-reason?
  [node output reason]
  (let [error (:error (try (g/node-value node output)
                           (catch Exception e (ex-data e))))]
    (and (g/error? error)
         (= reason (:reason error)))))

(deftest test-producing-vals-with-errors
  (testing "values with errors"
    (with-clean-system
      (let [[onode tnode] (tx-nodes (g/make-node world ErrorOutputNode)
                                    (g/make-node world SimpleTestNode))]
        (g/transact (g/connect onode :my-output tnode :my-input))
        (is (thrown-for-reason? tnode :passthrough "I am an error!")))))

  (with-redefs [in/warn (constantly nil)]
    (testing "array values with errors"
      (with-clean-system
        (let [[onode atnode] (tx-nodes (g/make-node world ErrorOutputNode)
                                       (g/make-node world SimpleArrayTestNode))]
          (g/transact (g/connect onode :my-output atnode :my-input))
          (is (thrown-for-reason? atnode :passthrough "I am an error!")))))

    (testing "chained values with errors"
      (with-clean-system
        (let [[onode tnode1 tnode2] (tx-nodes (g/make-node world ErrorOutputNode)
                                              (g/make-node world SimpleTestNode)
                                              (g/make-node world SimpleTestNode))]
          (g/transact (g/connect onode :my-output tnode1 :my-input))
          (g/transact (g/connect tnode1 :passthrough tnode2 :my-input))
          (is (thrown-for-reason? tnode2 :passthrough "I am an error!")))))

    (testing "chained array values with errors"
      (with-clean-system
        (let [[onode tnode1 atnode2] (tx-nodes (g/make-node world ErrorOutputNode)
                                               (g/make-node world SimpleTestNode)
                                               (g/make-node world SimpleArrayTestNode))]
          (g/transact (g/connect onode :my-output tnode1 :my-input))
          (g/transact (g/connect tnode1 :passthrough atnode2 :my-input))
          (is (thrown-for-reason? atnode2 :passthrough "I am an error!")))))))

(g/defnode SubTestNode
  (input my-input g/Str :substitute "beans")
  (output passthrough g/Str (g/fnk [my-input] my-input)))

(g/defnode SubArrayTestNode
  (input my-input g/Str :array :substitute "beans")
  (output passthrough [g/Str] (g/fnk [my-input] my-input)))

(deftest test-producing-vals-with-nil-substitutes
  (testing "values with nils do not trigger substitutes"
    (with-clean-system
      (let [[onode tnode] (tx-nodes (g/make-node world NilOutputNode)
                                    (g/make-node world SubTestNode))]
        (g/transact (g/connect onode :my-output tnode :my-input))
        (is (nil? (g/node-value tnode :passthrough))))))

  (testing "array values with nils do not trigger substitutes"
    (with-clean-system
      (let [[onode atnode] (tx-nodes (g/make-node world NilOutputNode)
                                           (g/make-node world SubArrayTestNode))]
        (g/transact (g/connect onode :my-output atnode :my-input))
        (is (= [nil] (g/node-value atnode :passthrough))))))

  (testing "chained values with nils do not trigger substitutes"
    (with-clean-system
      (let [[onode tnode1 tnode2] (tx-nodes (g/make-node world NilOutputNode)
                                           (g/make-node world SubTestNode)
                                           (g/make-node world SubTestNode))]
        (g/transact (g/connect onode :my-output tnode1 :my-input))
        (g/transact (g/connect tnode1 :passthrough tnode2 :my-input))
        (is (nil? (g/node-value tnode2 :passthrough))))))

  (testing "chained array values with nils do not trigger substitutes"
    (with-clean-system
      (let [[onode tnode1 atnode2] (tx-nodes (g/make-node world NilOutputNode)
                                           (g/make-node world SubTestNode)
                                           (g/make-node world SubArrayTestNode))]
        (g/transact (g/connect onode :my-output tnode1 :my-input))
        (g/transact (g/connect tnode1 :passthrough atnode2 :my-input))
        (is (= [nil] (g/node-value atnode2 :passthrough)))))))

(deftest test-producing-errors-with-substitutes
  (testing "values with errors do trigger substitutes"
    (with-clean-system
      (let [[onode tnode] (tx-nodes (g/make-node world ErrorOutputNode)
                                    (g/make-node world SubTestNode))]
        (g/transact (g/connect onode :my-output tnode :my-input))
        (is (= "beans" (g/node-value tnode :passthrough))))))

  (testing "array values with errors do trigger substitutes"
    (with-clean-system
      (let [[onode atnode] (tx-nodes (g/make-node world ErrorOutputNode)
                                           (g/make-node world SubArrayTestNode))]
        (g/transact (g/connect onode :my-output atnode :my-input))
        (is (= ["beans"] (g/node-value atnode :passthrough))))))

  (testing "chained values with errors do trigger substitutes"
    (with-clean-system
      (let [[onode tnode1 tnode2] (tx-nodes (g/make-node world ErrorOutputNode)
                                           (g/make-node world SubTestNode)
                                           (g/make-node world SubTestNode))]
        (g/transact (g/connect onode :my-output tnode1 :my-input))
        (g/transact (g/connect tnode1 :passthrough tnode2 :my-input))
        (is (= "beans" (g/node-value tnode2 :passthrough))))))

  (testing "chained array values with errors do trigger substitutes"
    (with-clean-system
      (let [[onode tnode1 atnode2] (tx-nodes (g/make-node world ErrorOutputNode)
                                           (g/make-node world SubTestNode)
                                           (g/make-node world SubArrayTestNode))]
        (g/transact (g/connect onode :my-output tnode1 :my-input))
        (g/transact (g/connect tnode1 :passthrough atnode2 :my-input))
        (is (= ["beans"] (g/node-value atnode2 :passthrough)))))))
