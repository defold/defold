(ns editor.graph-util
  (:require [dynamo.graph :as g]
            [internal.graph :as ig]
            [internal.graph.types :as gt]))

(set! *warn-on-reflection* true)

(defmacro passthrough [field]
  `(g/fnk [~field] ~field))

(defn disconnect-all [basis node-id label]
  (for [[src-node-id src-label] (g/sources-of basis node-id label)]
    (g/disconnect src-node-id src-label node-id label)))

(defn array-subst-remove-errors [arr]
  (vec (remove g/error? arr)))

(defn explicit-outputs [node]
  ;; don't include arcs from override-original nodes
  (mapv (fn [[_ src-label tgt-id tgt-label]] [src-label [tgt-id tgt-label]]) (g/explicit-outputs node)))
