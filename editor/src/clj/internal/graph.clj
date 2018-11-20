(ns internal.graph
  (:require [clojure.set :as set]
            [clojure.core.reducers :as r]
            [clojure.data :as data]
            [internal.graph.types :as gt]
            [schema.core :as s]
            [internal.util :as util]
            [internal.node :as in])
  (:import [internal.graph.types Arc]
           [clojure.lang IPersistentSet]))

;; A brief braindump on Overrides.
;;
;; Overrides is how we implement instancing or "templates" in the
;; editor. Currently it is used in collections for sub collections and
;; referenced game objects, in game objects for referenced components,
;; and in GUIs for templates (sub-GUIs) and layouts.
;;
;; What we want in "graphy" terms is a node that looks like some original
;; node - the thing being instantiated - except that we can change some
;; or all of its properties, or connect something else to its inputs.
;;
;; In most cases, the thing being instantiated is not implemented as a
;; single node, but rather one conceptual root node (often a resource
;; node) and a cluster of private helper nodes owned via :cascade-delete
;; inputs.
;;
;; An _override_ is a set of override nodes, each derived from an
;; original node. The set of nodes being _overridden_ is defined by a
;; traversal predicate and an original root node. When creating an
;; override, we start from the root node and recursively traverse the
;; :cascade-delete inputs "backwards" provided the arcs match the
;; traversal predicate, and for each node we reach - create a
;; corresponding override node. The purpose of the traversal predicate is
;; to make sure we cover the "hidden" private nodes - as far as
;; necessary.
;;
;; Once an _override_ has been created, the graph / transaction system
;; makes sure that any structural change to the original nodes is
;; reflected in the override. If we connect something to a
;; :cascade-delete input of a node that has been overridden, the
;; traversal will restart from that node and we might create a new
;; override node for the newly connected node. When disconnecting, any
;; corresponding override node will be deleted. Deleting the original
;; root node will delete the whole override.
;;
;; For override nodes, special rules apply for evaluation of properties
;; and finding input- and output arcs.
;;
;; The properties of override nodes report the same value as the
;; corresponding original, unless we explicitly set it to something else.
;; Once set, the new value will be reported. We can clear the property to
;; "revert" to the original behaviour - report the value of the original.
;;
;; For arcs, here are some reasonable expectations:
;;
;; * Creating an override should not affect the observable behaviour of
;; the original nodes. Any `node-value` should report the same value
;; before and after. There is an assumption here that the behaviour of
;; nodes never depend on what other nodes their outputs are connected to.
;;
;; * Just after creating an override, the override nodes should report
;; roughly the same property values (and output results) as their
;; corresponding original nodes - except for any node-id references and
;; the _properties output which f.e. contains information about the
;; original values.
;;
;; * Similar to how the property values work, we want to be able to connect
;; something else as input to an override node, and also revert to the
;; original input.
;;
;; For this to work, we introduced the idea of explicit and implicit
;; arcs.
;;
;; Any arc established with `connect` between override- or normal nodes
;; is an _explicit_ arc. Explicit arcs can also be `disconnect`'ed. If
;; you `connect` something else to a non-:array input, the old _explicit_
;; arc to that input is silently disconnected (deleted).
;;
;; An _implicit_ arc is derived from an _explicit_ or _implicit_ arc and
;; is only relevant for override nodes. You cannot `disconnect` an
;; _implicit_ arc. However, if you `connect` something else to an input
;; that used to have one or more _implicit_ arcs connected, the new
;; _explicit_ arc will shadow the _implicit_ arcs. For :array inputs,
;; connecting a single _explicit_ arc will shadow all _implicit_
;; arcs - you cannot selectively replace one of the incoming arcs. Later
;; `disconnect`ing the _explicit_ arc will effectively revert to the
;; _implicit_ arcs.
;;
;; The `node-value` mechanism does not differentiate between _explicit_
;; and _implicit_ arcs. When the value of an input is needed, we look for
;; all arcs (zero or one if non-:array input) connected to the input and
;; get the value(s) from the corresponding source node and output.
;;
;; In some cases one wants to find only the _explicit_ arcs, and for that
;; we have `explicit-outputs` and `-inputs`.
;;
;; How implicit arcs appear is described in examples below.
;;
;;
;; Example: Nodes O and I both included in an override. There may be
;; other nodes but we focus on O and I.
;;
;; (/override1 after a node means that node is an override node in the
;; override with id "override1". In practice overrides have numerical ids)
;;
;; Say we have two nodes O and I with an explicit arc between them:
;;
;;    O:output-label ---> I:input-label
;;
;; If we create an override including both nodes, we get this situation:
;;
;;    O/override1:output-label ~~~> I/override1:input-label
;;    |                             |
;;    V                             V
;;    O:output-label -------------> I:input-label
;;
;; Here, O is the original node of O/override1 and vice versa for I. The arc
;; between O and I is still _explicit_.
;;
;; Between O/override1 and I/override1 there is now an _implicit_
;; arc. This makes sense, because if we change a property of
;; O/override1 that value could affect I/override1. If we ask
;; O/override1 for its outgoing arcs from output-label, we should get
;; at least an arc to I/override1:input-label - and vice verse for
;; incoming arcs to I/override1:input-label. The outgoing arcs from
;; O:output-label however, should _not_ include
;; I/override1:input-label. Incoming arcs to I:input-label also
;; does not include O/override1:output-label.
;;
;;
;; Example: Nodes O and I, only I included in an override.
;;
;; Say only I is included in the override (we skip the override arrows):
;;
;;                     ~> I/override1:input-label
;;                    ~
;;                   ~
;;    O:output-label ---> I:input-label
;;
;; Here, an override was created but we were not really interested in
;; changing anything about O. In that case, it makes sense that
;; I/override1 "still" gets its input from O. Outgoing arcs from
;; O:output-label now include both I:input-label and I/override1:input-label,
;; but this should not affect the behaviour of O since it's an
;; output. Incoming arcs to I/override1:input-label is O:output-label.
;;
;;
;;
;; Example: Nodes O and I, only O included in an override.
;;
;;    O/override1:output-label
;;
;;
;;    O:output-label ---> I:input-label
;;
;; Here, since I was not included in the override, the most reasonable
;; option is to simply drop the arc. An _implicit_ connection between O/override1
;; and I could affect the behaviour of I - not what we want.
;;
;;
;;
;; Nodes can take part in several overrides, that is, have several
;; override nodes:
;;
;;     O/override1    O/override2   O/override3
;;     |              |             |
;;      \             V            /
;;       -----------> O <----------
;;
;; Also, we can create an override with an override node as root:
;;
;;                    O/override2
;;                    |
;;                    V
;;                    O/override1
;;                    |
;;                    V
;;                    O
;;
;; To complicate matters, there is no rule that an override may only
;; cover nodes "at the same level" - from the same override (or actual
;; real nodes).
;;
;;
;;                    O/template                I/template
;;                    |                         |
;;                    V                         |
;;                    O/landscape               |
;;                    |                         |
;;                    V                         V
;;                    O                         I
;;
;; Here, O/template and I/template are part of the same override
;; O/template also has an intermediate O/landscape override node along
;; its chain to the real node O.
;;
;;
;; This together also means that several override nodes in an override
;; can have a common original node along the chain towards the real node.
;;
;;
;;     O/override7    O/override7   O/override7
;;     |              |             |
;;     O/override5    |             |
;;     |              |             |
;;     O/override4    O/override6   |
;;     |              |             |
;;     O/override1    O/override2   O/override3
;;     |              |             |
;;      \             V            /
;;       -----------> O <----------
;;
;; To be precise, it's not enough to annotate the nodes with /"override
;; name" - the top nodes above are different nodes, and we should include
;; the whole override chain back to the real node.
;;
;;     O/override7,5,4,1  O/override7,6,2   O/override7,3
;;     |                  |                 |
;;    ...                ...               ...
;;
;;
;; These facts very much complicate the definition of _implicit_ arcs.
;;
;;
;;
;; Example: Implicit arcs where source override chain differs.
;;
;;    O/template,landscape    -> I/template
;;                           ~
;;                         ~
;;    O/landscape        ~
;;                     ~
;;                   ~
;;    O:output-label ---------> I:input-label
;;
;; Now, what produces input to I/template:input-label. Simple?
;; O:output-label. When we created the "landscape" override, we did not
;; include I. O/landscape does not affect I, and even though
;; O/template,landscape leads to O along the override chain - it's in no
;; way related to I/template. So there is an _implicit_ arc between
;; O:output-label and I/template:input-label.
;;
;;
;; Example: Implicit arcs where source override chain differs.
;;
;;    O/template2,template,landscape      I/template2,landscape
;;                                     ~>
;;                                  ~
;;    O/template,landscape      ~
;;                          ~
;;                      ~
;;    O/landscape    ~~~~~~~~~~~~~~~~~~~> I/landscape
;;
;;
;;    O:output-label -------------------> I:input-label
;;
;; This time, since I was included in the landscape override, there is an
;; _implicit_ arc between O/landscape:output-label and
;; I/landscape:input-label. This _implicit_ arc is still relevant for
;; I/template2, so there is also an _implicit_ arc between
;; O/landscape:output-label and I/template2,landscape:input-label.
;;
;;
;; Example: Implicit arcs where target override chain has intermediate overrides.
;;
;;    O/template2,template,landscape      I/template,landscape2,landscape1
;;                                   ~~~>
;;                               ~~~
;;    O/template,landscape2   ~~~
;;                         ~~~
;;                      ~~~
;;    O/landscape2  ~~~~~~~~~~~~~~~~~~~~>  I/landscape2,landscape1
;;
;;
;;                                  ~~~~> I/landscape1
;;                             ~~~~~
;;                       ~~~~~
;;    O:output-label -------------------> I:input-label
;;
;;
;; Here, as before there is an implicit arc O:output-label to
;; I/landscape1:input-label. What about I/landscape2,landscape1? Since
;; both O and I/landscape1 are included in the landscape2 override, the
;; implicit arc between them is still relevant for their override
;; nodes. Thus there is an implicit arc O/landscape2:output-label to
;; I/landscape2,landscape1:input-label. Further up, there is also an
;; implicit arc O/landscape2:output-label to
;; I/template,landscape2,landscape1:input-label.
;;
;;
;; Example: Implicit arcs where target override chain has intermediate overrides.
;;
;;    O/template,landscape2            ~> I/template,landscape2,square,landscape1
;;                                    ~
;;                                    ~
;;    O/landscape2                   ~ ~> I/landscape2,square,landscape1
;;                                   ~~
;;                                  ~
;;                        O/square ~~~~~> I/square,landscape1
;;
;;
;;                                 ~~~~~> I/landscape1
;;                            ~~~~~
;;                      ~~~~~~
;;    O:output-label -------------------> I:input-label
;;
;;
;; Here, similar to before, there is an implicit arc
;; O/square:output-label to I/square,landscape1:input-label. And
;; O/square remains the source for the implicit arcs to
;; I/landscape2,square,landscape1 and
;; I/template,landscape2,square,landscape1. Why isn't O/landscape2 etc
;; involved?
;; If we recreate this situation it becomes more clear:
;;
;; We start out with O and I, and an explicit arc between them.
;;
;; We create the landscape1 override, including only I. Since O was
;; not included, there will be an implicit arc between O and
;; I/landscape1.
;;
;; We create the square override, including both O and
;; I/landscape1. Since there is an implicit arc between O and
;; I/landscape1, there will be an implicit arc between their override
;; nodes O/square and I/square,landscape1.
;;
;; We create the landscape2 override, including the original O and
;; I/square,landscape1. Now, there is no arc between O and
;; I/square,landscape1. Thus, there is also no arc between
;; O/landscape2 and I/landscape2,square,landscape1. Instead
;; I/landscape2,square,landscape1 "still" has an implicit arc from
;; O/square.
;;
;; We create the template override, including O/landscape2 and
;; I/landscape2,square,landscape1. I/template,landscape2,square,landscape1
;; "still" has an implicit arc from O/square.
;;
;; The common rule in these examples is that for the implicit arcs to a
;; particular target node, the source node override chain should be the
;; longest and first possible subsequence of the target node override
;; chain.

