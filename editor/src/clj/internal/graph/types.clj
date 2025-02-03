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

(ns internal.graph.types
  (:require [clojure.data.int-map-fixed :as int-map])
  (:import [clojure.data.int_map_fixed PersistentIntMap]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defmacro ^:private bits->mask [bits]
  `(dec (bit-shift-left 1 ~bits)))

;; We currently do not use the sign bit, but presumably we could.
(def ^:const NID-BITS 25) ; Support 33554432 total nodes per graph.
(def ^:const NID-MASK (bits->mask NID-BITS))
(def ^:const GID-BITS 6) ; Support 64 total graphs.
(def ^:const GID-MASK (bits->mask GID-BITS))
(def ^:const NODE-ID-BITS (+ GID-BITS NID-BITS))
(def ^:const NODE-ID-MASK (bits->mask NODE-ID-BITS))

(defrecord Arc [source-id source-label target-id target-label])

(defn source-id [^Arc arc] (.source-id arc))
(defn source-label [^Arc arc] (.source-label arc))
(defn source [^Arc arc] [(.source-id arc) (.source-label arc)])
(defn target-id [^Arc arc] (.target-id arc))
(defn target-label [^Arc arc] (.target-label arc))
(defn target [^Arc arc] [(.target-id arc) (.target-label arc)])

(def ^:const ENDPOINT-NODE-ID-BITS NODE-ID-BITS)
(def ^:const ENDPOINT-NODE-ID-MASK NODE-ID-MASK)
(def ^:const ENDPOINT-LABEL-BITS Integer/SIZE)
(def ^:const ENDPOINT-LABEL-MASK (bits->mask ENDPOINT-LABEL-BITS))
(def ^:const ENDPOINT-BITS (+ ENDPOINT-NODE-ID-BITS ENDPOINT-LABEL-BITS))

(assert (<= ENDPOINT-BITS Long/SIZE) "Endpoint size exceeds size of long.")

(defonce ^:private endpoint-label-bits-lookup-atom (atom (int-map/int-map)))

(defn- endpoint-label-bits
  ^long [label]
  {:pre [(keyword? label)]}
  ;; Wasteful in terms of the number of bits used, but simple.
  (let [label-bits (bit-and (long (hash label)) ENDPOINT-LABEL-MASK)]
    (when-not (contains? @endpoint-label-bits-lookup-atom label-bits)
      (swap! endpoint-label-bits-lookup-atom assoc label-bits label))
    label-bits))

(defn endpoint
  ^long [^long node-id label]
  {:pre [(keyword? label)]}
  (let [label-bits (endpoint-label-bits label)]
    (bit-or
      (bit-shift-left node-id ENDPOINT-LABEL-BITS)
      (bit-and label-bits ENDPOINT-LABEL-MASK))))

(defn endpoint-node-id
  ^long [^long endpoint]
  (bit-and
    (bit-shift-right endpoint ENDPOINT-LABEL-BITS)
    ENDPOINT-NODE-ID-MASK))

(defn endpoint-label [^long endpoint]
  (let [label-bits (bit-and endpoint ENDPOINT-LABEL-MASK)]
    (get @endpoint-label-bits-lookup-atom label-bits)))

(defn endpoint? [x]
  (and (instance? Long x)
       (some? (endpoint-label x))))

(def endpoint-map int-map/int-map)

(defn endpoint-map? [value]
  (instance? PersistentIntMap value))

(defn node-id? [v] (integer? v))

(defprotocol Evaluation
  (produce-value       [this label evaluation-context] "Pull a value using an evaluation context"))

(defprotocol Node
  (node-id               [this]                          "Return an ID that can be used to get this node (or a future value of it).")
  (node-type             [this]                          "Return the node type that created this node.")
  (get-property          [this basis property]           "Return the value of the named property")
  (set-property          [this basis property value]     "Set the named property")
  (own-properties        [this]                          "Return a map of property name to value explicitly assigned to this node")
  (overridden-properties [this]                          "Return a map of property name to override value")
  (property-overridden?  [this property]))

(defprotocol OverrideNode
  (clear-property      [this basis property]           "Clear the named property (this is only valid for override nodes)")
  (override-id         [this]                          "Return the ID of the override this node belongs to, if any")
  (original            [this]                          "Return the ID of the original of this node, if any")
  (set-original        [this original-id]              "Set the ID of the original of this node, if any"))

(defprotocol IBasis
  (node-by-id-at    [this node-id])
  (node-by-property [this label value])
  (arcs-by-source   [this node-id] [this node-id label])
  (arcs-by-target   [this node-id] [this node-id label])
  (sources          [this node-id] [this node-id label])
  (targets          [this node-id] [this node-id label])
  (add-node         [this value])
  (delete-node      [this node-id])
  (replace-node     [this node-id value])
  (override-node    [this original-id override-id])
  (override-node-clear [this original-id])
  (add-override     [this override-id override])
  (delete-override  [this override-id])
  (replace-override [this override-id value])
  (connect          [this source-id source-label target-id target-label])
  (disconnect       [this source-id source-label target-id target-label])
  (connected?       [this source-id source-label target-id target-label])
  (dependencies     [this endpoints]
    "Follow arcs through the graphs, from outputs to the inputs
     connected to them, and from those inputs to the downstream
     outputs that use them, and so on. Continue following links until
     all reachable outputs are found.

     Takes a coll of endpoints and returns a set of endpoints")
  (original-node    [this node-id]))

(defn basis? [value]
  (satisfies? IBasis value))

;; ---------------------------------------------------------------------------
;; ID helpers
;; ---------------------------------------------------------------------------

(defn make-node-id
  ^long [^long gid ^long nid]
  (bit-or
    (bit-shift-left gid NID-BITS)
    (bit-and nid NID-MASK)))

(defn node-id->graph-id
  ^long [^long node-id]
  (bit-and
    (bit-shift-right node-id NID-BITS)
    GID-MASK))

(defn node-id->nid
  ^long [^long node-id]
  (bit-and node-id NID-MASK))

(defn node->graph-id
  ^long [node]
  (node-id->graph-id (node-id node)))

(defn make-override-id
  ^long [^long gid ^long oid]
  (bit-or
    (bit-shift-left gid NID-BITS)
    (bit-and oid NID-MASK)))

(defn override-id->graph-id
  ^long [^long override-id]
  (bit-and
    (bit-shift-right override-id NID-BITS)
    GID-MASK))
