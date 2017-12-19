(ns dynamo.integration.error-substitute-values
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [internal.node :as in]
            [schema.core :as s]
            [support.test-support :refer [with-clean-system tx-nodes]]))

(g/deftype StrArray [(s/maybe s/Str)])

(g/defnode SimpleOutputNode
  (output my-output g/Str (g/constantly "scones")))

(g/defnode SimpleTestNode
  (input my-input g/Str)
  (output passthrough g/Str (g/fnk [my-input] my-input)))

(g/defnode SimpleArrayTestNode
  (input my-input g/Str :array)
  (output passthrough StrArray (g/fnk [my-input] my-input)))

(g/defnode SimpleCachedTestNode
  (input my-input g/Str)
  (output passthrough g/Str :cached (g/fnk [my-input] my-input)))

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
  (output my-output g/Str (g/constantly nil)))

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
  (output my-output g/Str (g/constantly (g/error-fatal "I am an error!"))))

(defn thrown-for-reason?
  [node output reason]
  (let [error (:error (try (g/node-value node output)
                           (catch Exception e (ex-data e))))]
    (and (g/error-fatal? error)
         (= reason (:reason error)))))

(defn- cached? [cache node-id label]
  (contains? cache [node-id label]))

(deftest test-producing-vals-with-errors
  (testing "values with errors"
    (with-clean-system
      (let [[onode tnode] (tx-nodes (g/make-node world ErrorOutputNode)
                                    (g/make-node world SimpleTestNode))]
        (g/transact (g/connect onode :my-output tnode :my-input))
        (is (g/error? (g/node-value tnode :passthrough))))))

  (binding [in/*suppress-schema-warnings* true]
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
          (is (g/error? (g/node-value atnode2 :passthrough))))))

    (testing "outputs with error inputs are still cached"
      (with-clean-system
        (let [[onode tnode] (tx-nodes (g/make-node world ErrorOutputNode)
                                      (g/make-node world SimpleCachedTestNode))]
             (g/transact (g/connect onode :my-output tnode :my-input))
             (is (g/error? (g/node-value tnode :passthrough)))
             (is (cached? (g/cache) tnode :passthrough)))))))

(g/defnode SubTestNode
  (input my-input g/Str :substitute "beans")
  (output passthrough g/Str (g/fnk [my-input] my-input)))

(g/defnode SubArrayTestNode
  (input my-input g/Str :array :substitute (fn [_] ["beans"]))
  (output passthrough StrArray (g/fnk [my-input] my-input)))

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

(g/defnode FatalOutputNode
  (output error g/Str (g/constantly (g/error-fatal "fatal"))))

(g/defnode WarningOutputNode
  (output error g/Str (g/constantly (g/error-warning "warning"))))

(g/defnode InfoOutputNode
  (output error g/Str (g/constantly (g/->error 0 :label :info "info-value" "info" nil))))

(g/defnode SinkNode
  (input single-input g/Str)
  (output single-output g/Str (g/fnk [single-input] single-input))
  (property single-property g/Str
            (value (g/fnk [single-input] single-input)))

  (input array-input g/Str :array)
  (output array-output StrArray (g/fnk [array-input] array-input))

  (input single-subst-input g/Str :substitute (fn [_] "substitute"))
  (output single-subst-output g/Str (g/fnk [single-subst-input] single-subst-input))
  (property single-subst-property g/Str
            (value (g/fnk [single-subst-input] single-subst-input)))

  (input array-subst-input g/Str :array :substitute (fn [_] ["substitute"]))
  (output array-subst-output StrArray (g/fnk [array-subst-input] array-subst-input)))

(defn- connect-error-sink [error-node sink-node]
  (concat (g/connect error-node :error sink-node :single-input)
          (g/connect error-node :error sink-node :array-input)
          (g/connect error-node :error sink-node :single-subst-input)
          (g/connect error-node :error sink-node :array-subst-input)))

(deftest substitute-levels
  (with-clean-system
    (let [[fatal warning info
           fatal-sink warning-sink info-sink] (tx-nodes (g/make-node world FatalOutputNode)
                                                        (g/make-node world WarningOutputNode)
                                                        (g/make-node world InfoOutputNode)
                                                        (g/make-node world SinkNode)
                                                        (g/make-node world SinkNode)
                                                        (g/make-node world SinkNode))]

      (g/transact (concat (connect-error-sink fatal fatal-sink)
                          (connect-error-sink warning warning-sink)
                          (connect-error-sink info info-sink)))

      (is (g/error? (g/node-value fatal-sink :single-output)))
      (is (g/error? (g/node-value fatal-sink :single-property)))
      (is (g/error? (g/node-value fatal-sink :array-output)))
      (is (= "substitute" (g/node-value fatal-sink :single-subst-output)))
      (is (= "substitute" (g/node-value fatal-sink :single-subst-property)))
      (is (= ["substitute"] (g/node-value fatal-sink :array-subst-output)))

      (is (g/error? (g/node-value warning-sink :single-output)))
      (is (g/error? (g/node-value warning-sink :single-property)))
      (is (g/error? (g/node-value warning-sink :array-output)))
      (is (= "substitute" (g/node-value warning-sink :single-subst-output)))
      (is (= "substitute" (g/node-value warning-sink :single-subst-property)))
      (is (= ["substitute"] (g/node-value warning-sink :array-subst-output)))

      ;; :info error level never triggers substitution, instead the :value field is
      ;; extracted. This functionality feels very awkward and the corresponding
      ;; substitution code in node.clj could be simplified if info's behave like
      ;; other error levels.
      (is (= "info-value" (g/node-value info-sink :single-output)))
      (is (= "info-value" (g/node-value info-sink :single-property)))
      (is (= ["info-value"] (g/node-value info-sink :array-output)))
      (is (= "info-value" (g/node-value info-sink :single-subst-output)))
      (is (= "info-value" (g/node-value info-sink :single-subst-property)))
      (is (= ["info-value"] (g/node-value info-sink :array-subst-output))))))

(g/defnode LiteralNilSubstNode
  (input single-subst-input g/Str :substitute nil)
  (output single-subst-output g/Str (g/fnk [single-subst-input] single-subst-input))

  (input array-subst-input g/Str :array :substitute nil)
  (output array-subst-output g/Str (g/fnk [array-subst-input] array-subst-input)))

(deftest nil-substitute
  (with-clean-system
    (let [[fatal simple sink] (tx-nodes (g/make-node world FatalOutputNode)
                                        (g/make-node world SimpleOutputNode)
                                        (g/make-node world LiteralNilSubstNode))]

      (g/transact (concat (g/connect fatal :error sink :single-subst-input)
                          (g/connect fatal :error sink :array-subst-input)
                          (g/connect simple :my-output sink :array-subst-input)))

      (is (nil? (g/node-value sink :single-subst-output)))
      (is (nil? (g/node-value sink :array-subst-output))))))
