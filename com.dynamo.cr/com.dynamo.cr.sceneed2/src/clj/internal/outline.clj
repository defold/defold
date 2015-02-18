(ns internal.outline
  (:require [dynamo.types :as t]
            [plumbing.core :refer [defnk]]))

(defnk outline-tree-producer :- t/OutlineItem
  [this outline-label outline-children :- [t/OutlineItem]]
  {:label outline-label
   ;:icon "my type of icon"
   :node-ref (t/node-ref this)
   :children outline-children})
