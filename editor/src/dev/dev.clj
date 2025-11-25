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

(ns dev
  (:require [cljfx.api :as fx]
            [cljfx.fx.h-box :as fx.h-box]
            [cljfx.fx.progress-bar :as fx.progress-bar]
            [cljfx.fx.v-box :as fx.v-box]
            [clojure.pprint :as pprint]
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
            [editor.dialogs :as dialogs]
            [editor.fxui :as fxui]
            [editor.game-object :as game-object]
            [editor.graph-util :as gu]
            [editor.graphics.types :as graphics.types]
            [editor.handler :as handler]
            [editor.localization :as localization]
            [editor.math :as math]
            [editor.outline-view :as outline-view]
            [editor.pipeline.bob :as bob]
            [editor.prefs :as prefs]
            [editor.progress :as progress]
            [editor.properties-view :as properties-view]
            [editor.protobuf :as protobuf]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.scene-cache :as scene-cache]
            [editor.ui :as ui]
            [editor.util :as eutil]
            [editor.workspace :as workspace]
            [integration.test-util :as test-util]
            [internal.graph.types :as gt]
            [internal.node :as in]
            [internal.system :as is]
            [internal.util :as util]
            [jfx :as jfx]
            [lambdaisland.deep-diff2 :as deep-diff]
            [lambdaisland.deep-diff2.minimize-impl :as deep-diff.minimize-impl]
            [lambdaisland.deep-diff2.printer-impl :as deep-diff.printer-impl]
            [lambdaisland.deep-diff2.puget.color :as puget.color]
            [lambdaisland.deep-diff2.puget.printer :as puget.printer]
            [potemkin.namespaces :as namespaces]
            [service.log :as log]
            [util.coll :as coll :refer [pair]]
            [util.debug-util]
            [util.diff :as diff]
            [util.eduction :as e]
            [util.fn :as fn])
  (:import [com.defold.util WeakInterner]
           [com.dynamo.bob Platform]
           [com.dynamo.graphics.proto Graphics$TextureImage Graphics$TextureImage$Image]
           [com.google.protobuf Descriptors$FieldDescriptor Descriptors$FieldDescriptor$JavaType]
           [editor.code.data Cursor CursorRange]
           [editor.gl.pass RenderPass]
           [editor.gl.vertex2 VertexBuffer]
           [editor.resource FileResource MemoryResource ZipResource]
           [editor.types AABB]
           [editor.workspace BuildResource]
           [internal.graph.types Arc Endpoint]
           [java.beans BeanInfo Introspector MethodDescriptor PropertyDescriptor]
           [java.lang.reflect Modifier]
           [java.nio ByteBuffer]
           [javafx.scene Node]
           [javafx.stage Window]
           [javax.vecmath Matrix3d Matrix3f Matrix4d Matrix4f Point2d Point3d Point4d Quat4d Vector2d Vector3d Vector4d]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(namespaces/import-vars
  [util.debug-util stack-trace]
  [integration.test-util outline-node-id outline-node-info resource-outline-node-id resource-outline-node-info])

(defn javafx-tree [obj]
  (jfx/info-tree obj))

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
  (let [resource-node (project/get-resource-node (project) path-or-resource)]
    (if (some? resource-node)
      resource-node
      (throw (ex-info (str "Resource not found: '" path-or-resource "'")
                      {:path-or-resource path-or-resource})))))

(defn selection []
  (->> (g/node-value (project) :selected-node-ids-by-resource-node)
       (keep (fn [[resource-node selected-nodes]]
               (let [targets (g/targets-of resource-node :node-outline)]
                 (when (some (fn [[_target-node target-label]]
                               (= :active-outline target-label)) targets)
                   selected-nodes))))
       first))

(def sel (comp first selection))

(defn node-info
  ([node-id]
   {:pre [(some? node-id)
          (g/node-id? node-id)]}
   (g/with-auto-evaluation-context evaluation-context
     (node-info node-id evaluation-context)))
  ([node-id evaluation-context]
   {:pre [(some? node-id)
          (g/node-id? node-id)]}
   (let [basis (:basis evaluation-context)
         original-node-id (g/override-original basis node-id)
         override-node-ids (g/overrides basis node-id)]
     (cond-> (into (array-map :node-id node-id)
                   (gu/node-debug-info node-id evaluation-context))

             (some? original-node-id)
             (assoc :original-node-id original-node-id)

             (coll/not-empty override-node-ids)
             (assoc :override-node-ids override-node-ids)))))

