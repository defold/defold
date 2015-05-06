(ns internal.graph
  (:require [dynamo.util :refer [removev map-vals]]
            [internal.graph.types :as gt]
            [schema.core :as s]))

(deftype ArcBase [source target sourceLabel targetLabel]
  gt/Arc
  (head [_] [source sourceLabel])
  (tail [_] [target targetLabel])

  Object
  (toString [_]
    (str "[[" source sourceLabel "] -> [" target targetLabel "]]"))
  (equals [this that]
    (and (satisfies? gt/Arc that)
         (= source      (.source that))
         (= target      (.target that))
         (= sourceLabel (.sourceLabel that))
         (= targetLabel (.targetLabel that))))
  (hashCode [this]
    (+ (.hashCode source)      (mod (* 13 (.hashCode target)) 7)
       (.hashCode sourceLabel) (mod (* 19 (.hashCode targetLabel)) 23))))

(definline ^:private arc
  [source target source-label target-label]
  `(ArcBase. ~source ~target ~source-label ~target-label))

(defn empty-graph
  []
  {:nodes   {}
   :tarcs   []
   :tx-id   0})

(defn node-ids    [g] (keys (:nodes g)))
(defn node-values [g] (vals (:nodes g)))

(defn node        [g id]   (get-in g [:nodes id]))
(defn add-node    [g id n] (assoc-in g [:nodes id] n))

(defn remove-node
  [g n]
  (-> g
      (update :nodes dissoc n)
      (update :tarcs (fn [s] (removev #(or (= n (.source %))
                                           (= n (.target %))) s)))))

(defn transform-node
  [g n f & args]
  (if-let [node (get-in g [:nodes n])]
    (assoc-in g [:nodes n] (apply f node args))
    g))

(defn replace-node
  [g n r]
  (assoc-in g [:nodes n] r))

(defn arcs-from-source
  [g node label]
  (filter #(= [node label] (gt/head %)) (:tarcs g)))

(defn sources
  [g node label]
  (for [arc   (:tarcs g)
        :when (= [node label] (gt/tail arc))]
    (gt/head arc)))

(defn connect-target
  [g source source-label target target-label]
  (let [to (node g target)]
    (assert (not (nil? to)) (str "Attempt to connect " (pr-str source source-label target target-label)))
    (assert (some #{target-label} (gt/inputs to))    (str "No label " target-label " exists on node " to))
    (update g :tarcs conj (arc source target source-label target-label))))

(defn target-connected?
  [g source source-label target target-label]
  (some #{[source source-label]} (sources g target target-label)))

(defn disconnect-target
  [g source source-label target target-label]
  (update g :tarcs
          (fn [tarcs]
            (removev
             #(and (= source       (.source %))
                   (= target       (.target %))
                   (= source-label (.sourceLabel %))
                   (= target-label (.targetLabel %)))
             tarcs))))

(defn purge-arcs-from
  [g source]
  (update g :tarcs
          (fn [tarcs]
            (removev
             #(= source (.source %))
             tarcs))))

(defmacro for-graph
  [gsym bindings & body]
  (let [loop-vars (into [] (map first (partition 2 bindings)))
        rfn-args [gsym loop-vars]]
    `(reduce (fn ~rfn-args ~@body)
             ~gsym
             (for [~@bindings]
               [~@loop-vars]))))

;; ---------------------------------------------------------------------------
;; Dependency tracing
;; ---------------------------------------------------------------------------
(def maximum-graph-coloring-recursion 100)

(defn- pairs [m] (for [[k vs] m v vs] [k v]))

(defn- marked-outputs
  [basis target-node target-label]
  (get (gt/input-dependencies (gt/node-by-id basis target-node)) target-label))

(defn- marked-downstream-nodes
  [basis node-id output-label]
  (for [[target-node target-input] (gt/targets basis node-id output-label)
        affected-outputs           (marked-outputs basis target-node target-input)]
    [target-node affected-outputs]))

