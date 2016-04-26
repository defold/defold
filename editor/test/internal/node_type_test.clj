(ns internal.node-type-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [internal.graph.types :as gt]
            [internal.node :as in]
            [support.test-support :refer [with-clean-system tx-nodes]]))

(g/defnode SuperType
  (property super-prop g/Str)
  (input super-in g/Str)
  (output super-out g/Str (g/fnk [super-prop] super-prop)))

(g/defnode SubType
  (inherits SuperType)
  (property sub-prop g/Str)
  (input sub-in g/Str)
  (output sub-out g/Str (g/fnk [sub-prop] sub-prop)))

(g/defnode SubSubType
  (inherits SubType))

(deftest type-fns
  (is (= #{:super-out :sub-out :_properties :_declared-properties} (set (keys (g/declared-outputs SubType)))))
  (is (= #{:super-in :sub-in} (set (keys (g/declared-inputs SubType)))))
  (is (= #{:super-prop :sub-prop :_output-jammers :_node-id} (set (keys (g/declared-properties SubType))))))

(deftest input-prop-collision
  (is (thrown? AssertionError (eval '(g/defnode InputPropertyCollision
                                      (input value g/Str)
                                      (property value g/Str))))))

(deftest output-arg-missing
  (is (thrown? AssertionError (eval '(g/defnode OutputArgMissing
                                        (output out-value g/Str (g/fnk [missing-arg] nil)))))))

(deftest deep-inheritance
  (with-clean-system
    (let [[n] (tx-nodes (g/make-node world SubSubType))]
      (is (g/node-instance? SuperType n)))))

(g/defnode SimpleNode)

(g/defnode MultiInheritance
  (inherits SuperType)
  (inherits SimpleNode))

(deftest isa-node-types
  (is (isa? SubSubType SubType))
  (is (isa? SubSubType SuperType))
  (is (isa? MultiInheritance SimpleNode))
  (is (isa? MultiInheritance SuperType)))
