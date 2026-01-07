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

(ns internal.graph.graph-test
  (:require [clojure.core.cache :as cc]
            [clojure.string :as string]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [internal.graph :as ig]
            [internal.graph.generator :as ggen]
            [internal.graph.types :as gt]
            [internal.node :as in]
            [schema.core :as s]
            [support.test-support :refer [tx-nodes with-clean-system]]))

(defn occurrences [coll]
  (vals (frequencies coll)))

(defn random-graph
  []
  ((ggen/make-random-graph-builder)))

(deftest no-duplicate-node-ids
  (let [g (random-graph)]
    (is (= 1 (apply max (occurrences (ig/node-ids g)))))))

(deftest removing-node
  (let [v      "Any ig/node value"
        g      (random-graph)
        id     (inc (count (:nodes g)))
        g      (ig/add-node g id v)
        g      (ig/graph-remove-node g id nil)]
    (is (nil? (ig/node-id->node g id)))
    (is (empty? (filter #(= "Any ig/node value" %) (ig/node-values g))))))

(defn targets [g n l] (map gt/target (get-in g [:sarcs n l])))
(defn sources [g n l] (map gt/source (get-in g [:tarcs n l])))

(defn- source-arcs-without-targets
  [g]
  (for [source                (ig/node-ids g)
        source-label          (-> (ig/node-id->node g source) g/node-type in/output-labels)
        [target target-label] (targets g source source-label)
        :when                 (not (some #(= % [source source-label]) (sources g target target-label)))]
    [source source-label]))

(defn- target-arcs-without-sources
  [g]
  (for [target                (ig/node-ids g)
        target-label          (-> (ig/node-id->node g target) g/node-type in/input-labels)
        [source source-label] (sources g target target-label)
        :when                 (not (some #(= % [target target-label]) (targets g source source-label)))]
    [target target-label]))

(defn- arcs-are-reflexive?
  [g]
  (and (empty? (source-arcs-without-targets g))
       (empty? (target-arcs-without-sources g))))

(deftest reflexivity
  (let [g (random-graph)]
    (is (arcs-are-reflexive? g))))

(deftest transformable
  (let [g      (random-graph)
        id     (inc (count (:nodes g)))
        g      (ig/add-node g id {:number 0})
        g'     (ig/transform-node g id update-in [:number] inc)]
    (is (not= g g'))
    (is (= 1 (:number (ig/node-id->node g' id))))
    (is (= 0 (:number (ig/node-id->node g id))))))

(g/deftype T1        String)
(g/deftype T1-array  [String])
(g/deftype T2        Integer)
(g/deftype T2-array  [Integer])
(g/deftype Num       Number)
(g/deftype Num-array [Number])
(g/deftype Any       s/Any)
(g/deftype Any-array [s/Any])

(g/type-compatible? T1-array Any-array)

(deftest type-compatibility
  (are [first second compatible?] (= compatible? (g/type-compatible? first second))
    T1        T1         true
    T1        T2         false
    T1        T1-array   false
    T1-array  T1         false
    T1-array  T1-array   true
    T1-array  T2-array   false
    T1        T2-array   false
    T1-array  T2         false
    T2        Num        true
    T2-array  Num-array  true
    Num-array T2-array   false
    T1        Any        true
    T1        Any-array  false
    T1-array  Any-array  true
    T1-array  g/Any      true))

(g/defnode TestNode
  (property val g/Str)
  (property custom-val g/Str
            (value (g/fnk [val] val)))
  (output custom-val g/Str :cached (g/fnk [custom-val]
                                          (if (= custom-val "throw")
                                            (throw (ex-info "TestNode initialized to throw in output custom-val" {}))
                                            custom-val)))
  (output val-val g/Str :cached (g/fnk [val] (str val val))))

(g/defnode PassthroughNode
  (property str-prop g/Str
            (value (g/fnk [str-in] str-in))
            (dynamic str-prop-dynamic (g/fnk [] 123)))
  (input str-in g/Str :cascade-delete)
  (output str-out g/Str :cached (g/fnk [str-in] str-in)))

(deftest graph-override-cleanup
  (with-clean-system
    (let [[original override] (tx-nodes (g/make-nodes world [n (TestNode :val "original")]
                                                      (g/override n)))
          basis (g/now)]
      (is (= override (first (ig/get-overrides basis original))))
      (g/transact (g/delete-node original))
      (let [basis (g/now)]
        (is (empty? (ig/get-overrides basis original)))
        (is (empty? (get-in basis [:graphs world :overrides])))))))

(deftest graph-values
  (with-clean-system
    (g/set-graph-value! world :things {})
    (is (= {} (g/graph-value world :things)))
    (g/transact (g/update-graph-value world :things assoc :a 1))
    (is (= {:a 1} (g/graph-value world :things)))))

(deftest with-system
  (testing "property value change does not affect original"
    (with-clean-system
      (let [[n] (tx-nodes (g/make-nodes world [n (TestNode :val "original")]))]
        (is (= "original" (g/node-value n :val)))
        (let [clone (g/clone-system)]
          (g/with-system clone
            (g/transact (g/set-property n :val "cloned"))
            (is (= "cloned" (g/node-value n :val)))))
        (is (= "original" (g/node-value n :val))))))

  (testing "deleting node does not affect original"
    (with-clean-system
      (let [[n] (tx-nodes (g/make-nodes world [n (TestNode :val "original")]))]
        (let [clone (g/clone-system)]
          (g/with-system clone
            (g/transact (g/delete-node n))
            (is (= nil (g/node-by-id n)))))
        (is (not= nil (g/node-by-id n))))))

  (testing "changing cache does not affect original"
    (with-clean-system
      (let [[n] (tx-nodes (g/make-nodes world [n (TestNode :val "original")]))]
        (is (= "original" (g/node-value n :val)))

        (is (= ::miss (cc/lookup (g/cache) (gt/endpoint n :val-val) ::miss)))
        (is (= "originaloriginal" (g/node-value n :val-val)))
        (is (= "originaloriginal" (cc/lookup (g/cache) (gt/endpoint n :val-val) ::miss)))

        (let [clone (g/clone-system)]
          (g/with-system clone
            (is (= "originaloriginal" (cc/lookup (g/cache) (gt/endpoint n :val-val) ::miss)))

            (g/transact (g/set-property n :val "cloned"))
            (is (= "cloned" (g/node-value n :val)))

            (is (= ::miss (cc/lookup (g/cache) (gt/endpoint n :val-val) ::miss)))
            (is (= "clonedcloned" (g/node-value n :val-val)))
            (is (= "clonedcloned" (cc/lookup (g/cache) (gt/endpoint n :val-val) ::miss)))))

        (is (= "originaloriginal" (cc/lookup (g/cache) (gt/endpoint n :val-val) :miss)))))))

(deftest evaluation-context
  (testing "node-value sees state of graphs as given in evaluation-context"
    (with-clean-system
      (let [[n n2] (tx-nodes (g/make-nodes world [n (TestNode :val "initial")
                                                  n2 PassthroughNode]
                                           (g/connect n :val n2 :str-in)))
            init-ec (g/make-evaluation-context)]
        (g/transact (g/set-property n :val "changed"))
        (is (= "changed" (g/node-value n :val)))
        (is (= "changed" (g/node-value n2 :str-out)))
        (is (= "initial" (g/node-value n :val init-ec)))
        (is (= "initial" (g/node-value n2 :str-out init-ec)))
        (g/transact (g/delete-node n))
        (is (= nil (g/node-value n2 :str-out)))
        (is (= "initial" (g/node-value n2 :str-out init-ec))))))

  (testing "passing evaluation context does not automatically update cache"
    (with-clean-system
      (let [[n n2] (tx-nodes (g/make-nodes world [n (TestNode :val "initial")
                                                  n2 PassthroughNode]
                                           (g/connect n :val n2 :str-in)))
            init-ec (g/make-evaluation-context)]
        (is (= ::miss (cc/lookup (g/cache) (gt/endpoint n2 :str-out) ::miss)))
        (g/node-value n2 :str-out init-ec)
        (is (= ::miss (cc/lookup (g/cache) (gt/endpoint n2 :str-out) ::miss)))
        (g/update-cache-from-evaluation-context! init-ec)
        (is (= "initial" (cc/lookup (g/cache) (gt/endpoint n2 :str-out)))))))

  (testing "Updating cache does not change entries for invalidated outputs"
    (with-clean-system
      (let [[n n2] (tx-nodes (g/make-nodes world [n (TestNode :val "initial")
                                                  n2 PassthroughNode]
                                           (g/connect n :val n2 :str-in)))
            init-ec (g/make-evaluation-context)]
        (g/node-value n2 :str-out init-ec)
        (is (= ::miss (cc/lookup (g/cache) (gt/endpoint n2 :str-out) ::miss)))

        (g/transact (g/set-property n :val "change that invalidates n2 :str-out"))

        (g/update-cache-from-evaluation-context! init-ec)

        (is (= ::miss (cc/lookup (g/cache) (gt/endpoint n2 :str-out) ::miss))))))

  (testing "Update cache does not add entries for deleted nodes"
    (with-clean-system
      (let [[n n2] (tx-nodes (g/make-nodes world [n (TestNode :val "initial")
                                                  n2 PassthroughNode]
                                           (g/connect n :val n2 :str-in)))
            init-ec (g/make-evaluation-context)]
        (g/node-value n2 :str-out init-ec)
        (is (= ::miss (cc/lookup (g/cache) (gt/endpoint n2 :str-out) ::miss)))

        (g/transact (g/delete-node n2))

        (g/update-cache-from-evaluation-context! init-ec)

        (is (= ::miss (cc/lookup (g/cache) (gt/endpoint n2 :str-out) ::miss)))))))

(deftest tracer
  (with-clean-system
    (let [[tn n1] (tx-nodes (g/make-nodes world [tn (TestNode :val "initial")
                                                 n1 PassthroughNode]
                                          (g/connect tn :custom-val n1 :str-in)))]
      (let [result (atom nil)]
        (g/node-value n1 :str-out (g/make-evaluation-context {:tracer (g/make-tree-tracer result)}))
        (is (= @result
               {:node-id n1,
                :output-type :output,
                :label :str-out,
                :dependencies
                [{:node-id tn,
                  :output-type :output,
                  :label :custom-val,
                  :dependencies
                  [{:node-id tn,
                    :output-type :property,
                    :label :custom-val,
                    :dependencies
                    [{:node-id tn,
                      :output-type :raw-property,
                      :label :val,
                      :dependencies [],
                      :state :end}],
                    :state :end}],
                  :state :end}],
                :state :end}))

        ;; here we disable :local-temp because we're only testing for :local caching
        (let [ec (g/make-evaluation-context {:tracer (g/make-tree-tracer result) :no-local-temp true})]
          (g/node-value n1 :_properties ec)
          (is (= @result
                 {:node-id n1,
                  :output-type :output,
                  :label :_properties,
                  :dependencies
                  [{:node-id n1,
                    :output-type :dynamic,
                    :label [:str-prop :str-prop-dynamic],
                    :dependencies [],
                    :state :end}
                   {:node-id n1,
                    :output-type :property,
                    :label :str-prop,
                    :dependencies
                    [{:node-id tn,
                      :output-type :output,
                      :label :custom-val,
                      :dependencies
                      [{:node-id tn,
                        :output-type :property,
                        :label :custom-val,
                        :dependencies
                        [{:node-id tn,
                          :output-type :raw-property,
                          :label :val,
                          :dependencies [],
                          :state :end}],
                        :state :end}],
                      :state :end}],
                    :state :end}],
                  :state :end}))

          ;; Now, ec :local contains n :custom-val, so trace is slightly shorter.
          ;; Note that the tree tracer in the ec can be reused.

          (g/node-value n1 :_properties ec)
          (is (= @result
                 {:node-id n1,
                  :output-type :output,
                  :label :_properties,
                  :dependencies
                  [{:node-id n1,
                    :output-type :dynamic,
                    :label [:str-prop :str-prop-dynamic],
                    :dependencies [],
                    :state :end}
                   {:node-id n1,
                    :output-type :property,
                    :label :str-prop,
                    :dependencies
                    [{:node-id tn,
                      :output-type :cache,
                      :label :custom-val
                      :dependencies []
                      :state :end}]
                    :state :end}]
                  :state :end})))

        (g/transact (g/set-property tn :val "throw"))

        (let [ec (g/make-evaluation-context {:tracer (g/make-tree-tracer result)})]
          (is (thrown? Exception (g/node-value n1 :_properties ec)))
          (is (= @result
                 {:node-id n1,
                  :output-type :output,
                  :label :_properties,
                  :dependencies
                  [{:node-id n1,
                    :output-type :dynamic,
                    :label [:str-prop :str-prop-dynamic],
                    :dependencies [],
                    :state :end}
                   {:node-id n1,
                    :output-type :property,
                    :label :str-prop,
                    :dependencies
                    [{:node-id tn,
                      :output-type :output,
                      :label :custom-val,
                      :dependencies
                      [{:node-id tn,
                        :output-type :property,
                        :label :custom-val,
                        :dependencies
                        [{:node-id tn,
                          :output-type :raw-property,
                          :label :val,
                          :dependencies [],
                          :state :end}],
                        :state :end}],
                      :state :fail}],
                    :state :fail}],
                  :state :fail})))

        (g/transact (g/set-property tn :val "no throw"))
        (reset! result nil)

        (let [ec (g/make-evaluation-context {:tracer (juxt (g/make-tree-tracer result)
                                                           (fn [state node-id output-type label]
                                                             (when (= [state node-id label] [:end tn :custom-val])
                                                               (throw (ex-info "tracer somehow failed" {})))))})]

          (is (thrown? Exception (g/node-value n1 :_properties ec)))
          (is (= @result
                 {:node-id 1,
                  :output-type :output,
                  :label :_properties,
                  :dependencies
                  [{:node-id 1,
                    :output-type :dynamic,
                    :label [:str-prop :str-prop-dynamic],
                    :dependencies [],
                    :state :end}
                   {:node-id 1,
                    :output-type :property,
                    :label :str-prop,
                    :dependencies
                    [{:node-id 0,
                      :output-type :output,
                      :label :custom-val,
                      :dependencies
                      [{:node-id 0,
                        :output-type :property,
                        :label :custom-val,
                        :dependencies
                        [{:node-id 0,
                          :output-type :raw-property,
                          :label :val,
                          :dependencies [],
                          :state :end}],
                        :state :end}],
                      :state :fail}],
                    :state :fail}],
                  :state :fail})))))))

(deftest valid-node-value
  (with-clean-system
    (let [[error-node consumer-node]
          (tx-nodes
            (g/make-nodes
              world [error-node (TestNode :val "initial")
                     consumer-node PassthroughNode]
              (g/connect error-node :val consumer-node :str-in)))]
      (is (= (g/node-value consumer-node :str-out)
             (g/valid-node-value consumer-node :str-out)))
      (g/mark-defective! error-node (g/error-fatal "bad"))
      (let [error-value (g/node-value consumer-node :str-out)]
        (is (g/error? error-value))
        (when (is (thrown? Exception (g/valid-node-value consumer-node :str-out)))
          (try
            (g/valid-node-value consumer-node :str-out)
            (catch Exception exception
              (let [ex-message (ex-message exception)
                    ex-data (ex-data exception)]
                (is (string/includes? ex-message (name (in/type-name PassthroughNode))))
                (is (string/includes? ex-message (str consumer-node)))
                (is (string/includes? ex-message (str :str-out)))
                (is (string/includes? ex-message "produced an ErrorValue"))
                (when (is (map? ex-data))
                  (is (= :internal.graph.graph-test/PassthroughNode (:node-type-kw ex-data)))
                  (is (= :str-out (:label ex-data)))
                  (is (= error-value (:error ex-data))))))))))))

(deftest maybe-node-value
  (with-clean-system
    (let [check-valid-nodes!
          (fn check-valid-nodes! [producer-node consumer-node]
            (is (not (g/defective? producer-node)))
            (is (not (g/defective? consumer-node)))
            (testing "Declared property."
              (is (= "initial" (g/maybe-node-value producer-node :val))))
            (testing "Declared input."
              (is (= "initial" (g/maybe-node-value consumer-node :str-in))))
            (testing "Declared output."
              (is (= "initial" (g/maybe-node-value consumer-node :str-out))))
            (testing "Missing output."
              (is (nil? (g/maybe-node-value producer-node :non-existing-label))))
            (testing "Extern property."
              (is (= producer-node (g/maybe-node-value producer-node :_node-id))))
            (testing "Implicit :_properties."
              (is (= "initial" (get-in (g/maybe-node-value producer-node :_properties) [:properties :val :value]))))
            (testing "Implicit :_declared-properties."
              (is (= "initial" (get-in (g/maybe-node-value producer-node :_declared-properties) [:properties :val :value])))))

          check-defective-nodes!
          (fn check-defective-nodes! [producer-node consumer-node]
            (is (g/defective? producer-node))
            (is (not (g/defective? consumer-node)))
            (testing "Declared property."
              (is (nil? (g/maybe-node-value producer-node :val))))
            (testing "Declared input."
              (is (nil? (g/maybe-node-value consumer-node :str-in))))
            (testing "Declared output."
              (is (nil? (g/maybe-node-value consumer-node :str-out))))
            (testing "Missing output."
              (is (nil? (g/maybe-node-value producer-node :non-existing-label))))
            (testing "Extern property."
              (is (= producer-node (g/maybe-node-value producer-node :_node-id))))
            (testing "Implicit :_properties."
              (is (nil? (g/maybe-node-value producer-node :_properties))))
            (testing "Implicit :_declared-properties."
              (is (= "initial" (get-in (g/maybe-node-value producer-node :_declared-properties) [:properties :val :value])))))

          [producer-node consumer-node]
          (tx-nodes
            (g/make-nodes
              world [producer-node (TestNode :val "initial")
                     consumer-node PassthroughNode]
              (g/connect producer-node :val consumer-node :str-in)))

          [or-consumer-node or-producer-node]
          (tx-nodes
            (g/override consumer-node))]

      (testing "Nil arguments."
        (is (nil? (g/maybe-node-value nil nil)))
        (is (nil? (g/maybe-node-value nil :_node-id)))
        (is (nil? (g/maybe-node-value producer-node nil))))

      (testing "Non-defective producer-node."
        (testing "Original nodes."
          (check-valid-nodes! producer-node consumer-node))
        (testing "Override nodes."
          (check-valid-nodes! or-producer-node or-consumer-node)))

      (g/mark-defective! producer-node (g/error-fatal "bad"))

      (testing "Defective producer-node."
        (testing "Original nodes."
          (check-defective-nodes! producer-node consumer-node))
        (testing "Override nodes."
          (check-defective-nodes! or-producer-node or-consumer-node))))))

(deftest defective?-test
  (with-clean-system
    (let [error-node (g/make-node! world TestNode :val "initial")]
      (is (false? (g/defective? error-node)))
      (g/mark-defective! error-node (g/error-fatal "bad"))
      (is (true? (g/defective? error-node))))))
