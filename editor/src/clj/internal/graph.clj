(ns internal.graph
  (:require [clojure.set :as set]
            [clojure.core.reducers :as r]
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
;; or all of it's properties, or connect something else to it's inputs.
;;
;; In most cases, the thing being instantiated is not implemented as a
;; single node, but rather one conceptual root node (often a resource
;; node) and a cluster of private helper nodes owned via :cascade-delete
;; inputs.
;;
;; An _Override_ is a set of override nodes, each derived from an
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
;; _explicit_ arc will shadow the _implicit_ arcs. Later `disconnect`ing
;; the _explicit_ arc will effectively revert to the _implicit_ arcs.
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
;; O/override1 for it's outgoing arcs from output-label, we should get
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
;;    O/landscape2  ~~~~~~~~~~~~~~~~~~~>  I/landscape2,landscape1
;;
;;
;;                                 ~~~~>  I/landscape1
;;                            ~~~~~
;;                      ~~~~~
;;    O:output-label -------------------> I:input-label
;;
;;
;; Here, as before there is an implicit arc O:output-label to
;; I/landscape1:input-label. What about I/landscape2,landscape1?  Since
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
;;    O/template2,template,landscape      I/template,landscape2,square,landscape1
;;
;;
;;    O/template,landscape2
;;
;;
;;  O/square     O/landscape2             I/landscape2,square,landscape1
;;
;;
;;                                        I/square,landscape1
;;
;;
;;                                        I/landscape1
;;
;;
;;    O:output-label -------------------> I:input-label
;;
;;
;; Here, similar to before, there is an implicit arc
;; O/square:output-label to I/square,landscape1:input-label. And O/square
;; remains the source for the implicit arcs to I/landscape2,square,landscape1 and
;; I/template,landscape2,square,landscape1.
;;
;; The common rule in these examples is that for the implicit arcs to a
;; particular target node, the source node override chain should be the
;; longest and first possible subsequence of the target node override
;; chain.

(set! *warn-on-reflection* true)

(definline ^:private arc
  [source target source-label target-label]
  `(Arc. ~source ~target ~source-label ~target-label))

(defn arcs->tuples [arcs]
  (into [] (map (fn [^Arc arc]
                  [(.source arc) (.sourceLabel arc) (.target arc) (.targetLabel arc)]))
        arcs))


(defn arc-endpoints-p [p arc]
  (and (p (.source ^Arc arc))
       (p (.target ^Arc arc))))

(defn- rebuild-sarcs
  [basis graph-state]
  (let [gid      (:_gid graph-state)
        all-graphs (vals (assoc (:graphs basis) gid graph-state))
        all-arcs (mapcat (fn [g] (mapcat (fn [[nid label-tarcs]] (mapcat second label-tarcs))
                                         (:tarcs g)))
                         all-graphs)
        all-arcs-filtered (filter (fn [^Arc arc] (= (gt/node-id->graph-id (.source arc)) gid)) all-arcs)]
    (reduce
      (fn [sarcs arc] (update-in sarcs [(.source ^Arc arc) (.sourceLabel ^Arc arc)] util/conjv arc))
      {}
      all-arcs-filtered)))

(defn empty-graph
     []
     {:nodes      {}
      :sarcs      {}
      :successors {}
      :tarcs      {}
      :tx-id      0})

(defn node-ids    [g] (keys (:nodes g)))
(defn node-values [g] (vals (:nodes g)))

(defn add-node    [g id n] (assoc-in g [:nodes id] n))

(definline node-id->graph [basis node-id] `(get (:graphs ~basis) (gt/node-id->graph-id ~node-id)))
(definline node-id->node [graph node-id] `(get (:nodes ~graph) ~node-id))

(defn- overrides [graph node-id]
  (get (:node->overrides graph) node-id))

;; This function only removes the node from the single graph in which it exists
;; It should only be used for testing purposes
(defn graph-remove-node
  ([g n]
    (graph-remove-node g n (some-> (get-in g [:nodes n]) gt/original)))
  ([g n original-id]
    (graph-remove-node g n original-id false))
  ([g n original-id original-deleted?]
    (if (contains? (:nodes g) n)
      (let [g (reduce (fn [g or-n] (graph-remove-node g or-n n true)) g (overrides g n))
            sarcs (mapcat second (get-in g [:sarcs n]))
            tarcs (mapcat second (get-in g [:tarcs n]))
            override-id (when original-id (gt/override-id (get-in g [:nodes n])))
            override (when original-id (get-in g [:overrides override-id]))]
        (-> g
          (cond->
            (not original-deleted?) (update-in [:node->overrides original-id] (partial util/removev #{n}))
            (and override (= original-id (:root-id override))) (update :overrides dissoc override-id))
         (update :nodes dissoc n)
         (update :node->overrides dissoc n)
         (update :sarcs dissoc n)
         (update :sarcs (fn [s] (reduce (fn [s ^Arc arc]
                                          (update-in s [(.source arc) (.sourceLabel arc)]
                                            (fn [arcs] (util/removev (fn [^Arc arc] (= n (.target arc))) arcs))))
                                  s tarcs)))
         (update :tarcs dissoc n)
         (update :tarcs (fn [s] (reduce (fn [s ^Arc arc]
                                          (update-in s [(.target arc) (.targetLabel arc)]
                                            (fn [arcs] (util/removev (fn [^Arc arc] (= n (.source arc))) arcs))))
                                  s sarcs)))))
      g)))

(defn- arc-cross-graph? [^Arc arc]
  (not= (gt/node-id->graph-id (.source arc)) (gt/node-id->graph-id (.target arc))))

(defn- arcs-remove-node [arcs node-id]
  (util/removev (fn [^Arc arc] (or (= node-id (.source arc))
                                     (= node-id (.target arc)))) arcs))

(defn basis-remove-node
  ([basis n]
    (basis-remove-node basis n (some-> (get-in basis [:graphs (gt/node-id->graph-id n) :nodes n]) gt/original)))
  ([basis n original-id]
    (basis-remove-node basis n original-id false))
  ([basis n original-id original-deleted?]
    (let [g (node-id->graph basis n)
          basis (reduce (fn [basis or-n] (basis-remove-node basis or-n n true)) basis (overrides g n))
          sarcs (mapcat second (get-in g [:sarcs n]))
          tarcs (mapcat second (get-in g [:tarcs n]))
          override-id (when original-id (gt/override-id (get-in g [:nodes n])))
          override (when original-id (get-in g [:overrides override-id]))
          ext-sarcs (filterv arc-cross-graph? sarcs)
          ext-tarcs (filterv arc-cross-graph? tarcs)
          basis (-> basis
                  (update-in [:graphs (gt/node-id->graph-id n)]
                    (fn [g]
                      (-> g
                        (cond->
                          (not original-deleted?) (update-in [:node->overrides original-id] (partial util/removev #{n}))
                          (and override (= original-id (:root-id override))) (update :overrides dissoc override-id))
                        (update :nodes dissoc n)
                        (update :node->overrides dissoc n)
                        (update :sarcs dissoc n)
                        (update :sarcs (fn [s] (reduce (fn [s ^Arc arc]
                                                         (update-in s [(.source arc) (.sourceLabel arc)] arcs-remove-node n))
                                                 s tarcs)))
                        (update :tarcs dissoc n)
                        (update :tarcs (fn [s] (reduce (fn [s ^Arc arc]
                                                         (update-in s [(.target arc) (.targetLabel arc)] arcs-remove-node n))
                                                 s sarcs)))))))
          basis (reduce (fn [basis ^Arc arc]
                          (let [nid (.target arc)]
                            (update-in basis [:graphs (gt/node-id->graph-id nid) :tarcs nid (.targetLabel arc)] arcs-remove-node n)))
                  basis ext-sarcs)
          basis (reduce (fn [basis ^Arc arc]
                          (let [nid (.source arc)]
                            (update-in basis [:graphs (gt/node-id->graph-id nid) :tarcs nid (.sourceLabel arc)] arcs-remove-node n)))
                  basis ext-tarcs)]
      basis)))

(defn transform-node
  [g n f & args]
  (if-let [node (get-in g [:nodes n])]
    (assoc-in g [:nodes n] (apply f node args))
    g))

(defn connect-source
  [g source source-label target target-label]
  (let [from (node-id->node g source)]
    (assert (not (nil? from)) (str "Attempt to connect " (pr-str source source-label target target-label)))
    (update-in g [:sarcs source source-label] util/conjv (arc source target source-label target-label))))

(defn connect-target
  [g source source-label target target-label]
  (let [to (node-id->node g target)]
    (assert (not (nil? to)) (str "Attempt to connect " (pr-str source source-label target target-label)))
    (update-in g [:tarcs target target-label] util/conjv (arc source target source-label target-label))))

(defn disconnect-source
  [g source source-label target target-label]
  (cond-> g
    (node-id->node g source)
    (update-in  [:sarcs source source-label]
                (fn [arcs]
                  (util/removev
                    (fn [^Arc arc]
                      (and (= source       (.source arc))
                           (= target       (.target arc))
                           (= source-label (.sourceLabel arc))
                           (= target-label (.targetLabel arc))))
                    arcs)))))

(defn disconnect-target
  [g source source-label target target-label]
  (cond-> g
    (node-id->node g target)
    (update-in [:tarcs target target-label]
               (fn [arcs]
                 (util/removev
                   (fn [^Arc arc]
                     (and (= source       (.source arc))
                          (= target       (.target arc))
                          (= source-label (.sourceLabel arc))
                          (= target-label (.targetLabel arc))))
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
  (when-let [n (gt/node-by-id-at basis node-id)]
    (gt/original n)))

(defn override-originals [basis node-id]
  (into '() (take-while some? (iterate (partial override-original basis) node-id))))

(defn- override-of [graph node-id override-id]
  (some #(and (= override-id (gt/override-id (node-id->node graph %))) %) (overrides graph node-id)))

(defn- node-id->arcs [graph node-id arc-kw]
  (into [] cat (vals (-> graph (get arc-kw) (get node-id)))))

;; This should really be made interface methods of IBasis

(defn- graph-explicit-arcs-by-source
  ([graph src-id]
   (node-id->arcs graph src-id :sarcs))
  ([graph src-id src-label]
   (-> (:sarcs graph)
     (get src-id)
     (get src-label))))

(defn explicit-arcs-by-source
  ([basis src-id]
   (graph-explicit-arcs-by-source (node-id->graph basis src-id) src-id))
  ([basis src-id src-label]
   (graph-explicit-arcs-by-source (node-id->graph basis src-id) src-id src-label)))

(defn- graph-explicit-arcs-by-target
  ([graph tgt-id]
   (node-id->arcs graph tgt-id :tarcs))
  ([graph tgt-id tgt-label]
   (-> (:tarcs graph)
     (get tgt-id)
     (get tgt-label))))

(defn explicit-arcs-by-target
  ([basis tgt-id]
   (graph-explicit-arcs-by-target (node-id->graph basis tgt-id) tgt-id))
  ([basis tgt-id tgt-label]
   (graph-explicit-arcs-by-target (node-id->graph basis tgt-id) tgt-id tgt-label)))

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
  ([basis tgt-id]
   (mapv gt/head (explicit-inputs basis tgt-id)))
  ([basis tgt-id tgt-label]
   (mapv gt/head (explicit-inputs basis tgt-id tgt-label))))

(defn explicit-targets
  ([basis src-id]
   (mapv gt/tail (explicit-outputs basis src-id)))
  ([basis src-id tgt-label]
   (mapv gt/tail (explicit-outputs basis src-id tgt-label))))

(defn inputs
  ([basis node-id]
   (gt/arcs-by-tail basis node-id))
  ([basis node-id label]
   (gt/arcs-by-tail basis node-id label)))

(defn outputs
  ([basis node-id]
   (gt/arcs-by-head basis node-id))
  ([basis node-id label]
   (gt/arcs-by-head basis node-id label)))

(defn- lifted-target-arcs [basis override-chain override-node-chain conflicting-chain {target :target target-label :targetLabel :as arc}]
  (if (seq override-chain)
    (let [disallowed-override-ids (first conflicting-chain)
          target-graph (node-id->graph basis target)
          target-override-node-ids (overrides target-graph target)]
      (into []
            (comp
              ;; only follow what could make the source override chain
              ;; a subsequence of this target chain - don't traverse
              ;; up the wrong branch
              (remove (fn [target-override-node-id]
                        ;; faster than contains?
                        (. ^IPersistentSet disallowed-override-ids
                           (contains (gt/override-id (node-id->node target-graph target-override-node-id))))))
              ;; An explicit arc shadows/blocks implicit arcs
              (filter (fn [target-override-node-id]
                        (nil? (graph-explicit-arcs-by-target target-graph target-override-node-id target-label))))
              ;; Keep lifting, with different remaining chains
              ;; depending on if the current target override matches
              ;; the (current) source.
              (mapcat (fn [target-override-node-id]
                        (let [target-override-id (gt/override-id (node-id->node target-graph target-override-node-id))]
                          (if (= target-override-id (first override-chain))
                            (lifted-target-arcs basis
                                                (rest override-chain)
                                                (rest override-node-chain)
                                                (rest conflicting-chain)
                                                (assoc arc
                                                       :source (first override-node-chain)
                                                       :target target-override-node-id))
                            (lifted-target-arcs basis
                                                override-chain
                                                override-node-chain
                                                conflicting-chain
                                                (assoc arc :target target-override-node-id)))))))
            target-override-node-ids))
    [arc]))

(defn- propagate-target-arcs [basis target-arcs]
  (when (seq target-arcs)
    (let [source (:source (first target-arcs))
          source-graph (node-id->graph basis source)
          source-overrides (into #{} (map (comp gt/override-id #(node-id->node source-graph %))) (overrides source-graph source))]
      #_(assert (every? #(= source (:source %)) target-arcs))
      (loop [target-arcs target-arcs
             result target-arcs]
        (let [propagated-target-arcs (into []
                                           (mapcat (fn [{target :target label :targetLabel :as target-arc}]
                                                     (let [target-graph (node-id->graph basis target)]
                                                       (into []
                                                             (comp
                                                                 (map (partial node-id->node target-graph))
                                                                 (keep (fn [target-override-node]
                                                                         ;; no better matching override node, and no shadowing explicit arc
                                                                         (when (and (not (contains? source-overrides (gt/override-id target-override-node)))
                                                                                    (nil? (graph-explicit-arcs-by-target target-graph (gt/node-id target-override-node) label)))
                                                                           (assoc target-arc :target (gt/node-id target-override-node))))))
                                                             (overrides target-graph target)))))
                                           target-arcs)]
          (if (empty? propagated-target-arcs)
            result
            (recur propagated-target-arcs
                   (into result propagated-target-arcs))))))))

(def ^:private set-or-union (fn [s1 s2] (if s1 (set/union s1 s2) s2)))
(def ^:private merge-with-union (let [red-f (fn [m [k v]] (update m k set-or-union v))]
                                  (completing (fn [m1 m2] (reduce red-f m1 m2)))))

(defn- basis-dependencies [basis outputs-by-node-ids]
  (let [gid->succ (into {} (map (fn [[gid graph]] [gid (:successors graph)])) (:graphs basis))
        nid->succ (fn [nid]
                    (some-> nid
                      gt/node-id->graph-id
                      gid->succ
                      (get nid)))]
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
            (if-let [label->succ (nid->succ node-id)]
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

  ;; arcs-by-tail and arcs-by-head (should!) always return symmetric
  ;; results. Generally, we find the explicit arcs along the
  ;; override chain and then lift/propagate these up the source-
  ;; and target override trees as far as the source override chain
  ;; is the earliest and longest possible subsequence of the target
  ;; override chain, and there is no explicit arcs shadowing the
  ;; implicitly lifted ones.

  (arcs-by-tail
    [this node-id]
    (let [graph (node-id->graph this node-id)
          explicit-arcs+override-chains (loop [node-id node-id
                                               override-chain '()
                                               seen-inputs #{}
                                               result []]
                                          (let [node (node-id->node graph node-id)
                                                explicit-arcs (remove #(seen-inputs (:targetLabel %)) (graph-explicit-arcs-by-target graph node-id))
                                                result' (if (seq explicit-arcs)
                                                          (conj result [explicit-arcs override-chain])
                                                          result)]
                                            (if (gt/original node)
                                              (recur (gt/original node)
                                                     (conj override-chain (gt/override-id node))
                                                     (into seen-inputs (map :targetLabel) explicit-arcs)
                                                     result')
                                              result')))]
      (into []
            (mapcat (fn [[explicit-arcs override-chain]]
                      (loop [override-chain override-chain
                             arcs explicit-arcs]
                        (if (empty? override-chain)
                          (into [] (map #(assoc % :target node-id)) arcs)
                          (recur (rest override-chain)
                                 (into [] (map (fn [{source :source :as arc}]
                                                 (assoc arc :source (or (override-of (node-id->graph this source) source (first override-chain)) source))))
                                       arcs))))))
            explicit-arcs+override-chains)))

  (arcs-by-tail
    [this node-id label]
    (let [graph (node-id->graph this node-id)
          [override-chain explicit-arcs] (loop [node-id node-id
                                                chain '()]
                                           (let [arcs (graph-explicit-arcs-by-target graph node-id label)]
                                             (if (and (empty? arcs) (gt/original-node this node-id))
                                               (recur (gt/original-node this node-id)
                                                      (conj chain (gt/override-id (gt/node-by-id-at this node-id))))
                                               [chain arcs])))]
      (when (seq explicit-arcs)
        (loop [chain override-chain
               arcs explicit-arcs]
          (if (empty? chain)
            (into [] (map #(assoc % :target node-id)) arcs)
            (recur (rest chain)
                   (into [] (map (fn [{source :source :as arc}]
                                   (assoc arc :source (or (override-of (node-id->graph this source) source (first chain)) source))))
                         arcs)))))))

  (arcs-by-head
    [this node-id]
    (let [graph (node-id->graph this node-id)
          explicit-arcs+override-chains (loop [node-id node-id
                                               override-chain '()
                                               override-node-chain '()
                                               result []]
                                          (let [node (node-id->node graph node-id)
                                                explicit-arcs (graph-explicit-arcs-by-source graph node-id)
                                                result' (if (seq explicit-arcs)
                                                          (conj result [explicit-arcs override-chain override-node-chain])
                                                          result)]
                                            (if (gt/original node)
                                              (recur (gt/original node)
                                                     (conj override-chain (gt/override-id node))
                                                     (conj override-node-chain node-id)
                                                     result')
                                              result')))
          lifted-arcs (into []
                            (comp
                              (keep (fn [[explicit-arcs override-chain override-node-chain]]
                                      #_(assert (every? #(= (:source %) (:source (first explicit-arcs))) explicit-arcs))
                                      (let [source (:source (first explicit-arcs))
                                            source-graph (node-id->graph this source)
                                            conflicting-overrides-chain (mapv (fn [node next-override]
                                                                                (disj (into #{} (map #(gt/override-id (node-id->node source-graph %)))
                                                                                            (overrides source-graph node))
                                                                                      next-override))
                                                                              (conj override-node-chain source)
                                                                              override-chain)]
                                        (not-empty (into [] (mapcat (partial lifted-target-arcs this override-chain override-node-chain conflicting-overrides-chain)) explicit-arcs)))))
                              cat)
                            explicit-arcs+override-chains)]
      #_(when (seq lifted-arcs) (assert (every? #(= (:source %) (:source (first lifted-arcs))) lifted-arcs)))
      (propagate-target-arcs this lifted-arcs)))

  (arcs-by-head
    [this node-id label]
    (let [graph (node-id->graph this node-id)
          ;; Traverse original chain, rember explicit arcs from the
          ;; original + the override chain + override node chain from
          ;; that original to this node.
          explicit-arcs+override-chains (loop [node-id node-id
                                               override-chain '()
                                               override-node-chain '()
                                               result []]
                                          (let [node (node-id->node graph node-id)
                                                explicit-arcs (graph-explicit-arcs-by-source graph node-id label)
                                                result' (if (seq explicit-arcs)
                                                          (conj result [explicit-arcs override-chain override-node-chain])
                                                          result)]
                                            (if (gt/original node)
                                              (recur (gt/original node)
                                                     (conj override-chain (gt/override-id node))
                                                     (conj override-node-chain node-id)
                                                     result')
                                              result')))
          ;; Looking at the arcs we found, what arcs to new targets
          ;; are implied by following the override chains
          ;; at most up to this node?
          lifted-arcs (into []
                            (comp
                              (keep (fn [[explicit-arcs override-chain override-node-chain]]
                                      #_(assert (every? #(= (:source %) (:source (first explicit-arcs))) explicit-arcs))
                                      (let [source (:source (first explicit-arcs))
                                            source-graph (node-id->graph this source)
                                            ;; conflicting-overrides is to prevent following target
                                            ;; overrides along the wrong "branches" (for which there may be another
                                            ;; better -earlier- matching source node).
                                            conflicting-overrides-chain (mapv (fn [node next-override]
                                                                                (disj (into #{} (map #(gt/override-id (node-id->node source-graph %)))
                                                                                            (overrides source-graph node))
                                                                                      next-override))
                                                                              (conj override-node-chain source)
                                                                              override-chain)]
                                        (not-empty (into [] (mapcat (partial lifted-target-arcs this override-chain override-node-chain conflicting-overrides-chain)) explicit-arcs)))))
                              cat)
                            explicit-arcs+override-chains)]
      ;; Lifted arcs are now valid outgoing arcs from node-id label. But we're still missing
      ;; some possible targets reachable by following the branches from the respective targets as long
      ;; as there are no explicit incoming arcs and no "higher" override node of the source in the reached
      ;; target node override.
      #_(when (seq lifted-arcs) (assert (every? #(= (:source %) (:source (first lifted-arcs))) lifted-arcs)))
      (propagate-target-arcs this lifted-arcs)))

  (sources [this node-id] (mapv gt/head (inputs this node-id)))
  (sources [this node-id label] (mapv gt/head (inputs this node-id label)))

  (targets [this node-id] (mapv gt/tail (outputs this node-id)))
  (targets [this node-id label] (mapv gt/tail (outputs this node-id label)))

  (add-node
    [this node]
    (let [node-id (gt/node-id node)
          gid     (gt/node-id->graph-id node-id)
          graph   (add-node (get graphs gid) node-id node)]
      [(update this :graphs assoc gid graph) node]))

  (delete-node
    [this node-id]
    (basis-remove-node this node-id (-> this (gt/node-by-id-at node-id) gt/original)))

  (replace-node
    [this node-id new-node]
    (let [gid      (gt/node-id->graph-id node-id)
          new-node (assoc new-node :_node-id node-id)
          graph    (assoc-in (get graphs gid) [:nodes node-id] new-node)]
      [(update this :graphs assoc gid graph) new-node]))

  (replace-override
    [this override-id new-override]
    (let [gid      (gt/override-id->graph-id override-id)]
      (update-in this [:graphs gid :overrides] assoc override-id new-override)))

  (override-node
    [this original-node-id override-node-id]
    (let [gid      (gt/node-id->graph-id override-node-id)]
      (update-in this [:graphs gid :node->overrides original-node-id] util/conjv override-node-id)))

  (override-node-clear [this original-id-id]
    (let [gid (gt/node-id->graph-id original-id-id)]
      (update-in this [:graphs gid :node->overrides] dissoc original-id-id)))

  (add-override
    [this override-id override]
    (let [gid (gt/override-id->graph-id override-id)]
      (update-in this [:graphs gid :overrides] assoc override-id override)))

  (delete-override
    [this override-id]
    (let [gid (gt/override-id->graph-id override-id)]
      (update-in this [:graphs gid :overrides] dissoc override-id)))

  (connect
    [this src-id src-label tgt-id tgt-label]
    (let [src-gid       (gt/node-id->graph-id src-id)
          src-graph     (get graphs src-gid)
          tgt-gid       (gt/node-id->graph-id tgt-id)
          tgt-graph     (get graphs tgt-gid)
          tgt-node      (node-id->node tgt-graph tgt-id)
          tgt-type      (gt/node-type tgt-node this)]
      (assert (<= (:_volatility src-graph 0) (:_volatility tgt-graph 0)))
      (assert (in/has-input? tgt-type tgt-label) (str "No label " tgt-label " exists on node " tgt-node))
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
    (let [src-graph (node-id->graph this src-id)
          targets (gt/targets this src-id src-label)]
      (some #{[tgt-id tgt-label]} targets)))

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
  (let [gid (gt/override-id->graph-id override-id)]
    (get-in basis [:graphs gid :overrides override-id :traverse-fn])))

(defn- sarcs->arcs [sarcs]
  (into #{}
        (comp (mapcat vals)
              cat)
        (vals sarcs)))

(defn hydrate-after-undo
  [basis graph-state]
  (let [old-sarcs (get graph-state :sarcs)
        sarcs (rebuild-sarcs basis graph-state)
        sarcs-diff (clojure.set/difference (sarcs->arcs sarcs) (sarcs->arcs old-sarcs))
        gid (:_gid graph-state)
        new-basis (update basis :graphs assoc gid (assoc graph-state :sarcs sarcs))]
    {:basis new-basis
     :outputs-to-refresh (mapv (juxt :source :sourceLabel) sarcs-diff)}))

(defn- input-deps [basis node-id]
  (some-> (gt/node-by-id-at basis node-id)
    (gt/node-type basis)
    in/input-dependencies))

(defn- update-graph-successors [old-successors basis gid graph changes]
  (let [node-id->overrides (or (:node->overrides graph) (constantly nil))
        ;; Transducer to collect override-id's
        override-id-xf (keep #(some->> %
                                (node-id->node graph)
                                (gt/override-id)))
        changes+old-node-successors (mapv (fn [[node-id labels]]
                                            [node-id labels (old-successors node-id)])
                                          changes)
        [new-successors removed-successors] (r/fold
                                              (fn combinef
                                                ([] [{} []])
                                                ([a b] [(merge (first a) (first b))
                                                        (into (second a) (second b))]))
                                              (fn reducef
                                                ([] [{} []])
                                                ([[new-acc remove-acc] [node-id labels old-node-successors]]
                                                 (let [new-node-successors (if-let [node (gt/node-by-id-at basis node-id)]
                                                                             (let [;; Support data and functions
                                                                                   node-type (gt/node-type node basis)
                                                                                   deps-by-label (or (in/input-dependencies node-type) {})
                                                                                   node-and-overrides (tree-seq (constantly true) node-id->overrides node-id)
                                                                                   override-filter-fn (complement (into #{} override-id-xf node-and-overrides))
                                                                                   overrides (node-id->overrides node-id)
                                                                                   labels (or labels (in/output-labels node-type))
                                                                                   arcs-by-head (if (> (count labels) 1)
                                                                                                  (let [arcs (gt/arcs-by-head basis node-id)
                                                                                                        arcs-by-source-label (group-by #(.sourceLabel ^Arc %) arcs)]
                                                                                                    (fn [_ _ label]
                                                                                                      (arcs-by-source-label label)))
                                                                                                  (fn [basis node-id label]
                                                                                                    (gt/arcs-by-head basis node-id label)))]
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
                                                                                                                       (let [tgt-id (.target arc)
                                                                                                                             tgt-label (.targetLabel arc)]
                                                                                                                         (when-let [dep-labels (get (input-deps basis tgt-id) tgt-label)]
                                                                                                                           [tgt-id dep-labels]))))
                                                                                                               conj!
                                                                                                               deps
                                                                                                               (arcs-by-head basis node-id label))]
                                                                                           (assoc new-node-successors label (persistent! deps))))
                                                                                       (or old-node-successors {})
                                                                                       labels))
                                                                             nil)]
                                                   (if (nil? new-node-successors)
                                                     [new-acc (conj remove-acc node-id)]
                                                     [(assoc new-acc node-id new-node-successors) remove-acc]))))
                                              changes+old-node-successors)]
    (apply dissoc (merge old-successors new-successors) removed-successors)))

(defn update-successors
  [basis changes]
  ;; changes = {node-id #{outputs?}]}
  (reduce (fn [basis [gid changes]]
            (if-let [graph (get (:graphs basis) gid)]
              (update-in basis [:graphs gid :successors] update-graph-successors basis gid graph changes)
              basis))
          basis
          (group-by (comp gt/node-id->graph-id first) changes)))
