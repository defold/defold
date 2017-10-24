(ns editor.properties-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [editor.types :as t]
            [editor.properties :as properties]
            [support.test-support :refer [with-clean-system tx-nodes]]))

(g/defnode NumProp
  (property my-prop g/Num))

(g/defnode AnotherNumProp
  (property my-prop g/Num))

(g/defnode NumPropDiffName
  (property my-other-prop g/Num))

(g/defnode StrProp
  (property my-prop g/Str))

(g/defnode StrPropReadOnly
  (property my-prop g/Str
            (dynamic read-only? (g/constantly true))))

(g/defnode LinkedProps
  (property multi-properties g/Any
            (default {})
            (dynamic link (g/fnk [linked-properties] linked-properties))
            (dynamic override (g/fnk [user-properties] user-properties)))
  (input linked-properties g/Any)
  (input user-properties g/Any))

(defn- augment-props [p _node-id]
  (into {} (map (fn [[k v]]
                  [k (assoc v
                            :node-id _node-id
                            :type (type (:value v)))])
                p)))

(g/defnode UserProps
  (property user-properties g/Any)
  (output _properties g/Properties (g/fnk [_node-id user-properties]
                                          (update user-properties :properties augment-props _node-id))))

(g/defnode Vec3Prop
  (property my-prop t/Vec3))

