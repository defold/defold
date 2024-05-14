;; Copyright 2020-2024 The Defold Foundation
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
            [editor.buffers :as buffers]
            [editor.changes-view :as changes-view]
            [editor.code.data :as code.data]
            [editor.code.util :as code.util]
            [editor.collection :as collection]
            [editor.console :as console]
            [editor.curve-view :as curve-view]
            [editor.defold-project :as project]
            [editor.game-object :as game-object]
            [editor.gl.vertex2 :as vtx]
            [editor.math :as math]
            [editor.outline-view :as outline-view]
            [editor.prefs :as prefs]
            [editor.properties-view :as properties-view]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.scene-cache :as scene-cache]
            [editor.util :as eutil]
            [internal.graph.types :as gt]
            [internal.node :as in]
            [internal.system :as is]
            [internal.util :as util]
            [lambdaisland.deep-diff2 :as deep-diff]
            [lambdaisland.deep-diff2.minimize-impl :as deep-diff.minimize-impl]
            [lambdaisland.deep-diff2.puget.color :as puget.color]
            [lambdaisland.deep-diff2.puget.printer :as puget.printer]
            [util.coll :as coll :refer [pair]]
            [util.diff :as diff])
  (:import [com.defold.util WeakInterner]
           [editor.code.data Cursor CursorRange]
           [editor.gl.pass RenderPass]
           [editor.gl.vertex2 VertexBuffer]
           [editor.resource FileResource MemoryResource ZipResource]
           [editor.types AABB]
           [internal.graph.types Arc Endpoint]
           [java.beans BeanInfo Introspector MethodDescriptor PropertyDescriptor]
           [java.lang.reflect Modifier]
           [java.nio ByteBuffer]
           [javafx.stage Window]
           [javax.vecmath Matrix3d Matrix4d Point2d Point3d Point4d Quat4d Vector2d Vector3d Vector4d]))

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

(defn- throw-invalid-component-resource-node-id-exception [basis node-id]
  (throw (ex-info "The specified node cannot be resolved to a component ResourceNode."
                  {:node-id node-id
                   :node-type (g/node-type* basis node-id)})))

(defn- validate-component-resource-node-id
  ([node-id]
   (validate-component-resource-node-id (g/now) node-id))
  ([basis node-id]
   (if (and (g/node-instance? basis resource-node/ResourceNode node-id)
            (when-some [resource-type (resource/resource-type (resource-node/resource basis node-id))]
              (contains? (:tags resource-type) :component)))
     node-id
     (throw-invalid-component-resource-node-id-exception basis node-id))))

(defn to-component-resource-node-id
  ([node-id]
   (to-component-resource-node-id (g/now) node-id))
  ([basis node-id]
   (condp = (g/node-instance-match basis node-id [resource-node/ResourceNode
                                                  game-object/EmbeddedComponent
                                                  game-object/ReferencedComponent])
     resource-node/ResourceNode
     (validate-component-resource-node-id basis node-id)

     game-object/EmbeddedComponent
     (validate-component-resource-node-id basis (g/node-feeding-into basis node-id :embedded-resource-id))

     game-object/ReferencedComponent
     (validate-component-resource-node-id basis (g/node-feeding-into basis node-id :source-resource))

     :else
     (throw-invalid-component-resource-node-id-exception basis node-id))))

(defn to-game-object-node-id
  ([node-id]
   (to-game-object-node-id (g/now) node-id))
  ([basis node-id]
   (condp = (g/node-instance-match basis node-id [game-object/GameObjectNode
                                                  collection/GameObjectInstanceNode])
     game-object/GameObjectNode
     node-id

     collection/GameObjectInstanceNode
     (g/node-feeding-into basis node-id :source-resource)

     :else
     (throw (ex-info "The specified node cannot be resolved to a GameObjectNode."
                     {:node-id node-id
                      :node-type (g/node-type* basis node-id)})))))

