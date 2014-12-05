(ns internal.query
  (:require [internal.graph.dgraph :as dg]
            [internal.graph.lgraph :as lg]
            [internal.graph.query :as q]))

(defn node-by-id
  [world-ref id]
  (dg/node (:graph @world-ref) id))

(defn node-feeding-into
  [node label]
  (let [graph (-> node :world-ref deref :graph)]
    (dg/node graph
      (ffirst (lg/sources graph (:_id node) label)))))

(defn sources-of
  [target-node target-label]
  (let [graph (-> target-node :world-ref deref :graph)]
    (for [[node-id label] (lg/sources graph (:_id target-node) target-label)]
      [(dg/node graph node-id) label])))

(defn node-consuming
  ([node label]
     (node-consuming (-> node :world-ref deref :graph) node label))
  ([graph node label]
     (dg/node graph (ffirst (lg/targets graph (:_id node) label)))))

(defn query
  [world-ref clauses]
  (map #(node-by-id world-ref %) (q/query (:graph @world-ref) clauses)))

; ---------------------------------------------------------------------------
; Documentation
; ---------------------------------------------------------------------------
(doseq [[v doc]
       {*ns*
        "Functions for interrogating the world's graph."

        #'query
        "Query the project for resources that match all the clauses. Clauses are implicitly anded together.
A clause may be one of the following forms:

[:attribute value] - returns nodes that contain the given attribute and value.
(protocol protocolname) - returns nodes that satisfy the given protocol
(input fromnode)        - returns nodes that have 'fromnode' as an input
(output tonode)         - returns nodes that have 'tonode' as an output

All the list forms look for symbols in the first position. Be sure to quote the list
to distinguish it from a function call."}]
  (alter-meta! v assoc :doc doc))
