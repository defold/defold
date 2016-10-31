(ns editor.outline-view
  (:require
   [dynamo.graph :as g]
   [editor.defold-project :as project]
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
(def ^:private ^:dynamic *paste-into-parent* false)

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

(defn- ->iterator [^TreeView tree-item]
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
                                                        selected)) items)))

(defn- sync-selection [^TreeView tree-view ^TreeItem root selection]
  (when (and root (not (empty? selection)))
    (let [selected-ids (set selection)]
      (auto-expand (.getChildren root) selected-ids)
      (let [count (.getExpandedItemCount tree-view)
            selected-indices (filter #(selected-ids (item->node-id (.getTreeItem tree-view %))) (range count))]
        (when (not (empty? selected-indices))
          (doto (-> tree-view (.getSelectionModel))
            (.selectIndices (int (first selected-indices)) (int-array (rest selected-indices)))))))))

(defn- pathify
  ([root]
    (pathify [] root))
  ([path item]
    (let [path (conj path (:node-id item))]
      (-> item
        (assoc :path path)
        (update :children (fn [children] (mapv #(pathify path %) children)))))))

(g/defnk update-tree-view [_node-id ^TreeView tree-view root-cache active-resource active-outline open-resources selection selection-listener]
  (let [resource-set (set open-resources)
        root (get root-cache active-resource)
        ^TreeItem new-root (when active-outline (sync-tree root (tree-item (pathify active-outline))))
        new-cache (assoc (map-filter (fn [[resource _]] (contains? resource-set resource)) root-cache) active-resource new-root)]
    (binding [*programmatic-selection* true]
      (when new-root
        (.setExpanded new-root true))
      (.setRoot tree-view new-root)
      (sync-selection tree-view new-root selection)
      (g/transact (g/set-property _node-id :root-cache new-cache))
      tree-view)))

(g/defnode OutlineView
  (property tree-view TreeView)
  (property root-cache g/Any)
  (property selection-listener ListChangeListener)

  (input active-outline g/Any :substitute {})
  (input active-resource resource/Resource :substitute nil)
  (input open-resources g/Any :substitute [])
  (input selection g/Any :substitute [])

  (output tree-view TreeView :cached update-tree-view))

(ui/extend-menu ::outline-menu nil
                [{:label "Add"
                  :icon "icons/plus.png"
                  :command :add}
                 {:label "Add From File"
                  :icon "icons/plus.png"
                  :command :add-from-file}
                 {:label "Add Secondary"
                  :icon "icons/plus.png"
                  :command :add-secondary}
                 {:label "Add Secondary From File"
                  :icon "icons/plus.png"
                  :command :add-secondary-from-file}
                 {:label "Delete"
                  :icon "icons/cross.png"
                  :command :delete}])

(defn- item->path [^TreeItem item]
  (:path (.getValue item)))

(defn- item->id [^TreeItem item]
  (:node-id (.getValue item)))

(defn- root-iterators [^TreeView tree-view]
  (mapv ->iterator (ui/selection-roots tree-view item->path item->id)))

(handler/defhandler :delete :global
  (enabled? [selection outline-view]
            (and (< 0 (count selection))
                 (let [tree-view (g/node-value outline-view :tree-view)
                       item-iterators (root-iterators tree-view)]
                   (outline/delete? item-iterators))))
  (run [selection]
       (g/transact
         (concat
           (g/operation-label "Delete")
           (for [node-id selection]
             (g/delete-node node-id))))))

(defn- project [^TreeView tree-view]
  (project/get-project (:node-id (.getValue (.getRoot tree-view)))))

(def data-format-fn (fn []
                      (let [json "application/json"]
                        (or (DataFormat/lookupMimeType json)
                            (DataFormat. (into-array String [json]))))))

(handler/defhandler :copy :global
  (enabled? [selection] (< 0 (count selection)))
  (run [outline-view]
       (alter-var-root #'*paste-into-parent* (constantly true))
       (let [tree-view (g/node-value outline-view :tree-view)
             item-iterators (root-iterators tree-view)
             project (project tree-view)
             cb (Clipboard/getSystemClipboard)
             data (outline/copy item-iterators)]
         (.setContent cb {(data-format-fn) data}))))

(defn- paste-target-it [tree-view]
  (let [item-iterators (root-iterators tree-view)]
    (when (= 1 (count item-iterators))
      (let [target-it (first item-iterators)]
        (if *paste-into-parent*
          (outline/parent target-it)
          target-it)))))

(handler/defhandler :paste :global
  (enabled? [selection outline-view]
            (let [cb (Clipboard/getSystemClipboard)
                  tree-view (g/node-value outline-view :tree-view)
                  target-item-it (paste-target-it tree-view)
                  data-format (data-format-fn)]
              (and target-item-it
                   (.hasContent cb data-format)
                   (outline/paste? (project/graph (project tree-view)) target-item-it (.getContent cb data-format)))))
  (run [outline-view]
       (let [tree-view (g/node-value outline-view :tree-view)
             target-item-it (paste-target-it tree-view)
             project (project tree-view)
             cb (Clipboard/getSystemClipboard)
             data-format (data-format-fn)]
         (outline/paste! (project/graph project) target-item-it (.getContent cb data-format) (partial project/select project)))))

(handler/defhandler :cut :global
  (enabled? [selection outline-view]
            (let [tree-view (g/node-value outline-view :tree-view)
                  item-iterators (root-iterators tree-view)]
              (and (< 0 (count item-iterators))
                   (outline/cut? item-iterators))))
  (run [outline-view]
       (let [tree-view (g/node-value outline-view :tree-view)
             item-iterators (root-iterators tree-view)
             cb (Clipboard/getSystemClipboard)
             data-format (data-format-fn)]
         (.setContent cb {data-format (outline/cut! item-iterators)}))))

(defn- selection [^TreeView tree-view]
  (-> tree-view (.getSelectionModel) (.getSelectedItems)))

(defn- dump-mouse-event [^MouseEvent e]
  (prn "src" (.getSource e))
  (prn "tgt" (.getTarget e)))

(defn- dump-drag-event [^DragEvent e]
  (prn "src" (.getSource e))
  (prn "tgt" (.getTarget e))
  (prn "ges-src" (.getGestureSource e))
  (prn "ges-tgt" (.getGestureTarget e)))

(defn- drag-detected [tree-view ^MouseEvent e]
  (let [item-iterators (root-iterators tree-view)
        project (project tree-view)]
    (when (outline/drag? (project/graph project) item-iterators)
      (let [db (.startDragAndDrop ^Node (.getSource e) (into-array TransferMode TransferMode/COPY_OR_MOVE))
            data (outline/copy item-iterators)]
        (when-let [icon (and (= 1 (count item-iterators))
                             (:icon (outline/value (first item-iterators))))]
          (.setDragView db (jfx/get-image icon 16) 0 16))
        (.setContent db {(data-format-fn) data})
        (.consume e)))))

(defn- drag-done [tree-view e]
  ;; Moving items to external places is not supported
  ;; Moving items internally is handled by drag-dropped
  )

(defn- target [^Node node]
  (when node
    (if (instance? TreeCell node)
      node
      (target (.getParent node)))))

(defn- drag-over [tree-view ^DragEvent e]
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
                                 (root-iterators tree-view)
                                 [])
                project (project tree-view)]
            (when (outline/drop? (project/graph project) item-iterators (->iterator (.getTreeItem cell))
                                 (.getContent db (data-format-fn)))
              (let [modes (if (ui/drag-internal? e)
                            [TransferMode/MOVE]
                            [TransferMode/COPY])]
                (.acceptTransferModes e (into-array TransferMode modes)))
              (.consume e))))))))

(defn- drag-dropped [tree-view ^DragEvent e]
  (let [^TreeCell cell (target (.getTarget e))
        db (.getDragboard e)]
    (let [item-iterators (if (ui/drag-internal? e)
                           (root-iterators tree-view)
                           [])
          project (project tree-view)]
      (when (outline/drop! (project/graph project) item-iterators (->iterator (.getTreeItem cell))
                           (.getContent db (data-format-fn)) (partial project/select project))
        (.setDropCompleted e true)
        (.consume e)))))

(defn- drag-entered [tree-view ^DragEvent e]
  (let [^TreeCell cell (target (.getTarget e))]
    (when (and cell (not (.isEmpty cell)))
      (let [project        (project tree-view)
            item-iterators (if (ui/drag-internal? e)
                             (root-iterators tree-view)
                             [])
            db             (.getDragboard e)]
        (when (outline/drop? (project/graph project) item-iterators (->iterator (.getTreeItem cell))
                             (.getContent db (data-format-fn)))
          (ui/add-style! cell "drop-target")))

      (let [future (ui/->future 0.5 (fn []
                                      (-> cell (.getTreeItem) (.setExpanded true))
                                      (ui/user-data! cell :future-expand nil)))]
        (ui/user-data! cell :future-expand future)))))

(defn- drag-exited [tree-view ^DragEvent e]
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
                     (let [{:keys [label icon color outline-overridden?]} item]
                       (proxy-super setText label)
                       (proxy-super setGraphic (jfx/get-image-view icon 16))
                       (if outline-overridden?
                         (ui/add-style! this "overridden")
                         (ui/remove-style! this "overridden"))
                       (if-let [[r g b a] color]
                         (proxy-super setStyle (format "-fx-text-fill: rgba(%d,%d,%d,%f);"
                                                       (int (* 255 r))
                                                       (int (* 255 g))
                                                       (int (* 255 b))
                                                       a))
                         (proxy-super setStyle nil)))))))]
    (doto cell
      (.setOnDragEntered drag-entered-handler )
      (.setOnDragExited drag-exited-handler))))