(defn to-collection-node-id
  ([node-id]
   (to-collection-node-id (g/now) node-id))
  ([basis node-id]
   (condp = (g/node-instance-match basis node-id [collection/CollectionNode
                                                  collection/CollectionInstanceNode])
     collection/CollectionNode
     node-id

     collection/CollectionInstanceNode
     (g/node-feeding-into basis node-id :source-resource)

     :else
     (throw (ex-info "The specified node cannot be resolved to a CollectionNode."
                     {:node-id node-id
                      :node-type (g/node-type* basis node-id)})))))

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

(defn- class-name->symbol [^String class-name]
  (-> class-name
      (string/replace "[]" "-array") ; For arrays, e.g. "byte[]" -> "byte-array"
      (symbol)))

(defn- class-symbol [^Class class]
  (class-name->symbol (.getSimpleName class)))

(defn- namespaced-class-symbol [^Class class]
  (class-name->symbol (.getName class)))

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

(defn scene-cache-stats-by-context-id
  "Returns a sorted map where the keys are scene cache context ids mapped to a
  sorted map of cache-ids, to the number of entries in the context cache for that
  cache-id. The number of entries represent the number of unique request-ids
  in the cache. The contexts are typically OpenGL contexts, so in order to group
  the results, we use a string representation of the identity hash code of the
  OpenGL context as the context-id."
  []
  (util/group-into
    (sorted-map) (sorted-map)
    (fn key-fn [[[context-id _cache-id] _entry-count]]
      context-id)
    (fn value-fn [[[_context-id cache-id] entry-count]]
      (pair cache-id entry-count))
    (scene-cache/cache-stats)))

(defn cache-stats-by-cache-id
  "Returns a sorted map where the keys are scene cache cache-ids mapped to a
  sorted map of context ids, to the number of entries in the identified cache
  for that context. The number of entries represent the number of unique
  request-ids in the cache. The contexts are typically OpenGL contexts, so in
  order to group the results, we use a string representation of the identity
  hash code of the OpenGL context as the context-id."
  []
  (util/group-into
    (sorted-map) (sorted-map)
    (fn key-fn [[[_context-id cache-id] _entry-count]]
      cache-id)
    (fn value-fn [[[context-id _cache-id] entry-count]]
      (pair context-id entry-count))
    (scene-cache/cache-stats)))

(set! *warn-on-reflection* false)

(defn- buf-clj-attribute-data [^ByteBuffer buf ^long buf-vertex-attribute-offset ^long attribute-byte-size component-data-type]
  (let [primitive-type-kw (buffers/primitive-type-kw component-data-type)
        read-buf (-> buf
                     (.slice buf-vertex-attribute-offset attribute-byte-size)
                     (.order (.order buf))
                     (buffers/as-typed-buffer component-data-type))]
    (loop [clj-vector (vector-of primitive-type-kw)]
      (if (.hasRemaining read-buf)
        (let [attribute-component (.get read-buf) ; Return type differs by Buffer subclass.
              clj-vector (conj clj-vector attribute-component)]
          (recur clj-vector))
        clj-vector))))

(set! *warn-on-reflection* true)

(defn- buf-clj-vertex-data [^ByteBuffer buf vertex-attributes ^long vertex-stride]
  (let [buf-size (.limit buf)]
    (assert (zero? (rem buf-size vertex-stride)) "Buffer size does not confirm to vertex stride.")
    (mapv (fn [^long buf-vertex-offset]
            (persistent!
              (val
                (reduce (fn [[^long buf-vertex-attribute-offset clj-vertex] vertex-attribute]
                          (let [attribute-key (:name-key vertex-attribute)
                                attribute-byte-size (vtx/attribute-size vertex-attribute)
                                component-data-type (:type vertex-attribute)
                                clj-vertex-attribute-data (buf-clj-attribute-data buf buf-vertex-attribute-offset attribute-byte-size component-data-type)
                                clj-vertex-attribute (pair attribute-key clj-vertex-attribute-data)
                                clj-vertex (conj! clj-vertex clj-vertex-attribute)
                                buf-vertex-attribute-offset (+ buf-vertex-attribute-offset attribute-byte-size)]
                            (pair buf-vertex-attribute-offset clj-vertex)))
                        (pair buf-vertex-offset (transient []))
                        vertex-attributes))))
          (range 0 buf-size vertex-stride))))

