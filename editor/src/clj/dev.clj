(ns dev
  (:require [clj-async-profiler.core :as prof]
            [dynamo.graph :as g]
            [editor.asset-browser :as asset-browser]
            [editor.changes-view :as changes-view]
            [editor.console :as console]
            [editor.outline-view :as outline-view]
            [editor.prefs :as prefs]
            [editor.properties-view :as properties-view]
            [editor.ui :as ui]
            [internal.graph :as ig]
            [internal.util :as util])
  (:import [clojure.lang MapEntry]
           [com.sun.javafx.stage StageHelper]
           [java.io File]
           [javafx.stage Stage]))

(set! *warn-on-reflection* true)

(defn- pair [a b]
  (MapEntry/create a b))

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

(defn views-of-type [node-type]
  (keep (fn [node-id]
          (when (g/node-instance? node-type node-id)
            node-id))
        (g/node-ids (g/graph (g/node-id->graph-id (app-view))))))

(defn view-of-type [node-type]
  (first (views-of-type node-type)))

(defn windows []
  (filter #(.isShowing ^Stage %)
          (StageHelper/getStages)))

(def assets-view (partial view-of-type asset-browser/AssetBrowser))
(def changed-files-view (partial view-of-type changes-view/ChangesView))
(def outline-view (partial view-of-type outline-view/OutlineView))
(def properties-view (partial view-of-type properties-view/PropertiesView))

(defn console-view []
  (-> (view-of-type console/ConsoleNode) (g/targets-of :lines) ffirst))

(defn- node-value-type-symbol [node-value-type]
  (symbol (if-some [^Class class (:class (deref node-value-type))]
            (.getSimpleName class)
            (name (:k node-value-type)))))

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

(defn successor-tree
  "Returns a tree of all downstream inputs and outputs affected by a change to
  the specified node id and label. The result is a map of target node id to
  affected labels, recursively."
  ([node-id label]
   (let [basis (ig/update-successors (g/now) {node-id #{label}})]
     (successor-tree basis node-id label)))
  ([basis node-id label]
   (let [graph-id (g/node-id->graph-id node-id)
         successors-by-node-id (get-in basis [:graphs graph-id :successors])]
     (into (sorted-map)
           (map (fn [[successor-node-id successor-labels]]
                  (pair successor-node-id
                        (into (sorted-map)
                              (keep (fn [successor-label]
                                      (when-not (and (= node-id successor-node-id)
                                                     (= label successor-label))
                                        (pair successor-label
                                              (not-empty (successor-tree basis successor-node-id successor-label))))))
                              successor-labels))))
           (get-in successors-by-node-id [node-id label])))))

(defn successor-types
  "Returns a map of all downstream inputs and outputs affected by a change to
  the specified node id and label, recursively. The result is a flat map of
  target node types to the set of affected labels in that node type."
  ([node-id label]
   (let [basis (ig/update-successors (g/now) {node-id #{label}})]
     (successor-types basis node-id label)))
  ([basis node-id label]
   (letfn [(select [[node-id node-successors]]
             (mapcat (fn [[label next-successors]]
                       (cons (pair node-id label)
                             (mapcat select next-successors)))
                     node-successors))]
     (util/group-into (sorted-map)
                      (sorted-set)
                      (comp :k (partial g/node-type* basis) key)
                      val
                      (mapcat select
                              (successor-tree basis node-id label))))))

(defn successor-types*
  "Like successor-types, but you can query multiple inputs at once using a map."
  ([label-seqs-by-node-id]
   (let [label-sets-by-node-id (util/map-vals set label-seqs-by-node-id)
         basis (ig/update-successors (g/now) label-sets-by-node-id)]
     (successor-types* basis label-sets-by-node-id)))
  ([basis label-seqs-by-node-id]
   (reduce (partial merge-with into)
           (sorted-map)
           (for [[node-id labels] label-seqs-by-node-id
                 label labels]
             (successor-types basis node-id label)))))

(defn direct-successor-types
  "Returns a map of all downstream inputs and outputs immediately affected by a
  change to the specified node id and label. The result is a flat map of target
  node types to the set of affected labels in that node type."
  ([node-id label]
   (let [basis (ig/update-successors (g/now) {node-id #{label}})]
     (direct-successor-types basis node-id label)))
  ([basis node-id label]
   (util/group-into (sorted-map)
                    (sorted-set)
                    (comp :k (partial g/node-type* basis) key)
                    val
                    (mapcat (fn [[node-id node-successors]]
                              (map (fn [[label]]
                                     (pair node-id label))
                                   node-successors))
                            (successor-tree basis node-id label)))))

(defn direct-successor-types*
  "Like direct-successor-types, but you can query multiple inputs at once using
  a map."
  ([label-seqs-by-node-id]
   (let [label-sets-by-node-id (util/map-vals set label-seqs-by-node-id)
         basis (ig/update-successors (g/now) label-sets-by-node-id)]
     (direct-successor-types* basis label-sets-by-node-id)))
  ([basis label-seqs-by-node-id]
   (reduce (partial merge-with into)
           (sorted-map)
           (for [[node-id labels] label-seqs-by-node-id
                 label labels]
             (direct-successor-types basis node-id label)))))

;; -----------------------------------------------------------------------------
;; Helpers for clj-async-profiler
;; -----------------------------------------------------------------------------

(defn profile-event-types
  "Print all event types that can be sampled by the profiler. Available options:

  :pid - process to attach to (default: current process)"
  ([] (prof/list-event-types))
  ([options] (prof/list-event-types options)))

(defn profile-start
  "Start the profiler. Available options:

  :pid - process to attach to (default: current process)
  :interval - sampling interval in nanoseconds (default: 1000000 - 1ms)
  :threads - profile each thread separately
  :event - event to profile, see `profile-event-types` (default: :cpu)"
  ([] (profile-start {}))
  ([options] (print (prof/start options))))

(defn profile-stop
  "Stop the currently running profiler and and open the resulting flamegraph in
  the system-configured browser. Available options:

  :pid - process to attach to (default: current process)
  :min-width - minimum width in pixels for a frame to be shown on a flamegraph.
               Use this if the resulting flamegraph is too big and hangs your
               browser (default: nil, recommended: 1-5)
  :reverse? - if true, generate the reverse flamegraph which grows from callees
              up to callers (default: false)
  :icicle? - if true, invert the flamegraph upside down (default: false for
             regular flamegraph, true for reverse)"
  ([] (profile-stop {}))
  ([options]
   (let [augmented-options (assoc options :generate-flamegraph? true)
         ^File flamegraph-svg-file (prof/stop augmented-options)]
     (println "Stopped profiling")
     (ui/open-url (.toURI flamegraph-svg-file))
     nil)))

(defn profile-for
  "Run the profiler for the specified duration. Open the resulting flamegraph in
  the system-configured browser. For available options, see `profile-start` and
  `profile-stop`."
  ([duration-in-seconds]
   (profile-for duration-in-seconds {}))
  ([duration-in-seconds options]
   (profile-start options)
   (Thread/sleep (* duration-in-seconds 1000))
   (profile-stop options)))

(defmacro profile
  "Profile the execution of `body`. If the first argument is a map, treat it as
  options. For available options, see `profile-start` and `profile-stop`. `:pid`
  option is ignored, current process is always profiled."
  [options? & body]
  (let [[options body] (if (map? options?)
                         [(dissoc options? :pid) body]
                         [{} (cons options? body)])]
    `(let [options# ~options]
       (profile-start options#)
       (try
         ~@body
         (finally
           (profile-stop options#))))))
