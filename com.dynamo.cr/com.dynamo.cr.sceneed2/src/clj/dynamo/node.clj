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

(defn list-of [x] [x])
(defn map-of  [kt vt] {kt vt})
(defn number  [& {:as opts}] {:schema s/Num})
(defn error-message [m] {:error-message m})
(defn string [] {:schema s/Str :default ""})
(defn icon []   {:schema Icon})
(defn non-negative-integer [] (assoc (number) :default 0 :such-that #(<= 0 %) :or-else (error-message "Value must be greater than or equal to zero")))
(defn isotropic-scale []  (assoc (number) :default 1.0 :such-that #(<= 0 %) :or-else (error-message "Value must be greater than zero")))

(sm/defrecord Quaternion [a :- s/Num, b :- s/Num, c :- s/Num, d :- s/Num])
(sm/defrecord Vector3 [x :- s/Num, y :- s/Num, z :- s/Num])

(def IsotropicallyScalable
  {:properties {:isotropic-scale (isotropic-scale)}})

(def Rotatable
  {:properties {:rotation {:schema Quaternion}}})

(def Translatable
  {:properties {:translation {:schema Vector3}}})

(defnk outline-tree-producer :- OutlineItem
  [this g children :- [OutlineItem]]
  {:label "my name" :icon "my type of icon" :node-ref (:id this) :children children})

(def OutlineParent
  {:inputs  {}
   :transforms {:tree #'outline-tree-producer}})

(defn resource [] {})

(def Image
  {:properties {:resource (resource)}})

(defn image-contents [])

(def Persistent
  {:properties {:resource (resource)}
   :transforms {:contents #'image-contents}})

(defn animation-frames [])

(def Animation 
  {:properties {:fps (assoc (non-negative-integer) :default 30)}
   :transform  {:frames #'animation-frames}})

(defn outline-child-producer [this g]
  (assoc this :node-ref (:id this))
  )

(def OutlineChild 
  {:properties {:label (assoc (string) :default "my name") 
                :icon (assoc (icon) :default "pretty stuff")}
   :transforms {:child #'outline-child-producer}})

;; helpers

(declare get-value)

(defn get-inputs [target-node g target-label]
  (map (fn [[source-node source-label]] 
         (get-value g source-node source-label)) 
       (lg/sources g target-node target-label)))

(defn collect-inputs [node g input-schema]
  (reduce-kv 
    (fn [m k v ] 
      (case k
        :g         (assoc m k g)
        :this      (assoc m k node)
        s/Keyword  m
        (assoc m k (get-inputs node g k)))) 
    {} input-schema))

(defn has-schema? [v]
  (and (fn? v) (satisfies? pf/PFnk v)))

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