(definline node-id->graph  [gs node-id] `(get ~gs (gt/node-id->graph-id ~node-id)))

;; ---------------------------------------------------------------------------
;; Type checking
;; ---------------------------------------------------------------------------

(defn- check-single-type
  [out in]
  (or
   (= s/Any in)
   (= out in)
   (and (class? in) (class? out) (.isAssignableFrom ^Class in out))))

(defn type-compatible?
  [output-schema input-schema]
  (let [out-t-pl? (coll? output-schema)
        in-t-pl?  (coll? input-schema)]
    (or
     (= s/Any input-schema)
     (and out-t-pl? (= [s/Any] input-schema))
     (and (= out-t-pl? in-t-pl? true) (check-single-type (first output-schema) (first input-schema)))
     (and (= out-t-pl? in-t-pl? false) (check-single-type output-schema input-schema)))))

;; ---------------------------------------------------------------------------
;; Support for transactions
;; ---------------------------------------------------------------------------
(declare multigraph-basis)

(defn- assert-type-compatible
  [basis src-id src-label tgt-id tgt-label]
  (let [output-type   (gt/node-type (gt/node-by-id basis src-id))
        output-schema (gt/output-type output-type src-label)
        input-type    (gt/node-type (gt/node-by-id basis tgt-id))
        input-schema  (gt/input-type input-type tgt-label)]
    (assert (type-compatible? output-schema input-schema)
            (format "Cannot connect %s %s [%s] to %s %s [%s]. %s and %s are not compatible."
                    src-id src-label (:name output-type)
                    tgt-id tgt-label (:name input-type)
                    output-schema input-schema))))

(defrecord MultigraphBasis [graphs]
  gt/IBasis
  (node-by-id
    [_ node-id]
    (node (node-id->graph graphs node-id) node-id))

  (node-by-property
    [_ label value]
    (filter #(= value (get % label)) (mapcat vals graphs)))

  (sources
    [this node-id label]
    (filter #(gt/node-by-id this (first %))
            (sources (node-id->graph graphs node-id) node-id label)))

  (targets
    [this node-id label]
    (filter #(gt/node-by-id this (first %))
            (map gt/tail (mapcat #(arcs-from-source % node-id label) (vals graphs)))))

  (add-node
    [this node]
    (let [node-id (gt/node-id node)
          gid     (gt/node-id->graph-id node-id)
          graph   (add-node (get graphs gid) node-id node)]
      [(update this :graphs assoc gid graph) node]))

  (delete-node
    [this node-id]
    (let [node  (gt/node-by-id this node-id)
          gid   (gt/node-id->graph-id node-id)
          graph (remove-node (node-id->graph graphs node-id) node-id)]
      [(update this :graphs assoc gid graph) node]))

  (replace-node
    [this node-id new-node]
    (let [gid   (gt/node-id->graph-id node-id)
          graph (replace-node (node-id->graph graphs node-id) node-id (assoc new-node :_id node-id))
          node  (node graph node-id)]
      [(update this :graphs assoc gid graph) node]))

  (update-property
    [this node-id property f args]
    (let [gid   (gt/node-id->graph-id node-id)
          graph (apply transform-node (node-id->graph graphs node-id) node-id update-in [property] f args)
          node  (node graph node-id)]
      [(update this :graphs assoc gid graph) node]))

  (connect
    [this src-id src-label tgt-id tgt-label]
    (let [src-graph     (node-id->graph graphs src-id)
          tgt-gid       (gt/node-id->graph-id tgt-id)
          tgt-graph     (get graphs tgt-gid)]
      (assert (<= (:_volatility src-graph 0) (:_volatility tgt-graph 0)))
      (assert-type-compatible this src-id src-label tgt-id tgt-label)
      (update this :graphs assoc
              tgt-gid (connect-target tgt-graph src-id src-label tgt-id tgt-label))))

  (disconnect
    [this src-id src-label tgt-id tgt-label]
    (let [tgt-gid   (gt/node-id->graph-id tgt-id)
          tgt-graph (get graphs tgt-gid)]
      (update this :graphs assoc
              tgt-gid (disconnect-target tgt-graph src-id src-label tgt-id tgt-label))))

  (connected?
    [this src-id src-label tgt-id tgt-label]
    (let [tgt-graph (node-id->graph graphs tgt-id)]
      (target-connected? tgt-graph src-id src-label tgt-id tgt-label)))

  (dependencies
    [this to-be-marked]
    (loop [already-marked       #{}
           to-be-marked         to-be-marked
           iterations-remaining maximum-graph-coloring-recursion]
      (assert (< 0 iterations-remaining) "Output tracing stopped; probable cycle in the graph")
      (if (empty? to-be-marked)
        already-marked
        (let [next-wave (mapcat #(apply marked-downstream-nodes this %) to-be-marked)]
          (recur (into already-marked to-be-marked) next-wave (dec iterations-remaining)))))))

(defn multigraph-basis
  [graphs]
  (MultigraphBasis. graphs))