(defn outline-labels [node-id & outline-labels]
  (into (sorted-set)
        (map :label)
        (:children (apply outline-node-info node-id outline-labels))))

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

     (throw (ex-info "The specified node cannot be resolved to a CollectionNode."
                     {:node-id node-id
                      :node-type (g/node-type* basis node-id)})))))

(defn prefs []
  (prefs/project (workspace/project-directory (workspace))))

(defn localization []
  (some #(-> % :env :localization) (ui/contexts (ui/main-scene))))

(declare ^:private exclude-keys-deep-helper)

(defn- exclude-keys-deep-value-helper [excluded-map-entry? value]
  (cond
    (map? value)
    (exclude-keys-deep-helper excluded-map-entry? value)

    (coll? value)
    (coll/transform value
      (map (partial exclude-keys-deep-value-helper excluded-map-entry?)))

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
              (letfn [(finalize-into [target-map value]
                        (into (with-meta target-map
                                         (meta value))
                              (keep (fn [[k v]]
                                      (when-some [v' (util/deep-keep-kv-helper deep-keep-finalize-coll-value-fn wrapped-value-fn k v)]
                                        (pair k v'))))
                              value))]
                (try
                  (finalize-into (sorted-map) value)
                  (catch ClassCastException _
                    (finalize-into {} value))))))]
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

(defn focused-control
  ^Node []
  (some-> (ui/main-scene)
          (ui/focus-owner)))

(defn command-contexts
  ([]
   (when-some [focused-control (focused-control)]
     (command-contexts focused-control)))
  ([^Node control]
   (ui/node-contexts control true)))

(defn command-env
  ([command]
   (command-env (command-contexts) command nil))
  ([command user-data]
   (command-env (command-contexts) command user-data))
  ([command-contexts command user-data]
   (let [[_handler command-context] (handler/active command command-contexts user-data)]
     (:env command-context))))

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
  supplied, the node keys will be a pair of the node-type-kw and the node-id."
  ([node-id label]
   (successor-tree (g/now) node-id label))
  ([basis node-id label]
   (let [direct-connected-successors-fn (make-direct-connected-successors-fn basis)]
     (util/group-into
       (sorted-map)
       (sorted-map)
       (fn key-fn [[successor-node-id]]
         (pair (g/node-type-kw basis successor-node-id)
               successor-node-id))
       (fn value-fn [[successor-node-id successor-label]]
         (pair successor-label
               (not-empty
                 (successor-tree basis successor-node-id successor-label))))
       (direct-successors* direct-connected-successors-fn basis [(pair node-id label)])))))

(defn- successor-types-impl [successors-fn basis node-id-and-label-pairs]
  (let [direct-connected-successors-fn (make-direct-connected-successors-fn basis)]
    (into (sorted-map)
          (map (fn [[node-type-kw successor-labels]]
                 (pair node-type-kw
                       (into (sorted-map)
                             (frequencies successor-labels)))))
          (util/group-into {} []
                           (comp (partial g/node-type-kw basis) first)
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

(defn- flipped-descending-pairs [key-num-pairs]
  (->> key-num-pairs
       (map coll/flip)
       (sort (fn [[^long amount-a entry-a] [^long amount-b entry-b]]
               (cond (< amount-a amount-b) 1
                     (< amount-b amount-a) -1
                     (and (instance? Comparable entry-a)
                          (instance? Comparable entry-b)) (compare entry-a entry-b)
                     :else 0)))
       (vec)))

(defn ordered-occurrences
  "Returns a sorted list of [occurrence-count entry]. The list is sorted by
  occurrence count in descending order."
  [coll]
  (flipped-descending-pairs (frequencies coll)))

(defn make-report [pair-fn coll]
  "Produce a list of [sum category] pairs from a sequence of items. The
  resulting list will be in in descending order. The pair-fn is called for each
  item in the sequence, and is expected to return a [category value] pair.
  Returning nil from the pair-fn will exclude the item from the sum.
  Otherwise, the values will be summed for each category to create the list."
  (flipped-descending-pairs
    (coll/aggregate-into {} + (e/keep pair-fn coll))))

(defn cached-output-report
  "Returns a sorted list of what node outputs are in the system cache in the
  format [entry-occurrence-count [node-type-kw output-label]]. The list is
  sorted by entry occurrence count in descending order."
  []
  (let [system @g/*the-system*
        basis (is/basis system)]
    (ordered-occurrences
      (map (fn [[endpoint]]
             (let [node-id (g/endpoint-node-id endpoint)
                   output-label (g/endpoint-label endpoint)
                   node-type-kw (g/node-type-kw basis node-id)]
               (pair node-type-kw output-label)))
           (is/system-cache system)))))

(defn cached-output-name-report
  "Returns a sorted list of what output names are in the system cache in the
  format [entry-occurrence-count output-label]. The list is sorted by entry
  occurrence count in descending order."
  []
  (ordered-occurrences
    (e/map (comp g/endpoint-label key)
           (is/system-cache @g/*the-system*))))

(defn node-type-report
  "Returns a sorted list of what node types are in the system graph in the
  format [node-count node-type-kw]. The list is sorted by node count in
  descending order."
  []
  (let [system @g/*the-system*
        graphs (is/graphs system)]
    (ordered-occurrences
      (eduction
        (mapcat (fn [[_graph-id graph]]
                  (vals (:nodes graph))))
        (map (comp :k g/node-type))
        graphs))))

(defn- ns->namespace-name
  ^String [ns]
  (name (ns-name ns)))

(defn- class->canonical-symbol [^Class class]
  (symbol (.getName class)))

(defn- make-alias-names-by-namespace-name [ns]
  (into {(ns->namespace-name 'clojure.core) nil
         (ns->namespace-name ns) nil}
        (map (fn [[alias-symbol referenced-ns]]
               (pair (ns->namespace-name referenced-ns)
                     (name alias-symbol))))
        (ns-aliases ns)))

(defn- make-simple-symbols-by-canonical-symbol [ns]
  (into {}
        (map (fn [[alias-symbol imported-class]]
               (pair (class->canonical-symbol imported-class)
                     alias-symbol)))
        (ns-imports ns)))

(defn- simplify-namespace-name [namespace-name alias-names-by-namespace-name]
  {:pre [(or (nil? namespace-name) (string? namespace-name))
         (map? alias-names-by-namespace-name)]}
  (let [alias-name (get alias-names-by-namespace-name namespace-name ::not-found)]
    (case alias-name
      ::not-found namespace-name
      alias-name)))

(defn- simplify-symbol-name [symbol-name]
  (string/replace symbol-name
                  #"__(\d+)__auto__$"
                  "#"))

(defn- simplify-symbol [expression alias-names-by-namespace-name]
  (-> expression
      (namespace)
      (simplify-namespace-name alias-names-by-namespace-name)
      (symbol (-> expression name simplify-symbol-name))
      (with-meta (meta expression))))

(defn- simplify-keyword [expression alias-names-by-namespace-name]
  (-> expression
      (namespace)
      (simplify-namespace-name alias-names-by-namespace-name)
      (keyword (name expression))))

(defn- simplify-expression-impl [expression alias-names-by-namespace-name simple-symbols-by-canonical-symbol]
  (cond
    (record? expression)
    expression

    (map? expression)
    (into (coll/empty-with-meta expression)
          (map (fn [[key value]]
                 (pair (simplify-expression-impl key alias-names-by-namespace-name simple-symbols-by-canonical-symbol)
                       (simplify-expression-impl value alias-names-by-namespace-name simple-symbols-by-canonical-symbol))))
          expression)

    (or (vector? expression)
        (set? expression))
    (into (coll/empty-with-meta expression)
          (map #(simplify-expression-impl % alias-names-by-namespace-name simple-symbols-by-canonical-symbol))
          expression)

    (coll/list-or-cons? expression)
    (into (coll/empty-with-meta expression)
          (map #(simplify-expression-impl % alias-names-by-namespace-name simple-symbols-by-canonical-symbol))
          (reverse expression))

    (symbol? expression)
    (or (get simple-symbols-by-canonical-symbol expression)
        (simplify-symbol expression alias-names-by-namespace-name))

    (keyword? expression)
    (simplify-keyword expression alias-names-by-namespace-name)

    :else
    expression))

(defmacro simplify-expression
  ([expression]
   `(simplify-expression *ns* ~expression))
  ([ns expression]
   `(let [ns# ~ns]
      (#'simplify-expression-impl
        ~expression
        (#'make-alias-names-by-namespace-name ns#)
        (#'make-simple-symbols-by-canonical-symbol ns#)))))

(defn- pprint-code-impl [expression]
  (binding [pprint/*print-suppress-namespaces* false
            pprint/*print-right-margin* 100
            pprint/*print-miser-width* 60]
    (pprint/with-pprint-dispatch
      pprint/code-dispatch
      (pprint/pprint expression))))

(defmacro pprint-code
  "Pretty-print the supplied code expression while attempting to retain readable
  formatting. Useful when developing macros."
  ([expression]
   `(#'pprint-code-impl (simplify-expression ~expression)))
  ([ns expression]
   `(#'pprint-code-impl (simplify-expression ~ns ~expression))))

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

(defn- percentage-str [^long n ^long total]
  (eutil/format* "%.2f%%" (* 100.0 (double (/ n total)))))

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
                                            (str (name (g/node-type-kw basis (gt/endpoint-node-id source)))
                                                 " -> "
                                                 (name (g/node-type-kw basis (gt/endpoint-node-id target)))))
                                (:override :internal) (fn [[source]]
                                                        (name (g/node-type-kw basis (gt/endpoint-node-id source))))))
                         frequencies
                         (sort-by (comp - val))
                         (run! (fn [[label group-count]]
                                 (println (format "  %7s %s (%s total)"
                                                  (percentage-str group-count successor-count)
                                                  label
                                                  group-count))))))))))))

(defn- successor-pair-class [basis source-endpoint target-endoint]
  [(g/node-type-kw basis (gt/endpoint-node-id source-endpoint))
   (gt/endpoint-label source-endpoint)
   (g/node-type-kw basis (gt/endpoint-node-id target-endoint))
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
             (fn [^long i pair-class]
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
          (run! (fn [{:keys [pair-class direct-count ^long indirect-count]}]
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

(definline weak-interner-values [^WeakInterner weak-interner]
  `(.getValues ~(with-meta weak-interner {:tag `WeakInterner})))

(defn weak-interner-stats [^WeakInterner weak-interner]
  (let [info (weak-interner-info weak-interner)
        hash-table (:hash-table info)
        entry-count (:count info)
        capacity (count hash-table)
        occupancy-factor (/ (double entry-count) (double capacity))

        next-capacity
        (coll/first-where
          (fn [^long num]
            (< capacity num))
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

(defn clear-caches!
  "Clears the various caches used to enhance the performance of the editor and
  the tests. You can specify which caches to clear. When called with no
  arguments, clears all caches except the downloaded library dependency cache.

  Optional args:
    :library  Clear the downloaded library dependency cache.
    :project  Clear the loaded project cache used by tests.
    :scene    Clear the scene cache used to render Scene Views.
    :system   Clear the system cache used to cache node outputs."
  ([]
   (clear-caches! :project :scene :system))
  ([& cache-kws]
   (let [clear-cache? (set cache-kws)]
     (when (clear-cache? :library)
       (test-util/clear-cached-libraries!))
     (when (clear-cache? :project)
       (test-util/clear-cached-projects!))
     (when (clear-cache? :scene)
       (scene-cache/clear-all!))
     (when (and (g/cache)
                (clear-cache? :system))
       (g/clear-system-cache!)))))

(set! *warn-on-reflection* false)

(defn- buf-clj-attribute-data [^ByteBuffer buf ^long buf-vertex-attribute-offset ^long attribute-byte-size buffer-data-type]
  (let [primitive-type-kw (buffers/primitive-type-kw buffer-data-type)
        read-buf (-> buf
                     (.slice buf-vertex-attribute-offset attribute-byte-size)
                     (.order (.order buf))
                     (buffers/as-typed-buffer buffer-data-type))]
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
                                attribute-byte-size (graphics.types/attribute-info-byte-size vertex-attribute)
                                buffer-data-type (graphics.types/data-type-buffer-data-type (:data-type vertex-attribute))
                                clj-vertex-attribute-data (buf-clj-attribute-data buf buf-vertex-attribute-offset attribute-byte-size buffer-data-type)
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
              (let [printer (cond-> printer printer-opts (merge printer-opts))]
                (cls-tag-doc
                  printer
                  (class object)
                  (fmt-doc printer (object->value object)))))]

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
               (fmt-doc printer (symbol "pass" (.name render-pass))))

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

              {(namespaced-class-symbol BuildResource)
               project-resource-pprint-handler

               (namespaced-class-symbol FileResource)
               project-resource-pprint-handler

               (namespaced-class-symbol MemoryResource)
               (fn memory-resource-pprint-handler [printer resource]
                 (object-data-pprint-handler nil (comp vector resource/ext) printer resource))

               (namespaced-class-symbol ZipResource)
               project-resource-pprint-handler})

            vecmath-pprint-handlers
            (letfn [(vecmath-tuple-pprint-handler [printer tuple]
                      (object-data-pprint-handler nil math/vecmath->clj printer tuple))

                    (vecmath-matrix-pprint-handler [printer matrix]
                      (let [row-col-strs (math/vecmath-matrix-pprint-strings matrix)
                            fmt-col (fn [^String num-str]
                                      ;; Colorize zero values differently.
                                      (let [element (if (math/zero-vecmath-matrix-col-str? num-str)
                                                      :number
                                                      :string)]
                                        (col-txt printer element num-str)))
                            data (coll/transfer row-col-strs [:align]
                                   (map (fn [col-strs]
                                          (interpose " " (map fmt-col col-strs))))
                                   (interpose :break))]
                        [:group
                         (cls-tag printer (class matrix))
                         (col-doc printer :delimiter "[")
                         data
                         (col-doc printer :delimiter "]")]))]

              {(namespaced-class-symbol Matrix3d)
               vecmath-matrix-pprint-handler

               (namespaced-class-symbol Matrix3f)
               vecmath-matrix-pprint-handler

               (namespaced-class-symbol Matrix4d)
               vecmath-matrix-pprint-handler

               (namespaced-class-symbol Matrix4f)
               vecmath-matrix-pprint-handler

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
               vecmath-tuple-pprint-handler})

            protobuf-pprint-handlers
            {(namespaced-class-symbol Graphics$TextureImage)
             (partial object-data-pprint-handler {:sort-keys false}
                      (fn [^Graphics$TextureImage texture-image]
                        (let [alternatives (.getAlternativesList texture-image)
                              alternatives-count (count alternatives)
                              ^Graphics$TextureImage$Image image (first alternatives)]
                          (cond-> {:type (protobuf/pb-enum->val (.getType texture-image))}

                                  image
                                  (assoc :format (protobuf/pb-enum->val (.getFormat image))
                                         :width (.getWidth image)
                                         :height (.getHeight image))

                                  (> alternatives-count 1)
                                  (assoc :alternatives alternatives-count)

                                  :always
                                  (assoc :bytes (transduce (map (fn [^Graphics$TextureImage$Image image]
                                                                  (.getDataSize image)))
                                                           +
                                                           alternatives))))))}]

        (deep-diff/printer
          {:color-scheme
           {::deep-diff.printer-impl/deletion [:red]
            ::deep-diff.printer-impl/insertion [:green]
            ::deep-diff.printer-impl/other [:yellow]
            :boolean [:bold :cyan]
            :nil [:bold :cyan]
            :tag [:magenta]}

           :extra-handlers
           (merge editor-pprint-handlers
                  graph-pprint-handlers
                  java-pprint-handlers
                  resource-pprint-handlers
                  vecmath-pprint-handlers
                  protobuf-pprint-handlers)})))))

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

(defn build-output-infos [project proj-path]
  (let [resource-node (project/get-resource-node project proj-path)]
    (assert (some? resource-node) (format "Resource node not found for: '%s'" proj-path))
    (test-util/build-node! resource-node)
    (let [build-resource (test-util/node-build-resource resource-node)
          build-output-path (resource/proj-path build-resource)
          workspace (resource/workspace build-resource)]
      (test-util/make-build-output-infos-by-path workspace build-output-path))))

(defn bob-build-output-infos [project proj-path]
  (test-util/save-project! project)
  (let [bob-commands ["build"]
        bob-args {"platform" (.getPair (Platform/getHostPlatform))}
        result (bob/invoke! project bob-args bob-commands)]
    (when-let [exception (:exception result)]
      (throw exception))
    (if-let [error (:error result)]
      error
      (let [build-resource (test-util/build-resource project proj-path)
            build-output-path (resource/proj-path build-resource)
            workspace (resource/workspace build-resource)]
        (test-util/make-build-output-infos-by-path workspace build-output-path)))))

(defn- build-output-infos->diff-data [build-output-infos]
  (into (sorted-map)
        (map (fn [[build-output-path build-output-info]]
               (pair build-output-path
                     (select-keys build-output-info [:pb-map :dep-paths]))))
        build-output-infos))

(defn diff-editor-and-bob-build-output!
  ([project proj-path]
   (diff-editor-and-bob-build-output! project proj-path nil))
  ([project proj-path opts]
   (let [editor-build-output-infos (build-output-infos project proj-path)
         bob-build-output-infos (bob-build-output-infos project proj-path)]
     (diff-values! (build-output-infos->diff-data editor-build-output-infos)
                   (build-output-infos->diff-data bob-build-output-infos)
                   opts))))

(defn pb-class-info
  ([^Class pb-class]
   (pb-class-info pb-class fn/constantly-true))
  ([^Class pb-class field-info-predicate]
   (into (sorted-map)
         (keep (fn [^Descriptors$FieldDescriptor field-desc]
                 (let [field-name (.getName field-desc)
                       field-value-class (protobuf/field-value-class pb-class field-desc)
                       field-rule (cond (.isRepeated field-desc) :repeated
                                        (.isRequired field-desc) :required
                                        (.isOptional field-desc) :optional
                                        :else (assert false))
                       field-info (cond-> {:value-type field-value-class
                                           :field-rule field-rule}

                                          (= Descriptors$FieldDescriptor$JavaType/MESSAGE (.getJavaType field-desc))
                                          (assoc :message (pb-class-info field-value-class field-info-predicate)))]
                   (when (field-info-predicate field-info)
                     (pair field-name field-info)))))
         (.getFields (protobuf/pb-class->descriptor pb-class)))))

(defn pb-resource-type-info
  ([workspace]
   (pb-resource-type-info workspace fn/constantly-true))
  ([workspace field-info-predicate]
   (into (sorted-map)
         (keep (fn [[ext {:keys [test-info] :as _resource-type}]]
                 (when-let [pb-class (:ddf-type test-info)]
                   (let [read-defaults (:read-defaults test-info)
                         pb-class-info (pb-class-info pb-class field-info-predicate)]
                     (pair ext {:read-defaults read-defaults
                                :value-type pb-class
                                :message pb-class-info})))))
         (workspace/get-resource-type-map workspace))))

(defn pb-resource-exts-that-read-defaults [workspace]
  (test-util/protobuf-resource-exts-that-read-defaults workspace))

(def class-name-comparator #(compare (.getName ^Class %1) (.getName ^Class %2)))

(defn resource-pb-classes [workspace]
  (letfn [(info->value-types [{:keys [message value-type]}]
            (cond->> (mapcat info->value-types (vals message))
                     (and message value-type) (cons value-type)))]
    (into (sorted-set-by class-name-comparator)
          (mapcat info->value-types)
          (vals (pb-resource-type-info workspace)))))

(defn resource-pb-class-field-types
  ([workspace]
   (resource-pb-class-field-types workspace fn/constantly-true))
  ([workspace field-info-predicate]
   (into (sorted-map-by class-name-comparator)
         (keep (fn [^Class pb-class]
                 (some->> (into (sorted-map)
                                (keep (fn [[field-name {:keys [^Class value-type] :as field-info}]]
                                        (when (field-info-predicate field-info)
                                          (pair field-name value-type))))
                                (pb-class-info pb-class))
                          (not-empty)
                          (pair pb-class))))
         (resource-pb-classes workspace))))

(defn- progress-dialog-ui [{:keys [header-text progress localization] :as props}]
  {:pre [(string? header-text)
         (map? progress)
         (localization/message-pattern? (progress/message progress))]}
  {:fx/type dialogs/dialog-stage
   :on-close-request {:event-type :cancel}
   :showing (fxui/dialog-showing? props)
   :header {:fx/type fx.h-box/lifecycle
            :style-class "spacing-default"
            :alignment :center-left
            :children [{:fx/type fxui/legacy-label
                        :variant :header
                        :text header-text}]}
   :content {:fx/type fx.v-box/lifecycle
             :style-class ["dialog-content-padding" "spacing-smaller"]
             :children [{:fx/type fxui/legacy-label
                         :wrap-text false
                         :text (localization (progress/message progress))}
                        {:fx/type fx.progress-bar/lifecycle
                         :max-width Double/MAX_VALUE
                         :progress (or (progress/fraction progress)
                                       -1.0)}]} ; Indeterminate.
   :footer {:fx/type dialogs/dialog-buttons
            :children [{:fx/type fxui/button
                        :text "Cancel"
                        :cancel-button true
                        :on-action {:event-type :cancel}}]}})

(defn run-with-progress
  ([^String header-text worker-fn]
   (run-with-progress header-text nil worker-fn))
  ([^String header-text cancel-result worker-fn]
   (ui/run-now
     (let [state-atom (atom {:progress (progress/make (localization/message "progress.waiting") 0 0)})
           localization (g/with-auto-evaluation-context evaluation-context
                          (workspace/localization (workspace) evaluation-context))
           middleware (fx/wrap-map-desc assoc :fx/type progress-dialog-ui :header-text header-text :localization localization)
           opts {:fx.opt/map-event-handler
                 (fn [event]
                   (case (:event-type event)
                     :cancel (swap! state-atom assoc ::fxui/result cancel-result)))}
           renderer (fx/create-renderer :middleware middleware :opts opts)
           render-progress! #(swap! state-atom assoc :progress %)]
       (future
         (try
           (swap! state-atom assoc ::fxui/result (worker-fn render-progress!))
           (catch Throwable e
             (log/error :exception e)
             (swap! state-atom assoc ::fxui/result e))))
       (let [result (fxui/mount-renderer-and-await-result! state-atom renderer)]
         (if (instance? Throwable result)
           (throw result)
           result))))))

(defn node-load-infos-by-proj-path [node-load-infos]
  (coll/pair-map-by
    #(resource/proj-path (:resource %))
    node-load-infos))

(defn ext-resource-predicate [& type-exts]
  (let [type-ext-set (set type-exts)]
    (fn resource-matches-type-exts? [resource]
      (contains? type-ext-set (resource/type-ext resource)))))

(defn proj-path-resource-predicate [proj-path]
  (fn resource-matches-proj-path? [resource]
    (= proj-path (resource/proj-path resource))))

(defn referencing-proj-path-sets-by-proj-path
  ([node-load-infos-by-proj-path]
   (referencing-proj-path-sets-by-proj-path node-load-infos-by-proj-path resource/stateful?))
  ([node-load-infos-by-proj-path scan-resource?]
   (util/group-into
     (sorted-map) (sorted-set) key val
     (eduction
       (mapcat
         (fn [[referencing-proj-path referencing-node-load-info]]
           (when (scan-resource? (:resource referencing-node-load-info))
             (e/map #(pair % referencing-proj-path)
                    (:dependency-proj-paths referencing-node-load-info)))))
       (distinct)
       node-load-infos-by-proj-path))))

(defn dependency-proj-path-sets-by-proj-path
  ([node-load-infos-by-proj-path recursive]
   (dependency-proj-path-sets-by-proj-path node-load-infos-by-proj-path recursive fn/constantly-true))
  ([node-load-infos-by-proj-path recursive include-dependency-resource?]
   {:pre [(map? node-load-infos-by-proj-path)
          (ifn? include-dependency-resource?)]}
   (let [basis (g/now)

         workspace
         (some (fn [[_proj-path node-load-info]]
                 (some-> (:resource node-load-info)
                         (resource/workspace)))
               node-load-infos-by-proj-path)

         include-dependency-proj-path?
         (fn include-dependency-proj-path? [dependency-proj-path]
           (include-dependency-resource?
             (if-some [dependency-node-info (node-load-infos-by-proj-path dependency-proj-path)]
               (:resource dependency-node-info)
               (workspace/file-resource basis workspace dependency-proj-path))))] ; Referencing a missing resource.

     (if recursive
       (let [referencing-proj-path-sets-by-proj-path (referencing-proj-path-sets-by-proj-path node-load-infos-by-proj-path)]
         (letfn [(recursive-referencing-entries [dependency-proj-path]
                   (e/mapcat
                     (fn [referencing-proj-path]
                       (cons (pair referencing-proj-path dependency-proj-path)
                             (recursive-referencing-entries referencing-proj-path)))
                     (referencing-proj-path-sets-by-proj-path dependency-proj-path)))]
           (util/group-into
             (sorted-map) (sorted-set) key val
             (e/mapcat
               (fn [[dependency-proj-path]]
                 (when (include-dependency-proj-path? dependency-proj-path)
                   (recursive-referencing-entries dependency-proj-path)))
               referencing-proj-path-sets-by-proj-path))))
       (into (sorted-map)
             (keep
               (fn [[proj-path node-load-info]]
                 (some->> (:dependency-proj-paths node-load-info)
                          (into (sorted-set)
                                (filter include-dependency-proj-path?))
                          (coll/not-empty)
                          (pair proj-path))))
             node-load-infos-by-proj-path)))))

(defn dependency-proj-path-tree
  ([dependency-proj-path-sets-by-proj-path]
   (dependency-proj-path-tree dependency-proj-path-sets-by-proj-path (keys dependency-proj-path-sets-by-proj-path)))
  ([dependency-proj-path-sets-by-proj-path proj-paths]
   (->> proj-paths
        (into (empty dependency-proj-path-sets-by-proj-path)
              (map (fn [proj-path]
                     (pair proj-path
                           (dependency-proj-path-tree
                             dependency-proj-path-sets-by-proj-path
                             (dependency-proj-path-sets-by-proj-path proj-path))))))
        (coll/not-empty))))

(defn tree-depth
  ^long [tree]
  (transduce
    (map (fn [entry]
           (inc (tree-depth (val entry)))))
    max
    0
    tree))

(defn dependency-proj-path-report
  [dependency-proj-path-sets-by-proj-path]
  (->> dependency-proj-path-sets-by-proj-path
       (dependency-proj-path-tree)
       (sort-by #(tree-depth (val %))
                coll/descending-order)
       (take 30)))

(defn scene-view-batches
  ([scene-view pass]
   (scene-view-batches scene-view pass :batch-key))
  ([scene-view pass key-fn]
   {:pre [(g/node-id? scene-view)
          (instance? RenderPass pass)
          (ifn? key-fn)]}
   ;; Based on the scene/batch-render function.
   (let [flat-renderables (g/node-value scene-view :all-renderables)]
     (loop [renderables (get flat-renderables pass)
            offset 0
            batch-index 0
            batches (transient [])]
       (if-let [renderable (first renderables)]
         (let [first-key (key-fn renderable)
               first-render-fn (:render-fn renderable)
               batch-count (long
                             (loop [renderables (rest renderables)
                                    batch-count 1]
                               (let [renderable (first renderables)
                                     key (key-fn renderable)
                                     render-fn (:render-fn renderable)
                                     break (or (not= first-render-fn render-fn)
                                               (nil? first-key)
                                               (nil? key)
                                               (not= first-key key))]
                                 (if break
                                   batch-count
                                   (recur (rest renderables) (inc batch-count))))))]
           (when (> batch-count 0)
             (let [batch (subvec renderables 0 batch-count)]
               (recur (subvec renderables batch-count)
                      (+ offset batch-count)
                      (inc batch-index)
                      (conj! batches batch)))))
         (persistent! batches))))))

(defn clear-enable-all! []
  (clear-caches!)
  (handler/enable-disabled-handlers!)
  (ui/enable-stopped-timers!)
  (println "Re-enabled all disabled handlers and timers")
  nil)
