(ns internal.graph.tracing
  (:require [clojure.set :as set]
            [dynamo.types :as t]
            [dynamo.util :as u]
            [internal.graph.dgraph :as dg]
            [internal.graph.lgraph :as lg]))

; ---------------------------------------------------------------------------
; Configuration parameters
; ---------------------------------------------------------------------------
(def maximum-graph-coloring-recursion 100)

; ---------------------------------------------------------------------------
; Implementation
; ---------------------------------------------------------------------------
(defn- pairs [m] (for [[k vs] m v vs] [k v]))

(defn tap [x] (println x) x)

(defn- marked-outputs
  [graph target-node target-label]
  (get (t/output-dependencies (dg/node graph target-node)) target-label))

(defn- marked-downstream-nodes
  [graph node-id output-label]
  (for [[target-node target-input] (lg/targets graph node-id output-label)
        affected-outputs           (marked-outputs graph target-node target-input)]
    [target-node affected-outputs]))

; ---------------------------------------------------------------------------
; API
; ---------------------------------------------------------------------------
(defn trace-dependencies
  "Follow arcs through the graph, from outputs to the inputs connected
  to them, and from those inputs to the downstream outputs that use
  them, and so on. Continue following links until all reachable
  outputs are found.

  Returns a collection of [node-ref output-label] pairs."
  ([graph to-be-marked]
   (trace-dependencies graph #{} to-be-marked maximum-graph-coloring-recursion))
  ([graph already-marked to-be-marked iterations-remaining]
   (assert (< 0 iterations-remaining) "Output tracing stopped; probable cycle in the graph")
   (if (empty? to-be-marked)
     already-marked
     (let [next-wave (mapcat #(apply marked-downstream-nodes graph %)  to-be-marked)]
       (recur graph (into already-marked to-be-marked) next-wave (dec iterations-remaining))))))
