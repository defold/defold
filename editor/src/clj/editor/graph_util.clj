(ns editor.graph-util
  (:require [dynamo.graph :as g]
            [internal.system :as is]))

(set! *warn-on-reflection* true)

(defmacro passthrough [field]
  `(g/fnk [~field] ~field))

(defn prev-sequence-label [graph]
  (let [sys @g/*the-system*]
    (when-let [prev-step (some-> (is/graph-history sys graph)
                                 (is/undo-stack)
                                 (last))]
      (:sequence-label prev-step))))

(defn disconnect-all [basis node-id label]
  (for [[src-node-id src-label] (g/sources-of basis node-id label)]
    (g/disconnect src-node-id src-label node-id label)))