(defn- setup-tree-view [^TreeView tree-view ^ListChangeListener selection-listener selection-provider]
  (let [drag-entered-handler (ui/event-handler e (drag-entered tree-view e))
        drag-exited-handler (ui/event-handler e (drag-exited tree-view e))]
    (doto tree-view
      (.. getSelectionModel (setSelectionMode SelectionMode/MULTIPLE))
      (.. getSelectionModel getSelectedItems (addListener selection-listener))
      (.setOnDragDetected (ui/event-handler e (drag-detected tree-view e)))
      (.setOnDragDone (ui/event-handler e (drag-done tree-view e)))
      (.setOnDragOver (ui/event-handler e (drag-over tree-view e)))
      (.setOnDragDropped (ui/event-handler e (drag-dropped tree-view e)))
      (.setCellFactory (reify Callback (call ^TreeCell [this view] (make-tree-cell view drag-entered-handler drag-exited-handler)))))
    (ui/register-context-menu tree-view ::outline-menu)))

(defn- propagate-selection [^ListChangeListener$Change change selection-fn]
  (when-not *programmatic-selection*
    (alter-var-root #'*paste-into-parent* (constantly false))
    (when-let [changes (filter #(and (not (nil? %)) (item->node-id %)) (and change (.getList change)))]
      ;; TODO - handle selection order
      (selection-fn (map item->node-id changes)))))

(defn make-outline-view [graph tree-view selection-fn selection-provider]
  (let [selection-listener (reify ListChangeListener
                             (onChanged [this change]
                               (propagate-selection change selection-fn)))]
    (setup-tree-view tree-view selection-listener selection-provider)
    (first (g/tx-nodes-added (g/transact (g/make-node graph OutlineView :tree-view tree-view :selection-listener selection-listener))))))
