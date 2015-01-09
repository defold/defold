(ns dynamo.defnode-test
  (:require [clojure.test :refer :all]
            [schema.core :as s]
            [dynamo.node :as n]
            [dynamo.property :as dp]
            [internal.node :as in]))

(def Roger "rabbit")
(dp/defproperty TestP s/Str)

(defn- find-first [v k]
  (first (filter #(= k (first %)) v)))

(deftest compile-forms
  (testing "output"
    (let [selfie (in/compile-defnode-form 'A '(output self 'schema.core/Any dynamo.node/selfie))]
      (is (= 2 (count selfie)))
      (is (= #{:transforms :transform-types} (set (map first selfie))))
      (is (every? :self (map second selfie))))
    (let [cached (in/compile-defnode-form 'A '(output self 'schema.core/String :cached dynamo.node/selfie))]
      (is (= 3 (count cached)))
      (is (= #{:transforms :transform-types :cached} (set (map first cached))))
      (is (every? :self (map second cached)))
      (is (:self (second (find-first cached :cached)))))
    (let [on-update     (in/compile-defnode-form 'A '(output self 'schema.core/String :cached :on-update dynamo.node/selfie))
          order-swapped (in/compile-defnode-form 'A '(output self 'schema.core/String :cached :on-update dynamo.node/selfie))]
      (is (= 4 (count on-update) (count order-swapped)))
      (is (= #{:transforms :transform-types :cached :on-update} (set (map first on-update)) (set (map first order-swapped))))
      (is (every? :self (map second on-update)))
      (is (every? :self (map second order-swapped)))
      (is (:self (second (find-first on-update :cached))))
      (is (:self (second (find-first order-swapped :cached))))
      (is (:self (second (find-first on-update :on-update))))
      (is (:self (second (find-first order-swapped :on-update)))))
    (let [abstract (in/compile-defnode-form 'A '(output self 'schema.core/String :abstract dynamo.node/selfie))]
      (is (= 3 (count abstract)))
      (is (= #{:transforms :transform-types :abstract} (set (map first abstract))))
      (is (every? :self (map second abstract)))
      (is (:self (second (find-first abstract :abstract))))))

  (testing "super"
    (let [super (in/compile-defnode-form 'A '(inherits Roger))]
      (is (= 1 (count super)))
      (is (= :supers (ffirst super)))
      (is (= ['Roger] (second (first super))))))

  (testing "property"
    (let [prop (in/compile-defnode-form 'A '(property pname TestP))]
      (is (= 1 (count prop)))
      (is (= #{:properties} (set (map first prop))))
      (is (every? :pname (map second prop))))))

((:test (meta #'compile-forms)))

(doseq [sym-to-clean ['make-basic-node
                      'BasicNode
                      'make-i-root-node
                      'IRootNode
                      'make-i-root-node
                      'ChildNode
                      'make-child-node]]
  (ns-unmap 'dynamo.defnode-test sym-to-clean))

(n/defnode3 BasicNode)

(deftest basic-node-definition
  (is (var? (resolve (symbol "BasicNode"))))
  (let [ctor (resolve (symbol "make-basic-node"))]
    (is (var? ctor))
    (is (and
          (var? ctor)
          (fn? (var-get ctor))))
    (is (not (nil? (ctor))))
    (is (not (nil? (ctor :extra true))))
    (is (:extra (ctor :extra true)))))

(n/defnode3 IRootNode)
(n/defnode3 ChildNode (inherits IRootNode))
(n/defnode3 GChild (inherits ChildNode))
(n/defnode3 MixinNode)
(n/defnode3 GGChild
  (inherits ChildNode)
  (inherits MixinNode))

(deftest inheritance
  (is (var? (resolve (symbol "ChildNode"))))
  (let [ctor (resolve (symbol "make-child-node"))]
   (is (var? ctor))
   (is (and
         (var? ctor)
         (fn? (var-get ctor))))
   (is (not (nil? (ctor)))))
  (is (= [IRootNode] (:supers ChildNode)))
  (is (= [ChildNode] (:supers GChild)))
  (is (= [MixinNode ChildNode ] (:supers GGChild))))

(run-tests)