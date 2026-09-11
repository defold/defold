;; Copyright 2020-2026 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;;
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;;
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns internal.graph
  (:require [clojure.data.int-map :as int-map]
            [internal.graph.types :as gt]
            [internal.util :as util]
            [util.array :as array]
            [util.coll :as coll :refer [pair]]
            [util.defonce :as defonce]
            [util.eduction :as e]
            [util.pkid-vector :as pkid-vector])
  (:import [clojure.lang IPersistentSet Indexed]
           [com.github.benmanes.caffeine.cache Cache Caffeine]
           [internal.graph.types Arc Endpoint]
           [java.util ArrayList]
           [java.util.concurrent ConcurrentHashMap ForkJoinPool TimeUnit]))

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

(defonce ^:private unassigned-sentinel (Object.))

(def ^:private empty-arc-table (pkid-vector/pkid-vector))
(def ^:private empty-override-node-id-table (pkid-vector/pkid-vector))

(defn override-node-id-table-next-pkid
  ^long [override-node-id-table]
  (if override-node-id-table
    (pkid-vector/next-pkid override-node-id-table)
    0))

(defn- override-node-id-table-include
  [override-node-id-table pkid->override-node-id]
  {:pre [(map? pkid->override-node-id)]}
  (coll/reduce-kv-> pkid->override-node-id
    (or override-node-id-table empty-override-node-id-table)
    (fn [override-node-id-table pkid override-node-id]
      (pkid-vector/assoc-pkids override-node-id-table [pkid] override-node-id))))

(defn- override-node-id-table-exclude
  [override-node-id-table pkid->override-node-id]
  (when override-node-id-table
    (pkid-vector/dissoc-pkids
      override-node-id-table
      (mapv key pkid->override-node-id))))

(defn- override-node-id-table-locate
  [override-node-id-table selected-override-node-ids]
  (if (or (coll/empty? override-node-id-table)
          (coll/empty? selected-override-node-ids))
    {}
    (-> override-node-id-table
        (pkid-vector/find-pkids-by-value (set selected-override-node-ids))
        (coll/reduce-kv-> {}
          (fn [pkid->override-node-id override-node-id pkids]
            (coll/reduce-> pkids pkid->override-node-id
              (fn [pkid->override-node-id pkid]
                (assoc pkid->override-node-id pkid override-node-id))))))))

(defn- arc-table->pkid-vector [arc-table]
  (if (instance? Arc arc-table)
    (conj empty-arc-table arc-table)
    (or arc-table empty-arc-table)))

(defn- pkid-vector->arc-table [pkid-vector]
  ;; A bare Arc represents the canonical singleton state: one live arc at
  ;; pkid zero, with one as the next pkid. Keep non-canonical singleton tables
  ;; as PkidVectors so their stable pkid history is preserved.
  (if (and (= 1 (count pkid-vector))
           (= 1 (pkid-vector/next-pkid pkid-vector)))
    (nth pkid-vector 0)
    pkid-vector))

(defn arc-table-next-pkid
  ^long [arc-table]
  (cond
    (nil? arc-table) 0
    (instance? Arc arc-table) 1
    :else (pkid-vector/next-pkid arc-table)))

(defn arc-table-arcs [arc-table]
  (cond
    (nil? arc-table) nil
    (instance? Arc arc-table) (array/of arc-table)
    :else (coll/not-empty arc-table)))

(defn- arc-table-assoc-pkids [arc-table arc-pkids arc]
  (-> (arc-table->pkid-vector arc-table)
      (pkid-vector/assoc-pkids arc-pkids arc)
      (pkid-vector->arc-table)))

(defn- arc-table-dissoc-pkids [arc-table arc-pkids]
  (when arc-table
    (-> (arc-table->pkid-vector arc-table)
        (pkid-vector/dissoc-pkids arc-pkids)
        (pkid-vector->arc-table))))

(defn- arc-table-append [arc-table arc]
  (cond
    (nil? arc-table) arc
    (instance? Arc arc-table) (conj (conj empty-arc-table arc-table) arc)
    :else (-> (conj arc-table arc)
              (pkid-vector->arc-table))))

(defn- arc-table-find-arc-pkids [arc-table arc]
  (cond
    (nil? arc-table) []
    (instance? Arc arc-table) (if (= arc-table arc) [0] [])
    :else (pkid-vector/find-pkids arc-table arc)))

(defn- arc-table-find-pkids-by-arc [arc-table arcs]
  (cond
    (nil? arc-table) {}
    (instance? Arc arc-table) (if (contains? arcs arc-table) {arc-table [0]} {})
    :else (pkid-vector/find-pkids-by-value arc-table arcs)))

(defn- source-arc-table [basis arc]
  (-> basis gt/sarcs (get (gt/source-id arc)) (get (gt/source-label arc))))

(defn- target-arc-table [basis arc]
  (-> basis gt/tarcs (get (gt/target-id arc)) (get (gt/target-label arc))))

(defn- update-existing-arc-table [node-id->label->arc-table [node-id label :as node-id+label] arc-table-fn & args]
  (if-let [arc-table (-> node-id->label->arc-table (get node-id) (get label))]
    (assoc-in node-id->label->arc-table node-id+label (apply arc-table-fn arc-table args))
    node-id->label->arc-table))

(defn arcs->tuples [arcs]
  ;; TODO: Get rid of this and expose Arc instances directly.
  (mapv (fn [arc]
          [(gt/source-id arc) (gt/source-label arc) (gt/target-id arc) (gt/target-label arc)])
        arcs))

(defn arc-endpoints-p [p arc]
  (and (p (gt/source-id arc) (gt/source-label arc))
       (p (gt/target-id arc) (gt/target-label arc))))

;; Referentially transparent cache that supports explicit invalidation
;; and on-demand computation.
;; `cache` is an atom for the "current generation" of successors: when
;; invalidated, a new atom + new Successors instance is created.
;; `cache` value is a map: {node-id {output Endpoint/1}}
(defonce/type Successors [cache])

(defn- make-successors
  ^Successors ([] (make-successors {}))
  ^Successors ([m] {:pre [(map? m)]} (->Successors (atom m))))

(defn graph? [value]
  (gt/graph? value))

(defn node-by-id-at [graph node-id]
  (get (gt/nodes graph) node-id))

