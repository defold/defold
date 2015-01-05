(ns internal.outline
  (:require [dynamo.types :refer :all]
            [plumbing.core :refer [defnk]]))

(set! *warn-on-reflection* true)

;; Transform produces value
(defnk outline-tree-producer :- OutlineItem
  [this g children :- [OutlineItem]]
  {:label "my name" :icon "my type of icon" :node-ref this :children children})

