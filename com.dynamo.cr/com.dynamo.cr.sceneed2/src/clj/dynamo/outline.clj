(ns dynamo.outline
  (:require [dynamo.types :refer :all]
            [schema.core :as s]
            [plumbing.core :refer [defnk]]))

;; Value
(def OutlineItem
  {:label    s/Str
   :icon     Icon
   :node-ref NodeRef
   :children [(s/recursive #'OutlineItem)]})

;; Transform produces value
(defnk outline-tree-producer :- OutlineItem
  [this g children :- [OutlineItem]]
  {:label "my name" :icon "my type of icon" :node-ref this :children children})

;; Behavior
(def OutlineNode
  {:inputs  {:children (as-schema [(s/recursive #'OutlineNode)])}
   :transforms {:tree #'outline-tree-producer}})
