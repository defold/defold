(ns integration.app-view-test
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system tx-nodes]]
            [editor.app-view :as app-view]
            [editor.atlas :as atlas]
            [editor.collection :as collection]
            [editor.cubemap :as cubemap]
            [editor.game-object :as game-object]
            [editor.image :as image]
            [editor.defold-project :as project]
            [editor.scene :as scene]
            [editor.sprite :as sprite]
            [editor.resource :as resource]
            [integration.test-util :as test-util])
  (:import [java.io File]
           [javax.imageio ImageIO]))

(g/defnode ANode
  (inherits project/ResourceNode)
  (property value-piece g/Str)
  (property value g/Str
            (set (fn [basis self old-value new-value]
                   (let [input (g/node-value self :value-input {:basis basis})]
                     (g/set-property self :value-piece (str (first input)))))))
  (input value-input g/Str))

(deftest select-test
  (testing "asserts that all node-ids are non-nil"
    (with-clean-system
      (let [[workspace project app-view] (test-util/setup! world)
            [node-1 node-2] (tx-nodes
                              (g/make-node world ANode)
                              (g/make-node world ANode))]
        (is (thrown? java.lang.AssertionError (app-view/select app-view [node-1 nil node-2]))))))
  (testing "preserves selection order"
    (with-clean-system
      (let [[workspace project app-view] (test-util/setup! world)
            [node-1 node-2 node-3] (tx-nodes (g/make-node world ANode)
                                     (g/make-node world ANode)
                                     (g/make-node world ANode))]
        (g/transact (app-view/select app-view [node-2 node-3 node-1]))
        (is (= [node-2 node-3 node-1] (g/node-value project :selected-node-ids)))
        (g/transact (app-view/select app-view [node-3 node-1 node-2]))
        (is (= [node-3 node-1 node-2] (g/node-value project :selected-node-ids))))))
  (testing "ensures selected nodes are distinct, preserving order"
    (with-clean-system
      (let [[workspace project app-view] (test-util/setup! world)
            [node-1 node-2 node-3] (tx-nodes (g/make-node world ANode)
                                     (g/make-node world ANode)
                                     (g/make-node world ANode))]
        (g/transact (app-view/select app-view [node-2 node-3 node-2 node-1 node-3 node-3 node-1]))
        (is (= [node-2 node-3 node-1] (g/node-value project :selected-node-ids)))
        (g/transact (app-view/select app-view [node-1 node-3 node-2 node-1 node-3 node-2 node-2]))
        (is (= [node-1 node-3 node-2] (g/node-value project :selected-node-ids)))))))
