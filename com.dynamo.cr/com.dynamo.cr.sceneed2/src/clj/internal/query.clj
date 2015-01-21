(ns internal.query
  "Functions for interrogating the world's graph."
  (:require [internal.graph.dgraph :as dg]
            [internal.graph.lgraph :as lg]
            [internal.graph.query :as q]))

(defn- n->g
  "Get the graph this node belongs to."
  [node]
  (-> node :world-ref deref :graph))

(defn node-by-id
  "Get a node, given just a world and an id."
  [world-ref id]
  (dg/node (:graph @world-ref) id))

(defn node-feeding-into
  "Find the one-and-only node that sources this input on this node.
Should you use this on an input label with multiple connections, the result
is undefined."
  [node label]
  (let [graph (n->g node)]
    (dg/node graph
      (ffirst (lg/sources graph (:_id node) label)))))

(defn sources-of
  "Find the [node label] pairs for all connections into the given node's input label.
The result is a sequence of pairs. If you give the graph explicitly, then that point-in-time
will be searched. If you omit the graph argument, it means 'search the most recently
transacted point in time`."
  ([node label]
    (sources-of (n->g node) node label))
  ([graph node label]
    (map
      (fn [[node-id label]] [(dg/node graph node-id) label])
      (lg/sources graph (:_id node) label))))

(defn nodes-consuming
  "Find the [node label] pairs for all connections reached from the given node's input label.
The result is a sequence of pairs. If you give the graph explicitly, then that point-in-time
will be searched. If you omit the graph argument, it means 'search the most recently
transacted point in time`."
  ([node label]
     (nodes-consuming (n->g node) node label))
  ([graph node label]
     (map
       (fn [[node-id label]] [(dg/node graph node-id) label])
       (lg/targets graph (:_id node) label))))

(defn node-consuming
  "Like nodes-consuming, but only returns the first result."
  ([node label]
     (node-consuming (n->g node) node label))
  ([graph node label]
     (ffirst (nodes-consuming graph node label))))

(defn query
  "Query the project for resources that match all the clauses. Clauses are implicitly anded together.
A clause may be one of the following forms:

[:attribute value] - returns nodes that contain the given attribute and value.
(protocol protocolname) - returns nodes that satisfy the given protocol
(input fromnode)        - returns nodes that have 'fromnode' as an input
(output tonode)         - returns nodes that have 'tonode' as an output

All the list forms look for symbols in the first position. Be sure to quote the list
to distinguish it from a function call."
  [world-ref clauses]
  (map #(node-by-id world-ref %) (q/query (:graph @world-ref) clauses)))

