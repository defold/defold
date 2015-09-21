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
        (g/transact (g/connect onode  :my-output   tnode1 :my-input))
        (g/transact (g/connect tnode1 :passthrough tnode2 :my-input))
        (is (= "scones" (g/node-value tnode2 :passthrough))))))

  (testing "chained array values with no errors"
    (with-clean-system
      (let [[onode tnode1 atnode2] (tx-nodes (g/make-node world SimpleOutputNode)
                                             (g/make-node world SimpleTestNode)
                                             (g/make-node world SimpleArrayTestNode))]
        (g/transact (g/connect onode  :my-output   tnode1  :my-input))
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
        (g/transact (g/connect onode  :my-output   tnode1 :my-input))
        (g/transact (g/connect tnode1 :passthrough tnode2 :my-input))
        (is (nil?  (g/node-value tnode2 :passthrough))))))

  (testing "chained array values with nils"
    (with-clean-system
      (let [[onode tnode1 atnode2] (tx-nodes (g/make-node world NilOutputNode)
                                             (g/make-node world SimpleTestNode)
                                             (g/make-node world SimpleArrayTestNode))]
        (g/transact (g/connect onode  :my-output   tnode1 :my-input))
        (g/transact (g/connect tnode1 :passthrough atnode2 :my-input))
        (is (= [nil] (g/node-value atnode2 :passthrough)))))))

(g/defnode ErrorOutputNode
  (output my-output g/Str (g/always (g/error-severe "I am an error!"))))

(defn thrown-for-reason?
  [node output reason]
  (let [error (:error (try (g/node-value node output)
                           (catch Exception e (ex-data e))))]
    (and (g/error-severe? error)
         (= reason (:reason error)))))

(deftest test-producing-vals-with-errors
  (testing "values with errors"
    (with-clean-system
      (let [[onode tnode] (tx-nodes (g/make-node world ErrorOutputNode)
                                    (g/make-node world SimpleTestNode))]
        (g/transact (g/connect onode :my-output tnode :my-input))
        (is (g/error? (g/node-value tnode :passthrough))))))

  (with-redefs [in/warn (constantly nil)]
    (testing "array values with errors"
      (with-clean-system
        (let [[onode atnode] (tx-nodes (g/make-node world ErrorOutputNode)
                                       (g/make-node world SimpleArrayTestNode))]
          (g/transact (g/connect onode :my-output atnode :my-input))
          (is (g/error? (g/node-value atnode :passthrough))))))

    (testing "chained values with errors"
      (with-clean-system
        (let [[onode tnode1 tnode2] (tx-nodes (g/make-node world ErrorOutputNode)
                                              (g/make-node world SimpleTestNode)
                                              (g/make-node world SimpleTestNode))]
          (g/transact (g/connect onode :my-output tnode1 :my-input))
          (g/transact (g/connect tnode1 :passthrough tnode2 :my-input))
          (is (g/error? (g/node-value tnode2 :passthrough))))))

    (testing "chained array values with errors"
      (with-clean-system
        (let [[onode tnode1 atnode2] (tx-nodes (g/make-node world ErrorOutputNode)
                                               (g/make-node world SimpleTestNode)
                                               (g/make-node world SimpleArrayTestNode))]
          (g/transact (g/connect onode :my-output tnode1 :my-input))
          (g/transact (g/connect tnode1 :passthrough atnode2 :my-input))
          (is (g/error? (g/node-value atnode2 :passthrough))))))))

(g/defnode SubTestNode
  (input my-input g/Str :substitute "beans")
  (output passthrough g/Str (g/fnk [my-input] my-input)))

(g/defnode SubArrayTestNode
  (input my-input g/Str :array :substitute (fn [_] ["beans"]))
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

(g/defnode BaseInputNode
  (input the-input g/Str)
  (output the-input g/Str (g/fnk [the-input] the-input)))

(g/defnode DerivedSubstInputNode
  (inherits BaseInputNode)
  (input the-input g/Str :substitute "substitute")
  (output the-input g/Str (g/fnk [the-input] the-input)))

(deftest derived-can-substitute
  (testing "base node does no substitution"
    (with-clean-system
      (let [[err base] (tx-nodes (g/make-node world ErrorOutputNode)
                                 (g/make-node world BaseInputNode))]
        (g/transact (g/connect err :my-output base :the-input))
        (is (g/error? (g/node-value base :the-input))))))
  (testing "derived does substitution"
    (with-clean-system
      (let [[err der] (tx-nodes (g/make-node world ErrorOutputNode)
                                (g/make-node world DerivedSubstInputNode))]
        (g/transact (g/connect err :my-output der :the-input))
        (is (= "substitute" (g/node-value der :the-input)))))))

(defn subst-fn [err] "substitute")

(g/defnode NamedSubstFn
  (input the-input g/Str :substitute subst-fn)
  (output the-input g/Str (g/fnk [the-input] the-input)))

(deftest substitute-fn-can-be-defn
  (testing "substitute function can be a defn"
    (with-clean-system
      (let [[err sub] (tx-nodes (g/make-node world ErrorOutputNode)
                                (g/make-node world NamedSubstFn))]
        (g/transact (g/connect err :my-output sub :the-input))
        (is (= "substitute" (g/node-value sub :the-input)))))))

