(ns internal.dependency-test
  (:require [clojure.string :as str]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [support.test-support :as ts]
            [internal.util :refer :all]
            [internal.system :as is]))

(defn- dependencies
  [system & pairs]
  (ts/graph-dependencies (is/basis system) (partition 2 pairs)))

(g/defnode SingleOutput
  (output out-from-inline g/Str (g/fnk [] "out-from-inline")))

(g/defnode InputNoOutput
  (input unused-input g/Str))

(g/defnode InputUsedByOutput
  (input string-input g/Str)
  (output out-from-input g/Str (g/fnk [string-input] string-input)))

(deftest single-connection
  (testing "results include inputs"
    (ts/with-clean-system
      (let [[a] (ts/tx-nodes (g/make-node world SingleOutput))
            deps (dependencies system a :out-from-inline)]
        (is (= deps
               #{[a :out-from-inline]})))))

  (testing "without outputs, nobody cares"
    (ts/with-clean-system
      (let [[a b] (ts/tx-nodes (g/make-node world SingleOutput)
                               (g/make-node world InputNoOutput))
            _     (g/transact (g/connect a :out-from-inline b :unused-input))
            deps  (dependencies system a :out-from-inline)]
        (is (= deps
               #{[a :out-from-inline]})))))

  (testing "direct dependent outputs appear"
    (ts/with-clean-system
      (let [[a b] (ts/tx-nodes (g/make-node world SingleOutput)
                               (g/make-node world InputUsedByOutput))
            _     (g/transact (g/connect a :out-from-inline b :string-input))
            deps  (dependencies system a :out-from-inline)]
        (is (= deps
               #{[a :out-from-inline]
                 [b :out-from-input]}))))))

(deftest fan-in
  (testing "results include inputs"
    (ts/with-clean-system
      (let [[a b c d] (ts/tx-nodes (g/make-node world SingleOutput)
                                   (g/make-node world SingleOutput)
                                   (g/make-node world SingleOutput)
                                   (g/make-node world SingleOutput))
            deps (dependencies system
                               a :out-from-inline
                               b :out-from-inline
                               c :out-from-inline
                               d :out-from-inline)]
        (is (= deps
               #{[a :out-from-inline]
                 [b :out-from-inline]
                 [c :out-from-inline]
                 [d :out-from-inline]})))))

  (testing "multi-path dependency only appears once"
    (ts/with-clean-system
      (let [[a b c d x] (ts/tx-nodes (g/make-node world SingleOutput)
                                     (g/make-node world SingleOutput)
                                     (g/make-node world SingleOutput)
                                     (g/make-node world SingleOutput)
                                     (g/make-node world InputUsedByOutput))
            _           (g/transact
                         (concat (g/connect a :out-from-inline x :string-input)
                                 (g/connect b :out-from-inline x :string-input)
                                 (g/connect c :out-from-inline x :string-input)
                                 (g/connect d :out-from-inline x :string-input)))
            deps        (dependencies system
                                      a :out-from-inline
                                      b :out-from-inline
                                      c :out-from-inline
                                      d :out-from-inline)]
        (is (= deps
               #{[a :out-from-inline]
                 [b :out-from-inline]
                 [c :out-from-inline]
                 [d :out-from-inline]
                 [x :out-from-input]}))))))


(deftest fan-out
  (testing "all dependents are marked"
    (ts/with-clean-system
      (let [[a w x y z] (ts/tx-nodes (g/make-node world SingleOutput)
                                     (g/make-node world InputUsedByOutput)
                                     (g/make-node world InputUsedByOutput)
                                     (g/make-node world InputUsedByOutput)
                                     (g/make-node world InputUsedByOutput))
            _           (g/transact
                         (concat
                          (g/connect a :out-from-inline x :string-input)
                          (g/connect a :out-from-inline y :string-input)
                          (g/connect a :out-from-inline z :string-input)
                          (g/connect a :out-from-inline w :string-input)))
            deps        (dependencies system a :out-from-inline)]
        (def basis* (g/now))
        (is (= deps
               #{[a :out-from-inline]
                 [w :out-from-input]
                 [x :out-from-input]
                 [y :out-from-input]
                 [z :out-from-input]}))))))

(g/defnode MultipleOutputs
  (output output-1 g/Str (g/fnk [] "1"))
  (output output-2 g/Str (g/fnk [] "2"))
  (output output-3 g/Str (g/fnk [] "3"))
  (output output-4 g/Str (g/fnk [] "4"))
  (output output-5 g/Str (g/fnk [] "5")))

(g/defnode MultipleInputsIntoOneOutput
  (input input-1 g/Str)
  (input input-2 g/Str)
  (input input-3 g/Str)
  (input input-4 g/Str)
  (input input-5 g/Str)

  (output input-counter g/Int (g/fnk [input-1 input-2 input-3 input-4 input-5]
                                   (count (keep identity [input-1 input-2 input-3 input-4 input-5])))))

(deftest one-step-multipath
  (testing "one output to several inputs"
    (ts/with-clean-system
      (let [[a x] (ts/tx-nodes (g/make-node world SingleOutput)
                               (g/make-node world MultipleInputsIntoOneOutput))
            _     (g/transact
                   (concat
                    (g/connect a :out-from-inline x :input-1)
                    (g/connect a :out-from-inline x :input-2)
                    (g/connect a :out-from-inline x :input-3)
                    (g/connect a :out-from-inline x :input-4)
                    (g/connect a :out-from-inline x :input-5)))
            deps  (dependencies system a :out-from-inline)]
        (is (= deps
               #{[a :out-from-inline]
                 [x :input-counter]})))))

  (testing "several outputs to one input"
    (ts/with-clean-system
      (let [[a x] (ts/tx-nodes (g/make-node world MultipleOutputs)
                               (g/make-node world MultipleInputsIntoOneOutput))
            _     (g/transact
                   (concat
                    (g/connect a :output-1 x :input-1)
                    (g/connect a :output-2 x :input-2)
                    (g/connect a :output-3 x :input-3)
                    (g/connect a :output-4 x :input-4)
                    (g/connect a :output-5 x :input-5)))
            deps  (dependencies system a :output-1 a :output-2 a :output-3 a :output-4 a :output-5)]
        (is (= deps
               #{[a :output-1]
                 [a :output-2]
                 [a :output-3]
                 [a :output-4]
                 [a :output-5]
                 [x :input-counter]}))))))

(g/defnode SelfDependent
  (input string-input g/Str)

  (output uppercased g/Str (g/fnk [string-input] (str/upper-case string-input)))
  (output counted    g/Int (g/fnk [uppercased] (count uppercased))))

(g/defnode BadlyWrittenSelfDependent
  (input string-value g/Str)
  (output string-value g/Str (g/fnk [string-value] (str/upper-case string-value))))

(deftest with-self-dependencies
  (testing "dependencies propagate through fnks"
    (ts/with-clean-system
      (let [[a x] (ts/tx-nodes (g/make-node world SingleOutput)
                               (g/make-node world SelfDependent))
            _     (g/transact
                   (g/connect a :out-from-inline x :string-input))
            deps  (dependencies system a :out-from-inline)]
        (is (= deps
               #{[a :out-from-inline]
                 [x :uppercased]
                 [x :counted]})))))

  (testing "outputs can depend on inputs with the same name"
    (ts/with-clean-system
      (let [[a x] (ts/tx-nodes (g/make-node world SingleOutput)
                               (g/make-node world BadlyWrittenSelfDependent))
            _     (g/transact
                   (g/connect a :out-from-inline x :string-value))
            deps  (dependencies system a :out-from-inline)]
        (is (= deps
               #{[a :out-from-inline]
                 [x :string-value]}))))))

(g/defnode SingleOutput
  (output out-from-inline g/Str (g/fnk [] "out-from-inline")))

(g/defnode InputNoOutput
  (input unused-input g/Str))

(g/defnode InputUsedByOutput
  (input string-input g/Str)
  (output out-from-input g/Str (g/fnk [string-input] string-input)))

(deftest diamond-pattern
  (testing "multipath reaching the same node"
    (ts/with-clean-system
      (let [[a b c d] (ts/tx-nodes (g/make-node world SingleOutput)
                                   (g/make-node world InputUsedByOutput)
                                   (g/make-node world InputUsedByOutput)
                                   (g/make-node world InputUsedByOutput))
            _           (g/transact
                         (concat
                          (g/connect a :out-from-inline b :string-input)
                          (g/connect a :out-from-inline c :string-input)
                          (g/connect b :out-from-input  d :string-input)
                          (g/connect c :out-from-input  d :string-input)))
            deps        (dependencies system a :out-from-inline)]
        (is (= deps
               #{[a :out-from-inline]
                 [b :out-from-input]
                 [c :out-from-input]
                 [d :out-from-input]}))))))

(g/defnode TwoIndependentOutputs
  (input input-1 g/Str)
  (output output-1 g/Str (g/fnk [input-1] input-1))

  (input input-2 g/Str)
  (output output-2 g/Str (g/fnk [input-2] input-2)))

(deftest independence-of-outputs
  (testing "output is only marked when its specific inputs are  affected"
    (ts/with-clean-system
      (let [[a b x] (ts/tx-nodes (g/make-node world SingleOutput)
                                 (g/make-node world SingleOutput)
                                 (g/make-node world TwoIndependentOutputs))
            _       (g/transact
                     (concat
                      (g/connect a :out-from-inline x :input-1)
                      (g/connect b :out-from-inline x :input-2)))
            a-deps  (dependencies system a :out-from-inline)
            b-deps  (dependencies system b :out-from-inline)]
        (is (= a-deps
               #{[a :out-from-inline]
                 [x :output-1]}))
        (is (= b-deps
               #{[b :out-from-inline]
                 [x :output-2]}))))))
