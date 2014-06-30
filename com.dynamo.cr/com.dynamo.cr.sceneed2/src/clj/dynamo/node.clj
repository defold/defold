(ns dynamo.node
  (:require [schema.core :as s]
            [schema.macros :as sm]
            [plumbing.core :refer [fnk defnk]]
            [plumbing.fnk.pfnk :as pf]
            [internal.node :as in]
            [internal.graph.dgraph :as dg]
            [internal.graph.lgraph :as lg]))

(def Icon s/Str)
(def NodeRef s/Int)

(def OutlineItem
  {:label    s/Str
   :icon     Icon
   :node-ref NodeRef
   :children [(s/recursive #'OutlineItem)]})

(defnk outline-tree-producer :- OutlineItem
  [this g children :- [OutlineItem]]
  {:label "my name" :icon "my type of icon" :node-ref this :children children})

(def OutlineNode
  {:inputs  {:children [(s/recursive #'OutlineNode)]}
   :transforms {:tree #'outline-tree-producer}})

(defn error-message [m] {:error-message m})

(defn number               [& {:as opts}] (merge {:schema s/Num} opts))
(defn string               [& {:as opts}] (merge {:schema s/Str :default ""} opts))
(defn icon                 [& {:as opts}] (merge {:schema Icon} opts))
(defn non-negative-integer [& {:as opts}] (merge (number :default 0)   opts ))
(defn isotropic-scale      [& {:as opts}] (merge (number :default 1.0) opts))

(sm/defrecord Quaternion [a :- s/Num, b :- s/Num, c :- s/Num, d :- s/Num])
(sm/defrecord Vector3 [x :- s/Num, y :- s/Num, z :- s/Num])

(def IsotropicallyScalable
  {:properties {:isotropic-scale (isotropic-scale)}})

(def Rotatable
  {:properties {:rotation {:schema Quaternion}}})

(def Translatable
  {:properties {:translation {:schema Vector3}}})


(defn resource [] {})

(def Image
  {:properties {:resource (resource)}})

(defn image-contents [])

(def Persistent
  {:properties {:resource (resource)}
   :transforms {:contents #'image-contents}})

(defn animation-frames [])

(def Animation
  {:properties {:fps (non-negative-integer :default 30)}
   :transforms {:frames #'animation-frames}})
;; helpers

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

(defn has-schema? [v]
  (and (fn? v) (:schema (meta v))))

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
