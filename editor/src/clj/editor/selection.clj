(ns editor.selection
  (:require [dynamo.graph :as g]
            [schema.core :as s]))

(def ^:private TResourceNodeID (s/named g/TNodeID "resource-node-id"))
(def ^:private TSubSelection [(s/one g/TNodeID "selected-node-id") (s/one s/Keyword "property-label") s/Any])
(g/deftype NodeIDsByResourceNodeID {TResourceNodeID [(s/named g/TNodeID "selected-node-id")]})
(g/deftype PropertiesByResourceNodeID {TResourceNodeID [(s/named g/TProperties "selected-node-properties")]})
(g/deftype SubSelectionsByResourceNodeID {TResourceNodeID [(s/named TSubSelection "sub-selection-entry")]})

(g/defnode SelectionNode
  (property all-selections NodeIDsByResourceNodeID
            (set (fn [evaluation-context self _old-value new-value]
                   (let [basis (:basis evaluation-context)]
                     (concat
                       (g/disconnect-sources basis self :all-selected-node-ids)
                       (g/disconnect-sources basis self :all-selected-node-properties)
                       (sequence (comp (mapcat val)
                                       (distinct)
                                       (mapcat (fn [selected-node-id]
                                                 (concat
                                                   (g/connect selected-node-id :_node-id self :all-selected-node-ids)
                                                   (g/connect selected-node-id :_properties self :all-selected-node-properties)))))
                                 new-value))))))
  (property all-sub-selections SubSelectionsByResourceNodeID)

  (input all-selected-node-ids g/NodeID :array)
  (input all-selected-node-properties g/Properties :array)

  (output all-selected-node-ids-set g/Any :cached
          (g/fnk [all-selected-node-ids]
            (set all-selected-node-ids)))
  (output selected-node-ids-by-resource-node NodeIDsByResourceNodeID :cached
          (g/fnk [all-selected-node-ids-set all-selections]
            (into {}
                  (keep (fn [[resource-node selected-node-ids]]
                          (when-some [valid-node-ids (not-empty (filterv all-selected-node-ids-set selected-node-ids))]
                            [resource-node valid-node-ids])))
                  all-selections)))
  (output selected-node-properties-by-resource-node PropertiesByResourceNodeID :cached
          (g/fnk [all-selected-node-properties all-selections]
            (let [properties-by-node-id (into {}
                                              (map (juxt :node-id identity))
                                              all-selected-node-properties)]
              (into {}
                    (keep (fn [[resource-node selected-node-ids]]
                            (let [valid-node-properties (into []
                                                              (keep properties-by-node-id)
                                                              selected-node-ids)]
                              (when (seq valid-node-properties)
                                [resource-node valid-node-properties]))))
                    all-selections))))
  (output sub-selections-by-resource-node SubSelectionsByResourceNodeID :cached
          (g/fnk [all-selected-node-ids-set all-sub-selections]
            (let [valid-sub-selection-entry? (comp all-selected-node-ids-set first)]
              (into {}
                    (keep (fn [[resource-node sub-selection-entries]]
                            (when-some [valid-sub-selection-entries (not-empty (filterv valid-sub-selection-entry? sub-selection-entries))]
                              [resource-node valid-sub-selection-entries])))
                    all-sub-selections)))))

(defn make-selection-node! [graph-id]
  (g/make-node! graph-id SelectionNode))

(defn- update-selection-in-resource [old-selection-by-resource-node resource-node new-selection basis]
  (let [node-exists? (partial g/node-by-id basis)
        valid-entry? (comp node-exists? key)]
    (into {}
          (filter valid-entry?)
          (assoc old-selection-by-resource-node resource-node new-selection))))

(defn select
  ([selection-node resource-node node-ids]
   (select (g/now) selection-node resource-node node-ids))
  ([basis selection-node resource-node node-ids]
   (assert (every? some? node-ids) "Attempting to select nil values")
   (let [selected-node-ids (if (empty? node-ids)
                             [resource-node]
                             (into []
                                   (distinct)
                                   node-ids))]
     (g/update-property selection-node :all-selections update-selection-in-resource resource-node selected-node-ids basis))))

(defn sub-select
  ([selection-node resource-node sub-selection]
   (sub-select (g/now) selection-node resource-node sub-selection))
  ([basis selection-node resource-node sub-selection]
   (g/update-property selection-node :all-sub-selections update-selection-in-resource resource-node sub-selection basis)))

(defn remap-selection
  ([selection-node new-resource-nodes-by-old]
   (g/with-auto-evaluation-context evaluation-context
     (remap-selection evaluation-context selection-node new-resource-nodes-by-old)))
  ([evaluation-context selection-node new-resource-nodes-by-old]
   (let [all-selections (into {}
                              (keep (fn [[old-resource-node :as entry]]
                                      (when-some [new-resource-node (new-resource-nodes-by-old old-resource-node)]
                                        ;; The resource still exists.
                                        ;; If the resource node was replaced,
                                        ;; its owned nodes will also have been
                                        ;; replaced. Selection is no longer
                                        ;; valid, so we simply select the
                                        ;; resource node itself.
                                        (if (not= old-resource-node new-resource-node)
                                          [new-resource-node [new-resource-node]]
                                          entry))))
                              (g/node-value selection-node :all-selections evaluation-context))
         all-sub-selections (into {}
                                  (keep (fn [[old-resource-node :as entry]]
                                          ;; Discard sub-selection for replaced
                                          ;; or deleted resource nodes, since
                                          ;; the contents might have changed
                                          ;; drastically.
                                          (when (= old-resource-node (new-resource-nodes-by-old old-resource-node))
                                            entry)))
                                  (g/node-value selection-node :all-sub-selections evaluation-context))]
     (concat
       (g/set-property selection-node :all-selections all-selections)
       (g/set-property selection-node :all-sub-selections all-sub-selections)))))
