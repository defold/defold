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

(deftype ABACAB [])
(deftype Image [])

(g/defnode N1)
(g/defnode N2)

(deftest input-compatibility
  (let [n1 (g/construct N1)
        n2 (g/construct N2)]
    (are [out-node out out-type in-node in in-type expect-compat why]
      (= expect-compat (core/compatible? [out-node out out-type in-node in in-type]))
      n1 :image Image    n2 :image  ABACAB    nil                    "type mismatch"
      n1 :image Image    n2 :image  Image     [n1 :image n2 :image]  "ok"
      n1 :image Image    n2 :images [Image]   [n1 :image n2 :images] "ok"
      n1 :image Image    n2 :images Image     nil                    "plural name, singular type"
      n1 :name  String   n2 :names  [String]  [n1 :name n2 :names]   "ok"
      n1 :name  String   n2 :names  String    nil                    "plural name, singular type"
      n1 :names [String] n2 :names  [String]  [n1 :names n2 :names]  "ok"
      n1 :name  String   n2 :name   [String]  nil                    "singular name, plural type")))

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
