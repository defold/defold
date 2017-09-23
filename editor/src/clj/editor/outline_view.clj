(ns editor.outline-view
  (:require
   [dynamo.graph :as g]
   [editor.app-view :as app-view]
   [editor.error-reporting :as error-reporting]
   [editor.handler :as handler]
   [editor.jfx :as jfx]
   [editor.outline :as outline]
   [editor.resource :as resource]
   [editor.ui :as ui])
  (:import
   (com.defold.control TreeCell)
   (editor.outline ItemIterator)
   (javafx.collections FXCollections ObservableList ListChangeListener ListChangeListener$Change)
   (javafx.event Event)
   (javafx.scene Scene Node Parent)
   (javafx.scene.control Button Cell ColorPicker Label TextField TitledPane TextArea TreeView TreeItem Menu MenuItem MenuBar Tab ProgressBar SelectionMode)
   (javafx.scene.input Clipboard ClipboardContent DragEvent TransferMode DataFormat)
   (javafx.scene.input MouseEvent)
   (javafx.util Callback)))

(set! *warn-on-reflection* true)

(declare tree-item)

(def ^:private ^:dynamic *programmatic-selection* nil)
(def ^:private ^:dynamic *paste-into-parent* nil)

;; TreeItem creator
(defn- ^ObservableList list-children [parent]
  (let [children (:children parent)
        items (into-array TreeItem (map tree-item children))]
    (if (empty? children)
      (FXCollections/emptyObservableList)
      (doto (FXCollections/observableArrayList)
        (.addAll ^"[Ljavafx.scene.control.TreeItem;" items)))))

;; NOTE: Without caching stack-overflow... WHY?
(defn tree-item [parent]
  (let [cached (atom false)]
    (proxy [TreeItem] [parent]
      (isLeaf []
        (empty? (:children parent)))
      (getChildren []
        (let [this ^TreeItem this
              ^ObservableList children (proxy-super getChildren)]
          (when-not @cached
            (reset! cached true)
            (.setAll children (list-children parent)))
          children))
      (outline/value [^TreeItem this]
        (.getValue this))
      (outline/parent [^TreeItem this]
        (.getParent this)))))

(defrecord TreeItemIterator [^TreeItem tree-item]
  outline/ItemIterator
  (value [this] (.getValue tree-item))
  (parent [this] (when (.getParent tree-item)
                   (TreeItemIterator. (.getParent tree-item)))))

(defn- ->iterator [^TreeItem tree-item]
  (TreeItemIterator. tree-item))

(defn- map-filter [filter-fn m]
  (into {} (filter filter-fn m)))

(defn- item->node-id [^TreeItem item]
  (:node-id (.getValue item)))

(defn- sync-tree [old-root new-root]
  (let [item-seq (ui/tree-item-seq old-root)
        expanded (zipmap (map item->node-id item-seq)
                         (map #(.isExpanded ^TreeItem %) item-seq))]
    (doseq [^TreeItem item (ui/tree-item-seq new-root)]
      (when (get expanded (item->node-id item))
        (.setExpanded item true))))
  new-root)

(defn- auto-expand [items selected-ids]
  (reduce #(or %1 %2) false (map (fn [^TreeItem item] (let [id (item->node-id item)
                                                            selected (boolean (selected-ids id))
                                                            expanded (auto-expand (.getChildren item) selected-ids)]
                                                        (when expanded (.setExpanded item expanded))
                                                        (or selected expanded))) items)))

