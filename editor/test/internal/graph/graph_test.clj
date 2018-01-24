(ns internal.graph.graph-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [internal.graph :as ig]
            [internal.system :as is]
            [clojure.core.cache :as cc]
            [internal.graph.generator :as ggen]
            [internal.graph.types :as gt]
            [internal.node :as in]
            [schema.core :as s]
            [support.test-support :refer [with-clean-system tx-nodes]]))

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
    (is (nil? (ig/graph->node g id)))
    (is (empty? (filter #(= "Any ig/node value" %) (ig/node-values g))))))

(defn targets [g n l] (map gt/tail (get-in g [:sarcs n l])))
(defn sources [g n l] (map gt/head (get-in g [:tarcs n l])))

(defn- source-arcs-without-targets
  [g]
  (for [source                (ig/node-ids g)
        source-label          (-> (ig/graph->node g source) g/node-type in/output-labels)
        [target target-label] (targets g source source-label)
        :when                 (not (some #(= % [source source-label]) (sources g target target-label)))]
    [source source-label]))

(defn- target-arcs-without-sources
  [g]
  (for [target                (ig/node-ids g)
        target-label          (-> (ig/graph->node g target) g/node-type in/input-labels)
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
    (is (= 1 (:number (ig/graph->node g' id))))
    (is (= 0 (:number (ig/graph->node g id))))))

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
  (input str-in g/Str)
  (output str-out g/Str :cached (g/fnk [str-in] str-in)))

(deftest graph-override-cleanup
  (with-clean-system
    (let [[original override] (tx-nodes (g/make-nodes world [n (TestNode :val "original")]
                                                      (:tx-data (g/override n))))
          basis (is/basis system)]
      (is (= override (first (ig/get-overrides basis original))))
      (g/transact (g/delete-node original))
      (let [basis (is/basis system)]
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

        (is (= ::miss (cc/lookup (g/cache) [n :val-val] ::miss)))
        (is (= "originaloriginal" (g/node-value n :val-val)))
        (is (= "originaloriginal" (cc/lookup (g/cache) [n :val-val] ::miss)))

        (let [clone (g/clone-system)]
          (g/with-system clone
            (is (= "originaloriginal" (cc/lookup (g/cache) [n :val-val] ::miss)))

            (g/transact (g/set-property n :val "cloned"))
            (is (= "cloned" (g/node-value n :val)))

            (is (= ::miss (cc/lookup (g/cache) [n :val-val] ::miss)))
            (is (= "clonedcloned" (g/node-value n :val-val)))
            (is (= "clonedcloned" (cc/lookup (g/cache) [n :val-val] ::miss)))))

        (is (= "originaloriginal" (cc/lookup (g/cache) [n :val-val] :miss)))))))

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
        (is (= ::miss (cc/lookup (g/cache) [n2 :str-out] ::miss)))
        (g/node-value n2 :str-out init-ec)
        (is (= ::miss (cc/lookup (g/cache) [n2 :str-out] ::miss)))
        (g/update-cache-from-evaluation-context! init-ec)
        (is (= "initial" (cc/lookup (g/cache) [n2 :str-out]))))))

  (testing "Updating cache does not change entries for invalidated outputs"
    (with-clean-system
      (let [[n n2] (tx-nodes (g/make-nodes world [n (TestNode :val "initial")
                                                  n2 PassthroughNode]
                                           (g/connect n :val n2 :str-in)))
            init-ec (g/make-evaluation-context)]
        (g/node-value n2 :str-out init-ec)
        (is (= ::miss (cc/lookup (g/cache) [n2 :str-out] ::miss)))

        (g/transact (g/set-property n :val "change that invalidates n2 :str-out"))

        (g/update-cache-from-evaluation-context! init-ec)

        (is (= ::miss (cc/lookup (g/cache) [n2 :str-out] ::miss)))))))

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

        (let [ec (g/make-evaluation-context {:tracer (g/make-tree-tracer result)})]
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
                  :state :end}))

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
                    :state :fail}))))))))
