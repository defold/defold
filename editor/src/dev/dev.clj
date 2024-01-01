;; Copyright 2020-2023 The Defold Foundation
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

(ns dev
  (:require [clojure.pprint :as pprint]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.asset-browser :as asset-browser]
            [editor.changes-view :as changes-view]
            [editor.console :as console]
            [editor.curve-view :as curve-view]
            [editor.defold-project :as project]
            [editor.math :as math]
            [editor.outline-view :as outline-view]
            [editor.prefs :as prefs]
            [editor.properties-view :as properties-view]
            [editor.util :as eutil]
            [internal.graph.types :as gt]
            [internal.node :as in]
            [internal.system :as is]
            [internal.util :as util]
            [util.coll :as coll :refer [pair]])
  (:import [com.defold.util WeakInterner]
           [internal.graph.types Arc]
           [java.beans BeanInfo Introspector MethodDescriptor PropertyDescriptor]
           [java.lang.reflect Modifier]
           [javafx.stage Window]))

(set! *warn-on-reflection* true)

(defn workspace []
  0)

(defn project []
  (ffirst (g/targets-of (workspace) :resource-map)))

(defn app-view []
  (ffirst (g/targets-of (project) :selected-node-ids-by-resource-node)))

(defn active-resource []
  (->> (g/node-value (project) :selected-node-ids-by-resource-node)
       (keep (fn [[resource-node _selected-nodes]]
               (let [targets (g/targets-of resource-node :node-outline)]
                 (when (some (fn [[_target-node target-label]]
                               (= :active-outline target-label)) targets)
                   resource-node))))
       first))

(defn active-view []
  (some-> (app-view)
          (g/node-value :active-view)))

(defn resource-node [path-or-resource]
  (project/get-resource-node (project) path-or-resource))

(defn selection []
  (->> (g/node-value (project) :selected-node-ids-by-resource-node)
       (keep (fn [[resource-node selected-nodes]]
               (let [targets (g/targets-of resource-node :node-outline)]
                 (when (some (fn [[_target-node target-label]]
                               (= :active-outline target-label)) targets)
                   selected-nodes))))
       first))

(def sel (comp first selection))

(defn prefs []
  (prefs/make-prefs "defold"))

(declare ^:private exclude-keys-deep-helper)

(defn- exclude-keys-deep-value-helper [excluded-map-entry? value]
  (cond
    (map? value)
    (exclude-keys-deep-helper excluded-map-entry? value)

    (coll? value)
    (into (empty value)
          (map (partial exclude-keys-deep-value-helper excluded-map-entry?))
          value)

    :else
    value))

(defn- exclude-keys-deep-helper [excluded-map-entry? m]
  (let [filter-value (partial exclude-keys-deep-value-helper excluded-map-entry?)
        filter-map-entry (juxt key (comp filter-value val))]
    (with-meta (into (if (or (nil? m) (record? m))
                       {}
                       (empty m))
                     (comp (remove excluded-map-entry?)
                           (map filter-map-entry))
                     m)
               (meta m))))

(defn exclude-keys-deep [m excluded-keys]
  (assert (or (nil? m) (map? m)))
  (exclude-keys-deep-helper (comp (set excluded-keys) key) m))

