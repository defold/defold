;; Copyright 2020-2026 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;;
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;;
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns internal.dependency-test
  (:require [clojure.string :as str]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [support.test-support :as ts]
            [internal.graph.types :as gt]
            [internal.util :refer :all]
            [internal.system :as is]))

(defn- dependencies
  [& pairs]
  (ts/graph-dependencies (g/now) (map #(apply gt/endpoint %) (partition 2 pairs))))

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
            deps (dependencies a :out-from-inline)]
        (is (= deps
               #{(gt/endpoint a :out-from-inline)})))))

  (testing "without outputs, nobody cares"
    (ts/with-clean-system
      (let [[a b] (ts/tx-nodes (g/make-node world SingleOutput)
                               (g/make-node world InputNoOutput))
            _     (g/transact (g/connect a :out-from-inline b :unused-input))
            deps  (dependencies a :out-from-inline)]
        (is (= deps
               #{(gt/endpoint a :out-from-inline)})))))

  (testing "direct dependent outputs appear"
    (ts/with-clean-system
      (let [[a b] (ts/tx-nodes (g/make-node world SingleOutput)
                               (g/make-node world InputUsedByOutput))
            _     (g/transact (g/connect a :out-from-inline b :string-input))
            deps  (dependencies a :out-from-inline)]
        (is (= deps
               #{(gt/endpoint a :out-from-inline)
                 (gt/endpoint b :out-from-input)}))))))

(deftest fan-in
  (testing "results include inputs"
    (ts/with-clean-system
      (let [[a b c d] (ts/tx-nodes (g/make-node world SingleOutput)
                                   (g/make-node world SingleOutput)
                                   (g/make-node world SingleOutput)
                                   (g/make-node world SingleOutput))
            deps (dependencies
                               a :out-from-inline
                               b :out-from-inline
                               c :out-from-inline
                               d :out-from-inline)]
        (is (= deps
               #{(gt/endpoint a :out-from-inline)
                 (gt/endpoint b :out-from-inline)
                 (gt/endpoint c :out-from-inline)
                 (gt/endpoint d :out-from-inline)})))))

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
            deps        (dependencies
                                      a :out-from-inline
                                      b :out-from-inline
                                      c :out-from-inline
                                      d :out-from-inline)]
        (is (= deps
               #{(gt/endpoint a :out-from-inline)
                 (gt/endpoint b :out-from-inline)
                 (gt/endpoint c :out-from-inline)
                 (gt/endpoint d :out-from-inline)
                 (gt/endpoint x :out-from-input)}))))))


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
            deps        (dependencies a :out-from-inline)]
        (def basis* (g/now))
        (is (= deps
               #{(gt/endpoint a :out-from-inline)
                 (gt/endpoint w :out-from-input)
                 (gt/endpoint x :out-from-input)
                 (gt/endpoint y :out-from-input)
                 (gt/endpoint z :out-from-input)}))))))

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
            deps  (dependencies a :out-from-inline)]
        (is (= deps
               #{(gt/endpoint a :out-from-inline)
                 (gt/endpoint x :input-counter)})))))

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
            deps  (dependencies a :output-1 a :output-2 a :output-3 a :output-4 a :output-5)]
        (is (= deps
               #{(gt/endpoint a :output-1)
                 (gt/endpoint a :output-2)
                 (gt/endpoint a :output-3)
                 (gt/endpoint a :output-4)
                 (gt/endpoint a :output-5)
                 (gt/endpoint x :input-counter)}))))))

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
            deps  (dependencies a :out-from-inline)]
        (is (= deps
               #{(gt/endpoint a :out-from-inline)
                 (gt/endpoint x :uppercased)
                 (gt/endpoint x :counted)})))))

  (testing "outputs can depend on inputs with the same name"
    (ts/with-clean-system
      (let [[a x] (ts/tx-nodes (g/make-node world SingleOutput)
                               (g/make-node world BadlyWrittenSelfDependent))
            _     (g/transact
                   (g/connect a :out-from-inline x :string-value))
            deps  (dependencies a :out-from-inline)]
        (is (= deps
               #{(gt/endpoint a :out-from-inline)
                 (gt/endpoint x :string-value)}))))))

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
            deps        (dependencies a :out-from-inline)]
        (is (= deps
               #{(gt/endpoint a :out-from-inline)
                 (gt/endpoint b :out-from-input)
                 (gt/endpoint c :out-from-input)
                 (gt/endpoint d :out-from-input)}))))))

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
            a-deps  (dependencies a :out-from-inline)
            b-deps  (dependencies b :out-from-inline)]
        (is (= a-deps
               #{(gt/endpoint a :out-from-inline)
                 (gt/endpoint x :output-1)}))
        (is (= b-deps
               #{(gt/endpoint b :out-from-inline)
                 (gt/endpoint x :output-2)}))))))
