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

(ns dynamo.integration.dependencies
  (:require [clojure.test :refer :all]
            [dynamo.graph :as g]
            [internal.graph.types :as gt]
            [support.test-support :refer [with-clean-system tx-nodes graph-dependencies]]
            [schema.core :as s]))

(g/defnode ChainedOutputNode
  (property foo g/Str (default "c"))
  (output a-output g/Str (g/fnk [foo] (str foo "a")))
  (output b-output g/Str (g/fnk [a-output] (str a-output "k")))
  (output c-output g/Str (g/fnk [b-output] (str b-output "e"))))

(g/defnode GladosNode
  (input  encouragement g/Str)
  (output cake-output g/Str (g/fnk [encouragement] (str "The " encouragement " is a lie"))))

(deftest test-dependencies-within-same-node
  (testing "dependencies from one output to another"
    (with-clean-system
      (let [[node-id] (tx-nodes (g/make-node world ChainedOutputNode))]
        (is (= "cake" (g/node-value node-id :c-output)))
        (are [label expected-dependencies] (= (set (map (fn [l] (gt/endpoint node-id l)) expected-dependencies))
                                              (graph-dependencies [(gt/endpoint node-id label)]))
          :c-output  [:c-output]
          :b-output  [:b-output :c-output]
          :a-output  [:a-output :b-output :c-output]
          :foo       [:a-output :b-output :c-output :foo :_declared-properties :_properties]))))

  (testing "dependencies from one output to another, across nodes"
    (with-clean-system
      (let [[anode-id gnode-id] (tx-nodes (g/make-node world ChainedOutputNode)
                                          (g/make-node world GladosNode))]
        (g/transact
         (g/connect anode-id :c-output gnode-id :encouragement))
        (is (= #{(gt/endpoint anode-id :c-output)
                 (gt/endpoint anode-id :b-output)
                 (gt/endpoint gnode-id :cake-output)}
               (graph-dependencies [(gt/endpoint anode-id :b-output)])))))))

(defn- find-node [self path]
  (let [graph (g/node-id->graph-id self)
        nodes (g/graph-value graph :nodes)]
    (get nodes path)))

(g/deftype OutlineData {s/Keyword s/Any})

(g/defnode OutlineNode
  (input child-outlines OutlineData :array)
  (output node-outline OutlineData :abstract))

(g/defnode ResourceNode
  (property resource g/Keyword)
  (inherits OutlineNode)
  (output node-outline OutlineData :cached (g/fnk [_node-id child-outlines]
                                             {:node-id _node-id :children child-outlines})))

(g/defnode SpriteNode
  (inherits ResourceNode))

(g/defnode GameObjectNode
  (inherits ResourceNode)
  (output node-outline OutlineData :cached (g/fnk [_node-id child-outlines]
                                             {:node-id _node-id :children child-outlines})))

(g/defnode ComponentNode
  (inherits OutlineNode)

  (property resource g/Keyword
    (value (g/fnk [source-resource] source-resource))
    (set (fn [_evaluation-context self old-value new-value]
           (let [node (find-node self new-value)]
             (concat
               (g/connect node :resource self :source-resource)
               (g/connect node :node-outline self :node-outline))))))

  (input source-resource g/Keyword)
  (input node-outline OutlineData)
  (output node-outline OutlineData :cached (g/fnk [_node-id node-outline] (assoc node-outline :node-id _node-id))))

(deftest test-dependencies-across-super-types-and-setters
  (with-clean-system
    ; Simulate node creation
    (let [[go-node sprite-node] (tx-nodes
                                  (g/make-nodes world [go-node [GameObjectNode :resource :game-object]
                                                       sprite-node [SpriteNode :resource :sprite]]))]
      (g/set-graph-value! world :nodes {:game-object go-node :sprite sprite-node})
      ; Simulate node loading
      (let [[comp-node] (tx-nodes
                          (g/make-nodes world [comp-node [ComponentNode :resource :sprite]]
                            (g/connect comp-node :node-outline go-node :child-outlines)))]
        (is (= 1 (count (:children (g/node-value go-node :node-outline)))))
        (g/transact (g/delete-node comp-node))
        (is (= 0 (count (:children (g/node-value go-node :node-outline)))))))))
