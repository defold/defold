(ns internal.node-type-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [internal.graph.types :as gt]
            [internal.node :as in]
            [internal.util :as util]
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
  (is (= #{:_declared-properties :_overridden-properties :sub-out :sub-prop :_output-jammers :_properties :super-out :super-prop :_node-id}
         (util/key-set (g/declared-outputs SubType))))
  (is (= #{:super-in :sub-in} (util/key-set (g/declared-inputs SubType))))
  (is (= #{:super-prop :sub-prop} (g/declared-property-labels SubType))))

(deftest input-prop-collision
  (is (thrown? AssertionError (eval '(do (require '[dynamo.graph :as g])
                                         (g/defnode InputPropertyCollision
                                           (input value g/Str)
                                           (property value g/Str)))))))

(deftest output-arg-missing
  (is (thrown? AssertionError (eval '(do (require '[dynamo.graph :as g])
                                         (g/defnode OutputArgMissing
                                           (output out-value g/Str (g/fnk [missing-arg] nil))))))))

(deftest deep-inheritance
  (with-clean-system
    (let [[n] (tx-nodes (g/make-node world SubSubType))]
      (is (g/node-instance? SuperType n)))))

(g/defnode SimpleNode)

(g/defnode MultiInheritance
  (inherits SuperType)
  (inherits SimpleNode))

(defn node-type [val-ref]
  (:key @val-ref))

(deftest isa-node-types
  (is (isa? (node-type SubSubType) (node-type SubType)))
  (is (isa? (node-type SubSubType) (node-type SuperType)))
  (is (isa? (node-type MultiInheritance) (node-type SimpleNode)))
  (is (isa? (node-type MultiInheritance) (node-type SuperType))))
