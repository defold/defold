(ns editor.build-errors-view
  (:require [dynamo.graph :as g]
            [editor
             [dialogs :as dialogs]
             [resource :as resource]
             [ui :as ui]
             [workspace :as workspace]])
  (:import [javafx.scene.control TabPane TreeItem TreeView]
           [editor.resource FileResource]))

(set! *warn-on-reflection* true)

(defrecord BuildErrorResource [parent-node-id parent-resource parent-file-resource error]
  resource/Resource
  (children [this]      [])
  (resource-name [this] error)
  (resource-type [this] {:icon "icons/32/Icons_M_08_bigclose.png"})
  (source-type [this]   (resource/source-type parent-resource))
  (read-only? [this]    (resource/read-only? parent-resource))
  (path [this]          (resource/path parent-resource))
  (abs-path [this]      (resource/abs-path parent-resource))
  (proj-path [this]     (resource/proj-path parent-resource))
  (url [this]           (resource/url parent-resource))
  (workspace [this]     (resource/workspace parent-resource))
  (resource-hash [this] (resource/resource-hash parent-resource)))

(defn- open-parent-resource [open-resource-fn resource]
  (when-let [res (:parent-file-resource resource)]
    (ui/run-later (open-resource-fn res {:select-node (:parent-node-id resource)}))))

(defn- build-resource-tree [{:keys [causes _node-id _label user-data] :as error} parent-node-id parent-file]
  (let [res            (:resource (and _node-id (g/node-by-id _node-id)))
        parent-node-id (if res
                         _node-id parent-node-id)
        parent-file    (if (instance? FileResource res)
                         res parent-file)
        children       (->> causes
                            (map #(build-resource-tree % parent-node-id parent-file))
                            (remove nil?)
                            seq)
        error-res      (and user-data (->BuildErrorResource
                                       parent-node-id
                                       (:resource (g/node-by-id parent-node-id))
                                       parent-file
                                       user-data))]
    (cond
      (and res error-res)
      (assoc res :children [error-res])

      res
      (assoc res :children children)

      error-res
      error-res

      :else
      (first children))))

(defn- trim-resource-tree
  "Remove top-level single-child nodes"
  [{:keys [children] :as tree}]
  (let [first-child (first children)]
    (if (and (= 1 (count children))
             (instance? FileResource first-child))
      (trim-resource-tree first-child)
      tree)))

(defn make-build-errors-view [^TreeView errors-tree open-resource-fn]
  (ui/cell-factory! errors-tree (fn [r]
                                  {:text (or (resource/resource-name r)
                                             (:label (resource/resource-type r)))
                                   :icon (workspace/resource-icon r)}))
  (ui/on-double! errors-tree (fn [_]
                               (when-let [resource (ui/selection errors-tree)]
                                 (open-parent-resource open-resource-fn resource))))
  errors-tree)

(defn update-build-errors [^TreeView errors-tree build-errors]
  (let [res-tree (build-resource-tree build-errors nil nil)
        new-root (dialogs/tree-item (trim-resource-tree res-tree))]
    (.setRoot errors-tree new-root)
    (doseq [^TreeItem item (ui/tree-item-seq (.getRoot errors-tree))]
      (.setExpanded item true))
    (when-let [first-match (->> (ui/tree-item-seq (.getRoot errors-tree))
                                (filter (fn [^TreeItem item]
                                          (instance? BuildErrorResource (.getValue item))))
                                first)]
      (.select (.getSelectionModel errors-tree) first-match))
    ;; Select tab-pane
    (let [^TabPane tab-pane (.getParent (.getParent (.getParent errors-tree)))]
      (.select (.getSelectionModel tab-pane) 2))))

(defn clear-build-errors [^TreeView errors-tree]
  (.setRoot errors-tree nil))
