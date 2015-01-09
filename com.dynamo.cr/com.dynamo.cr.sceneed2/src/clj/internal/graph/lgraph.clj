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

(defn targets [g node label]
  (for [a     (dg/arcs-from g node)
        :when (= label (get-in a [:source-attributes :label]))]
    [(:target a) (get-in a [:target-attributes :label])]))

(defn sources [g node label]
  (for [a     (dg/arcs-to g node)
        :when (= label (get-in a [:target-attributes :label]))]
    [(:source a) (get-in a [:source-attributes :label])]))

(defn source-labels [g target-node label source-node]
  (map second (filter #(= source-node (first %)) (sources g target-node label))))

(defn connect
  [g source source-label target target-label]
  (let [from (dg/node g source)
        to   (dg/node g target)]
    (assert (not (nil? from)) (str "Attempt to connect " (pr-str source source-label target target-label)))
    (assert (not (nil? to)) (str "Attempt to connect " (pr-str source source-label target target-label)))
    (assert (some #{source-label} (outputs from)) (str "No label " source-label " exists on node " from))
    (assert (some #{target-label} (inputs to))    (str "No label " target-label " exists on node " to))
    (dg/add-arc g source {:label source-label} target {:label target-label})))

(defn disconnect
  [g source source-label target target-label]
  (dg/remove-arc g source {:label source-label} target {:label target-label}))

(defn connected?
  [g source source-label target target-label]
  (not
   (empty?
    (filter
     #(and (= source-label (-> % :source-attributes :label))
           (= target-label (-> % :target-attributes :label)))
     (dg/arcs-from-to g source target)))))
