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
  (let [schema (get-in (dg/node g target-node) [:inputs target-label])]
    (if (coll? schema)
      (map (fn [[source-node source-label]]
             (get-value g source-node source-label))
           (lg/sources g target-node target-label))
      (apply get-value g (first (lg/sources g target-node target-label))))))

(defn collect-inputs [node g input-schema]
  (reduce-kv
    (fn [m k v]
      (condp = k
        :g         (assoc m k g)
        :this      (assoc m k node)
        s/Keyword  m
        (assoc m k (get-inputs node g k))))
    {} input-schema))

(defn perform [transform node g]
  (cond
    (symbol?     transform)  (perform (resolve transform) node g)
    (var?        transform)  (perform (var-get transform) node g)
    (has-schema? transform)  (transform (collect-inputs node g (pf/input-schema transform)))
    (fn?         transform)  (transform node g)
    :else transform))

(defn get-value [g node label]
  (perform (get-in (dg/node g node) [:transforms label]) node g))

(defn add-node [g node]
  (lg/add-labeled-node g (in/node-inputs node) (in/node-outputs node) node))

(defmacro defnode [name & behaviors]
  (let [behavior (in/merge-behaviors behaviors)]
    `(let []
       ~(in/generate-type name behavior)
       ~@(mapcat in/input-mutators (keys (:inputs behavior)))
       ~(in/generate-constructor name behavior))))