(def select-keys-deep #'internal.util/select-keys-deep)

(defn- deep-keep-finalize-coll-value-fn [coll]
  (some-> coll not-empty util/with-sorted-keys))

(defn- deep-keep-wrapped-value-fn [value-fn]
  (letfn [(wrapped-value-fn [value]
            (if-not (record? value)
              (value-fn value)
              (deep-keep-finalize-coll-value-fn
                (into (with-meta (sorted-map)
                                 (meta value))
                      (keep (fn [entry]
                              (when-some [v' (util/deep-keep deep-keep-finalize-coll-value-fn wrapped-value-fn (val entry))]
                                (pair (key entry) v'))))
                      value))))]
    wrapped-value-fn))

(defn- deep-keep-kv-wrapped-value-fn [value-fn]
  (letfn [(wrapped-value-fn [key value]
            (if-not (record? value)
              (value-fn key value)
              (deep-keep-finalize-coll-value-fn
                (into (with-meta (sorted-map)
                                 (meta value))
                      (keep (fn [[k v]]
                              (when-some [v' (util/deep-keep-kv-helper deep-keep-finalize-coll-value-fn wrapped-value-fn k v)]
                                (pair k v'))))
                      value))))]
    wrapped-value-fn))

(defn deep-keep [value-fn value]
  (let [wrapped-value-fn (deep-keep-wrapped-value-fn value-fn)]
    (util/deep-keep deep-keep-finalize-coll-value-fn wrapped-value-fn value)))

(defn deep-keep-kv [value-fn value]
  (let [wrapped-value-fn (deep-keep-kv-wrapped-value-fn value-fn)]
    (util/deep-keep-kv deep-keep-finalize-coll-value-fn wrapped-value-fn value)))

(defn views-of-type [node-type]
  (keep (fn [node-id]
          (when (g/node-instance? node-type node-id)
            node-id))
        (g/node-ids (g/graph (g/node-id->graph-id (app-view))))))

(defn view-of-type [node-type]
  (first (views-of-type node-type)))

(defn windows []
  (Window/getWindows))

(def assets-view (partial view-of-type asset-browser/AssetBrowser))
(def changed-files-view (partial view-of-type changes-view/ChangesView))
(def outline-view (partial view-of-type outline-view/OutlineView))
(def properties-view (partial view-of-type properties-view/PropertiesView))
(def curve-view (partial view-of-type curve-view/CurveView))

(defn console-view []
  (some-> (view-of-type console/ConsoleNode)
          (g/targets-of :lines)
          (ffirst)))

(defn node-values [node-id & labels]
  (g/with-auto-evaluation-context evaluation-context
    (into {}
          (map (fn [label]
                 (let [value (g/node-value node-id label evaluation-context)]
                   (pair label value))))
          labels)))

(def node-type-key (comp :k g/node-type*))

(defn- class-symbol [^Class class]
  (-> (.getSimpleName class)
      (string/replace "[]" "-array") ; For arrays, e.g. "byte[]" -> "byte-array"
      (symbol)))

(defn- node-value-type-symbol [node-value-type]
  (if-some [class (:class (deref node-value-type))]
    (class-symbol class)
    (symbol (name (:k node-value-type)))))

(defn- node-label-type-keyword [node-type-def-key node-label-def-flags]
  (cond
    (and (= :input node-type-def-key)
         (node-label-def-flags :array))
    :array-input

    (and (= :property node-type-def-key)
         (node-label-def-flags :unjammable))
    :extern

    :else
    node-type-def-key))

(defn- label-infos [node-type-def node-type-def-key]
  (map (fn [[label {:keys [flags value-type]}]]
         (let [value-type-symbol (node-value-type-symbol value-type)
               label-type (node-label-type-keyword node-type-def-key flags)]
           [label value-type-symbol label-type]))
       (get node-type-def node-type-def-key)))

(defn node-labels
  "Returns all the available node labels on the specified node. The result is a
  sorted set of vectors that include the label, its value type and label type.
  Labels are classified as :property :output, :input, array-input or :extern.
  Labels that are both inputs and outputs will show up as inputs."
  [node-id]
  (into (sorted-set)
        (comp (mapcat (partial label-infos (deref (g/node-type* node-id))))
              (util/distinct-by first))
        [:input :property :output]))

(defn object-bean-info
  ^BeanInfo [^Object instance]
  (Introspector/getBeanInfo (.getClass instance)))

(defn object-props
  "Returns all the bean properties of the specified Object. The result is a
  sorted set of vectors that include the property name in keyword format and the
  value type symbol."
  [^Object instance]
  (into (sorted-set)
        (map (fn [^PropertyDescriptor pd]
               [(-> pd .getName keyword)
                (some-> (.getPropertyType pd) class-symbol)]))
        (.getPropertyDescriptors (object-bean-info instance))))

(def ^:private arrow-symbol (symbol "->"))

(def internal-method-name?
  (into #{}
        (map #(.intern ^String %))
        ["__getClojureFnMappings"
         "__initClojureFnMappings"
         "__updateClojureFnMappings"]))

(defn object-methods
  "Returns all the public methods on the specified Object. The result is a
  sorted set of vectors that include the method name in keyword format, followed
  by a vector of argument type symbols, an arrow (->), and the return type."
  [^Object instance]
  (into (sorted-set)
        (keep (fn [^MethodDescriptor md]
                (let [m (.getMethod md)
                      mod (.getModifiers m)]
                  (when (and (Modifier/isPublic mod)
                             (not (Modifier/isStatic mod)))
                    (let [name (.getName md)]
                      (when-not (internal-method-name? name)
                        [(keyword name)
                         (mapv class-symbol (.getParameterTypes m))
                         arrow-symbol
                         (class-symbol (.getReturnType m))]))))))
        (.getMethodDescriptors (object-bean-info instance))))

(defn direct-internal-successors [basis node-id label]
  (let [node-type (g/node-type* basis node-id)
        output-label->output-desc (in/declared-outputs node-type)]
    (keep (fn [[output-label output-desc]]
            (when (and (not= label output-label)
                       (contains? (:dependencies output-desc) label))
              (pair node-id output-label)))
          output-label->output-desc)))

(defn direct-override-successors [basis node-id label]
  (let [graph-id (g/node-id->graph-id node-id)
        graph (get-in basis [:graphs graph-id])]
    (map (fn [override-node-id]
           (pair override-node-id label))
         (get-in graph [:node->overrides node-id]))))

(defn direct-successors* [direct-connected-successors-fn basis node-id-and-label-pairs]
  (into #{}
        (mapcat (fn [[node-id label]]
                  (concat
                    (direct-internal-successors basis node-id label)
                    (direct-override-successors basis node-id label)
                    (direct-connected-successors-fn node-id label))))
        node-id-and-label-pairs))

(defn recursive-successors* [direct-connected-successors-fn basis node-id-and-label-pairs]
  (let [direct-successors (direct-successors* direct-connected-successors-fn basis node-id-and-label-pairs)]
    (if (empty? direct-successors)
      direct-successors
      (into direct-successors
            (recursive-successors* direct-connected-successors-fn basis direct-successors)))))

(defn direct-connected-successors-by-label [basis node-id]
  (util/group-into {} []
                   (fn key-fn [^Arc arc]
                     (.source-label arc))
                   (fn value-fn [^Arc arc]
                     (pair (.target-id arc)
                           (.target-label arc)))
                   (gt/arcs-by-source basis node-id)))

(defn make-direct-connected-successors-fn [basis]
  (let [direct-connected-successors-by-label-fn (memoize (partial direct-connected-successors-by-label basis))]
    (fn direct-connected-successors [node-id label]
      (-> node-id direct-connected-successors-by-label-fn label))))

(defn successor-tree
  "Returns a tree of all downstream inputs and outputs affected by a change to
  the specified node id and label. The result is a map of target node keys to
  affected labels, recursively. The node-key-fn takes a basis and a node-id,
  and should return the key to use for the node in the resulting map. If not
  supplied, the node keys will be a pair of the node-type-key and the node-id."
  ([node-id label]
   (successor-tree (g/now) node-id label))
  ([basis node-id label]
   (let [direct-connected-successors-fn (make-direct-connected-successors-fn basis)]
     (util/group-into
       (sorted-map)
       (sorted-map)
       (fn key-fn [[successor-node-id]]
         (pair (node-type-key basis successor-node-id)
               successor-node-id))
       (fn value-fn [[successor-node-id successor-label]]
         (pair successor-label
               (not-empty
                 (successor-tree basis successor-node-id successor-label))))
       (direct-successors* direct-connected-successors-fn basis [(pair node-id label)])))))

(defn- successor-types-impl [successors-fn basis node-id-and-label-pairs]
  (let [direct-connected-successors-fn (make-direct-connected-successors-fn basis)]
    (into (sorted-map)
          (map (fn [[node-type-key successor-labels]]
                 (pair node-type-key
                       (into (sorted-map)
                             (frequencies successor-labels)))))
          (util/group-into {} []
                           (comp (partial node-type-key basis) first)
                           second
                           (successors-fn direct-connected-successors-fn basis node-id-and-label-pairs)))))

(defn successor-types*
  "Like successor-types, but you can query multiple inputs at once by providing
  a sequence of [node-id label] pairs."
  ([node-id-and-label-pairs]
   (successor-types* (g/now) node-id-and-label-pairs))
  ([basis node-id-and-label-pairs]
   (successor-types-impl recursive-successors* basis node-id-and-label-pairs)))

(defn successor-types
  "Returns a map of all downstream inputs and outputs affected by a change to
  the specified node id and label, recursively. The result is a flat map of
  target node types to a map of the affected labels in that node type. The
  number associated with each affected label is how many upstream labels
  invalidate that label in the current search space."
  ([node-id label]
   (successor-types (g/now) node-id label))
  ([basis node-id label]
   (successor-types* basis [(pair node-id label)])))

(defn direct-successor-types*
  "Like direct-successor-types, but you can query multiple inputs at once by
  providing a sequence of [node-id label] pairs."
  ([node-id-and-label-pairs]
   (direct-successor-types* (g/now) node-id-and-label-pairs))
  ([basis node-id-and-label-pairs]
   (successor-types-impl direct-successors* basis node-id-and-label-pairs)))

(defn direct-successor-types
  "Returns a map of all downstream inputs and outputs immediately affected by a
  change to the specified node id and label. The result is a flat map of target
  node types to a map of the affected labels in that node type. The number
  associated with each affected label is how many upstream labels invalidate
  that label in the current search space."
  ([node-id label]
   (direct-successor-types (g/now) node-id label))
  ([basis node-id label]
   (direct-successor-types* basis [(pair node-id label)])))

(defn ordered-occurrences
  "Returns a sorted list of [occurrence-count entry]. The list is sorted by
  occurrence count in descending order."
  [coll]
  (sort (fn [[occurrence-count-a entry-a] [occurrence-count-b entry-b]]
          (cond (< occurrence-count-a occurrence-count-b) 1
                (< occurrence-count-b occurrence-count-a) -1
                (and (instance? Comparable entry-a)
                     (instance? Comparable entry-b)) (compare entry-a entry-b)
                :else 0))
        (map (fn [[entry occurrence-count]]
               (pair occurrence-count entry))
             (frequencies coll))))

(defn cached-output-report
  "Returns a sorted list of what node outputs are in the system cache in the
  format [entry-occurrence-count [node-type-key output-label]]. The list is
  sorted by entry occurrence count in descending order."
  []
  (let [system @g/*the-system*
        basis (is/basis system)]
    (ordered-occurrences
      (map (fn [[[node-id output-label]]]
             (let [node-type-key (node-type-key basis node-id)]
               (pair node-type-key output-label)))
           (is/system-cache system)))))

(defn cached-output-name-report
  "Returns a sorted list of what output names are in the system cache in the
  format [entry-occurrence-count output-label]. The list is sorted by entry
  occurrence count in descending order."
  []
  (ordered-occurrences
    (map (comp second key)
         (is/system-cache @g/*the-system*))))

;; Utilities for investigating successors performance

(defn- successor-pairs
  ([endpoints]
   (successor-pairs (g/now) endpoints))
  ([basis endpoints]
   (successor-pairs basis endpoints nil))
  ([basis endpoints successor-filter]
   (letfn [(get-successors [acc endpoints]
             (let [next-level
                   (into {}
                         (comp
                           (remove acc)
                           (map
                             (juxt
                               identity
                               (fn [endpoint]
                                 (let [node-id (gt/endpoint-node-id endpoint)
                                       label (gt/endpoint-label endpoint)
                                       graph-id (gt/node-id->graph-id node-id)]
                                   (cond->> (get-in basis [:graphs graph-id :successors node-id label])
                                            successor-filter
                                            (into [] (filter #(successor-filter [endpoint %])))))))))
                         endpoints)]
               (cond-> (into acc next-level)
                       (pos? (count next-level))
                       (recur (into #{} (mapcat val) next-level)))))]
     (let [endpoint->successors (get-successors {} endpoints)]
       (into #{}
             (mapcat
               (fn [[endpoint successors]]
                 (->Eduction
                   (map (partial pair endpoint))
                   successors)))
             endpoint->successors)))))

(defn- successor-pair-type
  "Returns either :override, :internal or :external"
  ([source-endpoint target-endpoint]
   (successor-pair-type (g/now) source-endpoint target-endpoint))
  ([basis source-endpoint target-endpoint]
   (cond
     (and
       (= (gt/endpoint-node-id source-endpoint)
          (gt/original-node basis (gt/endpoint-node-id target-endpoint)))
       (= (gt/endpoint-label source-endpoint)
          (gt/endpoint-label target-endpoint)))
     :override

     (= (gt/endpoint-node-id source-endpoint)
        (gt/endpoint-node-id target-endpoint))
     :internal

     :else
     :external)))

(defn- percentage-str [n total]
  (eutil/format* "%.2f%%" (double (* 100 (/ n total)))))

(defn successor-pair-stats-by-kind
  "For a given coll of endpoints, group all successors by successor 'type',
  where the 'type' is either:
  - override - connection from original to override node with the same label
  - internal - connection inside a node
  - external - connection between 2 different nodes"
  ([endpoints]
   (successor-pair-stats-by-kind (g/now) endpoints))
  ([basis endpoints]
   (let [pairs (successor-pairs basis endpoints)
         node-count (count (into #{} (comp cat (map gt/endpoint-node-id)) pairs))
         successor-count (count pairs)]
     (println "Total" node-count "nodes," successor-count "successors")
     (println "Successor contribution by type:")
     (->> pairs
          (group-by #(apply successor-pair-type basis %))
          (sort-by (comp - count val))
          (run! (fn [[kind kind-pairs]]
                  (let [kind-count (count kind-pairs)]
                    (println (format "  %s (%s, %s total)"
                                     (name kind)
                                     (percentage-str kind-count successor-count)
                                     kind-count))
                    (->> kind-pairs
                         (map (case kind
                                :external (fn [[source target]]
                                            (str (name (node-type-key basis (gt/endpoint-node-id source)))
                                                 " -> "
                                                 (name (node-type-key basis (gt/endpoint-node-id target)))))
                                (:override :internal) (fn [[source]]
                                                        (name (node-type-key basis (gt/endpoint-node-id source))))))
                         frequencies
                         (sort-by (comp - val))
                         (run! (fn [[label group-count]]
                                 (println (format "  %7s %s (%s total)"
                                                  (percentage-str group-count successor-count)
                                                  label
                                                  group-count))))))))))))

(defn- successor-pair-class [basis source-endpoint target-endoint]
  [(node-type-key basis (gt/endpoint-node-id source-endpoint))
   (gt/endpoint-label source-endpoint)
   (node-type-key basis (gt/endpoint-node-id target-endoint))
   (gt/endpoint-label target-endoint)])

(defn successor-pair-stats-by-external-connection-influence
  "For a particular successor tree, find all unique successor connection
  'classes', where a connection 'class' is a tuple of source node's type, source
  label, target node's type and target label. Then, for each 'class',
  re-calculate the successors again, but assuming there are no connections of
  this 'class'. Finally, print the stats about which 'classes' of successors
  contribute the most to the resulting number of successors.

  This reports helps to figure out, what kind of performance improvements we
  will get if we refactor the code to express some dependencies outside
  the graph.

  Might take a while to run, so it prints a progress report while running"
  ([endpoints]
   (successor-pair-stats-by-external-connection-influence (g/now) endpoints))
  ([basis endpoints]
   (let [original-pairs (successor-pairs basis endpoints)
         original-count (count original-pairs)
         ->external-pair-class (fn [[source-endpoint target-endpoint]]
                                 (when (= :external (successor-pair-type basis source-endpoint target-endpoint))
                                   (successor-pair-class basis source-endpoint target-endpoint)))
         pair-class-options (into #{} (keep ->external-pair-class) original-pairs)
         intermediate-results
         (into
           []
           (map-indexed
             (fn [i pair-class]
               (println (inc i) "/" (count pair-class-options))
               (let [this-pair-class? (comp #(= pair-class %) ->external-pair-class)]
                 {:pair-class pair-class
                  :direct-count (count
                                  (into [] (filter this-pair-class?) original-pairs))
                  :indirect-count (count
                                    (successor-pairs basis endpoints (complement this-pair-class?)))})))
           pair-class-options)]
     (println "Baseline:" original-count "successors")
     (println "Improvements by pair link class:")
     (->> intermediate-results
          (sort-by :indirect-count)
          (run! (fn [{:keys [pair-class direct-count indirect-count]}]
                  (let [[source-type source-label target-type target-label] pair-class
                        difference (- original-count indirect-count)]
                    (println (format "  %6s (of which %s direct) %s"
                                     (percentage-str difference original-count)
                                     (percentage-str direct-count difference)
                                     (str (name source-type)
                                          source-label
                                          " -> "
                                          (name target-type)
                                          target-label))))))))))

(defn print-dangling-outputs-stats
  "Collect all dangling outputs (outputs that are not connected to anything) and
  print the stats about them. Takes a while to run (e.g. a minute). Note: even
  if an output is shown as dangling, it still might be eventually connected to
  other nodes, e.g. on view open, so the output might contain false positives."
  []
  (let [basis (g/now)
        node-type-freqs (->> (get-in basis [:graphs 1 :nodes])
                             (keys)
                             (map #(g/node-type* basis %))
                             frequencies)]
    (->> (get-in basis [:graphs 1 :nodes])
         keys
         ;; for project node ids, collect external connections and union by node type
         (->Eduction
           (mapcat
             (fn [node-id]
               (let [connected-outputs (-> #{:_properties :_overridden-properties}
                                           (into (map second) (g/outputs basis node-id))
                                           (into (map peek) (g/inputs basis node-id)))]
                 (->Eduction
                   (map (partial pair (g/node-type* basis node-id)))
                   connected-outputs)))))
         (util/group-into {} #{} key val)
         ;; union with internal connection and finally diff from all defined outputs
         (into {}
               (map (juxt key (fn [[node-type connected-outputs]]
                                (let [with-internal-connections (into connected-outputs
                                                                      (mapcat :dependencies)
                                                                      (vals (g/declared-outputs node-type)))]
                                  (reduce disj (g/output-labels node-type) with-internal-connections))))))
         ;; sort by node count and print as a table
         (sort-by (comp - node-type-freqs key))
         (mapcat (fn [[node-type dangling-outputs]]
                   (map (fn [output]
                          {:type (name (:k node-type))
                           :output (name output)
                           :node-count (node-type-freqs node-type)})
                        dangling-outputs)))
         pprint/print-table)))

(defn weak-interner-info [^WeakInterner weak-interner]
  (into {}
        (map (fn [[key value]]
               (let [keyword-key (keyword key)]
                 (pair keyword-key
                       (case keyword-key
                         :hash-table (mapv (fn [entry-info]
                                             (when entry-info
                                               (into {}
                                                     (map (fn [[key value]]
                                                            (let [keyword-key (keyword key)]
                                                              (pair keyword-key
                                                                    (case keyword-key
                                                                      :status (keyword value)
                                                                      value)))))
                                                     entry-info)))
                                           value)
                         value)))))
        (.getDebugInfo weak-interner)))

(defn weak-interner-stats [^WeakInterner weak-interner]
  (let [info (weak-interner-info weak-interner)
        hash-table (:hash-table info)
        entry-count (:count info)
        capacity (count hash-table)
        occupancy-factor (/ (double entry-count) (double capacity))

        next-capacity
        (util/first-where
          #(< capacity %)
          (:growth-sequence info))

        attempt-frequencies
        (->> hash-table
             (keep :attempt)
             (frequencies)
             (into (sorted-map-by coll/descending-order)))

        elapsed-nanosecond-values
        (keep :elapsed hash-table)

        median-elapsed-nanoseconds
        (if (empty? elapsed-nanosecond-values)
          0
          (->> elapsed-nanosecond-values
               (math/median)
               (long)))

        max-elapsed-nanoseconds
        (if (empty? elapsed-nanosecond-values)
          0
          (->> elapsed-nanosecond-values
               (reduce max Long/MIN_VALUE)))]

    {:count entry-count
     :capacity capacity
     :next-capacity next-capacity
     :growth-threshold (:growth-threshold info)
     :occupancy-factor (math/round-with-precision occupancy-factor math/precision-general)
     :median-elapsed-nanoseconds median-elapsed-nanoseconds
     :max-elapsed-nanoseconds max-elapsed-nanoseconds
     :attempt-frequencies attempt-frequencies}))

(defn endpoint-interner-stats []
  ;; Trigger a GC and give it a moment to clear out unused weak references.
  (System/gc)
  (Thread/sleep 500)
  (weak-interner-stats gt/endpoint-interner))
