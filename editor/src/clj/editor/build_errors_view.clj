(ns editor.build-errors-view
  (:require [dynamo.graph :as g]
            [editor.defold-project :as project]
            [editor.outline :as outline]
            [editor.resource :as resource]
            [editor.ui :as ui]
            [editor.workspace :as workspace])
  (:import [editor.resource Resource MemoryResource]
           [java.util Collection]
           [javafx.collections ObservableList]
           [javafx.scene.control TabPane TreeItem TreeView]))

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

(defn- existing-resource [node-id]
  (when (g/node-instance? resource/ResourceNode node-id)
    (when-let [resource (g/node-value node-id :resource)]
      (when (and (instance? Resource resource)
                 (not (instance? MemoryResource resource))
                 (resource/exists? resource))
        resource))))

(defn- node-id-at-override-depth [override-depth node-id]
  (if (and (some? node-id) (pos? override-depth))
    (recur (dec override-depth) (g/override-original node-id))
    node-id))

(defn- parent-resource [errors origin-override-depth origin-override-id]
  (or (when-let [node-id (node-id-at-override-depth origin-override-depth (:_node-id (first errors)))]
        (when (or (nil? origin-override-id)
                  (not= origin-override-id (g/override-id node-id)))
          (when-let [resource (existing-resource node-id)]
            {:node-id (or (g/override-original node-id) node-id)
             :resource resource})))
      (when-some [remaining-errors (next errors)]
        (recur remaining-errors origin-override-depth origin-override-id))))

(defn find-override-value-origin [node-id label depth]
  (if (and node-id (g/override? node-id))
    (if-not (g/property-overridden? node-id label)
      (recur (g/override-original node-id) label (inc depth))
      [node-id depth])
    [node-id depth]))

(defn- error-line [error]
  (-> error :value ex-data :line))

(defn- missing-resource-node [node-id]
  (when (g/node-instance? resource/ResourceNode node-id)
    (not (existing-resource node-id))))

(defn- error-item [root-cause]
  (let [message (:message (first root-cause))
        error (assoc (first (drop-while (comp (fn [node-id] (or (nil? node-id) (missing-resource-node node-id))) :_node-id) root-cause))
                :message message)
        [origin-node-id origin-override-depth] (find-override-value-origin (:_node-id error) (:_label error) 0)
        origin-override-id (when (some? origin-node-id) (g/override-id origin-node-id))
        parent (parent-resource root-cause origin-override-depth origin-override-id)
        line (error-line error)]
    (cond-> {:parent parent
             :node-id origin-node-id
             :message (:message error)
             :severity (:severity error)}
            line (assoc :line line))))

(defn build-resource-tree [root-error]
  (let [items (->> (root-causes root-error)
                   (map error-item)
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

(defmulti make-tree-cell
  (fn [tree-item] (:type tree-item)))

(defmethod make-tree-cell :resource
  [tree-item]
  (let [resource (-> tree-item :value :resource)]
    {:text (resource/proj-path resource)
     :icon (workspace/resource-icon resource)
     :style (resource/style-classes resource)}))

(defmethod make-tree-cell :default
  [error-item]
  (let [line (:line error-item)
        message (cond->> (:message error-item)
                  line
                  (str "Line " line ": "))
        icon (case (:severity error-item)
               :info "icons/32/Icons_E_00_info.png"
               :warning "icons/32/Icons_E_01_warning.png"
               "icons/32/Icons_E_02_error.png")
        style (case (:severity error-item)
                :info #{"severity-info"}
                :warning #{"severity-warning"}
                #{"severity-error"})]
    {:text message
     :icon icon
     :style style}))

(defn- error-selection
  [node-id resource]
  (if (g/node-instance? outline/OutlineNode node-id)
    [node-id]
    (let [project (project/get-project node-id)]
      (when-some [resource-node (project/get-resource-node project resource)]
        [(g/node-value resource-node :_node-id)]))))

(defn- open-error [open-resource-fn selection]
  (when-some [error-item (first selection)]
    (if (= :resource (:type error-item))
      (let [{:keys [node-id resource]} (:value error-item)]
        (when (and (resource/openable-resource? resource) (resource/exists? resource))
          (ui/run-later
            (open-resource-fn resource [node-id] {}))))
      (let [resource (-> error-item :parent :resource)
            node-id (:node-id error-item)
            selection (error-selection node-id resource)
            opts (if-some [line (:line error-item)] {:line line} {})]
        (when (and (resource/openable-resource? resource) (resource/exists? resource))
          (ui/run-later
            (open-resource-fn resource selection opts)))))))

(defn make-build-errors-view [^TreeView errors-tree open-resource-fn]
  (doto errors-tree
    (.setShowRoot false)
    (ui/cell-factory! make-tree-cell)
    (ui/on-double! (fn [_]
                     (when-let [selection (ui/selection errors-tree)]
                       (open-error open-resource-fn selection))))))

(declare tree-item)

(defn- list-children
  ^Collection [parent]
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
      (ui/select-tab! tab-pane "build-errors-tab"))))

(defn clear-build-errors [^TreeView errors-tree]
  (.setRoot errors-tree nil))
