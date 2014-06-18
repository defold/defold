(ns dynamo.node
  (:require [schema.core :as s]
            [schema.macros :as sm]
            [plumbing.core :refer [fnk defnk]]
            [internal.node :as in]))

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

(defmacro defnode [name & behaviors]
  (let [behavior (merge-behaviors behaviors)]
    `(let []
       ~(in/generate-type name behavior)
       ~@(mapcat in/input-mutators (keys (:inputs behavior)))
       ~(in/generate-constructor name behavior))))
