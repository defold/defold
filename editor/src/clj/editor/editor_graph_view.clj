(ns editor.editor-graph-view
  (:require [clojure.spec.alpha :as s]
            [dynamo.graph :as g]
            [editor.error-reporting :as error-reporting]
            [editor.graph-view :as gv]
            [editor.resource :as resource]
            [editor.resource-node :as resource-node]
            [editor.ui :as ui]
            [internal.node :as in]
            [internal.graph :as ig]
            [internal.graph.types :as gt]
            [util.coll :refer [pair]]))

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
  (assert (g/node-id? node-id))
  (let [node-props (props-for-node node-id basis)
        node-position (initial-position-for-node existing-nodes node-props)]
    (assoc node-props ::gv/position node-position)))

(defn- add-node [nodes basis node-id]
  (conj (or nodes []) (make-node nodes node-id basis)))

(defn- ensure-node [nodes basis node-id]
  (assert (g/node-id? node-id))
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

(defn- expand-connection [graph basis node-id label]
  (assert (g/node-id? node-id))
  (assert (keyword? label))
  (let [target-endpoints (mapv gt/pair->endpoint (g/targets-of basis node-id label))
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
        source-endpoint (gt/endpoint node-id label)
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

(defonce ^:private view-data-atom (gv/make-view-data-atom {}))

(defn show-window! []
  (gv/show-window! view-data-atom error-reporting/report-exception!))

(defn- update-in-view-data! [ks f & args]
  (ui/run-now
    (let [basis (g/now)
          old-view-data @view-data-atom
          new-view-data (apply update-in old-view-data ks f basis args)]
      (when-not (identical? old-view-data new-view-data)
        (reset! view-data-atom new-view-data)
        nil))))

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

(defn refresh! []
  (update-in-view-data! [::gv/graph] refresh-graph))

(defn ensure-node! [node-id]
  (update-in-view-data! [::gv/graph ::gv/nodes] ensure-node node-id))

(defn expand-connection! [node-id label]
  (update-in-view-data! [::gv/graph] expand-connection node-id label))

;; -----------------------------------------------------------------------------

(require '[dev :as dev])
(defn dev-test! []
  (reset! view-data-atom {})
  (ensure-node! (dev/workspace))
  (expand-connection! (dev/workspace) :resource-map)
  (expand-connection! (dev/project) :texture-profiles))
