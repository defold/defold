(ns editor.graph-util
  (:require [dynamo.graph :as g]
            [internal.graph :as ig]
            [internal.graph.types :as gt]))

(set! *warn-on-reflection* true)

(defmacro passthrough [field]
  `(g/fnk [~field] ~field))

(defn array-subst-remove-errors [arr]
  (vec (remove g/error? arr)))

(defn outputs [node]
  (mapv #(do [(second (gt/head %)) (gt/tail %)]) (gt/arcs-by-head (g/now) node)))

(defn explicit-outputs [node]
    ;; don't include arcs from override-original nodes
  (mapv #(do [(second (gt/head %)) (gt/tail %)]) (ig/explicit-arcs-by-source (g/now) node)))
