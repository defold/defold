(ns editor.selection
  (:require [dynamo.graph :as g]
            [schema.core :as s]))

(def ^:private TResourceNodeID (s/named g/TNodeID "resource-node-id"))
(def ^:private TSubSelection [(s/one g/TNodeID "selected-node-id") s/Any])
(g/deftype NodeIDsByResourceNodeID {TResourceNodeID [(s/named g/TNodeID "selected-node-id")]})
(g/deftype PropertiesByResourceNodeID {TResourceNodeID [(s/named g/TProperties "selected-node-properties")]})
(g/deftype SubSelectionsByResourceNodeID {TResourceNodeID [(s/named TSubSelection "sub-selection-entry")]})

(g/defnode SelectionNode
  ;; NOTE: These are for internal use only. The truth should be obtained from
  ;; the selected-node-ids-by-resource-node and sub-selections-by-resource-node
  ;; outputs, since these can contain stale node ids. This is also the reason
  ;; why we establish connections in the set-all-selections function instead of
  ;; from a property setter function - the property setter will only be called
  ;; in case the value differs. If this test is performed on a stale value, the
  ;; connections will not be updated in some cases. Notably, this can happen if
  ;; you delete a node and then undo. The deleted node would not be selected as
  ;; it was re-introduced by the undo.
  (property all-selections NodeIDsByResourceNodeID)
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

(defn- update-selection-in-resource [old-selection-by-resource-node resource-node new-selection]
  (assoc old-selection-by-resource-node resource-node new-selection))

(defn- set-all-selections [basis selection-node selected-node-ids-by-resource-node]
  (concat
    (g/set-property selection-node :all-selections selected-node-ids-by-resource-node)
    (g/disconnect-sources basis selection-node :all-selected-node-ids)
    (g/disconnect-sources basis selection-node :all-selected-node-properties)
    (sequence (comp (mapcat val)
                    (distinct)
                    (mapcat (fn [selected-node-id]
                              (concat
                                (g/connect selected-node-id :_node-id selection-node :all-selected-node-ids)
                                (g/connect selected-node-id :_properties selection-node :all-selected-node-properties)))))
              selected-node-ids-by-resource-node)))

(defn select
  ([selection-node resource-node node-ids]
   (g/with-auto-evaluation-context evaluation-context
     (select evaluation-context selection-node resource-node node-ids)))
  ([evaluation-context selection-node resource-node node-ids]
   (assert (g/node-id? resource-node))
   (assert (every? g/node-id? node-ids))
   (let [basis (:basis evaluation-context)
         new-selected-node-ids (if (seq node-ids)
                                 (into [] (distinct) node-ids)
                                 [resource-node])
         old-all-selections (g/node-value selection-node :selected-node-ids-by-resource-node evaluation-context)
         new-all-selections (update-selection-in-resource old-all-selections resource-node new-selected-node-ids)]
     (when (not= old-all-selections new-all-selections)
       (set-all-selections basis selection-node new-all-selections)))))

(defn selected
  ([selection-node resource-node]
   (g/with-auto-evaluation-context evaluation-context
     (selected evaluation-context selection-node resource-node)))
  ([evaluation-context selection-node resource-node]
   (get (g/node-value selection-node :selected-node-ids-by-resource-node evaluation-context)
        resource-node)))

(defn sub-select [selection-node resource-node sub-selection]
  (assert (g/node-id? resource-node))
  (g/update-property selection-node :all-sub-selections update-selection-in-resource resource-node sub-selection))

(defn sub-selected
  ([selection-node resource-node]
   (g/with-auto-evaluation-context evaluation-context
     (sub-selected evaluation-context selection-node resource-node)))
  ([evaluation-context selection-node resource-node]
   (get (g/node-value selection-node :sub-selections-by-resource-node evaluation-context)
        resource-node)))

(defn remap-selection
  ([selection-node new-resource-nodes-by-old]
   (g/with-auto-evaluation-context evaluation-context
     (remap-selection evaluation-context selection-node new-resource-nodes-by-old)))
  ([evaluation-context selection-node new-resource-nodes-by-old]
   (let [basis (:basis evaluation-context)
         all-selections (into {}
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
                              (g/node-value selection-node :selected-node-ids-by-resource-node evaluation-context))
         all-sub-selections (into {}
                                  (keep (fn [[old-resource-node :as entry]]
                                          ;; Discard sub-selection for replaced
                                          ;; or deleted resource nodes, since
                                          ;; the contents might have changed
                                          ;; drastically.
                                          (when (= old-resource-node (new-resource-nodes-by-old old-resource-node))
                                            entry)))
                                  (g/node-value selection-node :sub-selections-by-resource-node evaluation-context))]
     (concat
       (set-all-selections basis selection-node all-selections)
       (g/set-property selection-node :all-sub-selections all-sub-selections)))))
