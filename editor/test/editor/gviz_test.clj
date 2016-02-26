(ns editor.gviz-test
  (:require [clojure.java.io :as io]
            [clojure.test :refer :all]
            [editor.gviz :as gviz]
            [integration.test-util :as test-util]
            [dynamo.graph :as g]
            [support.test-support :refer [with-clean-system tx-nodes]]))

(deftest installed []
  (gviz/installed?))

(g/defnode SimpleNode
  (property prop g/Str)
  (input in g/Str)
  (output out g/Str (g/fnk [in] in)))

(deftest simple []
  (with-clean-system
    (let [g (g/make-graph!)
          nodes (tx-nodes (g/make-nodes g [n0 [SimpleNode :prop "test"]
                                           n1 [SimpleNode :prop "test2"]]
                                        (g/connect n0 :out n1 :in)))
          dot (gviz/subgraph->dot (g/now))]
      (is (re-find #"SimpleNode" dot)))))

(deftest broken-graph []
  (with-clean-system
    (let [g (g/make-graph!)
          nodes (tx-nodes (g/make-nodes g [n0 [SimpleNode :prop "test"]
                                           n1 [SimpleNode :prop "test2"]]
                                        (g/connect n0 :out n1 :in)))
          basis (update-in (g/now) [:graphs g :nodes] dissoc (first nodes))
          dot (gviz/subgraph->dot basis)]
      (is (re-find #"red" dot)))))

(deftest gui []
  (with-clean-system
    (let [workspace (test-util/setup-workspace! world)
          project (test-util/setup-project! workspace)
          node-id (test-util/resource-node project "/logic/main.gui")
          basis (g/now)
          dot (gviz/subgraph->dot basis :root-id node-id :input-fn (fn [[s sl t tl]]
                                                                     (when-let [type (g/node-type* basis t)]
                                                                       ((g/cascade-deletes type) tl))))]
      (is (< 1000 (count dot))))))
