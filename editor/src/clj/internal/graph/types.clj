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

(ns internal.graph.types
  (:require [util.defonce :as defonce])
  (:import [clojure.lang IHashEq Keyword Murmur3 Util]
           [com.defold.util WeakInterner]
           [java.io Writer]
           [java.util.concurrent.atomic AtomicLong]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defonce/record Arc [source-id source-label target-id target-label])

(defn arc-print-data [^Arc arc]
  (into [(.-source-id arc) (.-source-label arc)
         (.-target-id arc) (.-target-label arc)]
        cat
        (dissoc arc :source-id :source-label :target-id :target-label)))

(defmethod print-method Arc [^Arc arc ^Writer writer]
  (.write writer "#g/arc ")
  (print-method (arc-print-data arc) writer))

(defn- read-arc [[source-id source-label target-id target-label & {:as kvs}]]
  (if kvs
    `(Arc. ~source-id ~source-label ~target-id ~target-label nil ~kvs)
    `(Arc. ~source-id ~source-label ~target-id ~target-label)))

(definline source-id [^Arc arc] `(.-source-id ~(with-meta arc {:tag `Arc})))
(definline source-label [^Arc arc] `(.-source-label ~(with-meta arc {:tag `Arc})))
(defn source [^Arc arc] [(.-source-id arc) (.-source-label arc)])
(definline target-id [^Arc arc] `(.-target-id ~(with-meta arc {:tag `Arc})))
(definline target-label [^Arc arc] `(.-target-label ~(with-meta arc {:tag `Arc})))
(defn target [^Arc arc] [(.-target-id arc) (.-target-label arc)])

(definline node-id-hash [node-id]
  `(Murmur3/hashLong ~node-id))

(defonce/type Endpoint [^long node-id ^Keyword label]
  Comparable
  (compareTo [_ that]
    (let [^Endpoint that that
          node-id-comparison (Long/compare node-id (.-node-id that))]
      (if (zero? node-id-comparison)
        (.compareTo label (.-label that))
        node-id-comparison)))
  IHashEq
  (hasheq [_]
    (Util/hashCombine
      (node-id-hash node-id)
      (.hasheq label)))
  Object
  (toString [_]
    (str "#g/endpoint [" node-id " " label "]"))
  (hashCode [_]
    (Util/hashCombine
      (node-id-hash node-id)
      (.hasheq label)))
  (equals [this that]
    (or (identical? this that)
        (and (instance? Endpoint that)
             (= node-id (.-node-id ^Endpoint that))
             (identical? label (.-label ^Endpoint that))))))

(defmethod print-method Endpoint [^Endpoint ep ^Writer writer]
  (.write writer "#g/endpoint [")
  (.write writer (str (.-node-id ep)))
  (.write writer " ")
  (.write writer (str (.-label ep)))
  (.write writer "]"))

(defonce ^WeakInterner endpoint-interner (WeakInterner. 65536))

(definline endpoint [node-id label]
  `(.intern endpoint-interner (->Endpoint ~node-id ~label)))

(defn- read-endpoint [[node-id-expr label-expr]]
  `(endpoint ~node-id-expr ~label-expr))

(definline endpoint-node-id [endpoint]
  `(.-node-id ~(with-meta endpoint {:tag `Endpoint})))

(definline endpoint-label [endpoint]
  `(.-label ~(with-meta endpoint {:tag `Endpoint})))

(defn endpoint? [x]
  (instance? Endpoint x))

(defn source-endpoint
  ^Endpoint [^Arc arc]
  (endpoint (source-id arc) (source-label arc)))

(defn target-endpoint
  ^Endpoint [^Arc arc]
  (endpoint (target-id arc) (target-label arc)))

(defn graph-id? [v] (integer? v))
(defn node-id? [v] (integer? v))

(defonce/protocol Evaluation
  (produce-value       [this label evaluation-context] "Pull a value using an evaluation context"))

(defonce/protocol Node
  (node-id               [this]                          "Return an ID that can be used to get this node (or a future value of it).")
  (node-type             [this]                          "Return the node type that created this node.")
  (get-property          [this basis property]           "Return the value of the named property")
  (set-property          [this basis property value]     "Set the named property")
  (assigned-properties   [this]                          "Return a map of property name to value explicitly assigned to this node")
  (overridden-properties [this]                          "Return a map of property name to override value")
  (property-overridden?  [this property]))

(defonce/protocol OverrideNode
  (clear-property      [this basis property]           "Clear the named property (this is only valid for override nodes)")
  (override-id         [this]                          "Return the ID of the override this node belongs to, if any")
  (original            [this]                          "Return the ID of the original of this node, if any")
  (set-original        [this original-id]              "Set the ID of the original of this node, if any"))

(defonce/record Graph [nodes sarcs successors tarcs tx-id graph-values overrides node->overrides])

(defn graph? [value]
  (instance? Graph value))

(definline nodes [graph] `(.-nodes ~(with-meta graph {:tag `Graph})))
(definline sarcs [graph] `(.-sarcs ~(with-meta graph {:tag `Graph})))
(definline successors [graph] `(.-successors ~(with-meta graph {:tag `Graph})))
(definline tarcs [graph] `(.-tarcs ~(with-meta graph {:tag `Graph})))
(definline tx-id [graph] `(.-tx-id ~(with-meta graph {:tag `Graph})))
(definline graph-values [graph] `(.-graph-values ~(with-meta graph {:tag `Graph})))
(definline overrides [graph] `(.-overrides ~(with-meta graph {:tag `Graph})))
(definline node->overrides [graph] `(.-node->overrides ~(with-meta graph {:tag `Graph})))

;; ---------------------------------------------------------------------------
;; ID helpers
;; ---------------------------------------------------------------------------

(defn next-node-id
  ^long [^AtomicLong node-id-generator]
  (.getAndIncrement node-id-generator))

;; Deprecated compatibility function. Remove after 2027-09-08.
(defn node-id->graph-id
  ^long [^long _node-id]
  0)

(defn next-override-id
  ^long [^AtomicLong override-id-generator]
  (.getAndIncrement override-id-generator))