(defn empty-graph
  []
  (gt/->Graph (int-map/int-map) {} (make-successors) {} 0 nil {} {}))

(defn node-ids [graph] (coll/keys (gt/nodes graph)))
(defn node-values [graph] (coll/vals (gt/nodes graph)))

(defn add-node [graph node-id node] (assoc-in graph [:nodes node-id] node))

(definline node-id->node [graph node-id] `(get (gt/nodes ~graph) ~node-id))

(defn- overrides
  "Returns the node-ids of the override nodes in the graph that directly
  override the specified original-node-id."
  [graph original-node-id]
  (coll/not-empty (get (gt/node->overrides graph) original-node-id)))

(defn- arcs-for-node [node-id->label->arc-table node-id]
  (let [label->arc-table (node-id->label->arc-table node-id)]
    (coll/into-> label->arc-table :eduction
      (map val)
      (mapcat arc-table-arcs))))

(defn transform-node
  [graph node-id f & args]
  (if-let [node (-> graph gt/nodes (get node-id))]
    (assoc-in graph [:nodes node-id] (apply f node args))
    graph))

(defn- assoc-source-arcs-at
  [graph arc source-arc-pkids]
  (update-in graph
             [:sarcs (gt/source-id arc) (gt/source-label arc)]
             arc-table-assoc-pkids
             source-arc-pkids
             arc))

(defn- assoc-target-arcs-at
  [graph arc target-arc-pkids]
  (update-in graph
             [:tarcs (gt/target-id arc) (gt/target-label arc)]
             arc-table-assoc-pkids
             target-arc-pkids
             arc))

(defn- dissoc-source-arcs-at
  [graph arc source-arc-pkids]
  (update graph :sarcs
          update-existing-arc-table
          [(gt/source-id arc) (gt/source-label arc)]
          arc-table-dissoc-pkids
          source-arc-pkids))

(defn- dissoc-target-arcs-at
  [graph arc target-arc-pkids]
  (update graph :tarcs
          update-existing-arc-table
          [(gt/target-id arc) (gt/target-label arc)]
          arc-table-dissoc-pkids
          target-arc-pkids))

(defn- replace-target-arc-at
  [graph target-id target-label arc]
  (assoc-in graph
            [:tarcs target-id target-label]
            (if-not arc
              empty-arc-table
              arc)))

(defn basis-perform-connect-arc-pkids [basis arc source+target-arc-pkids]
  (let [[source-arc-pkids target-arc-pkids] source+target-arc-pkids]
    (cond-> basis
      (coll/not-empty source-arc-pkids)
      (assoc-source-arcs-at arc source-arc-pkids)

      (coll/not-empty target-arc-pkids)
      (assoc-target-arcs-at arc target-arc-pkids))))

(defn basis-perform-disconnect-arc-pkids [basis arc source+target-arc-pkids]
  (let [[source-arc-pkids target-arc-pkids] source+target-arc-pkids]
    (cond-> basis
      (coll/not-empty source-arc-pkids)
      (dissoc-source-arcs-at arc source-arc-pkids)

      (coll/not-empty target-arc-pkids)
      (dissoc-target-arcs-at arc target-arc-pkids))))

(defn override-by-id
  [basis override-id]
  (-> basis gt/overrides (get override-id)))

;; ---------------------------------------------------------------------------
;; Dependency tracing
;; ---------------------------------------------------------------------------

(defn pre-traverse
  "Traverses a graph depth-first preorder from start, succ being
  a function that returns direct successors for the node. Returns a
  vector of node-ids."
  [basis start succ & {:keys [seen] :or {seen #{}}}]
  (loop [stack start
         next []
         seen seen
         result (transient [])]
    (if-let [nxt (first stack)]
      (if (contains? seen nxt)
        (recur (rest stack)
               next
               seen
               result)
        (recur (succ basis nxt)
               (conj next (rest stack))
               (conj seen nxt)
               (conj! result nxt)))
      (if-let [next-stack (peek next)]
        (recur next-stack
               (pop next)
               seen
               result)
        (persistent! result)))))

(defn get-overrides
  "Returns the node-ids of the override nodes that directly override the
  specified original-node-id in the graph."
  [basis original-node-id]
  (overrides basis original-node-id))

(defn override-original [basis node-id]
  (when-let [node (node-by-id-at basis node-id)]
    (gt/original node)))

(defn override-originals [basis node-id]
  (into '() (take-while some? (iterate (partial override-original basis) node-id))))

(defn override-of [graph node-id override-id]
  (let [^Indexed os (overrides graph node-id)
        n (count os)
        nodes (gt/nodes graph)]
    (loop [i 0]
      (if (= i n)
        nil
        (let [override-node-id (.nth os i)]
          (if (= override-id (gt/override-id (nodes override-node-id)))
            override-node-id
            (recur (inc i))))))))

(defn- node-id->arcs [node-id->label->arc-table node-id]
  (-> node-id->label->arc-table
      (arcs-for-node node-id)
      (coll/into-> [])
      (coll/not-empty)))

(defn explicit-arcs-by-source
  ([basis source-id]
   (node-id->arcs (gt/sarcs basis) source-id))
  ([basis source-id source-label]
   (arc-table-arcs (-> basis gt/sarcs (get source-id) (get source-label)))))

(defn explicit-arcs-by-target
  ([basis target-id]
   (node-id->arcs (gt/tarcs basis) target-id))
  ([basis target-id target-label]
   (arc-table-arcs (-> basis gt/tarcs (get target-id) (get target-label)))))

(defn explicit-inputs
  ([basis node-id]
   (explicit-arcs-by-target basis node-id))
  ([basis node-id label]
   (explicit-arcs-by-target basis node-id label)))

(defn explicit-outputs
  ([basis node-id]
   (explicit-arcs-by-source basis node-id))
  ([basis node-id label]
   (explicit-arcs-by-source basis node-id label)))

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
  [basis source-override-chain source-override-node-chain conflicting-source-overrides-chain arc]
  (let [target (gt/target-id arc)
        target-label (gt/target-label arc)]
    (if (coll/empty? source-override-chain)
      [arc]
      (let [^IPersistentSet disallowed-override-ids (first conflicting-source-overrides-chain)
            nodes (gt/nodes basis)
            target-override-node-ids (overrides basis target)]
        (into []
              (comp
                ;; only follow what could make the source override chain
                ;; a subsequence of this target chain - don't traverse
                ;; up the wrong branch
                (remove (fn [target-override-node-id]
                          ;; measurably faster than contains?
                          (.contains disallowed-override-ids (gt/override-id (nodes target-override-node-id)))))
                ;; An explicit arc shadows/blocks implicit arcs
                (filter (fn [target-override-node-id]
                          (coll/not-any?
                            (fn [arc]
                              (node-by-id-at basis (gt/source-id arc)))
                            (explicit-arcs-by-target basis target-override-node-id target-label))))
                ;; Keep lifting, with different remaining chains
                ;; depending on if the current target override matches
                ;; the (current) source.
                (mapcat (fn [target-override-node-id]
                          (let [target-override-id (gt/override-id (nodes target-override-node-id))]
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
                  (let [source (gt/source-id (first explicit-arcs))
                        nodes (gt/nodes basis)
                        ;; conflicting-overrides is to prevent following target
                        ;; overrides along the wrong "branches" (for which there may be another
                        ;; better -earlier- matching source node).
                        conflicting-overrides-chain (mapv (fn [node next-override]
                                                            (disj (into (int-map/int-set)
                                                                        (map #(gt/override-id (nodes %)))
                                                                        (overrides basis node))
                                                                  next-override))
                                                          (conj override-node-chain source)
                                                          override-chain)]
                    (coll/not-empty
                      (coll/into-> explicit-arcs []
                        (mapcat #(lift-source-arc basis override-chain override-node-chain conflicting-overrides-chain %)))))))
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
    (let [source (gt/source-id (first arcs))
          nodes (gt/nodes basis)
          source-overrides (into #{}
                                 (map (comp gt/override-id nodes))
                                 (overrides basis source))]
      ;; Here we can (assert (every? #(= source (:source-id %)) arcs)), but it's too costly to run permanently.
      (loop [arcs arcs
             result arcs]
        (let [propagated-arcs (into []
                                    (mapcat (fn [target-arc]
                                              (let [target (gt/target-id target-arc)
                                                    label (gt/target-label target-arc)]
                                                (into []
                                                      (comp
                                                        (map nodes)
                                                        (keep (fn [target-override-node]
                                                                ;; no better matching override node, and no shadowing explicit arc
                                                                (when (and (not (contains? source-overrides (gt/override-id target-override-node)))
                                                                           (coll/not-any?
                                                                             (fn [arc]
                                                                               (node-by-id-at basis (gt/source-id arc)))
                                                                             (explicit-arcs-by-target basis (gt/node-id target-override-node) label)))
                                                                  (assoc target-arc :target-id (gt/node-id target-override-node))))))
                                                      (overrides basis target)))))
                                    arcs)]
          (if (coll/empty? propagated-arcs)
            result
            (recur propagated-arcs
                   (into result propagated-arcs))))))))

(defn- lift-target-arcs [basis target-id target-override-chain arcs]
  (mapv
    (fn [arc]
      (let [original-source-id (gt/source-id arc)
            source-id (reduce (fn [source-id override-id]
                                (or (override-of basis source-id override-id)
                                    source-id))
                              original-source-id
                              target-override-chain)]
        (if (and (= source-id original-source-id)
                 (= target-id (gt/target-id arc)))
          arc
          (gt/->Arc source-id (gt/source-label arc) target-id (gt/target-label arc)))))
    arcs))

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
  [explicit-arcs-fn basis start-node-id]
  ;; We've tried writing this in a less convoluted fashion, but the performance was not satisfactory. Something like:
  ;; originals (into [] (take-while some?) (iterate (partial override-original this) node-id)) ; override-originals, but in the order we want
  ;; overrides (map (comp gt/override-id (partial node-id->node basis)) originals)
  ;; node-explicit-arcs (map #(explicit-arcs-fn basis %) originals)
  ;; override-node-chains (reductions conj '() originals)
  ;; override-chains (reductions conj '() overrides)
  ;; override-chains+explicit-arcs (map vector override-chains override-node-chains node-explicit-arcs)
  (let [nodes (gt/nodes basis)]
    (loop [node-id start-node-id
           override-chain '()
           override-node-chain '()
           result (transient [])]
      (if-let [node (get nodes node-id)]
        (let [explicit-arcs (explicit-arcs-fn basis node-id)
              result' (if (coll/empty? explicit-arcs)
                        result
                        (conj! result [override-chain override-node-chain explicit-arcs]))]
          (if-let [original (gt/original node)]
            (recur original
                   (conj override-chain (gt/override-id node))
                   (conj override-node-chain node-id)
                   result')
            (persistent! result')))
        (persistent! result)))))

(defn arcs-by-target
  ([basis node-id]
   (let [nodes (gt/nodes basis)]
     (if-not (get nodes node-id)
       []
       (let [override-chains+explicit-arcs
             (loop [node-id node-id
                    override-chain '()
                    seen-inputs #{}
                    result (transient [])]
               (if-let [node (get nodes node-id)]
                 (let [explicit-arcs (coll/into-> (explicit-arcs-by-target basis node-id) []
                                       (remove (comp seen-inputs gt/target-label))
                                       (filter (fn [arc]
                                                 (node-by-id-at basis (gt/source-id arc)))))
                       result' (if (coll/empty? explicit-arcs)
                                 result
                                 (conj! result (pair override-chain explicit-arcs)))]
                   (if-let [original (gt/original node)]
                     (recur original
                            (conj override-chain (gt/override-id node))
                            (into seen-inputs (map gt/target-label) explicit-arcs)
                            result')
                     (persistent! result')))
                 (persistent! result)))]
         (coll/into-> override-chains+explicit-arcs []
           (mapcat (fn [override-chain+explicit-arcs]
                     (let [override-chain (key override-chain+explicit-arcs)
                           explicit-arcs (val override-chain+explicit-arcs)]
                       (lift-target-arcs basis node-id override-chain explicit-arcs)))))))))
  ([basis node-id label]
   (let [nodes (gt/nodes basis)]
     (if-not (get nodes node-id)
       []
       (let [override-chain+explicit-arcs
             (loop [node-id node-id
                    chain '()]
               (if-let [node (get nodes node-id)]
                 (let [arcs (coll/into-> (explicit-arcs-by-target basis node-id label) []
                              (filter (fn [arc]
                                        (node-by-id-at basis (gt/source-id arc)))))
                       original (gt/original node)]
                   (if (and original (coll/empty? arcs))
                     (recur original (conj chain (gt/override-id node)))
                     (pair chain arcs)))
                 (pair chain [])))

             override-chain (key override-chain+explicit-arcs)
             explicit-arcs (val override-chain+explicit-arcs)]
         (lift-target-arcs basis node-id override-chain explicit-arcs))))))

(defn arcs-by-source
  ([basis node-id]
   (if-not (node-by-id-at basis node-id)
     []
     (let [;; Traverse original chain, collect explicit arcs from the
           ;; original + the override chain + override node chain from
           ;; that original to basis node.
           override-chains+explicit-arcs
           (collect-override-chains+explicit-arcs
             (fn [basis node-id]
               (coll/into-> (explicit-arcs-by-source basis node-id) []
                 (filter (fn [arc]
                           (node-by-id-at basis (gt/target-id arc))))))
             basis
             node-id)
           ;; Looking at the arcs we found, what arcs to new targets
           ;; are implied by following the override chains
           ;; at most up to basis node?
           lifted-arcs (lift-source-arcs basis override-chains+explicit-arcs)]
       ;; Lifted arcs are now valid outgoing arcs from node-id label. But we're still missing
       ;; some possible targets reachable by following the branches from the respective targets as long
       ;; as there are no explicit incoming arcs and no "higher" override node of the source in the reached
       ;; target node override.
       ;; Here we can (when (seq lifted-arcs) (assert (every? #(= (:source %) (:source (first lifted-arcs))) lifted-arcs)))
       (or (propagate-source-arcs basis lifted-arcs)
           []))))
  ([basis node-id label]
   (if-not (node-by-id-at basis node-id)
     []
     (let [override-chains+explicit-arcs
           (collect-override-chains+explicit-arcs
             (fn [basis node-id]
               (coll/into-> (explicit-arcs-by-source basis node-id label) []
                 (filter (fn [arc]
                           (node-by-id-at basis (gt/target-id arc))))))
             basis
             node-id)
           lifted-arcs (lift-source-arcs basis override-chains+explicit-arcs)]
       (or (propagate-source-arcs basis lifted-arcs)
           [])))))

(defn inputs
  ([basis node-id]
   (arcs-by-target basis node-id))
  ([basis node-id label]
   (arcs-by-target basis node-id label)))

(defn outputs
  ([basis node-id]
   (arcs-by-source basis node-id))
  ([basis node-id label]
   (arcs-by-source basis node-id label)))

(defn cascade-delete-sources
  "Successors function for use with pre-traverse that produces all the node ids
  that will be deleted along with the original node. Duplicates produced by this
  function will be discarded by pre-traverse."
  [basis node-id]
  (when-some [node (node-by-id-at basis node-id)]
    (let [override-id (gt/override-id node)]
      (loop [inputs (some-> node gt/node-type deref :cascade-deletes)
             result (coll/into-> (get-overrides basis node-id) [])]
        (if-some [input (first inputs)]
          (let [explicit (coll/into-> (explicit-arcs-by-target basis node-id input) :eduction
                           (keep (fn [arc]
                                   (let [source-id (gt/source-id arc)]
                                     (when (node-by-id-at basis source-id)
                                       source-id)))))
                implicit (coll/into-> (arcs-by-target basis node-id input) :eduction
                           (keep (fn [arc]
                                   (let [source-id (gt/source-id arc)]
                                     (when-let [node (node-by-id-at basis source-id)]
                                       (when (= override-id (gt/override-id node))
                                         source-id))))))]
            (recur (rest inputs)
                   (-> result
                       (into explicit)
                       (into implicit))))
          result)))))

(defn- invalidate-graph-successors
  ^Successors [^Successors successors changes]
  ;; changes = vector of pairs: node-id + #{output-label ...}|nil
  ;; when changes val is nil, it means that every output was invalidated
  (let [current-state @(.-cache successors)]
    (make-successors
      (persistent!
        (reduce
          (fn [acc e]
            (let [node-id (key e)
                  output->endpoints (acc node-id ::not-found)]
              (if (identical? ::not-found output->endpoints)
                acc
                (let [outputs (val e)]
                  (if (nil? outputs)
                    (dissoc! acc node-id)
                    (let [output->endpoints (reduce dissoc! (transient output->endpoints) outputs)]
                      (if (zero? (count output->endpoints))
                        (dissoc! acc node-id)
                        (assoc! acc node-id (persistent! output->endpoints)))))))))
          (transient current-state)
          changes)))))

(defn- input-deps [basis node-id]
  (some-> (node-by-id-at basis node-id) gt/node-type deref :input-dependencies))

(def ^:private empty-endpoints-array (array/empty-of-type Endpoint))

(defn- query-successors
  "The purpose of this fn is to return an array of endpoints that can be reached
  from the incoming changes (node-id + output)
   For a specific node-id-a + output-x, add:
     the internal input-dependencies, i.e. outputs consuming the given output
     the closest override-nodes, i.e. override-node-a + output-x, as they can be potential dependents
     all connected nodes, where node-id-a + output-x => [[node-id-b + input-y] ...] => [[node-id-b + output+z] ...]"
  [^Successors successors basis node-id label]
  (let [cache (.-cache successors)]
    (if-let [cached-value (-> @cache (get node-id) (get label))]
      cached-value
      (let [result (if-let [node (node-id->node basis node-id)]
                     (let [node-type (gt/node-type node)
                           overrides (get (gt/node->overrides basis) node-id)
                           deps-by-label (:input-dependencies @node-type)
                           dep-labels (get deps-by-label label)
                           outgoing-arcs (arcs-by-source basis node-id label)
                           deps (ArrayList. (int (+ (count dep-labels)
                                                    (count overrides)
                                                    (* (long 10) (count outgoing-arcs)))))]

                       ;; The internal dependent outputs.
                       (doseq [dep-label dep-labels]
                         (.add deps (gt/endpoint node-id dep-label)))

                       ;; The closest overrides.
                       (doseq [override-node-id overrides]
                         (.add deps (gt/endpoint override-node-id label)))

                       ;; The connected nodes and their outputs.
                       (doseq [outgoing-arc outgoing-arcs
                               :let [target-id (gt/target-id outgoing-arc)
                                     target-label (gt/target-label outgoing-arc)]
                               dep-label (get (input-deps basis target-id) target-label)]
                         (.add deps (gt/endpoint target-id dep-label)))

                       (if (.isEmpty deps)
                         empty-endpoints-array
                         (.toArray deps ^Endpoint/1 empty-endpoints-array)))
                     empty-endpoints-array)]
        (swap! cache update node-id assoc label result)
        result))))

(defn successors
  "Public only for tests and introspection tooling. Implementation detail."
  [basis node-id label]
  (query-successors
    (gt/successors basis)
    basis
    node-id
    label))

(def ^:private ^Cache basis-dependencies-cache
  (-> (Caffeine/newBuilder)
      (.expireAfterAccess 10 TimeUnit/SECONDS)
      (.maximumSize 32)
      (.build)))

(defn dependencies [basis endpoints]
  (assert (coll/every? gt/endpoint? endpoints))
  (if (coll/empty? endpoints)
    #{}
    (let [node-successors (gt/successors basis)
          cache-key (pair endpoints (System/identityHashCode node-successors))]
      (.get basis-dependencies-cache
            cache-key
            (fn [_]
              (let [pool (ForkJoinPool/commonPool)
                    all-endpoints (ConcurrentHashMap.)
                    make-task! (fn [endpoints]
                                 (fn []
                                   (into []
                                         (mapcat
                                           (fn [endpoint]
                                             (when (nil? (.putIfAbsent all-endpoints endpoint true))
                                               (let [node-id (gt/endpoint-node-id endpoint)
                                                     output (gt/endpoint-label endpoint)]
                                                 (query-successors node-successors basis node-id output)))))
                                         endpoints)))
                    endpoints->tasks-xf (comp (partition-all 512) (map make-task!))
                    future->tasks-xf (comp (mapcat deref) endpoints->tasks-xf)]
                (loop [tasks (into [] endpoints->tasks-xf endpoints)]
                  (let [next-tasks
                        (if (= 1 (count tasks))
                          (into [] endpoints->tasks-xf ((nth tasks 0)))
                          (into [] future->tasks-xf (.invokeAll pool tasks)))]
                    (when (pos? (count next-tasks))
                      (recur next-tasks))))
                (.keySet all-endpoints)))))))

(defn sources
  ([basis node-id] (mapv gt/source (inputs basis node-id)))
  ([basis node-id label] (mapv gt/source (inputs basis node-id label))))

(defn targets
  ([basis node-id] (mapv gt/target (outputs basis node-id)))
  ([basis node-id label] (mapv gt/target (outputs basis node-id label))))

(defn connected?
  [basis source-id source-label target-id target-label]
  (let [targets (targets basis source-id source-label)]
    (coll/any? #{[target-id target-label]} targets)))

(defn make-override [root-id traverse-fn init-props-fn]
  {:root-id root-id
   :traverse-fn traverse-fn
   :init-props-fn init-props-fn})

(defn override-traverse-fn [basis override-id]
  (-> basis gt/overrides (get override-id) :traverse-fn))

(defn update-successors
  [basis changes]
  ;; changes = {node-id #{output-label ...}|nil}
  ;; when changes value is nil, it means that every output was invalidated
  (update basis :successors invalidate-graph-successors changes))

(defn invalidate-all-successors [basis]
  (assoc basis :successors (make-successors)))

;; ---------------------------------------------------------------------------
;; Basis manipulation
;; ---------------------------------------------------------------------------

;; Basis manipulation is split into plan, perform and revert functions. The plan
;; function gathers all the data required to perform and revert the change. It's
;; also a good place to perform validation before the changes are performed.
;; Note that the plan functions should guard against the possibility of their
;; subjects being removed by earlier transaction steps. In this case, they
;; should return `nil`, and the caller is responsible for skipping the change.
;; Ideally, the perform and revert functions should also be resilient against
;; their subjects no longer existing in the graph to allow changes from
;; different undo stacks to operate on the same subjects.

(defn- find-arc-pkids [basis arc]
  (let [source-arc-pkids (arc-table-find-arc-pkids (source-arc-table basis arc) arc)
        target-arc-pkids (arc-table-find-arc-pkids (target-arc-table basis arc) arc)]
    (pair source-arc-pkids target-arc-pkids)))

(defn- find-arc-pkids-by-endpoint
  [node-id->label->arc-table endpoint-fn arcs]
  (coll/reduce-kv-> (util/group-into {} #{} endpoint-fn arcs) {}
    (fn [arc->pkids [node-id label] arcs]
      (let [arc-table (-> node-id->label->arc-table (get node-id) (get label))]
        (into arc->pkids (arc-table-find-pkids-by-arc arc-table arcs))))))

(defn- find-connected-arc-pkids [basis node-ids]
  (let [arcs (coll/into->
               (pair (e/mapcat #(explicit-arcs-by-source basis %) node-ids)
                     (e/mapcat #(explicit-arcs-by-target basis %) node-ids))
               #{}
               cat)
        source-arc->pkids (find-arc-pkids-by-endpoint (gt/sarcs basis) gt/source arcs)
        target-arc->pkids (find-arc-pkids-by-endpoint (gt/tarcs basis) gt/target arcs)]
    (coll/into-> arcs {}
      (map (fn [arc]
             (pair arc (pair (get source-arc->pkids arc [])
                             (get target-arc->pkids arc []))))))))

(defn basis-plan-add-override
  [_basis override-id root-id traverse-fn init-props-fn]
  {:override-id override-id
   :override (make-override root-id traverse-fn init-props-fn)})

(defn basis-perform-add-override
  [basis override-id override]
  (assoc-in basis [:overrides override-id] override))

(defn basis-revert-add-override
  [basis override-id]
  (update basis :overrides dissoc override-id))

(defn basis-plan-add-nodes
  [basis added-nodes]
  (when (coll/not-empty added-nodes)
    (let [node-ids (mapv gt/node-id added-nodes)]
      (assert (coll/every? gt/node-id? node-ids))
      {:added-nodes added-nodes
       :introduced-node-id->pkid->override-node-id
       (coll/reduce-> added-nodes {}
         (fn [introduced-node-id->pkid->override-node-id node]
           (if-let [original-node-id (gt/original node)]
             (let [node-id (gt/node-id node)
                   override-node-ids (-> basis gt/node->overrides (get original-node-id))
                   introduced-pkid->override-node-id (introduced-node-id->pkid->override-node-id original-node-id)
                   pkid (+ (override-node-id-table-next-pkid override-node-ids)
                           (count introduced-pkid->override-node-id))]

               (update introduced-node-id->pkid->override-node-id original-node-id assoc pkid node-id))
             introduced-node-id->pkid->override-node-id)))})))

(defn basis-perform-add-nodes
  [basis added-nodes introduced-node-id->pkid->override-node-id]
  (-> basis
      (assoc :nodes
        (persistent!
          (coll/reduce-> added-nodes (transient (gt/nodes basis))
            (fn [nodes node]
              (assoc! nodes (gt/node-id node) node)))))
      (cond->
        (coll/not-empty introduced-node-id->pkid->override-node-id)
        (assoc :node->overrides
          (persistent!
            (coll/reduce-kv-> introduced-node-id->pkid->override-node-id (transient (gt/node->overrides basis))
              (fn [node->overrides node-id pkid->override-node-id]
                (assoc! node->overrides node-id
                        (override-node-id-table-include (get node->overrides node-id) pkid->override-node-id)))))))))

(defn basis-revert-add-nodes
  [basis added-nodes introduced-node-id->pkid->override-node-id]
  (-> basis
      (assoc :nodes (coll/reduce-> added-nodes (gt/nodes basis) (map gt/node-id) dissoc))
      (cond->
        (coll/not-empty introduced-node-id->pkid->override-node-id)
        (assoc :node->overrides
          (persistent!
            (coll/reduce-kv-> introduced-node-id->pkid->override-node-id (transient (gt/node->overrides basis))
              (fn [node->overrides node-id pkid->override-node-id]
                (assoc! node->overrides node-id
                        (override-node-id-table-exclude (get node->overrides node-id) pkid->override-node-id)))))))))

(defn basis-plan-clear-override-nodes
  [basis original-node-id cleared-override-node-ids]
  (when-not (coll/empty? cleared-override-node-ids)
    (let [override-node-ids (-> basis gt/node->overrides (get original-node-id))
          removed-pkid->override-node-id (override-node-id-table-locate override-node-ids cleared-override-node-ids)]
      (when (coll/not-empty removed-pkid->override-node-id)
        {:original-node-id original-node-id
         :removed-pkid->override-node-id removed-pkid->override-node-id}))))

(defn basis-perform-clear-override-nodes
  [basis original-node-id removed-pkid->override-node-id]
  (update-in
    basis [:node->overrides original-node-id]
    override-node-id-table-exclude removed-pkid->override-node-id))

(defn basis-revert-clear-override-nodes
  [basis original-node-id removed-pkid->override-node-id]
  (let [restored-pkid->override-node-id
        (coll/into-> removed-pkid->override-node-id {}
          (filter
            (fn [[_pkid override-node-id]]
              (= original-node-id
                 (some-> (node-by-id-at basis override-node-id)
                         gt/original)))))]
    (if (coll/empty? restored-pkid->override-node-id)
      basis
      (update-in
        basis [:node->overrides original-node-id]
        override-node-id-table-include restored-pkid->override-node-id))))

(defn basis-plan-replace-override
  [basis override-id new-override]
  (when-let [old-override (override-by-id basis override-id)]
    {:override-id override-id
     :old-override old-override
     :new-override new-override}))

(defn basis-perform-replace-override
  [basis override-id new-override]
  (if-not (override-by-id basis override-id)
    basis
    (assoc-in basis [:overrides override-id] new-override)))

(defn basis-revert-replace-override
  [basis override-id old-override]
  (if-not (override-by-id basis override-id)
    basis
    (assoc-in basis [:overrides override-id] old-override)))

(defn- basis-set-override-node-original
  [basis override-node-id original-node-id]
  (if-let [override-node (node-by-id-at basis override-node-id)]
    (assoc-in
      basis [:nodes override-node-id]
      (gt/set-original override-node original-node-id))
    basis))

(defn basis-plan-repoint-override-node
  [basis override-node-id new-original-node-id]
  (when-let [override-node (node-by-id-at basis override-node-id)]
    (let [override-node-ids (-> basis gt/node->overrides (get new-original-node-id))]
      {:override-node-id override-node-id
       :old-original-node-id (gt/original override-node)
       :new-original-node-id new-original-node-id
       :new-original-pkid (override-node-id-table-next-pkid override-node-ids)})))

(defn basis-perform-repoint-override-node
  [basis override-node-id new-original-node-id new-original-pkid]
  (if-not (node-by-id-at basis override-node-id)
    basis
    (-> basis
        (basis-set-override-node-original override-node-id new-original-node-id)
        (update-in
          [:node->overrides new-original-node-id]
          override-node-id-table-include {new-original-pkid override-node-id}))))

(defn basis-revert-repoint-override-node
  [basis override-node-id old-original-node-id new-original-node-id new-original-pkid]
  (if-not (node-by-id-at basis override-node-id)
    basis
    (-> basis
        (basis-set-override-node-original override-node-id old-original-node-id)
        (update-in
          [:node->overrides new-original-node-id]
          override-node-id-table-exclude {new-original-pkid override-node-id}))))

(defn- basis-set-raw-property-state
  [basis node property-label raw-value]
  (let [node-id (gt/node-id node)
        new-node (if (identical? unassigned-sentinel raw-value)
                   (if (gt/original node)
                     (gt/clear-property node basis property-label)
                     (dissoc node property-label))
                   (gt/set-property node basis property-label raw-value))]
    (assoc-in basis [:nodes node-id] new-node)))

(defn basis-plan-set-raw-property
  [basis node-id property-label new-raw-value]
  (when-let [node (node-by-id-at basis node-id)]
    (let [assigned-properties (gt/assigned-properties node)
          old-raw-value (get assigned-properties property-label unassigned-sentinel)]
      (when (not= old-raw-value new-raw-value)
        {:node-id node-id
         :property-label property-label
         :old-raw-value old-raw-value
         :new-raw-value new-raw-value}))))

(defn basis-perform-set-raw-property
  [basis node-id property-label new-raw-value]
  (if-let [node (node-by-id-at basis node-id)]
    (basis-set-raw-property-state basis node property-label new-raw-value)
    basis))

(defn basis-revert-set-raw-property
  [basis node-id property-label old-raw-value]
  (if-let [node (node-by-id-at basis node-id)]
    (basis-set-raw-property-state basis node property-label old-raw-value)
    basis))

(defn basis-plan-clear-raw-property
  [basis node-id property-label]
  (when-let [node (node-by-id-at basis node-id)]
    (let [assigned-properties (gt/assigned-properties node)
          old-raw-value (get assigned-properties property-label unassigned-sentinel)]
      (when (not (identical? unassigned-sentinel old-raw-value))
        {:node-id node-id
         :property-label property-label
         :old-raw-value (get assigned-properties property-label unassigned-sentinel)}))))

(defn basis-perform-clear-raw-property
  [basis node-id property-label]
  (if-let [node (node-by-id-at basis node-id)]
    (assoc-in basis [:nodes node-id] (gt/clear-property node basis property-label))
    basis))

(defn basis-revert-clear-raw-property
  [basis node-id property-label old-raw-value]
  (if-let [node (node-by-id-at basis node-id)]
    (basis-set-raw-property-state basis node property-label old-raw-value)
    basis))

(defn basis-plan-update-graph-value
  [basis graph-value-key update-fn args]
  (let [graph-values (-> basis gt/graph-values)
        old-value (get graph-values graph-value-key unassigned-sentinel)
        new-value (apply update-fn
                         (if (identical? unassigned-sentinel old-value)
                           nil
                           old-value)
                         args)]
    {:graph-value-key graph-value-key
     :old-value old-value
     :new-value new-value}))

(defn basis-perform-update-graph-value
  [basis graph-value-key new-value]
  (assoc-in basis [:graph-values graph-value-key] new-value))

(defn basis-revert-update-graph-value
  [basis graph-value-key old-value]
  (if (identical? unassigned-sentinel old-value)
    (update basis :graph-values dissoc graph-value-key)
    (assoc-in basis [:graph-values graph-value-key] old-value)))

(defn basis-plan-replace-arc
  [basis old-arc new-arc]
  (let [source-id (gt/source-id new-arc)
        target-id (gt/target-id new-arc)]
    (when (and (node-by-id-at basis source-id)
               (node-by-id-at basis target-id))
      (let [source-label (gt/source-label new-arc)
            old-source-arc-pkids (when old-arc
                                   (arc-table-find-arc-pkids
                                     (source-arc-table basis old-arc)
                                     old-arc))

            source-arc-pkid
            (if (and (= source-id (some-> old-arc gt/source-id))
                     (= source-label (some-> old-arc gt/source-label))
                     (coll/not-empty old-source-arc-pkids))
              (nth old-source-arc-pkids 0)
              (arc-table-next-pkid (source-arc-table basis new-arc)))]
        {:new-arc new-arc
         :new-source-arc-pkids (int-map/int-set [source-arc-pkid])
         :old-arc old-arc
         :old-source-arc-pkids old-source-arc-pkids}))))

(defn basis-perform-replace-arc
  [basis old-arc old-source-arc-pkids new-arc new-source-arc-pkids]
  (let [target-arc (or new-arc old-arc)]
    (-> basis
        (cond->
          (and old-arc (coll/not-empty old-source-arc-pkids))
          (dissoc-source-arcs-at old-arc old-source-arc-pkids)

          (and new-arc (coll/not-empty new-source-arc-pkids))
          (assoc-source-arcs-at new-arc new-source-arc-pkids))
        (replace-target-arc-at (gt/target-id target-arc) (gt/target-label target-arc) new-arc))))

(defn basis-plan-connect-arc
  [basis arc]
  (when (and (node-by-id-at basis (gt/source-id arc))
             (node-by-id-at basis (gt/target-id arc)))
    (let [source-arc-pkid (arc-table-next-pkid (source-arc-table basis arc))
          target-arc-pkid (arc-table-next-pkid (target-arc-table basis arc))]
      {:arc->source+target-pkids
       {arc (pair (int-map/int-set [source-arc-pkid])
                  (int-map/int-set [target-arc-pkid]))}})))

(defn basis-perform-append-arc
  [basis arc]
  (let [source-id (gt/source-id arc)
        target-id (gt/target-id arc)]
    (if (and (node-by-id-at basis source-id)
             (node-by-id-at basis target-id))
      (-> basis
          (update-in [:sarcs source-id (gt/source-label arc)] arc-table-append arc)
          (update-in [:tarcs target-id (gt/target-label arc)] arc-table-append arc))
      basis)))

(defn basis-perform-connect-arcs
  [basis arc->source+target-pkids]
  (if (coll/empty? arc->source+target-pkids)
    basis
    (assoc basis
      :sarcs
      (persistent!
        (coll/reduce-kv-> arc->source+target-pkids (transient (gt/sarcs basis))
          (fn [sarcs arc [source-pkids _target-pkids]]
            (if (coll/empty? source-pkids)
              sarcs
              (let [source-id (gt/source-id arc)
                    source-label (gt/source-label arc)
                    label->arc-table (get sarcs source-id)]
                (assoc! sarcs source-id
                        (assoc label->arc-table source-label
                               (arc-table-assoc-pkids (get label->arc-table source-label) source-pkids arc))))))))
      :tarcs
      (persistent!
        (coll/reduce-kv-> arc->source+target-pkids (transient (gt/tarcs basis))
          (fn [tarcs arc [_source-pkids target-pkids]]
            (if (coll/empty? target-pkids)
              tarcs
              (let [target-id (gt/target-id arc)
                    target-label (gt/target-label arc)
                    label->arc-table (get tarcs target-id)]
                (assoc! tarcs target-id
                        (assoc label->arc-table target-label
                               (arc-table-assoc-pkids (get label->arc-table target-label) target-pkids arc)))))))))))

(defn basis-revert-connect-arcs
  [basis arc->source+target-pkids]
  (if (coll/empty? arc->source+target-pkids)
    basis
    (assoc basis
      :sarcs
      (persistent!
        (coll/reduce-kv-> arc->source+target-pkids (transient (gt/sarcs basis))
          (fn [sarcs arc [source-pkids _target-pkids]]
            (if (coll/empty? source-pkids)
              sarcs
              (let [source-id (gt/source-id arc)
                    source-label (gt/source-label arc)
                    label->arc-table (get sarcs source-id)]
                (if-let [arc-table (get label->arc-table source-label)]
                  (assoc! sarcs source-id
                          (assoc label->arc-table source-label
                                 (arc-table-dissoc-pkids arc-table source-pkids)))
                  sarcs))))))
      :tarcs
      (persistent!
        (coll/reduce-kv-> arc->source+target-pkids (transient (gt/tarcs basis))
          (fn [tarcs arc [_source-pkids target-pkids]]
            (if (coll/empty? target-pkids)
              tarcs
              (let [target-id (gt/target-id arc)
                    target-label (gt/target-label arc)
                    label->arc-table (get tarcs target-id)]
                (if-let [arc-table (get label->arc-table target-label)]
                  (assoc! tarcs target-id
                          (assoc label->arc-table target-label
                                 (arc-table-dissoc-pkids arc-table target-pkids)))
                  tarcs)))))))))

(defn basis-plan-disconnect-arc
  [basis arc]
  (let [source+target-pkids (find-arc-pkids basis arc)]
    (when (coll/any? coll/not-empty source+target-pkids)
      {:arc->source+target-pkids
       {arc source+target-pkids}})))

(defn basis-perform-disconnect-arcs
  [basis arc->source+target-pkids]
  (basis-revert-connect-arcs basis arc->source+target-pkids))

(defn basis-revert-disconnect-arcs
  [basis arc->source+target-pkids]
  (basis-perform-connect-arcs basis arc->source+target-pkids))

(defn basis-plan-delete-nodes
  [basis deleted-node-ids]
  (when (coll/not-empty deleted-node-ids)
    (let [deleted-nodes
          (coll/into-> (pre-traverse basis deleted-node-ids cascade-delete-sources) []
            (keep (fn [node-id]
                    (node-by-id-at basis node-id))))]

      (when (coll/not-empty deleted-nodes)
        (let [[deleted-node-ids deleted-nodes-by-id removed-overrides-by-id]
              (util/into-multiple
                [[] {} {}]
                [(map gt/node-id)
                 (map (coll/pair-fn gt/node-id))
                 (keep (fn [deleted-node]
                         (when-let [override-id (gt/override-id deleted-node)]
                           (when-let [override (override-by-id basis override-id)]
                             (when (= (gt/original deleted-node) (:root-id override))
                               (pair override-id override))))))]
                deleted-nodes)

              removed-arc->source+target-pkids
              (find-connected-arc-pkids basis deleted-node-ids)

              removed-node-id->override-node-ids-for-deleted-node-ids
              (coll/into-> deleted-node-ids {}
                (keep (fn [deleted-node-id]
                        (when-let [override-node-ids (coll/not-empty (-> basis gt/node->overrides (get deleted-node-id)))]
                          (pair deleted-node-id override-node-ids)))))

              removed-node-id->override-node-ids-for-originals-of-deleted-node-ids
              (coll/reduce-> deleted-nodes {}
                (fn [removed-node-id->override-node-ids deleted-node]
                  (let [deleted-node-id (gt/node-id deleted-node)
                        original-node-id (gt/original deleted-node)]
                    (if (or (not original-node-id)
                            (contains? deleted-nodes-by-id original-node-id)) ; Already covered by removed-node-id->override-node-ids-for-deleted-node-ids.
                      removed-node-id->override-node-ids
                      (update removed-node-id->override-node-ids original-node-id coll/conj-vector deleted-node-id)))))

              removed-node-id->override-node-ids
              (coll/merge-with
                coll/into-vector
                removed-node-id->override-node-ids-for-deleted-node-ids
                removed-node-id->override-node-ids-for-originals-of-deleted-node-ids)

              removed-node-id->pkid->override-node-id
              (coll/reduce-kv-> removed-node-id->override-node-ids {}
                (fn [removed-node-id->pkid->override-node-id original-node-id removed-override-node-ids]
                  (let [override-node-ids (-> basis gt/node->overrides (get original-node-id))]
                    (assoc
                      removed-node-id->pkid->override-node-id
                      original-node-id
                      (override-node-id-table-locate override-node-ids removed-override-node-ids)))))]

          {:deleted-nodes deleted-nodes
           :removed-arc->source+target-pkids removed-arc->source+target-pkids
           :removed-overrides-by-id removed-overrides-by-id
           :removed-node-id->pkid->override-node-id removed-node-id->pkid->override-node-id})))))

(defn basis-perform-delete-nodes
  [basis deleted-nodes removed-arc->source+target-pkids removed-overrides-by-id removed-node-id->pkid->override-node-id]
  (-> basis
      (basis-perform-disconnect-arcs removed-arc->source+target-pkids)
      (assoc :nodes (coll/reduce-> deleted-nodes (gt/nodes basis) (map gt/node-id) dissoc))
      (cond->
        (coll/not-empty removed-overrides-by-id)
        (assoc :overrides
          (persistent! (reduce dissoc! (transient (gt/overrides basis)) (coll/keys removed-overrides-by-id))))

        (coll/not-empty removed-node-id->pkid->override-node-id)
        (assoc :node->overrides
          (persistent!
            (coll/reduce-kv-> removed-node-id->pkid->override-node-id (transient (gt/node->overrides basis))
              (fn [node->overrides node-id pkid->override-node-id]
                (assoc! node->overrides node-id
                        (override-node-id-table-exclude (get node->overrides node-id) pkid->override-node-id)))))))))

(defn basis-revert-delete-nodes
  [basis deleted-nodes removed-arc->source+target-pkids removed-overrides-by-id removed-node-id->pkid->override-node-id]
  (-> basis
      (assoc :nodes
        (persistent!
          (coll/reduce-> deleted-nodes (transient (gt/nodes basis))
            (fn [nodes node]
              (assoc! nodes (gt/node-id node) node)))))
      (cond->
        (coll/not-empty removed-overrides-by-id)
        (assoc :overrides
          (persistent! (reduce-kv assoc! (transient (gt/overrides basis)) removed-overrides-by-id)))

        (coll/not-empty removed-node-id->pkid->override-node-id)
        (assoc :node->overrides
          (persistent!
            (coll/reduce-kv-> removed-node-id->pkid->override-node-id (transient (gt/node->overrides basis))
              (fn [node->overrides node-id pkid->override-node-id]
                (assoc! node->overrides node-id
                        (override-node-id-table-include (get node->overrides node-id) pkid->override-node-id)))))))
      (basis-revert-disconnect-arcs removed-arc->source+target-pkids)))
