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
         (let [^ArcBase that that ]
           (= source      (.source that))
           (= target      (.target that))
           (= sourceLabel (.sourceLabel that))
           (= targetLabel (.targetLabel that)))))
  (hashCode [this]
    (+ (.hashCode source)      (mod (* 13 (.hashCode target)) 7)
       (.hashCode sourceLabel) (mod (* 19 (.hashCode targetLabel)) 23))))

(definline ^:private arc
  [source target source-label target-label]
  `(ArcBase. ~source ~target ~source-label ~target-label))

(defn- conjv
  [coll x]
  (conj (or coll []) x))

(defn- rebuild-sarcs
  [basis graph-state]
  (def basis* basis)
  (let [gid      (:_gid graph-state)
        all-other-graphs (vals (assoc (:graphs basis) gid graph-state))
        all-arcs (flatten (mapcat (comp vals :tarcs) all-other-graphs))
        all-arcs-filtered (filter (fn [^ArcBase arc] (= (gt/node-id->graph-id (.source arc)) gid)) all-arcs)]
    (reduce
     (fn [sarcs arc] (update sarcs (.source arc) conjv arc))
     {}
     all-arcs-filtered)))

(defn empty-graph
  []
  {:nodes   {}
   :sarcs   {}
   :tarcs   {}
   :tx-id   0})

(defn node-ids    [g] (keys (:nodes g)))
(defn node-values [g] (vals (:nodes g)))

(defn node        [g id]   (get-in g [:nodes id]))
(defn add-node    [g id n] (assoc-in g [:nodes id] n))

(defn remove-node
  [g n]
  (-> g
      (update :nodes dissoc n)
      (update :sarcs dissoc n)
      (update :sarcs #(map-vals (fn [arcs] (removev (fn [^ArcBase arc] (= n (.target arc))) arcs)) %))
      (update :tarcs dissoc n)
      (update :tarcs #(map-vals (fn [arcs] (removev (fn [^ArcBase arc] (= n (.source arc))) arcs)) %))))

(defn transform-node
  [g n f & args]
  (if-let [node (get-in g [:nodes n])]
    (assoc-in g [:nodes n] (apply f node args))
    g))

(defn replace-node
  [g n r]
  (assoc-in g [:nodes n] r))

(defn sources
  [g node label]
  (map gt/head (filter #(= label (.targetLabel %)) (get-in g [:tarcs node]))))

(defn targets
  [g node label]
  (map gt/tail (filter #(= label (.sourceLabel %)) (get-in g [:sarcs node]))))

(defn connect-source
  [g source source-label target target-label]
  (let [from (node g source)]
    (assert (not (nil? from)) (str "Attempt to connect " (pr-str source source-label target target-label)))
    (update-in g [:sarcs source] conjv (arc source target source-label target-label))))

(defn connect-target
  [g source source-label target target-label]
  (let [to (node g target)]
    (assert (not (nil? to)) (str "Attempt to connect " (pr-str source source-label target target-label)))
    (assert (some #{target-label} (gt/inputs to))    (str "No label " target-label " exists on node " to))
    (update-in g [:tarcs target] conjv (arc source target source-label target-label))))

(defn source-connected?
  [g source source-label target target-label]
  (not
   (empty?
    (filter (fn [^ArcBase arc]
              (and (= source-label (.sourceLabel arc))
                   (= target-label (.targetLabel arc))
                   (= target       (.target arc))))
            (get-in g [:sarcs source])))))

(defn disconnect-source
  [g source source-label target target-label]
  (update-in g [:sarcs source]
          (fn [arcs]
            (removev
             (fn [^ArcBase arc]
               (and (= source       (.source arc))
                    (= target       (.target arc))
                    (= source-label (.sourceLabel arc))
                    (= target-label (.targetLabel arc))))
             arcs))))

(defn disconnect-target
  [g source source-label target target-label]
  (update-in g [:tarcs target]
          (fn [arcs]
            (removev
             (fn [^ArcBase arc]
               (and (= source       (.source arc))
                    (= target       (.target arc))
                    (= source-label (.sourceLabel arc))
                    (= target-label (.targetLabel arc))))
             arcs))))

(defmacro for-graph
  [gsym bindings & body]
  (let [loop-vars (into [] (map first (partition 2 bindings)))
        rfn-args [gsym loop-vars]]
    `(reduce (fn ~rfn-args ~@body)
             ~gsym
             (for [~@bindings]
               [~@loop-vars]))))