(defn- coalesce-nodes [nodes]
  (:properties (properties/coalesce (mapv #(g/node-value % :_properties) nodes))))

(defn- display-order [nodes]
  (:display-order (properties/coalesce (mapv #(g/node-value % :_properties) nodes))))

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
              (is (false? (reduce = (get-in props [:my-prop :values]))))
              (is (= [:my-prop] (display-order nodes)))))
    (testing "Different names are excluded"
             (let [nodes (g/tx-nodes-added (g/transact
                                            (concat
                                              (g/make-node world NumProp :my-prop 1.0)
                                              (g/make-node world NumPropDiffName :my-other-prop 1.0))))]
              (is (empty? (coalesce-nodes nodes)))
              (is (empty? (display-order nodes)))))
    (testing "Different types are excluded"
             (let [nodes (g/tx-nodes-added (g/transact
                                            (concat
                                              (g/make-node world NumProp :my-prop 1.0)
                                              (g/make-node world StrProp :my-prop "1.0"))))]
              (is (empty? (coalesce-nodes nodes)))
              (is (empty? (display-order nodes)))))))

(deftest linked-properties
  (with-clean-system
    (testing "Linked properties"
             (let [[str-node link-node] (g/tx-nodes-added (g/transact
                                                            (g/make-nodes world [str-node [StrProp :my-prop "1.0"]
                                                                                 linked-node [LinkedProps]]
                                                                          (g/connect str-node :_properties linked-node :linked-properties))))
                   properties (coalesce-nodes [link-node])
                   _ (properties/set-values! (:my-prop properties) (repeat "2.0"))
                   new-properties (coalesce-nodes [link-node])]
               (is (not (properties/overridden? (:my-prop properties))))
               (is (every? #(= "1.0" %) (get-in properties [:my-prop :values])))
               (is (every? #(= "2.0" %) (get-in new-properties [:my-prop :values])))))
    (testing "Overridden properties"
             (let [[user-node linked-node]
                   (g/tx-nodes-added
                     (g/transact
                       (g/make-nodes world [user-node [UserProps :user-properties {:properties {:int {:edit-type {:type g/Int} :value 1}}
                                                                                   :display-order [:int]}]]
                                     (:tx-data (g/override user-node)))))
                   property (fn [n] (-> [n] (coalesce-nodes) (first) (second)))]
               (is (not (properties/overridden? (property linked-node))))
               (is (every? #(= 1 %) (properties/values (property linked-node))))
               (properties/set-values! (property linked-node) (repeat 2))
               (is (every? #(= 2 %) (properties/values (property linked-node))))
               (is (properties/overridden? (property linked-node)))
               (properties/clear-override! (property linked-node))
               (is (not (properties/overridden? (property linked-node))))
               (is (every? #(= 1 %) (properties/values (property linked-node))))))))

(deftest multi-editing
  (with-clean-system
    (testing "Multi-editing"
             (let [nodes (g/tx-nodes-added (g/transact
                                             (concat
                                               (g/make-node world NumProp :my-prop 1.0)
                                               (g/make-node world AnotherNumProp :my-prop 2.0))))
                   properties (coalesce-nodes nodes)
                   _ (properties/set-values! (:my-prop properties) (repeat 3.0))
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
                   _ (properties/set-values! property (map #(assoc % 0 0.0) (:values property)))
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

(g/defnode DisplayLinkedProps
  (property a g/Num)
  (property linked-properties g/Any (default {})
            (dynamic link (g/fnk [linked-properties-input] linked-properties-input)))
  (property linked-properties-cat g/Any (default {})
            (dynamic link (g/fnk [linked-properties-input] linked-properties-input)))
  (property overridden-properties g/Any (default {})
            (dynamic override (g/fnk [user-properties-input] user-properties-input)))
  (property overridden-properties-cat g/Any (default {})
           (dynamic override (g/fnk [user-properties-input] user-properties-input)))
  (property b g/Num)

  (display-order [:a :b ["Linked" :linked-properties-cat] ["Overridden" :overridden-properties-cat]])

  (input linked-properties-input g/Any)
  (input user-properties-input g/Any))

(deftest multi-display-order
  (with-clean-system
    (let [link-node (first (g/tx-nodes-added (g/transact
                                               (g/make-nodes world [linked-node [DisplayLinkedProps]
                                                                    str-node [StrProp :my-prop "1.0"]
                                                                    user-node [UserProps :user-properties {:properties {:int {:edit-type {:type g/Int} :value 1}}
                                                                                                           :display-order [:int]}]]
                                                             (g/connect str-node :_properties linked-node :linked-properties-input)
                                                             (g/connect user-node :user-properties linked-node :user-properties-input)))))]
      (is (= [:a :b ["Linked" :my-prop] ["Overridden" [:overridden-properties-cat :int]] :my-prop [:overridden-properties :int]]
             (display-order [link-node]))))))

(deftest read-only
  (with-clean-system
    (let [nodes (tx-nodes
                  (g/make-nodes world [str-node [StrProp :my-prop "1.0"]
                                       str-node-ro [StrPropReadOnly :my-prop "1.0"]]))
          read-only-fn (fn [ns]
                         (properties/read-only? (get (coalesce-nodes ns) :my-prop)))]
      (is (false? (read-only-fn [(first nodes)])))
      (is (true? (read-only-fn [(second nodes)])))
      (is (true? (read-only-fn nodes))))))

(g/defnode DynamicSetFn
  (property a g/Num)
  (property count g/Any (default (atom 0)))

  (output _properties g/Properties (g/fnk [_declared-properties]
                                          (assoc-in _declared-properties [:properties :a :edit-type :set-fn]
                                                    (fn [evaluation-context self old new]
                                                      (swap! (g/node-value self :count evaluation-context) inc)
                                                      (g/set-property self :a new))))))

(deftest dynamic-set-fn
  (with-clean-system
    (let [n (first
              (tx-nodes
                (g/make-nodes world [n DynamicSetFn])))
          p (:a (coalesce-nodes [n]))]
      (is (= 0 @(g/node-value n :count)))
      (properties/set-values! p [1])
      (is (= 1 @(g/node-value n :count))))))

(g/defnode QuatAsEuler
  (property rotation t/Vec4
            (dynamic edit-type (g/constantly (properties/quat->euler)))))

(deftest value-conversion
  (with-clean-system
    (testing "Value conversion"
             (let [nodes (g/tx-nodes-added (g/transact
                                              (g/make-node world QuatAsEuler :rotation [0.0 0.0 0.0 1.0])))
                   property (-> (coalesce-nodes nodes) :rotation)]
               (is (= [[0.0 0.0 0.0]] (properties/values property)))
               (properties/set-values! property [[0.0 0.0 180.0]])
               (let [property (-> (coalesce-nodes nodes) :rotation)]
                 (is (= [[0.0 0.0 180.0]] (properties/values property))))))))


(g/defnode ErrorNode
  (property error? g/Bool)
  (property prop g/Any
            (value (g/fnk [error?]
                     (if error?
                       (g/error-fatal "error")
                       :ok)))))

(deftest property-value-errors
  (with-clean-system
    (testing "treats property value errors as errors"
      (let [[error-node :as nodes] (g/tx-nodes-added (g/transact
                                   (g/make-node world ErrorNode :error? false)))]
        (is (= [:ok] (properties/values (-> (coalesce-nodes nodes) :prop))))
        (is (not (g/error? (-> (coalesce-nodes nodes) :prop :errors))))
        (g/set-property! error-node :error? true)
        (is (= [nil] (properties/values (-> (coalesce-nodes nodes) :prop))))
        (is (g/error? (-> (coalesce-nodes nodes) :prop :errors)))))))
