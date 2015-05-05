(ns editor.scope-test
  (:require [clojure.test.check.clojure-test :refer [defspec]]
            [clojure.test.check.generators :as gen]
            [clojure.test.check.properties :as prop]
            [clojure.test.check :refer :all]
            [clojure.test :refer :all]
            [dynamo.graph :as g]
            [dynamo.graph.test-support :refer :all]
            [dynamo.types :as t]
            [editor.core :as core]))

(deftype Image [])

(g/defnode InputNode
  (input string-scalar String)
  (input non-matching-name-string-scalar String)
  (input images Image :array)
  (input names String))

(g/defnode DifferentInputNode
  (input string-scalar t/Num))

(g/defnode OutputNode
  (property string-scalar String)
  (property image Image)
  (property images [Image])
  (property name String))

(deftest input-compatibility
  (testing "Scalar Inputs"
    (with-clean-system
      (let [[output input different] (tx-nodes (g/make-node world OutputNode)
                                               (g/make-node world InputNode)
                                               (g/make-node world DifferentInputNode))]
        (is (core/compatible? [output :string-scalar input :string-scalar]))
        (is (not (core/compatible? [output :string-scalar input :non-matching-name-string-scalar])))
        (is (not (core/compatible? [output :string-scalar different :string-scalar]))))))
  (testing "Array Inputs"
    (with-clean-system
      (let [[output input different] (tx-nodes (g/make-node world OutputNode)
                                               (g/make-node world InputNode)
                                               (g/make-node world DifferentInputNode))]
        (is (core/compatible? [output :image input :images]))
        (is (not (core/compatible? [output :images input :images])))
        (is (not (core/compatible? [output :name input :names])))))))

(g/defnode DisposableNode
  t/IDisposable
  (dispose [this] (deliver (:latch this) true)))

(defspec scope-disposes-contained-nodes
  (prop/for-all [node-count gen/pos-int]
    (with-clean-system
      (let [[scope]        (tx-nodes (g/make-node world core/Scope))
            tx-result      (g/transact
                            (for [n (range node-count)]
                              (g/make-node world DisposableNode)))
            disposable-ids (map g/node-id (g/tx-nodes-added tx-result))]
        (g/transact (for [i disposable-ids]
                       (g/connect i :self scope :nodes)))
        (g/transact (g/delete-node scope))
        (yield)
        (let [disposed (take-waiting-to-dispose system)]
          (is (= (sort (conj disposable-ids (:_id scope))) (sort (map :_id disposed)))))))))
