(ns editor.properties-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.types :as t]
            [editor.properties :as properties]
            [support.test-support :refer [with-clean-system]]))

(g/defnode NumProp
  (property my-prop g/Num))

(g/defnode AnotherNumProp
  (property my-prop g/Num))

(g/defnode NumPropDiffName
  (property my-other-prop g/Num))

(g/defnode StrProp
  (property my-prop g/Str))

(g/defnode LinkedProps
  (property multi-property g/Any
            (dynamic link (g/fnk [linked-properties] linked-properties)))
  (input linked-properties g/Any))

(g/defnode Vec3Prop
  (property my-prop t/Vec3))

(defn coalesce-nodes [nodes]
  (properties/coalesce (mapv #(g/node-value % :properties) nodes)))

(deftest coalesce-properties
  (with-clean-system
    (testing "Empty coalescing"
             (is (empty? (coalesce-nodes []))))
    (testing "Single coalescing"
             (let [nodes (g/tx-nodes-added (g/transact
                                            (concat
                                              (g/make-node world NumProp :my-prop 1.0)
                                              (g/make-node world AnotherNumProp :my-prop 2.0))))]
              (is (not (empty? (coalesce-nodes nodes))))))
    (testing "Same types are included"
             (let [nodes (g/tx-nodes-added (g/transact
                                            (concat
                                              (g/make-node world NumProp :my-prop 1.0)
                                              (g/make-node world AnotherNumProp :my-prop 1.0))))
                   props (coalesce-nodes nodes)]
              (is (not (empty? props)))
              (is (every? #(= 1.0 %) (get-in props [:my-prop :values])))))
    (testing "Different values are kept"
             (let [nodes (g/tx-nodes-added (g/transact
                                            (concat
                                              (g/make-node world NumProp :my-prop 1.0)
                                              (g/make-node world AnotherNumProp :my-prop 2.0))))
                   props (coalesce-nodes nodes)]
              (is (not (empty? props)))
              (is (false? (reduce = (get-in props [:my-prop :values]))))))
    (testing "Different names are excluded"
             (let [nodes (g/tx-nodes-added (g/transact
                                            (concat
                                              (g/make-node world NumProp :my-prop 1.0)
                                              (g/make-node world NumPropDiffName :my-other-prop 1.0))))]
              (is (empty? (coalesce-nodes nodes)))))
    (testing "Different types are excluded"
             (let [nodes (g/tx-nodes-added (g/transact
                                            (concat
                                              (g/make-node world NumProp :my-prop 1.0)
                                              (g/make-node world StrProp :my-prop "1.0"))))]
              (is (empty? (coalesce-nodes nodes)))))))

(deftest linked-properties
  (with-clean-system
    (testing "Linked properties"
             (let [[str-node link-node] (g/tx-nodes-added (g/transact
                                                            (g/make-nodes world [str-node [StrProp :my-prop "1.0"]
                                                                                 linked-node [LinkedProps]]
                                                                          (g/connect str-node :properties linked-node :linked-properties))))
                   properties (coalesce-nodes [link-node])
                   _ (properties/set-value! (:my-prop properties) (repeat "2.0"))
                   new-properties (coalesce-nodes [link-node])]
               (is (every? #(= "1.0" %) (get-in properties [:my-prop :values])))
               (is (every? #(= "2.0" %) (get-in new-properties [:my-prop :values])))))))

(deftest multi-editing
  (with-clean-system
    (testing "Multi-editing"
             (let [nodes (g/tx-nodes-added (g/transact
                                             (concat
                                               (g/make-node world NumProp :my-prop 1.0)
                                               (g/make-node world AnotherNumProp :my-prop 2.0))))
                   properties (coalesce-nodes nodes)
                   _ (properties/set-value! (:my-prop properties) (repeat 3.0))
                   new-properties (coalesce-nodes nodes)]
               (is (every? #(not= 3.0 %) (get-in properties [:my-prop :values])))
               (is (every? #(= 3.0 %) (get-in new-properties [:my-prop :values])))))))

(deftest multi-editing-complex
  (with-clean-system
    (testing "Multi-editing"
             (let [nodes (g/tx-nodes-added (g/transact
                                             (concat
                                               (g/make-node world Vec3Prop :my-prop [1.0 2.0 3.0])
                                               (g/make-node world Vec3Prop :my-prop [2.0 3.0 4.0]))))
                   properties (coalesce-nodes nodes)
                   property (:my-prop properties)
                   _ (properties/set-value! property (map #(assoc % 0 0.0) (:values property)))
                   new-properties (coalesce-nodes nodes)]
               (is (every? #(not= 0.0 (nth % 0)) (get-in properties [:my-prop :values])))
               (is (every? #(= 0.0 (nth % 0)) (get-in new-properties [:my-prop :values])))))))

(deftest unifying-values
  (testing "Empty"
           (is (= nil (properties/unify-values []))))
  (testing "One"
           (is (= 1 (properties/unify-values [1]))))
  (testing "One and nil"
           (is (= nil (properties/unify-values [1 nil]))))
  (testing "Equality"
           (let [vals (repeat 10 1)]
             (is (= 1 (properties/unify-values (take 10 (repeat 1)))))))
  (testing "Early-out"
           (let [count 100
                 counter (atom 0)
                 vals (map #(do (swap! counter inc) %) (range count))]
             (is (nil? (properties/unify-values vals)))
             (is (< @counter count)))))
