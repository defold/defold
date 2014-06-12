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


;;   "given the name of the protocol p as a symbol, "
(deftype ProtocolClause [p]
  Clause
  (bind [this next graph candidates]
    (let [rfn (fn [matches candidate-id]
                (let [node-value (node graph candidate-id)]
                  (if (satisfies? p node-value)
                    (conj matches candidate-id)
                    matches)))]
      #(next graph (reduce rfn #{} candidates)))))

(defn- bomb
  [& info]
  (throw (ex-info (apply str info) {})))

(defn- make-protocol-clause
  [clause]
  (let [prot (second clause)]
    (if-let [prot (if (:on-interface prot) prot (var-get (resolve (second clause))))]
      (ProtocolClause. prot)
      (bomb "Cannot resolve " (second clause)))))

(defn- clause-instance
  [clause]
  (cond 
    (vector? clause)  (ScanningClause. (first clause) (second clause))
    (list? clause)    (if (= 'protocol (first clause))
                        (make-protocol-clause clause)
                        (bomb "Unrecognized query function: " clause)) 
    :else             (bomb "Unrecognized clause: " clause)))

(defn- add-clause
  [ls clause]
  (fn [graph candidates]
    (bind (clause-instance clause) ls graph candidates)))

(defn- tail
  [graph candidates]
  candidates)

;; ProtocolClause - (protocol protocol-symbol)
;; ScanningClause - [:attr value]
;; Both (any order):
;; [(protocol symbol) [:attr value]]

(defn query 
  [g clauses]
  (let [qfn (reduce add-clause tail (reverse clauses))]
    (trampoline qfn g (into #{} (node-ids g)))))