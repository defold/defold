(ns dynamo.selection
  (:require [dynamo.graph :as g]
            [dynamo.types :as t]))

(g/defnode Selection
  (input selected-nodes 'g/Node :array)

  (output selection      t/Any     (g/fnk [selected-nodes] (mapv g/node-id selected-nodes)))
  (output selection-node Selection (g/fnk [self] self)))

(defmulti selected-nodes (fn [x] (class x)))