(set! *warn-on-reflection* true)

(definline ^:private arc
  [source-id source-label target-id target-label]
  `(Arc. ~source-id ~source-label ~target-id ~target-label))

(defn arcs->tuples [arcs]
  (mapv (fn [^Arc arc] [(.source-id arc) (.source-label arc) (.target-id arc) (.target-label arc)]) arcs))

(defn arc-endpoints-p [p arc]
  (and (p (.source-id ^Arc arc))
       (p (.target-id ^Arc arc))))

(defn- rebuild-sarcs
  [basis graph-state]
  (let [graph-id (:_graph-id graph-state)
        all-graphs (vals (assoc (:graphs basis) graph-id graph-state))
        all-arcs (mapcat (fn [graph] (mapcat (fn [[_ label-tarcs]] (mapcat second label-tarcs))
                                             (:tarcs graph)))
                         all-graphs)
        all-arcs-filtered (filter (fn [^Arc arc] (= (gt/node-id->graph-id (.source-id arc)) graph-id)) all-arcs)]
    (reduce
      (fn [sarcs arc] (update-in sarcs [(.source-id ^Arc arc) (.source-label ^Arc arc)] util/conjv arc))
      {}
      all-arcs-filtered)))

(defn empty-graph
  []
  {:nodes {}
   :sarcs {}
   :successors {}
   :tarcs {}
   :tx-id 0})

(defn node-ids [graph] (keys (:nodes graph)))
(defn node-values [graph] (vals (:nodes graph)))

(defn add-node [graph node-id node] (assoc-in graph [:nodes node-id] node))

(definline node-id->graph [basis node-id] `(get (:graphs ~basis) (gt/node-id->graph-id ~node-id)))
(definline node-id->node [graph node-id] `(get (:nodes ~graph) ~node-id))

(defn- overrides [graph node-id]
  (get (:node->overrides graph) node-id))

;; This function only removes the node from the single graph in which it exists
;; It should only be used for testing purposes
(defn graph-remove-node
  ([graph node-id]
   (graph-remove-node graph node-id (some-> (get-in graph [:nodes node-id]) gt/original)))
  ([graph node-id original-id]
   (graph-remove-node graph node-id original-id false))
  ([graph node-id original-id original-deleted?]
   (if (contains? (:nodes graph) node-id)
     (let [graph (reduce (fn [graph override-node-id] (graph-remove-node graph override-node-id node-id true)) graph (overrides graph node-id))
           sarcs (mapcat second (get-in graph [:sarcs node-id]))
           tarcs (mapcat second (get-in graph [:tarcs node-id]))
           override-id (when original-id (gt/override-id (get-in graph [:nodes node-id])))
           override (when original-id (get-in graph [:overrides override-id]))]
       (-> graph
           (cond->
               (not original-deleted?) (update-in [:node->overrides original-id] (partial util/removev #{node-id}))
               (and override (= original-id (:root-id override))) (update :overrides dissoc override-id))
           (update :nodes dissoc node-id)
           (update :node->overrides dissoc node-id)
           (update :sarcs dissoc node-id)
           (update :sarcs (fn [s] (reduce (fn [s ^Arc arc]
                                            (update-in s [(.source-id arc) (.source-label arc)]
                                                       (fn [arcs] (util/removev (fn [^Arc arc] (= node-id (.target-id arc))) arcs))))
                                          s tarcs)))
           (update :tarcs dissoc node-id)
           (update :tarcs (fn [s] (reduce (fn [s ^Arc arc]
                                            (update-in s [(.target-id arc) (.target-label arc)]
                                                       (fn [arcs] (util/removev (fn [^Arc arc] (= node-id (.source-id arc))) arcs))))
                                          s sarcs)))))
     graph)))

(defn- arc-cross-graph? [^Arc arc]
  (not= (gt/node-id->graph-id (.source-id arc)) (gt/node-id->graph-id (.target-id arc))))

(defn- arcs-remove-node [arcs node-id]
  (util/removev (fn [^Arc arc] (or (= node-id (.source-id arc))
                                   (= node-id (.target-id arc)))) arcs))

(defn basis-remove-node
  ([basis node-id]
   (basis-remove-node basis node-id (some-> (get-in basis [:graphs (gt/node-id->graph-id node-id) :nodes node-id]) gt/original)))
  ([basis node-id original-id]
   (basis-remove-node basis node-id original-id false))
  ([basis node-id original-id original-deleted?]
   (let [graph (node-id->graph basis node-id)
         basis (reduce (fn [basis override-node-id] (basis-remove-node basis override-node-id node-id true)) basis (overrides graph node-id))
         sarcs (mapcat second (get-in graph [:sarcs node-id]))
         tarcs (mapcat second (get-in graph [:tarcs node-id]))
         override-id (when original-id (gt/override-id (get-in graph [:nodes node-id])))
         override (when original-id (get-in graph [:overrides override-id]))
         ext-sarcs (filterv arc-cross-graph? sarcs)
         ext-tarcs (filterv arc-cross-graph? tarcs)
         basis (-> basis
                   (update-in [:graphs (gt/node-id->graph-id node-id)]
                              (fn [graph]
                                (-> graph
                                    (cond->
                                        (not original-deleted?) (update-in [:node->overrides original-id] (partial util/removev #{node-id}))
                                        (and override (= original-id (:root-id override))) (update :overrides dissoc override-id))
                                    (update :nodes dissoc node-id)
                                    (update :node->overrides dissoc node-id)
                                    (update :sarcs dissoc node-id)
                                    (update :sarcs (fn [s] (reduce (fn [s ^Arc arc]
                                                                     (update-in s [(.source-id arc) (.source-label arc)] arcs-remove-node node-id))
                                                                   s tarcs)))
                                    (update :tarcs dissoc node-id)
                                    (update :tarcs (fn [s] (reduce (fn [s ^Arc arc]
                                                                     (update-in s [(.target-id arc) (.target-label arc)] arcs-remove-node node-id))
                                                                   s sarcs)))))))
         basis (reduce (fn [basis ^Arc arc]
                         (let [target-id (.target-id arc)]
                           (update-in basis [:graphs (gt/node-id->graph-id target-id) :tarcs target-id (.target-label arc)] arcs-remove-node node-id)))
                       basis ext-sarcs)
         basis (reduce (fn [basis ^Arc arc]
                         (let [source-id (.source-id arc)]
                           (update-in basis [:graphs (gt/node-id->graph-id source-id) :tarcs source-id (.source-label arc)] arcs-remove-node node-id)))
                       basis ext-tarcs)]
     basis)))

(defn transform-node
  [graph node-id f & args]
  (if-let [node (get-in graph [:nodes node-id])]
    (assoc-in graph [:nodes node-id] (apply f node args))
    graph))

(defn connect-source
  [graph source-id source-label target-id target-label]
  (let [from (node-id->node graph source-id)]
    (assert (not (nil? from)) (str "Attempt to connect " (pr-str source-id source-label target-id target-label)))
    (update-in graph [:sarcs source-id source-label] util/conjv (arc source-id source-label target-id target-label))))

(defn connect-target
  [graph source-id source-label target-id target-label]
  (let [to (node-id->node graph target-id)]
    (assert (not (nil? to)) (str "Attempt to connect " (pr-str source-id source-label target-id target-label)))
    (update-in graph [:tarcs target-id target-label] util/conjv (arc source-id source-label target-id target-label))))

(defn disconnect-source
  [graph source-id source-label target-id target-label]
  (cond-> graph
    (node-id->node graph source-id)
    (update-in [:sarcs source-id source-label]
               (fn [arcs]
                 (util/removev
                   (fn [^Arc arc]
                     (and (= source-id (.source-id arc))
                          (= target-id (.target-id arc))
                          (= source-label (.source-label arc))
                          (= target-label (.target-label arc))))
                   arcs)))))

(defn disconnect-target
  [graph source-id source-label target-id target-label]
  (cond-> graph
    (node-id->node graph target-id)
    (update-in [:tarcs target-id target-label]
               (fn [arcs]
                 (util/removev
                   (fn [^Arc arc]
                     (and (= source-id (.source-id arc))
                          (= target-id (.target-id arc))
                          (= source-label (.source-label arc))
                          (= target-label (.target-label arc))))
                   arcs)))))

(defn override-by-id
  [basis override-id]
  (get-in basis [:graphs (gt/override-id->graph-id override-id) :overrides override-id]))


;; ---------------------------------------------------------------------------
;; Dependency tracing
;; ---------------------------------------------------------------------------

(defn pre-traverse
  "Traverses a graph depth-first preorder from start, successors being
  a function that returns direct successors for the node. Returns a
  lazy seq of nodes."
  [basis start succ & {:keys [seen] :or {seen #{}}}]
  (loop [stack start
         next []
         seen seen
         result (transient [])]
    (if-let [nxt (first stack)]
      (if (contains? seen nxt)
        (recur (rest stack) next seen result)
        (let [seen (conj seen nxt)]
          (recur (succ basis nxt) (conj next (rest stack))
                 seen (conj! result nxt))))
      (if-let [next-stack (peek next)]
        (recur next-stack (pop next) seen result)
        (persistent! result)))))

(defn get-overrides [basis node-id]
  (overrides (node-id->graph basis node-id) node-id))

(defn override-original [basis node-id]
  (when-let [node (gt/node-by-id-at basis node-id)]
    (gt/original node)))

(defn override-originals [basis node-id]
  (into '() (take-while some? (iterate (partial override-original basis) node-id))))

(defn- override-of [graph node-id override-id]
  (some #(and (= override-id (gt/override-id (node-id->node graph %))) %) (overrides graph node-id)))

(defn- node-id->arcs [graph node-id arc-kw]
  (into [] cat (vals (-> graph (get arc-kw) (get node-id)))))

;; This should really be made interface methods of IBasis

(defn- graph-explicit-arcs-by-source
  ([graph source-id]
   (node-id->arcs graph source-id :sarcs))
  ([graph source-id source-label]
   (-> (:sarcs graph)
       (get source-id)
       (get source-label))))

(defn explicit-arcs-by-source
  ([basis source-id]
   (graph-explicit-arcs-by-source (node-id->graph basis source-id) source-id))
  ([basis source-id source-label]
   (graph-explicit-arcs-by-source (node-id->graph basis source-id) source-id source-label)))

(defn- graph-explicit-arcs-by-target
  ([graph target-id]
   (node-id->arcs graph target-id :tarcs))
  ([graph target-id target-label]
   (-> (:tarcs graph)
       (get target-id)
       (get target-label))))

(defn explicit-arcs-by-target
  ([basis target-id]
   (graph-explicit-arcs-by-target (node-id->graph basis target-id) target-id))
  ([basis target-id target-label]
   (graph-explicit-arcs-by-target (node-id->graph basis target-id) target-id target-label)))

(defn explicit-inputs
  ([basis node-id]
   (graph-explicit-arcs-by-target (node-id->graph basis node-id) node-id))
  ([basis node-id label]
   (graph-explicit-arcs-by-target (node-id->graph basis node-id) node-id label)))

(defn explicit-outputs
  ([basis node-id]
   (graph-explicit-arcs-by-source (node-id->graph basis node-id) node-id))
  ([basis node-id label]
   (graph-explicit-arcs-by-source (node-id->graph basis node-id) node-id label)))

(defn explicit-sources
  ([basis target-id]
   (mapv gt/source (explicit-inputs basis target-id)))
  ([basis target-id target-label]
   (mapv gt/source (explicit-inputs basis target-id target-label))))

(defn explicit-targets
  ([basis source-id]
   (mapv gt/target (explicit-outputs basis source-id)))
  ([basis source-id target-label]
   (mapv gt/target (explicit-outputs basis source-id target-label))))

(defn inputs
  ([basis node-id]
   (gt/arcs-by-target basis node-id))
  ([basis node-id label]
   (gt/arcs-by-target basis node-id label)))

(defn outputs
  ([basis node-id]
   (gt/arcs-by-source basis node-id))
  ([basis node-id label]
   (gt/arcs-by-source basis node-id label)))

(defn- lift-source-arc
  "Used by arcs-by-source/lift-source-arcs to infer all implicit arcs
  the explicit arc `arc` gives rise to if we follow the
  `source-override-node-chain` upwards from the source-id of
  `arc`.
  Returns a list of arcs where:
  * source-id is the final node in source-override-node-chain
  * target-id is an override node in the final override in
  source-override-chain - whose chain of originals passes through the
  original target of `arc` - for which `source-override-chain` is the
  first and longest subsequence of its own chain of overrides.
  * the `arc` has not been shadowed by an intermediate explicit arc"
  [basis source-override-chain source-override-node-chain conflicting-source-overrides-chain ^Arc arc]
  (let [target (.target-id arc)
        target-label (.target-label arc)]
    (if (empty? source-override-chain)
      [arc]
      (let [^IPersistentSet disallowed-override-ids (first conflicting-source-overrides-chain)
            target-graph (node-id->graph basis target)
            target-override-node-ids (overrides target-graph target)]
        (into []
              (comp
                ;; only follow what could make the source override chain
                ;; a subsequence of this target chain - don't traverse
                ;; up the wrong branch
                (remove (fn [target-override-node-id]
                          ;; measurably faster than contains?
                          (.contains disallowed-override-ids (gt/override-id (node-id->node target-graph target-override-node-id)))))
                ;; An explicit arc shadows/blocks implicit arcs
                (filter (fn [target-override-node-id]
                          (nil? (graph-explicit-arcs-by-target target-graph target-override-node-id target-label))))
                ;; Keep lifting, with different remaining chains
                ;; depending on if the current target override matches
                ;; the (current) source.
                (mapcat (fn [target-override-node-id]
                          (let [target-override-id (gt/override-id (node-id->node target-graph target-override-node-id))]
                            (if (= target-override-id (first source-override-chain))
                              (lift-source-arc basis
                                               (rest source-override-chain)
                                               (rest source-override-node-chain)
                                               (rest conflicting-source-overrides-chain)
                                               (assoc arc
                                                      :source-id (first source-override-node-chain)
                                                      :target-id target-override-node-id))
                              (lift-source-arc basis
                                               source-override-chain
                                               source-override-node-chain
                                               conflicting-source-overrides-chain
                                               (assoc arc :target-id target-override-node-id)))))))
              target-override-node-ids)))))

(defn- lift-source-arcs
  [basis override-chains+explicit-arcs]
  (into []
        (comp
          (filter (fn [[_ _ explicit-arcs]] (seq explicit-arcs)))
          (keep (fn [[override-chain override-node-chain explicit-arcs]]
                  ;; Here we can (assert (every? #(= (:source-id %) (:source-id (first explicit-arcs))) explicit-arcs))
                  (let [source (.source-id ^Arc (first explicit-arcs))
                        source-graph (node-id->graph basis source)
                        ;; conflicting-overrides is to prevent following target
                        ;; overrides along the wrong "branches" (for which there may be another
                        ;; better -earlier- matching source node).
                        conflicting-overrides-chain (mapv (fn [node next-override]
                                                            (disj (into #{} (map #(gt/override-id (node-id->node source-graph %)))
                                                                        (overrides source-graph node))
                                                                  next-override))
                                                          (conj override-node-chain source)
                                                          override-chain)]
                    (not-empty (into [] (mapcat (partial lift-source-arc basis override-chain override-node-chain conflicting-overrides-chain)) explicit-arcs)))))
          cat)
        override-chains+explicit-arcs))

(defn- propagate-source-arcs
  "Used by arcs-by-source. After having found a set of arcs from a
  source node (using lift-source-arc), find further implicit
  targets by traversing up the target overrides as long as there is no
  shadowing input, and no corresponding override node of the source
  that should be the implicit arc source."
  [basis arcs]
  (when (seq arcs)
    (let [source (.source-id ^Arc (first arcs))
          source-graph (node-id->graph basis source)
          source-overrides (into #{} (map (comp gt/override-id #(node-id->node source-graph %))) (overrides source-graph source))]
      ;; Here we can (assert (every? #(= source (:source-id %)) arcs)), but it's too costly to run permantently.
      (loop [arcs arcs
             result arcs]
        (let [propagated-arcs (into []
                                    (mapcat (fn [^Arc target-arc]
                                              (let [target (.target-id target-arc)
                                                    label (.target-label target-arc)
                                                    target-graph (node-id->graph basis target)]
                                                (into []
                                                      (comp
                                                        (map (partial node-id->node target-graph))
                                                        (keep (fn [target-override-node]
                                                                ;; no better matching override node, and no shadowing explicit arc
                                                                (when (and (not (contains? source-overrides (gt/override-id target-override-node)))
                                                                           (nil? (graph-explicit-arcs-by-target target-graph (gt/node-id target-override-node) label)))
                                                                  (assoc target-arc :target-id (gt/node-id target-override-node))))))
                                                      (overrides target-graph target)))))
                                    arcs)]
          (if (empty? propagated-arcs)
            result
            (recur propagated-arcs
                   (into result propagated-arcs))))))))

(defn- lift-target-arcs [basis target-id target-override-chain arcs]
  (loop [override-chain target-override-chain
         arcs arcs]
    (if (empty? override-chain)
      (mapv #(assoc % :target-id target-id) arcs)
      (recur (rest override-chain)
             (mapv (fn [^Arc arc]
                     (let [source (.source-id arc)]
                       (if-some [source-override-node (override-of (node-id->graph basis source) source (first override-chain))]
                         (assoc arc :source-id source-override-node)
                         arc)))
                   arcs)))))

(defn- collect-override-chains+explicit-arcs
  "Used by arcs-by-source to find explicit arcs from all original nodes
  of the start node.
  Returns a list of 3-tuples [override-chain override-node-chain
  explicit-arcs] where:
  * explicit-arcs is the explicit arcs found at the current node
  * override-node-chain is the sequence of override node ids to follow
  from the current node to reach the start node
  * override-chain is the sequence of override ids to follow from the
  current node to reach the start node override this is really just
  the override-ids of override-node-chain"
  [explicit-arcs-fn graph start-node-id]
  ;; We've tried writing this in a less convoluted fashion, but the performance was not satisfactory. Something like:
  ;; originals (into [] (take-while some?) (iterate (partial override-original this) node-id)) ; override-originals, but in the order we want
  ;; overrides (map (comp gt/override-id (partial node-id->node graph)) originals)
  ;; node-explicit-arcs (map #(explicit-arcs-fn graph %) originals)
  ;; override-node-chains (reductions conj '() originals)
  ;; override-chains (reductions conj '() overrides)
  ;; override-chains+explicit-arcs (map vector override-chains override-node-chains node-explicit-arcs)
  (loop [node-id start-node-id
         override-chain '()
         override-node-chain '()
         result (transient [])]
    (let [node (node-id->node graph node-id)
          explicit-arcs (explicit-arcs-fn graph node-id)
          result' (if (seq explicit-arcs)
                    (conj! result [override-chain override-node-chain explicit-arcs])
                    result)]
      (if-some [original (gt/original node)]
        (recur original
               (conj override-chain (gt/override-id node))
               (conj override-node-chain node-id)
               result')
        (persistent! result')))))

(def ^:private set-or-union (fn [s1 s2] (if s1 (set/union s1 s2) s2)))
(def ^:private merge-with-union (let [red-f (fn [m [k v]] (update m k set-or-union v))]
                                  (completing (fn [m1 m2] (reduce red-f m1 m2)))))

(defn- basis-dependencies [basis outputs-by-node-ids]
  (let [graph-id->node-successor-map (into {} (map (fn [[graph-id graph]] [graph-id (:successors graph)])) (:graphs basis))
        node-id->successor-map (fn [node-id]
                                 (some-> node-id
                                         gt/node-id->graph-id
                                         graph-id->node-successor-map
                                         (get node-id)))]
    (loop [;; 'todo' is the running stack (actually a map) of entries to traverse
           ;; it's expensive to iterate a map, so start by turning it into a seq
           todo (seq outputs-by-node-ids)
           ;; collect next batch of entries in a map, to coalesce common node ids
           next-todo {}
           ;; final transitive closure of entries found, as a map
           result {}]
      (if-let [[node-id outputs] (first todo)]
        (let [seen? (get result node-id)]
          ;; termination condition is when we have seen *every* output already
          (if (and seen? (every? seen? outputs))
            ;; completely remove the node-id from todo as we have seen *every* output
            (recur (next todo) next-todo result)
            ;; does the node-id have any successors at all?
            (if-let [label->succ (node-id->successor-map node-id)]
              ;; ignore the outputs we have already seen
              (let [outputs (if seen? (set/difference outputs seen?) outputs)
                    ;; Add every successor to the stack for later processing
                    next-todo (transduce (map #(label->succ %)) merge-with-union next-todo outputs)
                    ;; And include the unseen output labels to the result
                    result (update result node-id set-or-union outputs)]
                (recur (next todo) next-todo result))
              ;; There were no successors, recur without that node-id
              (recur (next todo) next-todo result))))
        ;; check if there is a next batch of entries to process
        (if-let [todo (seq next-todo)]
          (recur todo {} result)
          result)))))


(defrecord MultigraphBasis [graphs]
  gt/IBasis
  (node-by-id-at
    [this node-id]
    (node-id->node (node-id->graph this node-id) node-id))

  (node-by-property
    [_ label value]
    (filter #(= value (get % label)) (mapcat vals graphs)))

  ;; arcs-by-target and arcs-by-source (should!) always return symmetric
  ;; results. Generally, we find the explicit arcs along the
  ;; override chain and then lift/propagate these up the source-
  ;; and target override trees as far as the source override chain
  ;; is the earliest and longest possible subsequence of the target
  ;; override chain, and there is no explicit arcs shadowing the
  ;; implicitly lifted ones.

  (arcs-by-target
    [this node-id]
    (let [graph (node-id->graph this node-id)
          override-chains+explicit-arcs (loop [node-id node-id
                                               override-chain '()
                                               seen-inputs #{}
                                               result (transient [])]
                                          (let [node (node-id->node graph node-id)
                                                explicit-arcs (remove (comp seen-inputs :target-label) (graph-explicit-arcs-by-target graph node-id))
                                                result' (if (seq explicit-arcs)
                                                          (conj! result [override-chain explicit-arcs])
                                                          result)]
                                            (if-some [original (gt/original node)]
                                              (recur original
                                                     (conj override-chain (gt/override-id node))
                                                     (into seen-inputs (map :target-label) explicit-arcs)
                                                     result')
                                              (persistent! result'))))]
      (into []
            (mapcat (fn [[override-chain explicit-arcs]]
                      (lift-target-arcs this node-id override-chain explicit-arcs)))
            override-chains+explicit-arcs)))

  (arcs-by-target
    [this node-id label]
    (let [graph (node-id->graph this node-id)
          [override-chain explicit-arcs] (loop [node-id node-id
                                                chain '()]
                                           (let [arcs (graph-explicit-arcs-by-target graph node-id label)
                                                 node (gt/node-by-id-at this node-id)
                                                 original (gt/original node)]
                                             (if (and (empty? arcs) original)
                                               (recur original (conj chain (gt/override-id node)))
                                               [chain arcs])))]
      (lift-target-arcs this node-id override-chain explicit-arcs)))

  (arcs-by-source
    [this node-id]
    (let [graph (node-id->graph this node-id)
          ;; Traverse original chain, collect explicit arcs from the
          ;; original + the override chain + override node chain from
          ;; that original to this node.
          override-chains+explicit-arcs (collect-override-chains+explicit-arcs graph-explicit-arcs-by-source graph node-id)
          ;; Looking at the arcs we found, what arcs to new targets
          ;; are implied by following the override chains
          ;; at most up to this node?
          lifted-arcs (lift-source-arcs this override-chains+explicit-arcs)]
      ;; Lifted arcs are now valid outgoing arcs from node-id label. But we're still missing
      ;; some possible targets reachable by following the branches from the respective targets as long
      ;; as there are no explicit incoming arcs and no "higher" override node of the source in the reached
      ;; target node override.
      ;; Here we can (when (seq lifted-arcs) (assert (every? #(= (:source %) (:source (first lifted-arcs))) lifted-arcs)))
      (propagate-source-arcs this lifted-arcs)))

  (arcs-by-source
     [this node-id label]
     (let [graph (node-id->graph this node-id)
           override-chains+explicit-arcs (collect-override-chains+explicit-arcs (fn [graph node-id] (graph-explicit-arcs-by-source graph node-id label)) graph node-id)
           lifted-arcs (lift-source-arcs this override-chains+explicit-arcs)]
       (propagate-source-arcs this lifted-arcs)))

  (sources [this node-id] (mapv gt/source (inputs this node-id)))
  (sources [this node-id label] (mapv gt/source (inputs this node-id label)))

  (targets [this node-id] (mapv gt/target (outputs this node-id)))
  (targets [this node-id label] (mapv gt/target (outputs this node-id label)))

  (add-node
    [this node]
    (let [node-id (gt/node-id node)
          graph-id (gt/node-id->graph-id node-id)
          graph (add-node (get graphs graph-id) node-id node)]
      [(update this :graphs assoc graph-id graph) node]))

  (delete-node
    [this node-id]
    (basis-remove-node this node-id (-> this (gt/node-by-id-at node-id) gt/original)))

  (replace-node
    [this node-id new-node]
    (let [graph-id (gt/node-id->graph-id node-id)
          new-node (assoc new-node :_node-id node-id)
          graph (assoc-in (get graphs graph-id) [:nodes node-id] new-node)]
      [(update this :graphs assoc graph-id graph) new-node]))

  (replace-override
    [this override-id new-override]
    (let [graph-id (gt/override-id->graph-id override-id)]
      (update-in this [:graphs graph-id :overrides] assoc override-id new-override)))

  (override-node
    [this original-node-id override-node-id]
    (let [graph-id (gt/node-id->graph-id override-node-id)]
      (update-in this [:graphs graph-id :node->overrides original-node-id] util/conjv override-node-id)))

  (override-node-clear [this original-id]
    (let [graph-id (gt/node-id->graph-id original-id)]
      (update-in this [:graphs graph-id :node->overrides] dissoc original-id)))

  (add-override
    [this override-id override]
    (let [graph-id (gt/override-id->graph-id override-id)]
      (update-in this [:graphs graph-id :overrides] assoc override-id override)))

  (delete-override
    [this override-id]
    (let [graph-id (gt/override-id->graph-id override-id)]
      (update-in this [:graphs graph-id :overrides] dissoc override-id)))

  (connect
    [this source-id source-label target-id target-label]
    (let [source-graph-id (gt/node-id->graph-id source-id)
          source-graph (get graphs source-graph-id)
          target-graph-id (gt/node-id->graph-id target-id)
          target-graph (get graphs target-graph-id)
          target-node (node-id->node target-graph target-id)
          target-node-type (gt/node-type target-node this)]
      (assert (<= (:_volatility source-graph 0) (:_volatility target-graph 0)))
      (assert (in/has-input? target-node-type target-label) (str "No label " target-label " exists on node " target-node))
      (if (= source-graph-id target-graph-id)
        (update this :graphs assoc
                source-graph-id (-> source-graph
                                    (connect-target source-id source-label target-id target-label)
                                    (connect-source source-id source-label target-id target-label)))
        (update this :graphs assoc
                source-graph-id (connect-source source-graph source-id source-label target-id target-label)
                target-graph-id (connect-target target-graph source-id source-label target-id target-label)))))

  (disconnect
    [this source-id source-label target-id target-label]
    (let [source-graph-id (gt/node-id->graph-id source-id)
          source-graph (get graphs source-graph-id)
          target-graph-id (gt/node-id->graph-id target-id)
          target-graph (get graphs target-graph-id)]
      (if (= source-graph-id target-graph-id)
        (update this :graphs assoc
                source-graph-id (-> source-graph
                                    (disconnect-source source-id source-label target-id target-label)
                                    (disconnect-target source-id source-label target-id target-label)))
        (update this :graphs assoc
                source-graph-id (disconnect-source source-graph source-id source-label target-id target-label)
                target-graph-id (disconnect-target target-graph source-id source-label target-id target-label)))))

  (connected?
    [this source-id source-label target-id target-label]
    (let [source-graph (node-id->graph this source-id)
          targets (gt/targets this source-id source-label)]
      (some #{[target-id target-label]} targets)))

  (dependencies
    [this outputs-by-node-ids]
    (basis-dependencies this outputs-by-node-ids))

  (original-node [this node-id]
    (when-let [node (gt/node-by-id-at this node-id)]
      (gt/original node))))

(defn multigraph-basis
  [graphs]
  (MultigraphBasis. graphs))

(defn make-override [root-id traverse-fn]
  {:root-id root-id
   :traverse-fn traverse-fn})

(defn override-traverse-fn [basis override-id]
  (let [graph-id (gt/override-id->graph-id override-id)]
    (get-in basis [:graphs graph-id :overrides override-id :traverse-fn])))

(defn- sarcs->arcs [sarcs]
  (into #{}
        (comp (mapcat vals)
              cat)
        (vals sarcs)))

(defn old-hydrate-after-undo
  [basis graph-state]
  (let [old-sarcs (get graph-state :sarcs)
        sarcs (rebuild-sarcs basis graph-state)
        sarcs-diff (clojure.set/difference (sarcs->arcs sarcs) (sarcs->arcs old-sarcs))
        graph-id (:_graph-id graph-state)
        new-basis (update basis :graphs assoc graph-id (assoc graph-state :sarcs sarcs))]
    {:basis new-basis
     :outputs-to-refresh (mapv (juxt :source-id :source-label) sarcs-diff)}))

(defn new-hydrate-after-undo [basis graph-state]
  (let [graph-id (:_graph-id graph-state)
        other-graphs (vals (dissoc (:graphs basis) graph-id))
        old-sarcs (get graph-state :sarcs)
        old-arc? (fn [^Arc arc]
                   ;; The number of arcs from a specific source to a different
                   ;; graph will be small. A linear search should be fine.
                   (some (partial = arc)
                         (get-in old-sarcs [(.source-id arc) (.source-label arc)])))
        new-arcs (into #{}
                       (comp (mapcat :tarcs)
                             (mapcat val)
                             (mapcat val)
                             (filter (fn [^Arc arc]
                                       (and (= graph-id (gt/node-id->graph-id (.source-id arc)))
                                            (not (old-arc? arc))))))
                       other-graphs)
        sarcs (if (empty? new-arcs)
                old-sarcs
                (persistent!
                  (reduce (fn [sarcs ^Arc arc]
                            (let [source-id (.source-id arc)
                                  source-label (.source-label arc)]
                              (assoc! sarcs source-id
                                      (if-some [arcs-by-source-label (get sarcs source-id)]
                                        (update arcs-by-source-label source-label conj arc)
                                        {source-label [arc]}))))
                          (transient old-sarcs)
                          new-arcs)))
        new-basis (update basis :graphs assoc graph-id (assoc graph-state :sarcs sarcs))
        outputs-to-refresh (mapv (juxt :source-id :source-label) new-arcs)]
    {:basis new-basis
     :outputs-to-refresh outputs-to-refresh}))

(defn- hydrate-result->sarcs-by-graph-id [hydrate-result]
  (into {}
        (map (fn [[gid g]]
               [gid (sarcs->arcs (:sarcs g))]))
        (:graphs (:basis hydrate-result))))

(def ^:private failing-hydrate-input (atom nil))

(defn hydrate-after-undo [basis graph-state]
  (let [old-result (old-hydrate-after-undo basis graph-state)
        new-result (new-hydrate-after-undo basis graph-state)
        [sarcs-only-in-old sarcs-only-in-new] (data/diff (hydrate-result->sarcs-by-graph-id old-result)
                                                         (hydrate-result->sarcs-by-graph-id new-result))
        [refreshed-outputs-only-in-old refreshed-outputs-only-in-new] (data/diff (set (:outputs-to-refresh old-result))
                                                                                 (set (:outputs-to-refresh new-result)))]
    (when-not (and (nil? sarcs-only-in-old)
                   (nil? sarcs-only-in-new)
                   (nil? refreshed-outputs-only-in-old)
                   (nil? refreshed-outputs-only-in-new))
      (reset! failing-hydrate-input [basis graph-state sarcs-only-in-old sarcs-only-in-new refreshed-outputs-only-in-old refreshed-outputs-only-in-new])
      (throw (ex-info "New implementation of hydrate-after-undo is not equivalent!"
                      {:sarcs-only-in-old sarcs-only-in-old
                       :sarcs-only-in-new sarcs-only-in-new
                       :refreshed-outputs-only-in-old refreshed-outputs-only-in-old
                       :refreshed-outputs-only-in-new refreshed-outputs-only-in-new})))
    new-result))

(defn- input-deps [basis node-id]
  (some-> (gt/node-by-id-at basis node-id)
          (gt/node-type basis)
          in/input-dependencies))

(defn- update-graph-successors [old-successors basis graph-id graph changes]
  "The purpose of this fn is to build a data structure that reflects which set of node-id + outputs that can be reached from the incoming changes (map of node-id + outputs)
   For a specific node-id-a + output-x, add:
     the internal input-dependencies, i.e. outputs consuming the given output
     the closest override-nodes, i.e. override-node-a + output-x, as they can be potential dependents
     all connected nodes, where node-id-a + output-x => [[node-id-b + input-y] ...] => [[node-id-b + output+z] ...]"
  (let [node-id->overrides (or (:node->overrides graph) (constantly nil))
        ;; Transducer to collect override-id's
        override-id-xf (keep #(some->> %
                                       (node-id->node graph)
                                       (gt/override-id)))
        changes+old-node-successors (mapv (fn [[node-id labels]]
                                            [node-id labels (old-successors node-id)])
                                          changes)
        [new-successors removed-successor-entries] (r/fold
                                                     (fn combinef
                                                       ([] [{} []])
                                                       ([[new-successors-1 removed-successor-entries-1] [new-successors-2 removed-successor-entries-2]]
                                                        [(merge new-successors-1 new-successors-2) (into removed-successor-entries-1 removed-successor-entries-2)]))
                                                     (fn reducef
                                                       ([] [{} []])
                                                       ([[new-acc remove-acc] [node-id labels old-node-successors]]
                                                        (let [new-node-successors (if-some [node (gt/node-by-id-at basis node-id)]
                                                                                    (let [node-type (gt/node-type node basis)
                                                                                          deps-by-label (or (in/input-dependencies node-type) {})
                                                                                          node-and-overrides (tree-seq (constantly true) node-id->overrides node-id)
                                                                                          override-filter-fn (complement (into #{} override-id-xf node-and-overrides))
                                                                                          overrides (node-id->overrides node-id)
                                                                                          labels (or labels (in/output-labels node-type))
                                                                                          arcs-by-source (if (> (count labels) 1)
                                                                                                           (let [arcs (gt/arcs-by-source basis node-id)
                                                                                                                 arcs-by-source-label (group-by #(.source-label ^Arc %) arcs)]
                                                                                                             (fn [_ _ label]
                                                                                                               (arcs-by-source-label label)))
                                                                                                           (fn [basis node-id label]
                                                                                                             (gt/arcs-by-source basis node-id label)))]
                                                                                      (reduce (fn [new-node-successors label]
                                                                                                (let [single-label #{label}
                                                                                                      dep-labels (get deps-by-label label)
                                                                                                      ;; The internal dependent outputs
                                                                                                      deps (cond-> (transient {})
                                                                                                             (and dep-labels (> (count dep-labels) 0))
                                                                                                             (assoc! node-id dep-labels))
                                                                                                      ;; The closest overrides
                                                                                                      deps (transduce (map #(vector % single-label)) conj! deps overrides)
                                                                                                      ;; The connected nodes and their outputs
                                                                                                      deps (transduce (keep (fn [^Arc arc]
                                                                                                                              (let [target-id (.target-id arc)
                                                                                                                                    target-label (.target-label arc)]
                                                                                                                                (when-let [dep-labels (get (input-deps basis target-id) target-label)]
                                                                                                                                  [target-id dep-labels]))))
                                                                                                                      conj!
                                                                                                                      deps
                                                                                                                      (arcs-by-source basis node-id label))]
                                                                                                  (assoc new-node-successors label (persistent! deps))))
                                                                                              (or old-node-successors {})
                                                                                              labels))
                                                                                    nil)]
                                                          (if (nil? new-node-successors)
                                                            [new-acc (conj remove-acc node-id)]
                                                            [(assoc new-acc node-id new-node-successors) remove-acc]))))
                                                     changes+old-node-successors)]
    (apply dissoc (merge old-successors new-successors) removed-successor-entries)))

(defn update-successors
  [basis changes]
  ;; changes = {node-id #{outputs}}
  (reduce (fn [basis [graph-id changes]]
            (if-let [graph (get (:graphs basis) graph-id)]
              (update-in basis [:graphs graph-id :successors] update-graph-successors basis graph-id graph changes)
              basis))
          basis
          (group-by (comp gt/node-id->graph-id first) changes)))
