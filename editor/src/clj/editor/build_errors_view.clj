(ns editor.build-errors-view
  (:require [dynamo.graph :as g]
            [editor.outline :as outline]
            [editor.resource :as resource]
            [editor.ui :as ui]
            [editor.workspace :as workspace])
  (:import [clojure.lang MapEntry PersistentQueue]
           [java.util Collection]
           [javafx.collections ObservableList]
           [javafx.scene.control TabPane TreeItem TreeView]))

(set! *warn-on-reflection* true)

(defn- pair [a b]
  (MapEntry. a b))

(defn- queue [item]
  (conj PersistentQueue/EMPTY item))

(defn- openable-resource [evaluation-context node-id]
  (when (g/node-instance? (:basis evaluation-context) resource/ResourceNode node-id)
    (when-some [resource (g/node-value node-id :resource evaluation-context)]
      (when (resource/openable-resource? resource)
        resource))))

(defn- node-id-at-override-depth [override-depth node-id]
  (if (and (some? node-id) (pos? override-depth))
    (recur (dec override-depth) (g/override-original node-id))
    node-id))

(defn- parent-resource [evaluation-context errors origin-override-depth origin-override-id]
  (or (when-some [node-id (node-id-at-override-depth origin-override-depth (:_node-id (first errors)))]
        (let [basis (:basis evaluation-context)]
          (when (or (nil? origin-override-id)
                    (not= origin-override-id (g/override-id basis node-id)))
            (when-some [resource (openable-resource evaluation-context node-id)]
              {:node-id (or (g/override-original basis node-id) node-id)
               :resource resource}))))
      (when-some [remaining-errors (next errors)]
        (recur evaluation-context remaining-errors origin-override-depth origin-override-id))))

(defn- error-outline-node-id [basis errors origin-override-depth origin-override-id]
  (or (when-some [node-id (node-id-at-override-depth origin-override-depth (:_node-id (first errors)))]
        (when (g/node-instance? basis outline/OutlineNode node-id)
          node-id))
      (when-some [remaining-errors (next errors)]
        (recur basis remaining-errors origin-override-depth origin-override-id))))

(defn- find-override-value-origin [basis node-id label depth]
  (if (and node-id (g/override? basis node-id))
    (if-not (g/property-overridden? basis node-id label)
      (recur basis (g/override-original basis node-id) label (inc depth))
      (pair node-id depth))
    (pair node-id depth)))

(defn- error-line [error]
  (-> error :value ex-data :line))

(defn- missing-resource-node? [evaluation-context node-id]
  (and (g/node-instance? (:basis evaluation-context) resource/ResourceNode node-id)
       (some? (g/node-value node-id :resource evaluation-context))
       (some? (g/node-value node-id :_output-jammers evaluation-context))))

(defn- error-item [evaluation-context root-cause]
  (let [message (:message (first root-cause))
        errors (drop-while (comp (fn [node-id]
                                   (or (nil? node-id)
                                       (missing-resource-node? evaluation-context node-id)))
                                 :_node-id)
                           root-cause)
        error (assoc (first errors) :message message)
        basis (:basis evaluation-context)
        [origin-node-id origin-override-depth] (find-override-value-origin basis (:_node-id error) (:_label error) 0)
        origin-override-id (when (some? origin-node-id) (g/override-id basis origin-node-id))
        outline-node-id (error-outline-node-id basis errors origin-override-depth origin-override-id)
        parent (parent-resource evaluation-context errors origin-override-depth origin-override-id)
        line (error-line error)]
    (cond-> {:parent parent
             :node-id outline-node-id
             :message (:message error)
             :severity (:severity error)}
            line (assoc :line line))))

(defn- push-causes [queue error path]
  (let [new-path (conj path error)]
    (reduce (fn [queue error]
              (conj queue (pair error new-path)))
            queue
            (:causes error))))

(defn- root-causes-helper [queue]
  (lazy-seq
    (when-some [[error path] (peek queue)]
      (if (seq (:causes error))
        (root-causes-helper (push-causes (pop queue) error path))
        (cons (conj path error)
              (root-causes-helper (pop queue)))))))

(defn- root-causes
  "Returns a lazy sequence of distinct error items from the causes in the supplied ErrorValue."
  [error-value]
  ;; Use a throwaway evaluation context to avoid context creation overhead.
  ;; We don't evaluate any outputs, so there is no need to update the cache.
  (let [evaluation-context (g/make-evaluation-context)]
    (sequence (comp (take 10000)
                    (map (partial error-item evaluation-context))
                    (distinct))
              (root-causes-helper (queue (pair error-value (list)))))))

(defn- error-items [root-error]
  (->> (root-causes root-error)
       (group-by :parent)
       (sort-by (comp resource/resource->proj-path :resource key))
       (mapv (fn [[resource errors]]
               (if resource
                 {:type :resource
                  :value resource
                  :children errors}
                 {:type :default
                  :children errors})))))

(defn build-resource-tree [root-error]
  {:label "root"
   :children (error-items root-error)})

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

(defn- find-outline-node [resource-node-id error-node-id]
  (if-not (g/node-instance? outline/OutlineNode error-node-id)
    resource-node-id
    (some (fn [{:keys [node-id] :as node-outline}]
            (when (or (= error-node-id node-id)
                      (= error-node-id (:node-id (:alt-outline node-outline))))
              node-id))
          (tree-seq (comp sequential? :children)
                    :children
                    (g/node-value resource-node-id :node-outline)))))

(defn error-item-open-info [error-item]
  (if (= :resource (:type error-item))
    (let [{resource :resource resource-node-id :node-id} (:value error-item)]
      (when (and (resource/openable-resource? resource)
                 (resource/exists? resource))
        [resource resource-node-id {}]))
    (let [{resource :resource resource-node-id :node-id} (:parent error-item)]
      (when (and (resource/openable-resource? resource)
                 (resource/exists? resource))
        (let [error-node-id (:node-id error-item)
              outline-node-id (find-outline-node resource-node-id error-node-id)
              opts (if-some [line (:line error-item)] {:line line} {})]
          [resource outline-node-id opts])))))

(defn make-build-errors-view [^TreeView errors-tree open-resource-fn]
  (doto errors-tree
    (.setShowRoot false)
    (ui/cell-factory! make-tree-cell)
    (ui/on-double! (fn [_]
                     (when-some [[resource selected-node-id opts] (some-> errors-tree ui/selection first error-item-open-info)]
                       (open-resource-fn resource [selected-node-id] opts))))))

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
