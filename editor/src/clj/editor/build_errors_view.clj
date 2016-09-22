(ns editor.build-errors-view
  (:require [dynamo.graph :as g]
            [editor.dialogs :as dialogs]
            [editor.resource :as resource]
            [editor.ui :as ui]
            [editor.workspace :as workspace])
  (:import [javafx.scene.control TabPane TreeItem TreeView]
           [editor.resource FileResource]))

(set! *warn-on-reflection* true)

(defn persistent-queue
  [coll]
  (into clojure.lang.PersistentQueue/EMPTY coll))

(defn root-causes
  [error-value]
  (letfn [(push-causes [queue error path]
            (let [new-path (conj path error)]
              (reduce (fn [queue error]
                        (conj queue [error new-path])) queue (:causes error))))]
    (loop [queue (persistent-queue [[error-value (list)]])
           ret #{}]
      (if-let [[error path] (peek queue)]
        (cond
          (seq (:causes error))
          (recur (push-causes (pop queue) error path) ret)

          :else
          (recur (pop queue) (conj ret {:error error :parents path})))
        ret))))

(defn parent-file-resource
  [errors]
  (->> errors
       (map (fn [{:keys [_node-id] :as error}]
              {:node-id _node-id
               :resource (some-> _node-id g/node-by-id :resource)}))
       (filter (fn [{:keys [resource]}]
                 (instance? FileResource resource)))
       (first)))

(defn- build-resource-tree
  [root-error]
  (let [items (->> (root-causes root-error)
                   (map (fn [{:keys [error parents] :as root-cause}]
                          (assoc root-cause :parent (parent-file-resource parents))))
                   (group-by :parent)
                   (reduce-kv (fn [ret resource errors]
                                (conj ret {:type :resource
                                           :value resource
                                           :children errors}))
                              []))]
    {:label "root"
     :children items}))

(defmulti make-tree-cell
  (fn [tree-item] (:type tree-item)))

(defmethod make-tree-cell :resource
  [tree-item]
  (let [resource (-> tree-item :value :resource)]
    {:text (resource/resource-name resource )
     :icon (workspace/resource-icon resource)}))

(defmethod make-tree-cell :default
  [tree-item]
  {:text (-> tree-item :error :message)
   :icon "icons/32/Icons_M_08_bigclose.png"})

(defn- open-error [open-resource-fn selection]
  (when-let [error (:error selection)]
    (ui/run-later
      (open-resource-fn (-> selection :parent :resource) [(-> selection :parent :node-id)]))))

(defn make-build-errors-view [^TreeView errors-tree open-resource-fn]
  (doto errors-tree
    (.setShowRoot false)
    (ui/cell-factory! make-tree-cell)
    (ui/on-double! (fn [_]
                     (when-let [selection (ui/selection errors-tree)]
                       (open-error open-resource-fn selection))))))

(defn update-build-errors [^TreeView errors-tree build-errors]
  (def err build-errors)
  (let [res-tree (build-resource-tree build-errors)
        new-root (dialogs/tree-item res-tree)]
    (.setRoot errors-tree new-root)
    (doseq [^TreeItem item (ui/tree-item-seq (.getRoot errors-tree))]
      (.setExpanded item true))
    (when-let [first-error (->> (ui/tree-item-seq (.getRoot errors-tree))
                                (filter (fn [^TreeItem item] (.isLeaf item)))
                                first)]
      (.select (.getSelectionModel errors-tree) first-error))
    ;; Select tab-pane
    (let [^TabPane tab-pane (.getParent (.getParent (.getParent errors-tree)))]
      (.select (.getSelectionModel tab-pane) 2))))

(defn clear-build-errors [^TreeView errors-tree]
  (.setRoot errors-tree nil))