(defn vertex-buffer-to-clj
  [^VertexBuffer vbuf]
  (let [{:keys [attributes ^long size]} (.vertex-description vbuf)
        vertex-count (count vbuf)
        buf (.buf vbuf)]
    {:bytes (* vertex-count size)
     :count vertex-count
     :stride size
     :verts (buf-clj-vertex-data buf attributes size)}))

(defn vertex-buffer-print-data [^VertexBuffer vbuf]
  (-> (vertex-buffer-to-clj vbuf)
      (update :verts (fn [verts]
                       (conj (subvec verts 0 (min 3 (count verts)))
                             '...)))))

(defmulti matrix-row (fn [matrix ^long _row-index] (class matrix)))

(defmethod matrix-row Matrix3d
  [^Matrix3d matrix ^long row-index]
  (let [row (double-array 3)]
    (.getRow matrix row-index row)
    row))

(defmethod matrix-row Matrix4d
  [^Matrix4d matrix ^long row-index]
  (let [row (double-array 4)]
    (.getRow matrix row-index row)
    row))

(def pretty-printer
  (let [fmt-doc puget.printer/format-doc
        col-doc puget.color/document
        col-txt puget.color/text]
    (letfn [(cls-tag [printer ^Class class]
              (col-doc printer :class-name [:span "#" (.getSimpleName class)]))

            (cls-tag-doc [printer ^Class class document]
              [:group
               (cls-tag printer class)
               document])

            (object-data-pprint-handler [printer-opts object->value printer object]
              (cls-tag-doc
                (cond-> pretty-printer printer-opts (merge printer-opts))
                (class object)
                (fmt-doc printer (object->value object))))]

      (let [editor-pprint-handlers
            {(namespaced-class-symbol AABB)
             (letfn [(aabb->value [^AABB aabb]
                       {:min (math/vecmath->clj (.min aabb))
                        :max (math/vecmath->clj (.max aabb))})]
               (partial object-data-pprint-handler {:sort-keys false} aabb->value))

             (namespaced-class-symbol Cursor)
             (partial object-data-pprint-handler nil code.data/cursor-print-data)

             (namespaced-class-symbol CursorRange)
             (partial object-data-pprint-handler nil code.data/cursor-range-print-data)

             (namespaced-class-symbol RenderPass)
             (fn render-pass-pprint-handler [printer ^RenderPass render-pass]
               (fmt-doc printer (symbol "pass" (.nm render-pass))))

             (namespaced-class-symbol VertexBuffer)
             (partial object-data-pprint-handler nil vertex-buffer-print-data)}

            graph-pprint-handlers
            {(namespaced-class-symbol Arc)
             (partial object-data-pprint-handler nil (juxt gt/source-id gt/source-label gt/target-id gt/target-label))

             (namespaced-class-symbol Endpoint)
             (partial object-data-pprint-handler nil (juxt g/endpoint-node-id g/endpoint-label))}

            java-pprint-handlers
            {(namespaced-class-symbol Class)
             (fn class-pprint-handler [printer ^Class class]
               (col-txt printer :class-name (.getName class)))}

            resource-pprint-handlers
            (letfn [(project-resource->value [resource]
                      [(resource/proj-path resource)])

                    (project-resource-pprint-handler [printer resource]
                      (object-data-pprint-handler nil project-resource->value printer resource))]

              {(namespaced-class-symbol FileResource)
               project-resource-pprint-handler

               (namespaced-class-symbol MemoryResource)
               (fn memory-resource-pprint-handler [printer resource]
                 (object-data-pprint-handler nil (comp vector resource/ext) printer resource))

               (namespaced-class-symbol ZipResource)
               project-resource-pprint-handler})

            vecmath-pprint-handlers
            (letfn [(vecmath-tuple-pprint-handler [printer vecmath-value]
                      (object-data-pprint-handler nil math/vecmath->clj printer vecmath-value))

                    (vecmath-matrix-pprint-handler [^long dim printer matrix]
                      (let [fmt-num #(eutil/format* "%.3f" %)
                            num-strs (into []
                                           (mapcat (fn [^long row-index]
                                                     (let [row (matrix-row matrix row-index)]
                                                       (map fmt-num row))))
                                           (range dim))
                            first-col-width (transduce (comp (take-nth 4)
                                                             (map count))
                                                       max
                                                       0
                                                       num-strs)
                            rest-col-width (transduce (map count)
                                                      max
                                                      0
                                                      num-strs)
                            first-col-width-fmt (str \% first-col-width \s)
                            rest-col-width-fmt (str \% rest-col-width \s)
                            fmt-col (fn [^long index num-str]
                                      (let [element (case num-str
                                                      ("-0.000" "0.000") :number
                                                      :string)
                                            fmt (if (zero? (rem index 4))
                                                  first-col-width-fmt
                                                  rest-col-width-fmt)]
                                        (col-txt printer element (format fmt num-str))))
                            data (into [:align]
                                       (comp (partition-all dim)
                                             (map (fn [row-num-strs]
                                                    (interpose " " (map-indexed fmt-col row-num-strs))))
                                             (interpose :break))
                                       num-strs)]
                        [:group
                         (cls-tag printer (class matrix))
                         (col-doc printer :delimiter "[")
                         data
                         (col-doc printer :delimiter "]")]))]

              {(namespaced-class-symbol Matrix3d)
               (partial vecmath-matrix-pprint-handler 3)

               (namespaced-class-symbol Matrix4d)
               (partial vecmath-matrix-pprint-handler 4)

               (namespaced-class-symbol Point2d)
               vecmath-tuple-pprint-handler

               (namespaced-class-symbol Point3d)
               vecmath-tuple-pprint-handler

               (namespaced-class-symbol Point4d)
               vecmath-tuple-pprint-handler

               (namespaced-class-symbol Quat4d)
               vecmath-tuple-pprint-handler

               (namespaced-class-symbol Vector2d)
               vecmath-tuple-pprint-handler

               (namespaced-class-symbol Vector3d)
               vecmath-tuple-pprint-handler

               (namespaced-class-symbol Vector4d)
               vecmath-tuple-pprint-handler})]

        (deep-diff/printer
          {:extra-handlers
           (merge editor-pprint-handlers
                  graph-pprint-handlers
                  java-pprint-handlers
                  resource-pprint-handlers
                  vecmath-pprint-handlers)})))))

(defonce last-pprint-value-atom (atom nil))

(defn pprint
  ([value]
   (pprint value nil))
  ([value printer-opts]
   (reset! last-pprint-value-atom value)
   (deep-diff/pretty-print value (cond-> pretty-printer printer-opts (merge printer-opts)))))

(defn last-pprint []
  @last-pprint-value-atom)

(defonce repl-pprint-tap-atom (atom nil))

(defn install-repl-pprint-tap! []
  (let [[old-repl-pprint-tap new-repl-pprint-tap] (swap-vals! repl-pprint-tap-atom #(or % (bound-fn* pprint)))]
    (when old-repl-pprint-tap
      (remove-tap old-repl-pprint-tap))
    (when new-repl-pprint-tap
      (add-tap new-repl-pprint-tap))))

(defn uninstall-repl-pprint-tap! []
  (when-some [repl-pprint-tap (first (reset-vals! repl-pprint-tap-atom nil))]
    (remove-tap repl-pprint-tap)))

(defn diff-values!
  ([expected-value actual-value]
   (diff-values! expected-value actual-value nil))
  ([expected-value actual-value opts]
   (let [diff (deep-diff/diff expected-value actual-value)]
     (if (deep-diff.minimize-impl/has-diff-item? diff)
       (deep-diff/pretty-print
         (cond-> diff
                 (:minimize opts) (deep-diff/minimize))
         (cond-> pretty-printer
                 opts (merge opts)))
       (println "Values are identical.")))))

(defn- to-diffable-text
  ^String [value]
  (if (and (seqable? value)
           (string? (first value)))
    (code.util/join-lines value)
    (str value)))

(defn diff-text! [expected-lines-or-string actual-lines-or-string]
  (let [expected-string (to-diffable-text expected-lines-or-string)
        actual-string (to-diffable-text actual-lines-or-string)]
    (println
      (or (some->> (diff/make-diff-output-lines expected-string actual-string 3)
                   (code.util/join-lines))
          "Strings are identical."))))