(defn- sync-selection [^TreeView tree-view selection]
  (let [root (.getRoot tree-view)]
    (.. tree-view getSelectionModel clearSelection)
    (when (and root (not (empty? selection)))
      (let [selected-ids (set selection)]
        (when (auto-expand (.getChildren root) selected-ids)
          (.setExpanded root true))
        (let [count (.getExpandedItemCount tree-view)
              selected-indices (filter #(selected-ids (item->node-id (.getTreeItem tree-view %))) (range count))]
          (when (not (empty? selected-indices))
            (ui/select-indices! tree-view selected-indices)))))))

(defn- decorate
  ([dirty-resources root]
   (:item (decorate dirty-resources [] root (:outline-reference? root))))
  ([dirty-resources path item parent-reference?]
   (let [path (conj path (:node-id item))
         data (mapv #(decorate dirty-resources path % (or parent-reference? (:outline-reference? item))) (:children item))
         item (assoc item
                :path path
                :parent-reference? parent-reference?
                :children (mapv :item data)
                :child-error? (boolean (some :child-error? data))
                :child-overridden? (boolean (some :child-overridden? data))
                :dirty? (and (:outline-reference? item) (contains? dirty-resources (:link item))))]
     {:item item
      :child-error? (or (:child-error? item) (:outline-error? item))
      :child-overridden? (or (:child-overridden? item) (:outline-overridden? item))})))

(g/defnk produce-tree-root
  [active-outline active-resource-node dirty-resources open-resource-nodes raw-tree-view]
  (let [resource-node-set (set open-resource-nodes)
        root-cache (or (ui/user-data raw-tree-view ::root-cache) {})
        [root outline prev-dirty-resources] (get root-cache active-resource-node)
        new-root (if (or (not= outline active-outline) (and (nil? root) (nil? outline)) (not (identical? prev-dirty-resources dirty-resources)))
                   (sync-tree root (tree-item (decorate dirty-resources active-outline)))
                   root)
        new-cache (assoc (map-filter (fn [[resource-node _]] (contains? resource-node-set resource-node)) root-cache) active-resource-node [new-root active-outline dirty-resources])]
    (ui/user-data! raw-tree-view ::root-cache new-cache)
    new-root))

(defn- update-tree-view-selection!
  [^TreeView tree-view selection]
  (binding [*programmatic-selection* true]
    (sync-selection tree-view selection)
    tree-view))

(defn- update-tree-view-root!
  [^TreeView tree-view ^TreeItem root selection]
  (binding [*programmatic-selection* true]
    (when (not (identical? (.getRoot tree-view) root))
      (when root
        (.setExpanded root true)
        (.setRoot tree-view root)))
    (sync-selection tree-view selection)
    tree-view))

(g/defnk update-tree-view [^TreeView raw-tree-view ^TreeItem root selection]
  (if (identical? (.getRoot raw-tree-view) root)
    (if (= (set selection) (set (map :node-id (ui/selection raw-tree-view))))
      raw-tree-view
      (update-tree-view-selection! raw-tree-view selection))
    (update-tree-view-root! raw-tree-view root selection)))

(defn- item->value [^TreeItem item]
  (.getValue item))

(defn- alt-selection [selection]
  (let [alt-selection (mapv (fn [item]
                              (:alt-outline item item))
                            selection)]
    (if (not= selection alt-selection)
      alt-selection
      [])))

(g/defnode OutlineView
  (property raw-tree-view TreeView)

  (input active-outline g/Any :substitute {})
  (input active-resource-node g/NodeID :substitute nil)
  (input dirty-resources g/Any :substitute #{})
  (input open-resource-nodes g/Any :substitute [])
  (input selection g/Any :substitute [])

  (output root TreeItem :cached produce-tree-root)
  (output tree-view TreeView :cached update-tree-view)
  (output tree-selection g/Any :cached (g/fnk [tree-view] (ui/selection tree-view)))
  (output tree-selection-root-its g/Any :cached (g/fnk [tree-view] (mapv ->iterator (ui/selection-root-items tree-view (comp :path item->value) (comp :node-id item->value)))))
  (output succeeding-tree-selection g/Any :cached (g/fnk [tree-view tree-selection-root-its]
                                                         (->> tree-selection-root-its
                                                           (mapv :tree-item)
                                                           (ui/succeeding-selection tree-view))))
  (output alt-tree-selection g/Any :cached (g/fnk [tree-selection]
                                                  (alt-selection tree-selection))))

(ui/extend-menu ::outline-menu nil
                [{:label "Open"
                  :icon "icons/32/Icons_S_14_linkarrow.png"
                  :command :open}
                 {:label "Open As"
                  :icon "icons/32/Icons_S_14_linkarrow.png"
                  :command :open-as}
                 {:label "Show in Asset Browser"
                  :icon "icons/32/Icons_S_14_linkarrow.png"
                  :command :show-in-asset-browser}
                 {:label "Show in Desktop"
                  :icon "icons/32/Icons_S_14_linkarrow.png"
                  :command :show-in-desktop}
                 {:label "Referencing Files"
                  :command :referencing-files}
                 {:label "Dependencies"
                  :command :dependencies}
                 {:label :separator}
                 {:label "Add"
                  :icon "icons/32/Icons_M_07_plus.png"
                  :command :add
                  :expand? true}
                 {:label "Add From File"
                  :icon "icons/32/Icons_M_07_plus.png"
                  :command :add-from-file}
                 {:label "Add Secondary"
                  :icon "icons/32/Icons_M_07_plus.png"
                  :command :add-secondary}
                 {:label "Add Secondary From File"
                  :icon "icons/32/Icons_M_07_plus.png"
                  :command :add-secondary-from-file}
                 {:label :separator}
                 {:label "Cut"
                  :command :cut}
                 {:label "Copy"
                  :command :copy}
                 {:label "Paste"
                  :command :paste}
                 {:label "Delete"
                  :icon "icons/32/Icons_M_06_trash.png"
                  :command :delete}])

(defn- selection->nodes [selection]
  (handler/adapt-every selection Long))

(defn- root-iterators [outline-view]
  (g/node-value outline-view :tree-selection-root-its))

(handler/defhandler :delete :workbench
  (active? [selection] (handler/selection->node-ids selection))
  (enabled? [selection outline-view]
            (and (< 0 (count selection))
                 (-> (root-iterators outline-view)
                   outline/delete?)))
  (run [app-view selection selection-provider outline-view]
       (let [next (-> (handler/succeeding-selection selection-provider)
                    handler/selection->node-ids)]
         (g/transact
           (concat
             (g/operation-label "Delete")
             (for [node-id (handler/selection->node-ids selection)]
               (do
                 (g/delete-node (g/override-root node-id))))
             (when (seq next)
               (app-view/select app-view next)))))))

(def data-format-fn (fn []
                      (let [json "application/json"]
                        (or (DataFormat/lookupMimeType json)
                            (DataFormat. (into-array String [json]))))))

;; Iff every item-iterator has the same parent, return that parent, else nil
(defn- single-parent-it [item-iterators]
  (when (not-empty item-iterators)
    (let [parents (map outline/parent item-iterators)]
      (when (apply = parents)
        (first parents)))))

(defn- set-paste-parent! [root-its]
  (alter-var-root #'*paste-into-parent* (constantly (some-> (single-parent-it root-its) outline/value :node-id))))

(handler/defhandler :copy :workbench
  (active? [selection] (handler/selection->node-ids selection))
  (enabled? [selection] (< 0 (count selection)))
  (run [outline-view]
       (let [root-its (root-iterators outline-view)
             cb (Clipboard/getSystemClipboard)
             data (outline/copy root-its)]
         (set-paste-parent! root-its)
         (.setContent cb {(data-format-fn) data}))))

(defn- paste-target-it [item-iterators]
  (let [single-parent (single-parent-it item-iterators)]
    (or (when (= (some-> single-parent outline/value :node-id) *paste-into-parent*)
          single-parent)
      (first item-iterators))))

(handler/defhandler :paste :workbench
  (active? [selection] (handler/selection->node-ids selection))
  (enabled? [project selection outline-view]
            (let [cb (Clipboard/getSystemClipboard)
                  target-item-it (paste-target-it (root-iterators outline-view))
                  data-format (data-format-fn)]
              (and target-item-it
                   (.hasContent cb data-format)
                   (outline/paste? (g/node-id->graph-id project) target-item-it (.getContent cb data-format)))))
  (run [project outline-view app-view]
    (let [target-item-it (paste-target-it (root-iterators outline-view))
          cb (Clipboard/getSystemClipboard)
          data-format (data-format-fn)]
      (outline/paste! (g/node-id->graph-id project) target-item-it (.getContent cb data-format) (partial app-view/select app-view))
      (set-paste-parent! (root-iterators outline-view)))))

(handler/defhandler :cut :workbench
  (active? [selection] (handler/selection->node-ids selection))
  (enabled? [selection outline-view]
            (let [item-iterators (root-iterators outline-view)]
              (and (< 0 (count item-iterators))
                   (outline/cut? item-iterators))))
  (run [app-view selection-provider outline-view]
       (let [item-iterators (root-iterators outline-view)
             cb (Clipboard/getSystemClipboard)
             data-format (data-format-fn)
             next (-> (handler/succeeding-selection selection-provider)
                    handler/selection->node-ids)]
         (.setContent cb {data-format (outline/cut! item-iterators (if next (app-view/select app-view next)))}))))

(defn- dump-mouse-event [^MouseEvent e]
  (prn "src" (.getSource e))
  (prn "tgt" (.getTarget e)))

(defn- dump-drag-event [^DragEvent e]
  (prn "src" (.getSource e))
  (prn "tgt" (.getTarget e))
  (prn "ges-src" (.getGestureSource e))
  (prn "ges-tgt" (.getGestureTarget e)))

(defn- drag-detected [proj-graph outline-view ^MouseEvent e]
  (let [item-iterators (root-iterators outline-view)]
    (when (outline/drag? proj-graph item-iterators)
      (let [db (.startDragAndDrop ^Node (.getSource e) (into-array TransferMode TransferMode/COPY_OR_MOVE))
            data (outline/copy item-iterators)]
        (when-let [icon (and (= 1 (count item-iterators))
                             (:icon (outline/value (first item-iterators))))]
          (.setDragView db (jfx/get-image icon 16) 0 16))
        (.setContent db {(data-format-fn) data})
        (.consume e)))))

(defn- target [^Node node]
  (when node
    (if (instance? TreeCell node)
      node
      (target (.getParent node)))))

(defn- drag-over [proj-graph outline-view ^DragEvent e]
  (if (not (instance? TreeCell (.getTarget e)))
    (when-let [parent (.getParent ^Node (.getTarget e))]
      (Event/fireEvent parent (.copyFor e (.getSource e) parent)))
    (let [^TreeCell cell (.getTarget e)]
      ;; Auto scrolling
      (let [view (.getTreeView cell)
            view-y (.getY (.sceneToLocal view (.getSceneX e) (.getSceneY e)))
            height (.getHeight (.getBoundsInLocal view))]
        (when (< view-y 15)
          (.scrollTo view (dec (.getIndex cell))))
        (when (> view-y (- height 15))
          (.scrollTo view (inc (.getIndex cell)))))
      (let [db (.getDragboard e)]
        (when (and (instance? TreeCell cell)
                   (not (.isEmpty cell))
                   (.hasContent db (data-format-fn)))
          (let [item-iterators (if (ui/drag-internal? e)
                                 (root-iterators outline-view)
                                 [])]
            (when (outline/drop? proj-graph item-iterators (->iterator (.getTreeItem cell))
                                 (.getContent db (data-format-fn)))
              (let [modes (if (ui/drag-internal? e)
                            [TransferMode/MOVE]
                            [TransferMode/COPY])]
                (.acceptTransferModes e (into-array TransferMode modes)))
              (.consume e))))))))

(defn- drag-dropped [proj-graph app-view outline-view ^DragEvent e]
  (let [^TreeCell cell (target (.getTarget e))
        db (.getDragboard e)]
    (let [item-iterators (if (ui/drag-internal? e)
                           (root-iterators outline-view)
                           [])]
      (when (outline/drop! proj-graph item-iterators (->iterator (.getTreeItem cell))
                           (.getContent db (data-format-fn)) (partial app-view/select app-view))
        (.setDropCompleted e true)
        (.consume e)))))

(defn- drag-entered [proj-graph outline-view ^DragEvent e]
  (let [^TreeCell cell (target (.getTarget e))
        db (.getDragboard e)]
    (when (and cell
               (not (.isEmpty cell))
               (.hasContent db (data-format-fn)))
      (let [item-iterators (if (ui/drag-internal? e)
                             (root-iterators outline-view)
                             [])]
        (when (outline/drop? proj-graph item-iterators (->iterator (.getTreeItem cell))
                             (.getContent db (data-format-fn)))
          (ui/add-style! cell "drop-target")))

      (let [future (ui/->future 0.5 (fn []
                                      (-> cell (.getTreeItem) (.setExpanded true))
                                      (ui/user-data! cell :future-expand nil)))]
        (ui/user-data! cell :future-expand future)))))

(defn- drag-exited [^DragEvent e]
  (when-let [cell (target (.getTarget e))]
    (ui/remove-style! cell "drop-target")
    (when-let [future (ui/user-data cell :future-expand)]
      (ui/cancel future)
      (ui/user-data! cell :future-expand nil))))

(defn make-tree-cell
  [^TreeView tree-view drag-entered-handler drag-exited-handler]
  (let [cell (proxy [TreeCell] []
               (updateItem [item empty]
                 (let [this ^TreeCell this]
                   (proxy-super updateItem item empty)
                   (ui/update-tree-cell-style! this)
                   (if empty
                     (do
                       (proxy-super setText nil)
                       (proxy-super setGraphic nil)
                       (proxy-super setContextMenu nil)
                       (proxy-super setStyle nil))                                                                    
                     (let [{:keys [label icon link outline-error? outline-overridden? outline-reference? parent-reference? child-error? child-overridden? dirty?]} item
                           icon (if outline-error? "icons/32/Icons_E_02_error.png" icon)
                           label (if (and link outline-reference?) (format "%s - %s" label (resource/resource->proj-path link)) label)]
                       (proxy-super setText label)
                       (proxy-super setGraphic (jfx/get-image-view icon 16))
                       (if dirty?
                         (ui/add-style! this "unsaved")
                         (ui/remove-style! this "unsaved"))
                       (if parent-reference?
                         (ui/add-style! this "parent-reference")
                         (ui/remove-style! this "parent-reference"))
                       (if outline-reference?
                         (ui/add-style! this "reference")
                         (ui/remove-style! this "reference"))
                       (if outline-error?
                         (ui/add-style! this "error")
                         (ui/remove-style! this "error"))
                       (if outline-overridden?
                         (ui/add-style! this "overridden")
                         (ui/remove-style! this "overridden"))
                       (if child-error?
                         (ui/add-style! this "child-error")
                         (ui/remove-style! this "child-error"))
                       (if child-overridden?
                         (ui/add-style! this "child-overridden")
                         (ui/remove-style! this "child-overridden")))))))]
    (doto cell
      (.setOnDragEntered drag-entered-handler)
      (.setOnDragExited drag-exited-handler))))

(defrecord SelectionProvider [outline-view]
  handler/SelectionProvider
  (selection [this]
    (g/node-value outline-view :tree-selection))
  (succeeding-selection [this]
    (g/node-value outline-view :succeeding-tree-selection))
  (alt-selection [this]
    (g/node-value outline-view :alt-tree-selection)))

(defn- propagate-selection [selected-items app-view]
  (when-not *programmatic-selection*
    (set-paste-parent! nil)
    (when-let [selection (into [] (keep :node-id) selected-items)]
      ;; TODO - handle selection order
      (app-view/select! app-view selection))))

(defn- outline-data->resource [outline-data]
  ;; The resource property on ResourceNodes take priority over :link metadata.
  ;; This allows for ResourceNodes such as components to :link to a primary
  ;; related resource. For example, a CollectionProxy can :link to the
  ;; Collection it instantiates, or a SpineModel can link to its SpineScene.
  (let [node-id (:node-id outline-data)]
    (or (when (g/node-instance? resource/ResourceNode node-id)
          (let [resource (g/node-value node-id :resource)]
            (when (resource/openable-resource? resource)
              resource)))
        (:link outline-data))))

(defn- setup-tree-view [proj-graph ^TreeView tree-view outline-view app-view]
  (let [drag-entered-handler (ui/event-handler e (drag-entered proj-graph outline-view e))
        drag-exited-handler (ui/event-handler e (drag-exited e))]
    (doto tree-view
      (.. getSelectionModel (setSelectionMode SelectionMode/MULTIPLE))
      (.setEventDispatcher (ui/make-shortcut-dispatcher tree-view [(ui/key-combo "Space")]))
      (.setOnDragDetected (ui/event-handler e (drag-detected proj-graph outline-view e)))
      (.setOnDragOver (ui/event-handler e (drag-over proj-graph outline-view e)))
      (.setOnDragDropped (ui/event-handler e (error-reporting/catch-all! (drag-dropped proj-graph app-view outline-view e))))
      (.setCellFactory (reify Callback (call ^TreeCell [this view] (make-tree-cell view drag-entered-handler drag-exited-handler))))
      (ui/observe-selection #(propagate-selection %2 app-view))
      (ui/bind-double-click! :open)
      (ui/register-context-menu ::outline-menu)
      (ui/context! :outline {} (SelectionProvider. outline-view) {} {java.lang.Long :node-id
                                                                     resource/Resource outline-data->resource}))))

(defn- setup-outline-view [view-graph app-view tree-view]
  (first
    (g/tx-nodes-added
      (g/transact
        (g/make-nodes view-graph [outline-view [OutlineView :raw-tree-view tree-view]]
                      (g/connect app-view :dirty-resources outline-view :dirty-resources))))))

(defn make-outline-view [view-graph proj-graph tree-view app-view]
  (let [outline-view (setup-outline-view view-graph app-view tree-view)]
    (setup-tree-view proj-graph tree-view outline-view app-view)
    outline-view))
