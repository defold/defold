(ns dynamo.outline
  (:require [dynamo.node :refer [defnode]]
            [dynamo.types :refer :all]
            [schema.core :as s]
            [plumbing.core :refer [defnk]]))

(set! *warn-on-reflection* true)

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
(defnode OutlineNode
  (input  children [OutlineItem])
  (output tree     [OutlineItem] outline-tree-producer))
