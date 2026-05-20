(ns hooks.editor-gl-pass
  (:require [clj-kondo.hooks-api :as api]))

(defn- render-pass-node [name-node selection-node model-transform-node depth-clipping-node]
  (api/list-node
    [(api/token-node 'RenderPass.)
     (api/token-node (str (api/sexpr name-node)))
     selection-node
     model-transform-node
     depth-clipping-node]))

(defn- pass-def-node [[name-node selection-node model-transform-node depth-clipping-node]]
  (api/list-node
    [(api/token-node 'def)
     name-node
     (render-pass-node name-node selection-node model-transform-node depth-clipping-node)]))

(defn- pass-vector-node [passes]
  (api/vector-node
    (mapv (comp api/token-node api/sexpr first) passes)))

(defn- pass-list-def-node [name passes]
  (api/list-node
    [(api/token-node 'def)
     (api/token-node name)
     (pass-vector-node passes)]))

(defn make-passes [{:keys [node]}]
  (let [[_ & body] (:children node)
        passes (partition 4 body)
        selection-passes (filter #(true? (api/sexpr (second %))) passes)
        render-passes (remove #(true? (api/sexpr (second %))) passes)]
    {:node
     (api/list-node
       (list*
         (api/token-node 'do)
         (concat
           (map pass-def-node passes)
           [(pass-list-def-node 'all-passes passes)
            (pass-list-def-node 'selection-passes selection-passes)
            (pass-list-def-node 'render-passes render-passes)])))}))
