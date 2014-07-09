(ns dynamo.node
  (:require [schema.core :as s]
            [schema.macros :as sm]
            [plumbing.core :refer [fnk defnk]]
            [plumbing.fnk.pfnk :as pf]
            [internal.node :as in]
            [internal.graph.dgraph :as dg]
            [internal.graph.lgraph :as lg]
            [dynamo.types :refer :all]))

(declare get-value)

(defn get-inputs [target-node g target-label]
  (let [schema (get-in target-node [:inputs target-label])]
    (if (vector? schema)
      (map (fn [[source-node source-label]]
             (get-value g (dg/node g source-node) source-label))
           (lg/sources g (:_id target-node) target-label))
      (let [[first-source-node first-source-label] (first (lg/sources g (:_id target-node) target-label))]
        (get-value g (dg/node g first-source-node) first-source-label)))))

(defn collect-inputs [node g input-schema seed]
  (reduce-kv
    (fn [m k v]
      (condp = k
        :g         (assoc m k g)
        :this      (assoc m k node)
        :project   m
        s/Keyword  m
        (assoc m k (get-inputs node g k))))
    seed input-schema))

(defn perform [transform node g seed]
  (cond
    (symbol?     transform)  (perform (resolve transform) node g seed)
    (var?        transform)  (perform (var-get transform) node g seed)
    (has-schema? transform)  (transform (collect-inputs node g (pf/input-schema transform) seed))
    (fn?         transform)  (transform node g)
    :else transform))

(defn get-value [g node label & [seed]]
  (perform (get-in node [:transforms label]) node g seed))

(defn add-node [g node]
  (lg/add-labeled-node g (in/node-inputs node) (in/node-outputs node) node))

(defmacro defnode [name & behaviors]
  (let [behavior (in/merge-behaviors behaviors)]
    `(let []
       ~(in/generate-type name behavior)
       ~@(mapcat in/input-mutators (keys (:inputs behavior)))
       ~(in/generate-constructor name behavior))))
