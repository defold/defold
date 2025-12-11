;; Copyright 2020-2025 The Defold Foundation
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
  (:require [clojure.core.reducers :as r]
            [clojure.data.int-map :as int-map]
            [internal.graph.types :as gt]
            [internal.node :as in]
            [internal.util :as util]
            [util.coll :as coll :refer [pair]]
            [util.defonce :as defonce])
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

(definline ^:private arc
  [source-id source-label target-id target-label]
  `(Arc. ~source-id ~source-label ~target-id ~target-label))

(defn arcs->tuples [arcs]
  ;; TODO: Get rid of this and expose Arc instances directly.
  (mapv (fn [^Arc arc]
          [(.source-id arc) (.source-label arc) (.target-id arc) (.target-label arc)])
        arcs))

(defn arc-endpoints-p [p ^Arc arc]
  (and (p (.source-id arc) (.source-label arc))
       (p (.target-id arc) (.target-label arc))))

(defn empty-graph
  []
  {:nodes (int-map/int-map)
   :sarcs {}
   :successors {}
   :tarcs {}
   :tx-id 0})

(defn node-ids [graph] (keys (:nodes graph)))
(defn node-values [graph] (vals (:nodes graph)))

(defn add-node [graph node-id node] (assoc-in graph [:nodes node-id] node))

(definline node-id->graph [basis node-id] `(get (:graphs ~basis) (gt/node-id->graph-id ~node-id)))
(definline node-id->node [graph node-id] `(get (:nodes ~graph) ~node-id))

(defn- overrides
  "Returns the node-ids of the override nodes in the graph that directly
  override the specified original-node-id."
  [graph original-node-id]
  (get (:node->overrides graph) original-node-id))

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
   (basis-remove-node basis node-id (gt/original-node basis node-id) false))
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
  specified original-node-id in its graph."
  [basis original-node-id]
  (overrides (node-id->graph basis original-node-id) original-node-id))

(defn override-original [basis node-id]
  (when-let [node (gt/node-by-id-at basis node-id)]
    (gt/original node)))

(defn override-originals [basis node-id]
  (into '() (take-while some? (iterate (partial override-original basis) node-id))))

(defn override-of [graph node-id override-id]
  (let [^Indexed os (overrides graph node-id)
        n (count os)
        nodes (:nodes graph)]
    (loop [i 0]
      (if (= i n)
        nil
        (let [override-node-id (.nth os i)]
          (if (= override-id (gt/override-id (nodes override-node-id)))
            override-node-id
            (recur (inc i))))))))

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

(defn cascade-delete-sources
  "Successors function for use with pre-traverse that produces all the node ids
  that will be deleted along with the original node. Duplicates produced by this
  function will be discarded by pre-traverse."
  [basis node-id]
  (when-some [node (gt/node-by-id-at basis node-id)]
    (let [override-id (gt/override-id node)]
      (loop [inputs (some-> node gt/node-type in/cascade-deletes)
             result (vec (get-overrides basis node-id))]
        (if-some [input (first inputs)]
          (let [explicit (map first (explicit-sources basis node-id input))
                implicit (keep (fn [[node-id]]
                                 (when-some [node (gt/node-by-id-at basis node-id)]
                                   (when (= override-id (gt/override-id node))
                                     node-id)))
                               (gt/sources basis node-id input))]
            (recur (rest inputs)
                   (-> result
                       (into explicit)
                       (into implicit))))
          result)))))

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
            target-graph-nodes (:nodes target-graph)
            target-override-node-ids (overrides target-graph target)]
        (into []
              (comp
                ;; only follow what could make the source override chain
                ;; a subsequence of this target chain - don't traverse
                ;; up the wrong branch
                (remove (fn [target-override-node-id]
                          ;; measurably faster than contains?
                          (.contains disallowed-override-ids (gt/override-id (target-graph-nodes target-override-node-id)))))
                ;; An explicit arc shadows/blocks implicit arcs
                (filter (fn [target-override-node-id]
                          (nil? (graph-explicit-arcs-by-target target-graph target-override-node-id target-label))))
                ;; Keep lifting, with different remaining chains
                ;; depending on if the current target override matches
                ;; the (current) source.
                (mapcat (fn [target-override-node-id]
                          (let [target-override-id (gt/override-id (target-graph-nodes target-override-node-id))]
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
                        source-graph-nodes (:nodes source-graph)
                        ;; conflicting-overrides is to prevent following target
                        ;; overrides along the wrong "branches" (for which there may be another
                        ;; better -earlier- matching source node).
                        conflicting-overrides-chain (mapv (fn [node next-override]
                                                            (disj (into (int-map/int-set)
                                                                        (map #(gt/override-id (source-graph-nodes %)))
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
          source-graph-nodes (:nodes source-graph)
          source-overrides (into #{}
                                 (map (comp gt/override-id source-graph-nodes))
                                 (overrides source-graph source))]
      ;; Here we can (assert (every? #(= source (:source-id %)) arcs)), but it's too costly to run permantently.
      (loop [arcs arcs
             result arcs]
        (let [propagated-arcs (into []
                                    (mapcat (fn [^Arc target-arc]
                                              (let [target (.target-id target-arc)
                                                    label (.target-label target-arc)
                                                    target-graph (node-id->graph basis target)
                                                    target-graph-nodes (:nodes target-graph)]
                                                (into []
                                                      (comp
                                                        (map target-graph-nodes)
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
    (let [override-id (first override-chain)]
      (if (nil? override-id)
        (mapv #(assoc % :target-id target-id) arcs)
        (recur (rest override-chain)
               (mapv (fn [^Arc arc]
                       (let [source-id (.source-id arc)]
                         (if-some [source-override-node-id (override-of (node-id->graph basis source-id) source-id override-id)]
                           (assoc arc :source-id source-override-node-id)
                           arc)))
                     arcs))))))

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
  (let [graph-nodes (:nodes graph)]
    (loop [node-id start-node-id
           override-chain '()
           override-node-chain '()
           result (transient [])]
      (let [node (graph-nodes node-id)
            explicit-arcs (explicit-arcs-fn graph node-id)
            result' (if (seq explicit-arcs)
                      (conj! result [override-chain override-node-chain explicit-arcs])
                      result)]
        (if-some [original (gt/original node)]
          (recur original
                 (conj override-chain (gt/override-id node))
                 (conj override-node-chain node-id)
                 result')
          (persistent! result'))))))

(def ^:private ^Cache basis-dependencies-cache
  (-> (Caffeine/newBuilder)
      (.expireAfterAccess 10 TimeUnit/SECONDS)
      (.maximumSize 32)
      (.build)))

(defn- basis-dependencies [basis endpoints]
  (assert (every? gt/endpoint? endpoints))
  (if (coll/empty? endpoints)
    #{}
    (let [graph-id->node-successor-map
          (persistent!
            (reduce-kv
              (fn [acc graph-id graph]
                (assoc! acc graph-id (:successors graph)))
              (transient {})
              (:graphs basis)))
          cache-key (into [endpoints]
                          (map #(System/identityHashCode (val %)))
                          graph-id->node-successor-map)]
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
                                                 (-> node-id
                                                     gt/node-id->graph-id
                                                     graph-id->node-successor-map
                                                     (get node-id)
                                                     (get output))))))
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

(defonce/record MultigraphBasis [graphs]
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
          graph-nodes (:nodes graph)
          override-chains+explicit-arcs (loop [node-id node-id
                                               override-chain '()
                                               seen-inputs #{}
                                               result (transient [])]
                                          (let [node (graph-nodes node-id)
                                                explicit-arcs (remove (comp seen-inputs :target-label) (graph-explicit-arcs-by-target graph node-id))
                                                result' (if (seq explicit-arcs)
                                                          (conj! result (pair override-chain explicit-arcs))
                                                          result)]
                                            (if-some [original (gt/original node)]
                                              (recur original
                                                     (conj override-chain (gt/override-id node))
                                                     (into seen-inputs (map :target-label) explicit-arcs)
                                                     result')
                                              (persistent! result'))))]
      (into []
            (mapcat (fn [override-chain+explicit-arcs]
                      (let [override-chain (key override-chain+explicit-arcs)
                            explicit-arcs (val override-chain+explicit-arcs)]
                        (lift-target-arcs this node-id override-chain explicit-arcs))))
            override-chains+explicit-arcs)))

  (arcs-by-target
    [this node-id label]
    (let [graph (node-id->graph this node-id)

          override-chain+explicit-arcs
          (loop [node-id node-id
                 chain '()]
            (let [arcs (graph-explicit-arcs-by-target graph node-id label)
                  node (gt/node-by-id-at this node-id)
                  original (gt/original node)]
              (if (and original (empty? arcs))
                (recur original (conj chain (gt/override-id node)))
                (pair chain arcs))))

          override-chain (key override-chain+explicit-arcs)
          explicit-arcs (val override-chain+explicit-arcs)]
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
      (update this :graphs assoc graph-id graph)))

  (delete-node
    [this node-id]
    (basis-remove-node this node-id (-> this (gt/node-by-id-at node-id) gt/original)))

  (replace-node
    [this node-id new-node]
    (let [graph-id (gt/node-id->graph-id node-id)
          new-node (assoc new-node :_node-id node-id)
          graph (assoc-in (get graphs graph-id) [:nodes node-id] new-node)]
      (update this :graphs assoc graph-id graph)))

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
          target-node-type (gt/node-type target-node)]
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
    [this endpoints]
    (basis-dependencies this endpoints))

  (original-node [this node-id]
    (when-let [node (gt/node-by-id-at this node-id)]
      (gt/original node))))

(defn multigraph-basis
  [graphs]
  (MultigraphBasis. graphs))

(defn make-override [root-id traverse-fn init-props-fn]
  {:root-id root-id
   :traverse-fn traverse-fn
   :init-props-fn init-props-fn})

(defn override-traverse-fn [basis override-id]
  (let [graph-id (gt/override-id->graph-id override-id)]
    (get-in basis [:graphs graph-id :overrides override-id :traverse-fn])))

(defn hydrate-after-undo [basis graph-state]
  ;; NOTE: This was originally written in a simpler way. This longer-form
  ;; implementation is optimized in order to solve performance issues in graphs
  ;; with a large number of connections.
  (let [graph-id (:_graph-id graph-state)
        graphs (:graphs basis)
        other-graphs (dissoc graphs graph-id)
        old-sarcs (get graph-state :sarcs)
        arc-from-graph? #(= graph-id (gt/node-id->graph-id (.source-id ^Arc %)))
        arc-to-graph? #(= graph-id (gt/node-id->graph-id (.target-id ^Arc %)))

        ;; Create a sarcs-like map structure containing just the Arcs that
        ;; connect nodes in our graph to nodes in other graphs. Use the tarcs
        ;; from the other graphs as the source of truth.
        external-sarcs (into {}
                             (map (juxt key (comp (partial group-by gt/source-label) val)))
                             (group-by gt/source-id
                                       (into #{}
                                             (comp (mapcat :tarcs)
                                                   (mapcat val)
                                                   (mapcat val)
                                                   (filter arc-from-graph?))
                                             (vals other-graphs))))

        ;; Remove any sarcs that previously connected nodes in our graph to
        ;; nodes in other graphs. We will replace these connections with the
        ;; ones in external-sarcs above.
        internal-sarcs (reduce-kv (fn [internal-sarcs source-id arcs-by-source-label]
                                    (if-some [internal-arcs-by-source-label (not-empty
                                                                              (persistent!
                                                                                (reduce-kv (fn [internal-arcs-by-source-label source-label arcs]
                                                                                             (if-some [internal-arcs (not-empty (filterv arc-to-graph? arcs))]
                                                                                               (assoc! internal-arcs-by-source-label source-label internal-arcs)
                                                                                               (dissoc! internal-arcs-by-source-label source-label)))
                                                                                           (transient arcs-by-source-label)
                                                                                           arcs-by-source-label)))]
                                      (assoc! internal-sarcs source-id internal-arcs-by-source-label)
                                      (dissoc! internal-sarcs source-id)))
                                  (transient old-sarcs)
                                  old-sarcs)

        ;; The merge of the above internal and external sarcs are the new sarcs.
        new-sarcs (persistent!
                    (reduce-kv (fn [new-sarcs source-id external-arcs-by-source-label]
                                 (assoc! new-sarcs source-id
                                         (persistent!
                                           (reduce-kv (fn [new-arcs-by-source-label source-label external-arcs]
                                                        (assoc! new-arcs-by-source-label source-label
                                                                (into (get new-arcs-by-source-label source-label [])
                                                                      external-arcs)))
                                                      (transient (get new-sarcs source-id {}))
                                                      external-arcs-by-source-label))))
                               internal-sarcs
                               external-sarcs))

        ;; We must refresh all outputs for which an Arc was introduced. In
        ;; addition to being invalidated, we will update successors for these
        ;; outputs outside this function.
        outputs-to-refresh (into []
                                 (mapcat (fn [[source-id external-arcs-by-source-label]]
                                           (if-some [old-arcs-by-source-label (old-sarcs source-id)]
                                             (keep (fn [[source-label external-arcs]]
                                                     ;; The number of arcs from a specific source to a different
                                                     ;; graph will be small. A linear search should be fine.
                                                     (let [old-arcs (old-arcs-by-source-label source-label)
                                                           old-arc? #(some (partial = %) old-arcs)]
                                                       (when (not-every? old-arc? external-arcs)
                                                         (gt/endpoint source-id source-label))))
                                                   external-arcs-by-source-label)
                                             (map (partial gt/endpoint source-id)
                                                  (keys external-arcs-by-source-label)))))
                                 external-sarcs)]

    {:basis (update basis :graphs assoc graph-id (assoc graph-state :sarcs new-sarcs))
     :outputs-to-refresh outputs-to-refresh}))

(defn- input-deps [basis node-id]
  (some-> (gt/node-by-id-at basis node-id) gt/node-type in/input-dependencies))

(defn- update-graph-successors
  "The purpose of this fn is to build a data structure that reflects which set of endpoints that can be reached from the incoming changes (map of node-id + outputs)
   For a specific node-id-a + output-x, add:
     the internal input-dependencies, i.e. outputs consuming the given output
     the closest override-nodes, i.e. override-node-a + output-x, as they can be potential dependents
     all connected nodes, where node-id-a + output-x => [[node-id-b + input-y] ...] => [[node-id-b + output+z] ...]"
  [old-successors basis graph changes]
  (let [node-id->overrides (or (:node->overrides graph) (constantly nil))
        changes+old-node-successors (mapv (fn [[node-id labels]]
                                            [node-id labels (old-successors node-id)])
                                          changes)
        [new-successors removed-successor-entries] (r/fold
                                                     (fn combinef
                                                       ([]
                                                        (pair {} []))
                                                       ([[new-successors-1 removed-successor-entries-1] [new-successors-2 removed-successor-entries-2]]
                                                        (pair (into new-successors-1 new-successors-2) (into removed-successor-entries-1 removed-successor-entries-2))))
                                                     (fn reducef
                                                       ([]
                                                        (pair {} []))
                                                       ([[new-acc remove-acc] [node-id labels old-node-successors]]
                                                        (let [new-node-successors (when-some [node (gt/node-by-id-at basis node-id)]
                                                                                    (let [deps (ArrayList.)
                                                                                          node-type (gt/node-type node)
                                                                                          deps-by-label (in/input-dependencies node-type)
                                                                                          overrides (node-id->overrides node-id)
                                                                                          labels (or labels (in/output-labels node-type))
                                                                                          arcs-by-source (if (> (count labels) 1)
                                                                                                           (let [arcs (gt/arcs-by-source basis node-id)
                                                                                                                 arcs-by-source-label (util/group-into #(.source-label ^Arc %) arcs)]
                                                                                                             (fn [_ _ label]
                                                                                                               (arcs-by-source-label label)))
                                                                                                           (fn [basis node-id label]
                                                                                                             (gt/arcs-by-source basis node-id label)))]
                                                                                      (reduce (fn [new-node-successors label]
                                                                                                (let [dep-labels (get deps-by-label label)
                                                                                                      outgoing-arcs (arcs-by-source basis node-id label)]
                                                                                                  (.clear deps)
                                                                                                  (.ensureCapacity deps (+ (count dep-labels)
                                                                                                                           (count overrides)
                                                                                                                           (* (long 10) (count outgoing-arcs))))

                                                                                                  ;; The internal dependent outputs.
                                                                                                  (doseq [dep-label dep-labels]
                                                                                                    (.add deps (gt/endpoint node-id dep-label)))

                                                                                                  ;; The closest overrides.
                                                                                                  (doseq [override-node-id overrides]
                                                                                                    (.add deps (gt/endpoint override-node-id label)))

                                                                                                  ;; The connected nodes and their outputs.
                                                                                                  (doseq [^Arc outgoing-arc outgoing-arcs
                                                                                                          :let [target-id (.target-id outgoing-arc)
                                                                                                                target-label (.target-label outgoing-arc)]
                                                                                                          dep-label (get (input-deps basis target-id) target-label)]
                                                                                                    (.add deps (gt/endpoint target-id dep-label)))

                                                                                                  (if (.isEmpty deps)
                                                                                                    (dissoc new-node-successors label)
                                                                                                    (let [^"[Linternal.graph.types.Endpoint;" storage (make-array Endpoint (.size deps))
                                                                                                          endpoint-array (.toArray deps storage)] ; Use a typed array to aid memory profilers.
                                                                                                      (assoc new-node-successors label endpoint-array)))))
                                                                                              old-node-successors
                                                                                              labels)))]
                                                          (if (pos? (count new-node-successors))
                                                            (pair (assoc new-acc node-id new-node-successors) remove-acc)
                                                            (pair new-acc (conj remove-acc node-id))))))
                                                     changes+old-node-successors)]
    (persistent!
      (reduce dissoc!
              (transient (into old-successors new-successors))
              removed-successor-entries))))

(defn update-successors
  [basis changes]
  ;; changes = {node-id #{outputs}}
  (reduce (fn [basis [graph-id changes]]
            (if-let [graph (get (:graphs basis) graph-id)]
              (update-in basis [:graphs graph-id :successors] update-graph-successors basis graph changes)
              basis))
          basis
          (util/group-into (comp gt/node-id->graph-id first) changes)))
