(ns editor.editor-graph-view
  (:require [cljfx.coerce :as fx.coerce]
            [clojure.spec.alpha :as s]
            [dynamo.graph :as g]
            [editor.error-reporting :as error-reporting]
            [editor.graph-view :as gv]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.ui :as ui]
            [internal.node :as in]
            [internal.graph :as ig]
            [internal.graph.types :as gt]
            [util.coll :refer [pair] :as coll])
  (:import [java.util List]))

(defn- node-title [node-type node-id basis]
  (or (when (g/node-type-match? resource/ResourceNode node-type)
        (some-> (resource-node/as-resource-original basis node-id)
                resource/resource-name))
      (name (:k node-type))))

(defn- props-for-node [node-id basis]
  (let [node-type (g/node-type* basis node-id)
        declared-inputs (in/declared-inputs node-type)
        declared-outputs (in/declared-outputs node-type)
        input-arcs (ig/inputs basis node-id)
        output-arcs (ig/outputs basis node-id)
        connected-input-label? (into #{} (map gt/target-label) input-arcs)
        connected-output-label? (into #{} (map gt/source-label) output-arcs)]
    {::gv/id node-id
     ::gv/title (node-title node-type node-id basis)
     ::gv/inputs (into []
                       (map (fn [[input-label input-info]]
                              (cond-> {::gv/id input-label
                                       ::gv/title (name input-label)}
                                      (connected-input-label? input-label)
                                      (assoc ::gv/style-class "connected"))))
                       (sort-by key declared-inputs))
     ::gv/outputs (into []
                        (keep (fn [[output-label output-info]]
                                (when (not= :_output-jammers output-label)
                                  (cond-> {::gv/id output-label
                                           ::gv/title (name output-label)}
                                          (connected-output-label? output-label)
                                          (assoc ::gv/style-class "connected")))))
                        (sort-by key declared-outputs))}))

(defn- initial-position-for-node [existing-nodes node-props]
  ;; TODO: Choose a suitable location based on topology and placement of existing nodes.
  (let [column-width (+ gv/node-width 100.0)
        x (* column-width (count existing-nodes))
        y ^double (or (some-> (first existing-nodes) ::gv/position ui/point-y) 0.0)]
    (ui/point x y)))

(defn- make-node [existing-nodes node-id basis]
  {:pre [(g/node-id? node-id)]}
  (let [node-props (props-for-node node-id basis)
        node-position (initial-position-for-node existing-nodes node-props)]
    (assoc node-props ::gv/position node-position)))

(defn- add-node [nodes basis node-id]
  (conj (or nodes []) (make-node nodes node-id basis)))

(defn- ensure-node [nodes basis node-id]
  {:pre [(g/node-id? node-id)]}
  (if (some #(= node-id (::gv/id %))
            nodes)
    (or nodes [])
    (add-node nodes basis node-id)))

(defn- indices-by-id-map [items]
  (into {}
        (map-indexed (fn [index props]
                       (pair (s/assert ::gv/id (::gv/id props))
                             index)))
        items))

(defn- make-connection [nodes node-id->index arc]
  (let [source-node-props (nodes (node-id->index (gt/source-id arc)))
        target-node-props (nodes (node-id->index (gt/target-id arc)))
        from-position (gv/node-output-jack-position source-node-props (gt/source-label arc))
        to-position (gv/node-input-jack-position target-node-props (gt/target-label arc))]
    {::gv/id arc
     ::gv/from-position from-position
     ::gv/to-position to-position}))

(defn- expand-output [graph basis node-id output-label]
  {:pre [(g/node-id? node-id)
         (keyword? output-label)]}
  (let [target-endpoints (mapv gt/pair->endpoint (g/targets-of basis node-id output-label))
        [nodes node-id->index] (transduce (comp (map gt/endpoint-node-id)
                                                (distinct))
                                          (fn ensure-target-nodes
                                            ([result] result)
                                            ([result target-node-id]
                                             (let [node-id->index (second result)]
                                               (if (contains? node-id->index target-node-id)
                                                 result
                                                 (let [nodes (add-node (first result) basis target-node-id)
                                                       node-index (count node-id->index)
                                                       node-id->index (assoc node-id->index target-node-id node-index)]
                                                   (pair nodes node-id->index))))))
                                          (let [nodes (or (::gv/nodes graph) [])]
                                            (pair nodes
                                                  (indices-by-id-map nodes)))
                                          target-endpoints)
        source-endpoint (gt/endpoint node-id output-label)
        [connections] (reduce (fn ensure-connections [result target-endpoint]
                                (let [arc->index (second result)
                                      arc (gt/arc-from-endpoints source-endpoint target-endpoint)]
                                  (if (contains? arc->index arc)
                                    result
                                    (let [new-connection (make-connection nodes node-id->index arc)
                                          new-connection-index (count arc->index)
                                          connections (conj (first result) new-connection)
                                          arc->index (assoc arc->index arc new-connection-index)]
                                      (pair connections arc->index)))))
                              (let [connections (or (::gv/connections graph) [])]
                                (pair connections
                                      (indices-by-id-map connections)))
                              target-endpoints)]
    (assoc graph
      ::gv/nodes nodes
      ::gv/connections connections)))

(defn- refresh-nodes [nodes basis]
  (let [refreshed-nodes (mapv (fn [node-props]
                                (merge node-props (props-for-node (::gv/id node-props) basis)))
                              nodes)]
    (if (every? true? (map identical? nodes refreshed-nodes))
      nodes
      refreshed-nodes)))

(defn- refresh-connections [connections nodes]
  (let [node-id->index (indices-by-id-map nodes)
        refreshed-connections (mapv (fn [connection-props]
                                (merge connection-props (make-connection nodes node-id->index (::gv/id connection-props))))
                              connections)]
    (if (every? true? (map identical? connections refreshed-connections))
      connections
      refreshed-connections)))

(defn- refresh-graph [graph basis]
  (let [refreshed-nodes (refresh-nodes (::gv/nodes graph) basis)
        refreshed-connections (refresh-connections (::gv/connections graph) refreshed-nodes)]
    (assoc graph
      ::gv/nodes refreshed-nodes
      ::gv/connections refreshed-connections)))

(defn- update-view-data! [view-data-atom f & args]
  (ui/run-now
    (let [basis (g/now)
          old-view-data @view-data-atom
          new-view-data (apply f old-view-data basis args)]
      (when-not (identical? old-view-data new-view-data)
        (reset! view-data-atom new-view-data)
        nil))))

(defn- update-in-view-data! [view-data-atom ks f & args]
  (update-view-data! view-data-atom #(apply update-in %1 ks f %2 args)))

(defn- ensure-node! [view-data-atom node-id]
  (update-in-view-data! view-data-atom [::gv/graph ::gv/nodes] ensure-node node-id))

(defn- expand-output! [view-data-atom node-id output-label]
  (update-in-view-data! view-data-atom [::gv/graph] expand-output node-id output-label))

(defn- update-existing-vector
  ([m k f]
   (if-some [v (get m k)]
     (assoc m k (mapv f v))
     m))
  ([m k f a]
   (if-some [v (get m k)]
     (assoc m k (mapv #(f % a) v))
     m))
  ([m k f a b]
   (if-some [v (get m k)]
     (assoc m k (mapv #(f % a b) v))
     m))
  ([m k f a b c]
   (if-some [v (get m k)]
     (assoc m k (mapv #(f % a b c) v))
     m))
  ([m k f a b c & args]
   (if-some [v (get m k)]
     (assoc m k (mapv #(apply f % a b c args) v))
     m)))

(defn- set-style-class [element ^String style-class enabled]
  {:pre [(string? style-class)]}
  (let [^List old-style-classes (fx.coerce/style-class (::gv/style-class element))]
    (if enabled
      (if old-style-classes
        (if (neg? (coll/index-of old-style-classes style-class))
          (assoc element ::gv/style-class (conj old-style-classes style-class))
          element)
        (assoc element ::gv/style-class [style-class]))
      (if old-style-classes
        (let [index (coll/index-of old-style-classes style-class)]
          (if (neg? index)
            element
            (assoc element ::gv/style-class (coll/remove-at old-style-classes index))))
        element))))

(defn- on-output-clicked [view-data basis node-id output-label]
  (let [clicked-output (gt/endpoint node-id output-label)
        read-inputs #{}
        read-outputs #{}
        written-inputs #{}
        written-outputs (gt/dependencies basis [(gt/endpoint node-id output-label)])]
    (letfn [(apply-jack-highlight [{label ::gv/id :as jack} node-id read-endpoints written-endpoints]
              (let [endpoint (gt/endpoint node-id label)]
                (let [clicked (= clicked-output endpoint)
                      read (and (not clicked) (contains? read-endpoints (gt/endpoint node-id label)))
                      written (and (not clicked) (contains? written-endpoints (gt/endpoint node-id label)))]
                  (-> jack
                      (set-style-class "red" written)
                      (set-style-class "green" read)
                      (set-style-class "blue" clicked)))))
            (apply-node-highlight [{node-id ::gv/id :as node}]
              (-> node
                  (update-existing-vector ::gv/outputs apply-jack-highlight node-id read-outputs written-outputs)
                  (update-existing-vector ::gv/inputs apply-jack-highlight node-id read-inputs written-inputs)))
            (apply-connection-highlight [{arc ::gv/id :as connection}]
              (let [written (contains? written-outputs (gt/source-endpoint arc))]
                (set-style-class connection "red" written)))]
      (update
        view-data ::gv/graph
        (fn [graph]
          (-> graph
              (expand-output basis node-id output-label)
              (update-existing-vector ::gv/nodes apply-node-highlight)
              (update-existing-vector ::gv/connections apply-connection-highlight)))))))

(defn- graph-view-event-handler [view-data-atom {::gv/keys [event-type element-type] :as event}]
  (prn event)
  (when (and (= :node-pressed event-type)
             (= :output element-type))
    (let [{node-id ::gv/id output-label ::gv/element-id} event]
      (update-view-data! view-data-atom on-output-clicked node-id output-label))))

;; -----------------------------------------------------------------------------

(require '[dev :as dev])

(defonce ^:private view-data-atom (gv/make-view-data-atom {}))

(defn show-window! []
  (gv/show-window! view-data-atom #(graph-view-event-handler %1 %2) error-reporting/report-exception!))

(defn dev-refresh! []
  (update-in-view-data! view-data-atom [::gv/graph] refresh-graph))

(defn dev-test! []
  (reset! view-data-atom {})
  (ensure-node! view-data-atom (dev/workspace)))
