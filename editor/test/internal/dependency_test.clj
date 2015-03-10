(ns internal.dependency-test
  (:require [clojure.string :as str]
            [clojure.test :refer :all]
            [dynamo.node :as n]
            [dynamo.system :as ds]
            [dynamo.system.test-support :as ts]
            [dynamo.types :as t]
            [dynamo.util :refer :all]
            [internal.transaction :as it]
            [plumbing.core :refer [fnk]]
            [schema.core :as s]))

(defn- id [x] (or (:_id x) (:node-id x)))

(defn- dependencies
  [world-ref & pairs]
  (ds/output-dependencies (:graph @world-ref)
                          (map-first id (partition 2 pairs))))

(n/defnode SingleOutput
  (output out-from-inline s/Str (fnk [] "out-from-inline")))

(n/defnode InputNoOutput
  (input unused-input s/Str))

(n/defnode InputUsedByOutput
  (input string-input s/Str)
  (output out-from-input s/Str (fnk [string-input] string-input)))

(deftest single-connection
  (testing "results include inputs"
    (ts/with-clean-system
      (let [[a] (ts/tx-nodes (n/construct SingleOutput))
            deps (dependencies world-ref a :out-from-inline)]
        (is (= deps
               #{[(id a) :out-from-inline]})))))

  (testing "without outputs, nobody cares"
    (ts/with-clean-system
      (let [[a b] (ts/tx-nodes (n/construct SingleOutput)
                               (n/construct InputNoOutput))
            _     (ds/transactional (ds/connect a :out-from-inline b :unused-input))
            deps  (dependencies world-ref a :out-from-inline)]
        (is (= deps
               #{[(id a) :out-from-inline]})))))

  (testing "direct dependent outputs appear"
    (ts/with-clean-system
      (let [[a b] (ts/tx-nodes (n/construct SingleOutput)
                               (n/construct InputUsedByOutput))
            _     (ds/transactional (ds/connect a :out-from-inline b :string-input))
            deps  (dependencies world-ref a :out-from-inline)]
        (is (= deps
               #{[(id a) :out-from-inline]
                 [(id b) :out-from-input]}))))))

(deftest fan-in
  (testing "results include inputs"
    (ts/with-clean-system
      (let [[a b c d] (ts/tx-nodes (n/construct SingleOutput)
                                   (n/construct SingleOutput)
                                   (n/construct SingleOutput)
                                   (n/construct SingleOutput))
            deps (dependencies world-ref
                               a :out-from-inline
                               b :out-from-inline
                               c :out-from-inline
                               d :out-from-inline)]
        (is (= deps
               #{[(id a) :out-from-inline]
                 [(id b) :out-from-inline]
                 [(id c) :out-from-inline]
                 [(id d) :out-from-inline]})))))

  (testing "multi-path dependency only appears once"
    (ts/with-clean-system
      (let [[a b c d x] (ts/tx-nodes (n/construct SingleOutput)
                                     (n/construct SingleOutput)
                                     (n/construct SingleOutput)
                                     (n/construct SingleOutput)
                                     (n/construct InputUsedByOutput))
            _           (ds/transactional
                         (ds/connect a :out-from-inline x :string-input)
                         (ds/connect b :out-from-inline x :string-input)
                         (ds/connect c :out-from-inline x :string-input)
                         (ds/connect d :out-from-inline x :string-input))
            deps        (dependencies world-ref
                                      a :out-from-inline
                                      b :out-from-inline
                                      c :out-from-inline
                                      d :out-from-inline)]
        (is (= deps
               #{[(id a) :out-from-inline]
                 [(id b) :out-from-inline]
                 [(id c) :out-from-inline]
                 [(id d) :out-from-inline]
                 [(id x) :out-from-input]}))))))


(deftest fan-out
  (testing "all dependents are marked"
    (ts/with-clean-system
      (let [[a w x y z] (ts/tx-nodes (n/construct SingleOutput)
                                     (n/construct InputUsedByOutput)
                                     (n/construct InputUsedByOutput)
                                     (n/construct InputUsedByOutput)
                                     (n/construct InputUsedByOutput))
            _           (ds/transactional
                         (ds/connect a :out-from-inline x :string-input)
                         (ds/connect a :out-from-inline y :string-input)
                         (ds/connect a :out-from-inline z :string-input)
                         (ds/connect a :out-from-inline w :string-input))
            deps        (dependencies world-ref a :out-from-inline)]
        (is (= deps
               #{[(id a) :out-from-inline]
                 [(id w) :out-from-input]
                 [(id x) :out-from-input]
                 [(id y) :out-from-input]
                 [(id z) :out-from-input]}))))))

(n/defnode MultipleOutputs
  (output output-1 s/Str (fnk [] "1"))
  (output output-2 s/Str (fnk [] "2"))
  (output output-3 s/Str (fnk [] "3"))
  (output output-4 s/Str (fnk [] "4"))
  (output output-5 s/Str (fnk [] "5")))

(n/defnode MultipleInputsIntoOneOutput
  (input input-1 s/Str)
  (input input-2 s/Str)
  (input input-3 s/Str)
  (input input-4 s/Str)
  (input input-5 s/Str)

  (output input-counter s/Int (fnk [input-1 input-2 input-3 input-4 input-5]
                                   (count (keep identity [input-1 input-2 input-3 input-4 input-5])))))

(deftest one-step-multipath
  (testing "one output to several inputs"
    (ts/with-clean-system
      (let [[a x] (ts/tx-nodes (n/construct SingleOutput)
                               (n/construct MultipleInputsIntoOneOutput))
            _     (ds/transactional
                   (ds/connect a :out-from-inline x :input-1)
                   (ds/connect a :out-from-inline x :input-2)
                   (ds/connect a :out-from-inline x :input-3)
                   (ds/connect a :out-from-inline x :input-4)
                   (ds/connect a :out-from-inline x :input-5))
            deps  (dependencies world-ref a :out-from-inline)]
        (is (= deps
               #{[(id a) :out-from-inline]
                 [(id x) :input-counter]})))))

  (testing "several outputs to one input"
    (ts/with-clean-system
      (let [[a x] (ts/tx-nodes (n/construct MultipleOutputs)
                               (n/construct MultipleInputsIntoOneOutput))
            _     (ds/transactional
                   (ds/connect a :output-1 x :input-1)
                   (ds/connect a :output-2 x :input-2)
                   (ds/connect a :output-3 x :input-3)
                   (ds/connect a :output-4 x :input-4)
                   (ds/connect a :output-5 x :input-5))
            deps  (dependencies world-ref a :output-1 a :output-2 a :output-3 a :output-4 a :output-5)]
        (is (= deps
               #{[(id a) :output-1]
                 [(id a) :output-2]
                 [(id a) :output-3]
                 [(id a) :output-4]
                 [(id a) :output-5]
                 [(id x) :input-counter]}))))))

(n/defnode SelfDependent
  (input string-input s/Str)

  (output uppercased s/Str (fnk [string-input] (str/upper-case string-input)))
  (output counted    s/Int (fnk [uppercased] (count uppercased))))

(n/defnode BadlyWrittenSelfDependent
  (input string-value s/Str)
  (output string-value s/Str (fnk [string-value] (str/upper-case string-value))))

(n/defnode PropertyShadowingInput
  (input string-value s/Str)
  (property string-value s/Str (default "Hey there!")))

(deftest with-self-dependencies
  (testing "dependencies propagate through fnks"
    (ts/with-clean-system
      (let [[a x] (ts/tx-nodes (n/construct SingleOutput)
                               (n/construct SelfDependent))
            _     (ds/transactional
                   (ds/connect a :out-from-inline x :string-input))
            deps  (dependencies world-ref a :out-from-inline)]
        (is (= deps
               #{[(id a) :out-from-inline]
                 [(id x) :uppercased]
                 [(id x) :counted]})))))

  (testing "outputs can depend on inputs with the same name"
    (ts/with-clean-system
      (let [[a x] (ts/tx-nodes (n/construct SingleOutput)
                               (n/construct BadlyWrittenSelfDependent))
            _     (ds/transactional
                   (ds/connect a :out-from-inline x :string-value))
            deps  (dependencies world-ref a :out-from-inline)]
        (is (= deps
               #{[(id a) :out-from-inline]
                 [(id x) :string-value]})))))

  #_(testing "properties are not confused with inputs"
    (ts/with-clean-system
      (let [[a x] (ts/tx-nodes (n/construct SingleOutput)
                               (n/construct PropertyShadowingInput))
            _     (ds/transactional
                   (ds/connect a :out-from-inline x :string-value))
            deps  (dependencies world-ref a :out-from-inline)]
        (is (= deps
               #{[(id a) :out-from-inline]}))))))

(n/defnode SingleOutput
  (output out-from-inline s/Str (fnk [] "out-from-inline")))

(n/defnode InputNoOutput
  (input unused-input s/Str))

(n/defnode InputUsedByOutput
  (input string-input s/Str)
  (output out-from-input s/Str (fnk [string-input] string-input)))

(deftest diamond-pattern
  (testing "multipath reaching the same node"
    (ts/with-clean-system
      (let [[a b c d] (ts/tx-nodes (n/construct SingleOutput)
                                     (n/construct InputUsedByOutput)
                                     (n/construct InputUsedByOutput)
                                     (n/construct InputUsedByOutput))
            _           (ds/transactional
                         (ds/connect a :out-from-inline b :string-input)
                         (ds/connect a :out-from-inline c :string-input)
                         (ds/connect b :out-from-input  d :string-input)
                         (ds/connect c :out-from-input  d :string-input))
            deps        (dependencies world-ref a :out-from-inline)]
        (is (= deps
               #{[(id a) :out-from-inline]
                 [(id b) :out-from-input]
                 [(id c) :out-from-input]
                 [(id d) :out-from-input]}))))))

(n/defnode TwoIndependentOutputs
  (input input-1 s/Str)
  (output output-1 s/Str (fnk [input-1] input-1))

  (input input-2 s/Str)
  (output output-2 s/Str (fnk [input-2] input-2)))

(deftest independence-of-outputs
  (testing "output is only marked when its specific inputs are  affected"
    (ts/with-clean-system
      (let [[a b x] (ts/tx-nodes (n/construct SingleOutput)
                                 (n/construct SingleOutput)
                                 (n/construct TwoIndependentOutputs))
            _       (ds/transactional
                     (ds/connect a :out-from-inline x :input-1)
                     (ds/connect b :out-from-inline x :input-2))
            a-deps  (dependencies world-ref a :out-from-inline)
            b-deps  (dependencies world-ref b :out-from-inline)]
        (def gr* (:graph @world-ref))
        (is (= a-deps
               #{[(id a) :out-from-inline]
                 [(id x) :output-1]}))
        (is (= b-deps
               #{[(id b) :out-from-inline]
                 [(id x) :output-2]}))))))
