(ns dynamo.selection
  (:require [dynamo.graph :as g]
            [dynamo.types :as t]
            [plumbing.core :refer [defnk fnk]]
            [schema.core :as s]))

(g/defnode Selection
  (input selected-nodes ['t/Node])

  (output selection      s/Any     (fnk [selected-nodes] (mapv :_id selected-nodes)))
  (output selection-node Selection (fnk [self] self)))

(defmulti selected-nodes (fn [x] (class x)))
