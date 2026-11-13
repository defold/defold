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
            [internal.graph :as ig]
            [internal.graph.types :as gt]
            [support.test-support :as ts]
            [util.coll :as coll])
  (:import [java.util.concurrent CountDownLatch TimeUnit]))

(defn- dependencies
  [& pairs]
  (ts/graph-dependencies (g/now) (mapv #(apply gt/endpoint %) (partition 2 pairs))))

(g/defnode SingleOutput
  (output out-from-inline g/Str (g/fnk [] "out-from-inline")))

(g/defnode InputNoOutput
  (input unused-input g/Str))

(g/defnode InputUsedByOutput
  (input string-input g/Str)
  (output out-from-input g/Str (g/fnk [string-input] string-input)))

(g/defnode DependencyCacheSource
  (property value g/Str (default "before"))
  (output value-out g/Str (g/fnk [value] value)))

;; Verifies dependency results for equal roots, value changes, connection
;; changes, historical bases, and undo/redo.
(deftest dependency-cache-lifetime-test
  (ts/with-clean-system
    (let [[source target extra-target]
          (ts/tx-nodes
            (g/make-nodes [source DependencyCacheSource
                           target InputUsedByOutput
                           _extra-target InputUsedByOutput]
              (g/connect source :value-out target :string-input)))
          initial-basis (g/now)
          roots #{(gt/endpoint source :value-out)}
          initial-result (ig/dependencies initial-basis roots)
          initial-endpoints #{(gt/endpoint source :value-out)
                              (gt/endpoint target :out-from-input)}
          connected-endpoints (conj initial-endpoints
                                    (gt/endpoint extra-target :out-from-input))]
      (is (= initial-endpoints (set initial-result)))
      (is (= initial-endpoints
             (set (ig/dependencies initial-basis
                                   (hash-set (gt/endpoint source :value-out))))))
      (g/transact (g/set-property source :value "after"))
      (is (= "after" (g/node-value target :out-from-input)))
      (is (= initial-endpoints (set (ig/dependencies (g/now) roots))))
      (g/transact {:undo-key ::connection}
        (g/connect source :value-out extra-target :string-input))
      (is (= connected-endpoints (set (ig/dependencies (g/now) roots))))
      (is (= initial-endpoints (set (ig/dependencies initial-basis roots))))
      (g/undo! ::connection)
      (is (= initial-endpoints (set (ig/dependencies (g/now) roots))))
      (g/redo! ::connection)
      (is (= connected-endpoints (set (ig/dependencies (g/now) roots)))))))

(deftest single-connection
  (testing "results include inputs"
    (ts/with-clean-system
      (let [[a] (ts/tx-nodes (g/make-node SingleOutput))
            deps (dependencies a :out-from-inline)]
        (is (= deps
               #{(gt/endpoint a :out-from-inline)})))))

  (testing "without outputs, nobody cares"
    (ts/with-clean-system
      (let [[a b] (ts/tx-nodes (g/make-node SingleOutput)
                               (g/make-node InputNoOutput))
            _     (g/transact (g/connect a :out-from-inline b :unused-input))
            deps  (dependencies a :out-from-inline)]
        (is (= deps
               #{(gt/endpoint a :out-from-inline)})))))

  (testing "direct dependent outputs appear"
    (ts/with-clean-system
      (let [[a b] (ts/tx-nodes (g/make-node SingleOutput)
                               (g/make-node InputUsedByOutput))
            _     (g/transact (g/connect a :out-from-inline b :string-input))
            deps  (dependencies a :out-from-inline)]
        (is (= deps
               #{(gt/endpoint a :out-from-inline)
                 (gt/endpoint b :out-from-input)}))))))

(deftest fan-in
  (testing "results include inputs"
    (ts/with-clean-system
      (let [[a b c d] (ts/tx-nodes (g/make-node SingleOutput)
                                   (g/make-node SingleOutput)
                                   (g/make-node SingleOutput)
                                   (g/make-node SingleOutput))
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
      (let [[a b c d x] (ts/tx-nodes (g/make-node SingleOutput)
                                     (g/make-node SingleOutput)
                                     (g/make-node SingleOutput)
                                     (g/make-node SingleOutput)
                                     (g/make-node InputUsedByOutput))
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
      (let [[a w x y z] (ts/tx-nodes (g/make-node SingleOutput)
                                     (g/make-node InputUsedByOutput)
                                     (g/make-node InputUsedByOutput)
                                     (g/make-node InputUsedByOutput)
                                     (g/make-node InputUsedByOutput))
            _           (g/transact
                          (concat
                            (g/connect a :out-from-inline x :string-input)
                            (g/connect a :out-from-inline y :string-input)
                            (g/connect a :out-from-inline z :string-input)
                            (g/connect a :out-from-inline w :string-input)))
            deps        (dependencies a :out-from-inline)]
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

;; Dependency walks must keep every endpoint when fresh or cached successor rows
;; span several batches, cross row boundaries, or are empty.
(deftest wide-successor-frontier-test
  (ts/with-clean-system
    (let [[source other-source empty-source]
          (ts/tx-nodes (g/make-nodes [_source SingleOutput
                                      _other-source SingleOutput
                                      _empty-source SingleOutput]))
          targets (vec (ts/tx-nodes
                         (mapv (fn [_] (g/make-node InputUsedByOutput))
                               (range (inc (* 2 ig/dependency-batch-size))))))
          _ (g/transact
              (mapv (fn [^long i target]
                      (g/connect (if (< i (inc ig/dependency-batch-size)) source other-source) :out-from-inline target :string-input))
                    (range (count targets))
                    targets))
          roots (mapv #(gt/endpoint % :out-from-inline) [source other-source empty-source])
          basis (ig/invalidate-all-successors (g/now))
          expected (coll/into-> targets (set roots) (map #(gt/endpoint % :out-from-input)))
          actual (ig/dependencies basis roots)]
      (is (= expected (set actual)))
      (is (= (count expected) (count actual)))
      (is (= expected (set (ig/dependencies basis roots))))
      (testing "different roots reuse cached successor rows"
        (let [roots [(gt/endpoint source :out-from-inline)]
              expected (coll/into-> (subvec targets 0 (inc ig/dependency-batch-size))
                         (set roots)
                         (map #(gt/endpoint % :out-from-input)))
              actual (ig/dependencies basis roots)]
          (is (= expected (set actual)))
          (is (= (count expected) (count actual))))))))

;; Dependency walks must finish short and long converging branches across task
;; batches, including after disconnection and undo.
(deftest uneven-branch-dependencies-test
  (ts/with-clean-system
    (let [[source sink] (ts/tx-nodes (g/make-node SingleOutput)
                                     (g/make-node MultipleInputsIntoOneOutput))
          branches (vec (g/tx-nodes-added
                          (g/transact
                            (mapv (fn [_] (g/make-node InputUsedByOutput))
                                  (range (inc ig/dependency-batch-size))))))
          chain (vec (g/tx-nodes-added
                       (g/transact (mapv (fn [_] (g/make-node InputUsedByOutput)) (range 40)))))
          path (into [(branches 0)] chain)
          roots #{(gt/endpoint source :out-from-inline)}
          expected (coll/into-> (into branches chain)
                     #{(gt/endpoint source :out-from-inline)
                       (gt/endpoint sink :input-counter)}
                     (map #(gt/endpoint % :out-from-input)))]
      (g/transact
        [(mapv #(g/connect source :out-from-inline % :string-input) branches)
         (mapv (fn [from to] (g/connect from :out-from-input to :string-input)) path chain)
         (g/connect (branches 1) :out-from-input sink :input-1)
         (g/connect (peek chain) :out-from-input sink :input-2)])
      (let [basis (g/now)
            result (ig/dependencies basis roots)]
        (is (= expected (set result)))
        (is (= (count expected) (count result)))
        (g/transact (g/disconnect source :out-from-inline (branches 0) :string-input))
        (is (= (reduce disj expected (mapv #(gt/endpoint % :out-from-input) path))
               (set (ig/dependencies (g/now) roots))))
        (is (= expected (set (ig/dependencies basis roots))))
        (g/undo! :undo/global)
        (is (= expected (set (ig/dependencies (g/now) roots))))))))

;; Concurrent dependency walks must keep their own graph generation when a
;; connection has changed, including several labels on the same source node.
(deftest concurrent-successor-generations-test
  (ts/with-clean-system
    (let [[source old-target new-target]
          (ts/tx-nodes
            (g/make-nodes [source MultipleOutputs
                           old-target InputUsedByOutput
                           _new-target InputUsedByOutput]
              (g/connect source :output-1 old-target :string-input)))
          roots (mapv #(gt/endpoint source %) [:output-1 :output-2 :output-3 :output-4 :output-5])
          old-basis (g/now)
          _ (g/transact [(g/disconnect source :output-1 old-target :string-input)
                         (g/connect source :output-1 new-target :string-input)])
          new-basis (g/now)
          ready (CountDownLatch. 2)
          start (CountDownLatch. 1)
          results (mapv (fn [basis]
                          (future
                            (.countDown ready)
                            (.await start)
                            (set (ig/dependencies basis roots))))
                        [old-basis new-basis])]
      (try
        (is (.await ready 10 TimeUnit/SECONDS))
        (finally (.countDown start)))
      (is (= (conj (set roots) (gt/endpoint old-target :out-from-input))
             (deref (results 0) 10000 ::timed-out)))
      (is (= (conj (set roots) (gt/endpoint new-target :out-from-input))
             (deref (results 1) 10000 ::timed-out))))))

;; Concurrent walks with different roots must preserve dependency results when
;; caching different labels of one node in a shared cache, including a later
;; walk that reuses those rows with another root collection.
(deftest concurrent-successor-cache-writes-test
  (ts/with-clean-system
    (let [[source] (ts/tx-nodes (g/make-node MultipleOutputs))
          target-groups (mapv (fn [_]
                                (vec (ts/tx-nodes
                                       (mapv (fn [_] (g/make-node InputUsedByOutput))
                                             (range (inc ig/dependency-batch-size))))))
                              (range 2))
          _ (g/transact
              (mapv (fn [label targets]
                      (mapv #(g/connect source label % :string-input) targets))
                    [:output-1 :output-2]
                    target-groups))
          basis (ig/invalidate-all-successors (g/now))
          root-groups [[(gt/endpoint source :output-1)]
                       [(gt/endpoint source :output-2)
                        (gt/endpoint source :output-3)]]
          expected-results (mapv (fn [roots targets]
                                   (coll/into-> targets (set roots)
                                     (map #(gt/endpoint % :out-from-input))))
                                 root-groups
                                 target-groups)
          ready (CountDownLatch. 2)
          start (CountDownLatch. 1)
          results (mapv (fn [roots]
                          (future
                            (.countDown ready)
                            (.await start)
                            (set (ig/dependencies basis roots))))
                        root-groups)]
      (try
        (is (.await ready 10 TimeUnit/SECONDS))
        (finally (.countDown start)))
      (doseq [[expected result] (mapv vector expected-results results)]
        (is (= expected (deref result 10000 ::timed-out))))
      (let [roots (into (root-groups 0) (root-groups 1))
            expected (coll/into-> target-groups (set roots)
                       (comp cat (map #(gt/endpoint % :out-from-input))))
            actual (ig/dependencies basis roots)]
        (is (= expected (set actual)))
        (is (= (count expected) (count actual)))))))

(deftest one-step-multipath
  (testing "one output to several inputs"
    (ts/with-clean-system
      (let [[a x] (ts/tx-nodes (g/make-node SingleOutput)
                               (g/make-node MultipleInputsIntoOneOutput))
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
      (let [[a x] (ts/tx-nodes (g/make-node MultipleOutputs)
                               (g/make-node MultipleInputsIntoOneOutput))
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
      (let [[a x] (ts/tx-nodes (g/make-node SingleOutput)
                               (g/make-node SelfDependent))
            _     (g/transact
                    (g/connect a :out-from-inline x :string-input))
            deps  (dependencies a :out-from-inline)]
        (is (= deps
               #{(gt/endpoint a :out-from-inline)
                 (gt/endpoint x :uppercased)
                 (gt/endpoint x :counted)})))))

  (testing "outputs can depend on inputs with the same name"
    (ts/with-clean-system
      (let [[a x] (ts/tx-nodes (g/make-node SingleOutput)
                               (g/make-node BadlyWrittenSelfDependent))
            _     (g/transact
                    (g/connect a :out-from-inline x :string-value))
            deps  (dependencies a :out-from-inline)]
        (is (= deps
               #{(gt/endpoint a :out-from-inline)
                 (gt/endpoint x :string-value)}))))))

(deftest diamond-pattern
  (testing "multipath reaching the same node"
    (ts/with-clean-system
      (let [[a b c d] (ts/tx-nodes (g/make-node SingleOutput)
                                   (g/make-node InputUsedByOutput)
                                   (g/make-node InputUsedByOutput)
                                   (g/make-node InputUsedByOutput))
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
      (let [[a b x] (ts/tx-nodes (g/make-node SingleOutput)
                                 (g/make-node SingleOutput)
                                 (g/make-node TwoIndependentOutputs))
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