(definline node-id->graph  [gs node-id] `(get ~gs (gt/node-id->graph-id ~node-id)))

(defn node-by-id-at
  [basis node-id]
  (node (node-id->graph (:graphs basis) node-id) node-id))

;; ---------------------------------------------------------------------------
;; Dependency tracing
;; ---------------------------------------------------------------------------
(def maximum-graph-coloring-recursion 100)

(defn- pairs [m] (for [[k vs] m v vs] [k v]))

(defn- marked-outputs
  [basis target-node target-label]
  (get (gt/input-dependencies (node-by-id-at basis target-node)) target-label))

(defn- marked-downstream-nodes
  [basis node-id output-label]
  (for [[target-node target-input] (gt/targets basis node-id output-label)
        affected-outputs           (marked-outputs basis target-node target-input)]
    [target-node affected-outputs]))

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
  (let [output-type   (gt/node-type (node-by-id-at basis src-id))
        output-schema (gt/output-type output-type src-label)
        input-type    (gt/node-type (node-by-id-at basis tgt-id))
        input-schema  (gt/input-type input-type tgt-label)]
    (assert output-schema (format "Attempting to connect %s (a %s) %s to %s (a %s) %s, but %s does not have an output, input, or property named %s"
                                  src-id (:name output-type) src-label
                                  tgt-id (:name input-type) tgt-label
                                  (:name output-type) src-label))
    (assert input-schema  (format "Attempting to connect %s (a %s) %s to %s (a %s) %s, but %s does not have an input named %s"
                                  src-id (:name output-type) src-label
                                  tgt-id (:name input-type) tgt-label
                                  (:name input-type) tgt-label))
    (assert (type-compatible? output-schema input-schema)
            (format "Attempting to connect %s (a %s) %s to %s (a %s) %s, but %s and %s do not have compatible types."
                    src-id (:name output-type) src-label
                    tgt-id (:name input-type) tgt-label
                    output-schema input-schema))))

(defrecord MultigraphBasis [graphs]
  gt/IBasis
  (node-by-property
    [_ label value]
    (filter #(= value (get % label)) (mapcat vals graphs)))

  (sources
    [this node-id label]
    (filter #(node-by-id-at this (first %))
            (sources (node-id->graph graphs node-id) node-id label)))

  (targets
    [this node-id label]
    (filter #(node-by-id-at this (first %))
            (targets (node-id->graph graphs node-id) node-id label)))

  (add-node
    [this node]
    (let [node-id (gt/node-id node)
          gid     (gt/node-id->graph-id node-id)
          graph   (add-node (get graphs gid) node-id node)]
      [(update this :graphs assoc gid graph) node]))

  (delete-node
    [this node-id]
    (let [node  (node-by-id-at this node-id)
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
    (let [src-gid       (gt/node-id->graph-id src-id)
          src-graph     (get graphs src-gid)
          tgt-gid       (gt/node-id->graph-id tgt-id)
          tgt-graph     (get graphs tgt-gid)]
      (assert (<= (:_volatility src-graph 0) (:_volatility tgt-graph 0)))
      (assert-type-compatible this src-id src-label tgt-id tgt-label)
      (if (= src-gid tgt-gid)
        (update this :graphs assoc
                src-gid (-> src-graph
                            (connect-target src-id src-label tgt-id tgt-label)
                            (connect-source src-id src-label tgt-id tgt-label)))
        (update this :graphs assoc
                src-gid (connect-source src-graph src-id src-label tgt-id tgt-label)
                tgt-gid (connect-target tgt-graph src-id src-label tgt-id tgt-label)))))

  (disconnect
    [this src-id src-label tgt-id tgt-label]
    (let [src-gid   (gt/node-id->graph-id src-id)
          src-graph (get graphs src-gid)
          tgt-gid   (gt/node-id->graph-id tgt-id)
          tgt-graph (get graphs tgt-gid)]
      (if (= src-gid tgt-gid)
        (update this :graphs assoc
                src-gid (-> src-graph
                            (disconnect-source src-id src-label tgt-id tgt-label)
                            (disconnect-target src-id src-label tgt-id tgt-label)))
        (update this :graphs assoc
                src-gid (disconnect-source src-graph src-id src-label tgt-id tgt-label)
                tgt-gid (disconnect-target tgt-graph src-id src-label tgt-id tgt-label)))))

  (connected?
    [this src-id src-label tgt-id tgt-label]
    (let [src-graph (node-id->graph graphs src-id)]
      (source-connected? src-graph src-id src-label tgt-id tgt-label)))

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

(defn hydrate-after-undo
  [graphs graph-state]
  (assoc graph-state :sarcs (rebuild-sarcs (multigraph-basis (map-vals deref graphs)) graph-state)))
