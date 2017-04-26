(ns editor.build-errors-view
  (:require [dynamo.graph :as g]
            [editor.resource :as resource]
            [editor.scene :as scene]
            [editor.ui :as ui]
            [editor.workspace :as workspace]
            [internal.util :as util])
  (:import [javafx.collections ObservableList]
           [javafx.scene.control TabPane TreeItem TreeView]
           [editor.resource FileResource]))

(set! *warn-on-reflection* true)

(defn- persistent-queue
  [coll]
  (into clojure.lang.PersistentQueue/EMPTY coll))

(defn- root-causes
  "Returns a set of ErrorValue paths. Each path starts with the root cause and
  continues with every parent up to and including the supplied error-value.
  The causes are stripped from ErrorValues in the returned path."
  [error-value]
  (letfn [(push-causes [queue error path]
            (let [new-path (conj path (dissoc error :causes))]
              (reduce (fn [queue error]
                        (conj queue [error new-path])) queue (:causes error))))]
    (loop [queue (persistent-queue [[error-value (list)]])
           ret []]
      (if-let [[error path] (peek queue)]
        (cond
          (seq (:causes error))
          (recur (push-causes (pop queue) error path) ret)

          :else
          (recur (pop queue) (conj ret (conj path (dissoc error :causes)))))
        (distinct ret)))))

(defn- parent-file-resource
  [errors]
  (when-let [error (first errors)]
    (let [node-id (:_node-id error)
          resource (and (some? node-id)
                        (g/node-instance? resource/ResourceNode node-id)
                        (g/node-value node-id :resource))]
      (if (and (instance? FileResource resource)
               (resource/exists? resource))
        {:node-id node-id
         :resource resource}
        (recur (next errors))))))

(defn- scene-node-id
  [root-cause parent-node-id]
  (or (->> root-cause
           (keep :_node-id)
           (take-while #(not= parent-node-id %))
           (util/first-where #(g/node-instance? scene/SceneNode %)))
      parent-node-id))

(defn- build-resource-tree
  [root-error]
  (let [items (->> (root-causes root-error)
                   (map (fn [root-cause]
                          (let [error (dissoc (first root-cause) :_label)
                                parent (parent-file-resource root-cause)
                                node-id (scene-node-id root-cause (:node-id parent))]
                            {:error error
                             :parent parent
                             :node-id node-id})))
                   (group-by :parent)
                   (sort-by (comp resource/resource->proj-path :resource key))
                   (mapv (fn [[resource errors]]
                           (if resource
                             {:type :resource
                              :value resource
                              :children (vec (distinct errors))}
                             {:type :default
                              :children (vec (distinct errors))}))))]
    {:label "root"
     :children items}))

(defn error-line
  [error]
  (-> error :value ex-data :line))

(defmulti make-tree-cell
  (fn [tree-item] (:type tree-item)))

(defmethod make-tree-cell :resource
  [tree-item]
  (let [resource (-> tree-item :value :resource)]
    {:text (resource/proj-path resource)
     :icon (workspace/resource-icon resource)
     :style (resource/style-classes resource)}))

(defmethod make-tree-cell :default
  [{:keys [error] :as tree-item}]
  (let [line (error-line error)
        message (cond->> (:message error)
                  line
                  (str "Line " line ": "))]
    {:text message
     :icon "icons/32/Icons_M_08_bigclose.png"}))

(defn- open-error [open-resource-fn selection]
  (when-let [selection (util/first-where :error selection)]
    (when-let [error (:error selection)]
      (let [resource (-> selection :parent :resource)
            error-node-id (:node-id selection)
            node-id (or (some-> error-node-id g/override-original) error-node-id)
            opts {:line (error-line error)}]
        (when (and (some? resource) (resource/exists? resource))
          (ui/run-later
            (open-resource-fn resource [node-id] opts)))))))

(defn make-build-errors-view [^TreeView errors-tree open-resource-fn]
  (doto errors-tree
    (.setShowRoot false)
    (ui/cell-factory! make-tree-cell)
    (ui/on-double! (fn [_]
                     (when-let [selection (ui/selection errors-tree)]
                       (open-error open-resource-fn selection))))))

(declare tree-item)

(defn- ^ObservableList list-children [parent]
  (map tree-item (:children parent)))

(defn- tree-item [parent]
  (let [cached (atom false)]
    (proxy [TreeItem] [parent]
      (isLeaf []
        (empty? (:children (.getValue ^TreeItem this))))
      (getChildren []
        (let [this ^TreeItem this
              ^ObservableList children (proxy-super getChildren)]
          (when-not @cached
            (reset! cached true)
            (.setAll children (list-children (.getValue this))))
          children)))))

(defn update-build-errors [^TreeView errors-tree build-errors]
  (let [res-tree (build-resource-tree build-errors)
        new-root (tree-item res-tree)]
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
