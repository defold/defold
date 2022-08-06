;; Copyright 2020-2022 The Defold Foundation
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
  (:require [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.asset-browser :as asset-browser]
            [editor.changes-view :as changes-view]
            [editor.console :as console]
            [editor.curve-view :as curve-view]
            [editor.defold-project :as project]
            [editor.outline-view :as outline-view]
            [editor.prefs :as prefs]
            [editor.properties-view :as properties-view]
            [internal.graph.types :as gt]
            [internal.node :as in]
            [internal.system :as is]
            [internal.util :as util]
            [util.coll :refer [pair]])
  (:import [internal.graph.types Arc]
           [java.beans BeanInfo Introspector MethodDescriptor PropertyDescriptor]
           [java.lang.reflect Modifier]
           [javafx.stage Window]))

(set! *warn-on-reflection* true)

(defn workspace []
  0)

(defn project []
  (ffirst (g/targets-of (workspace) :resource-types)))

(defn app-view []
  (ffirst (g/targets-of (project) :_node-id)))

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

(defn deep-select [value-pred value]
  (cond
    (map? value)
    (not-empty
      (into (if (record? value)
              {}
              (empty value))
            (keep (fn [[k v]]
                    (when-some [v' (deep-select value-pred v)]
                      [k v'])))
            value))

    (coll? value)
    (not-empty
      (into (sorted-map)
            (keep-indexed (fn [i v]
                            (when-some [v' (deep-select value-pred v)]
                              [i v'])))
            value))

    (value-pred value)
    value))

(defn deep-keep [value-selector value]
  (cond
    (map? value)
    (not-empty
      (into (if (record? value)
              {}
              (empty value))
            (keep (fn [[k v]]
                    (when-some [v' (deep-keep value-selector v)]
                      [k v'])))
            value))

    (coll? value)
    (not-empty
      (into (sorted-map)
            (keep-indexed (fn [i v]
                            (when-some [v' (deep-keep value-selector v)]
                              [i v'])))
            value))

    :else
    (value-selector value)))

(defn deep-keep-kv
  ([value-selector value]
   (deep-keep-kv value-selector nil value))
  ([value-selector key value]
   (cond
     (map? value)
     (not-empty
       (into (if (record? value)
               {}
               (empty value))
             (keep (fn [[k v]]
                     (when-some [v' (deep-keep-kv value-selector k v)]
                       [k v'])))
             value))

     (coll? value)
     (not-empty
       (into (sorted-map)
             (keep-indexed (fn [i v]
                             (when-some [v' (deep-keep-kv value-selector i v)]
                               [i v'])))
             value))

     :else
     (value-selector key value))))

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
  (-> (view-of-type console/ConsoleNode) (g/targets-of :lines) ffirst))

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
