(ns internal.graph.lgraph
  "Labeled graph. Nodes have sets of input and output labels. 
   Arcs must match those labels when connecting. An edge must
   run from an output label to an input label."
  (:require [internal.graph.dgraph :as dg]))

(defn add-labeled-node
  [g inputs outputs ^clojure.lang.Associative v]
  (dg/add-node g (assoc v ::inputs inputs ::outputs outputs)))

(defn inputs [node]  (::inputs node))
(defn outputs [node] (::outputs node))

(defn connect 
  [g source source-label target target-label]
  (let [from (dg/node g source)
        to   (dg/node g target)]
    (assert (some? #{source-label} (outputs from)))
    (assert (some? #{target-label} (inputs to)))
    (dg/add-arc g source {:label source-label} target {:label target-label})))
