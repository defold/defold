(ns editor.selection
  (:require [dynamo.graph :as g]))

(g/defnode SelectionNode
  (property all-selections g/Any)
  (property all-sub-selections g/Any)

  (input all-selected-node-ids g/Any :array)
  (input all-selected-node-properties g/Any :array)

  (output selected-node-ids-by-resource-node g/Any :cached (g/fnk [all-selected-node-ids all-selections]
                                                             (let [selected-node-id-set (set all-selected-node-ids)]
                                                               (into {}
                                                                     (map (fn [[key vals]]
                                                                            [key (filterv selected-node-id-set
                                                                                          vals)]))
                                                                     all-selections))))
  (output selected-node-properties-by-resource-node g/Any :cached (g/fnk [all-selected-node-properties all-selections]
                                                                    (let [props (into {}
                                                                                      (map (juxt :node-id identity))
                                                                                      all-selected-node-properties)]
                                                                      (into {}
                                                                            (map (fn [[key vals]]
                                                                                   [key (into []
                                                                                              (keep props)
                                                                                              vals)]))
                                                                            all-selections))))
  (output sub-selections-by-resource-node g/Any :cached (g/fnk [all-selected-node-ids all-sub-selections]
                                                          (let [selected-node-id-set (set all-selected-node-ids)]
                                                            (into {}
                                                                  (map (fn [[key vals]]
                                                                         [key (filterv (comp selected-node-id-set first)
                                                                                       vals)]))
                                                                  all-sub-selections)))))

(defn make-selection-node! [graph-id]
  (g/make-node! graph-id SelectionNode))

(defn- update-selection [old-selection-by-resource-node resource-node new-selection]
  (let [node-exists? (partial g/node-by-id (g/now))
        valid-entry? (comp node-exists? key)]
    (into {}
          (filter valid-entry?)
          (assoc old-selection-by-resource-node resource-node new-selection))))

(defn- perform-selection [selection-node all-selections]
  (concat
    (g/set-property selection-node :all-selections all-selections)
    (g/disconnect-sources basis selection-node :all-selected-node-ids)
    (g/disconnect-sources basis selection-node :all-selected-node-properties)
    (sequence (comp (mapcat val)
                    (distinct)
                    (mapcat (fn [selected-node-id]
                              (concat
                                (g/connect selected-node-id :_node-id selection-node :all-selected-node-ids)
                                (g/connect selected-node-id :_properties selection-node :all-selected-node-properties)))))
              all-selections)))

(defn select [selection-node resource-node node-ids]
  (assert (every? some? node-ids) "Attempting to select nil values")
  (let [selected-node-ids (if (seq node-ids)
                            (into [] (distinct) node-ids)
                            [resource-node])
        all-selections (-> (g/node-value selection-node :all-selections)
                           (update-selection resource-node selected-node-ids))]
    (perform-selection selection-node all-selections)))

(defn- perform-sub-selection [project all-sub-selections]
  (g/set-property project :all-sub-selections all-sub-selections))

(defn sub-select [selection-node resource-node sub-selection]
  (g/update-property selection-node :all-sub-selections update-selection resource-node sub-selection))

(defn remap-selection [selection-node new-resource-nodes-by-old]
  (let [all-selections (into {}
                             (keep (fn [[old-resource-node :as entry]]
                                     (when-some [new-resource-node (new-resource-nodes-by-old old-resource-node)]
                                       ;; The resource still exists.
                                       ;; If the resource node was replaced, its
                                       ;; owned nodes will also have been
                                       ;; replaced. Selection is no longer
                                       ;; valid, so we simply select the
                                       ;; resource node itself.
                                       (if (not= old-resource-node new-resource-node)
                                         [new-resource-node [new-resource-node]]
                                         entry))))
                             (g/node-value selection-node :all-selections))
        all-sub-selections (into {}
                                 (keep (fn [[old-resource-node :as entry]]
                                         (when-some [new-resource-node (new-resource-nodes-by-old old-resource-node)]
                                           ;; The resource still exists.
                                           ;; If the resource node was replaced,
                                           ;; its contents might have changed a
                                           ;; lot. Clear the sub-selection just
                                           ;; in case.
                                           (if (not= old-resource-node new-resource-node)
                                             [new-resource-node []]
                                             entry))))
                                 (g/node-value selection-node :all-sub-selections))]
    (concat
      (perform-selection selection-node all-selections)
      (perform-sub-selection selection-node all-sub-selections))))
