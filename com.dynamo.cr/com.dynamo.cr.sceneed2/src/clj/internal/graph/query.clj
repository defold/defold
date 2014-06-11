(ns internal.graph.query
  "Simplistic query engine for graphs."
  (:require [internal.graph.dgraph :refer :all]
            [internal.graph.lgraph :refer :all]))

;; enhancements:
;;  - additive matches for first clause, subtractive for rest.
;;  - use indexes for commonly-queries nodes
;;  - cache built queries in the graph itself.
;;  - hash join?
(defprotocol Clause
  (bind [this next graph candidates] "Match against a graph structure, passing the matching subset on to the continuation"))

(deftype ScanningClause [attr value]
  Clause
  (bind [this next graph candidates]
    (let [rfn (fn [matches candidate-id]
                (let [node-value (node graph candidate-id)]
                  (if (= value (get node-value attr))
                    (conj matches candidate-id)
                    matches)))]
      #(next graph (reduce rfn #{} candidates)))))

(defn- clause-instance
  [clause]
  (ScanningClause. (first clause) (second clause)))

(defn- add-clause
  [ls clause]
  (fn [graph candidates]
    (bind (clause-instance clause) ls graph candidates)))

(defn- tail
  [graph candidates]
  candidates)

(defn query 
  [g clauses]
  (let [qfn (reduce add-clause tail (reverse clauses))]
    (trampoline qfn g (into #{} (node-ids g)))))