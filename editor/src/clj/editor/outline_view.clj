(ns editor.outline-view
  (:require [dynamo.graph :as g]
            [clojure.edn :as edn]
            [editor.handler :as handler]
            [editor.jfx :as jfx]
            [editor.ui :as ui]
            [editor.project :as project]
            [editor.workspace :as workspace])
  (:import [com.defold.editor Start]
           [com.jogamp.opengl.util.awt Screenshot]
           [javafx.application Platform]
           [javafx.beans.value ChangeListener]
           [javafx.collections FXCollections ObservableList ListChangeListener]
           [javafx.embed.swing SwingFXUtils]
           [javafx.event ActionEvent EventHandler]
           [javafx.fxml FXMLLoader]
           [javafx.geometry Insets]
           [javafx.scene.input Clipboard ClipboardContent DragEvent TransferMode DataFormat]
           [javafx.scene Scene Node Parent]
           [javafx.scene.control Button ColorPicker Label TextField TitledPane TextArea TreeView TreeItem TreeCell Menu MenuItem MenuBar Tab ProgressBar SelectionMode]
           [javafx.scene.image Image ImageView WritableImage PixelWriter]
           [javafx.scene.input MouseEvent]
           [javafx.scene.layout AnchorPane GridPane StackPane HBox Priority]
           [javafx.scene.paint Color]
           [javafx.stage Stage FileChooser]
           [javafx.util Callback]
           [java.io File]
           [java.nio.file Paths]
           [java.util.prefs Preferences]
           [javax.media.opengl GL GL2 GLContext GLProfile GLDrawableFactory GLCapabilities]))

(declare tree-item)

(def ^:private ^:dynamic *programmatic-selection* nil)

; TreeItem creator
(defn- ^ObservableList list-children [parent]
  (let [children (:children parent)
        sort-by-fn (:sort-by-fn parent)
        children (if sort-by-fn (sort-by sort-by-fn children) children)
        items (into-array TreeItem (map tree-item children))]
    (if (empty? children)
      (FXCollections/emptyObservableList)
      (doto (FXCollections/observableArrayList)
        (.addAll ^"[Ljavafx.scene.control.TreeItem;" items)))))

; NOTE: Without caching stack-overflow... WHY?
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
          children)))))

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
  (reduce #(or %1 %2) false (map (fn [item] (let [id (item->node-id item)
                                                  selected (boolean (selected-ids id))
                                                  expanded (auto-expand (.getChildren item) selected-ids)]
                                              (when expanded (.setExpanded item expanded))
                                              selected)) items)))

(defn- sync-selection [^TreeView tree-view root selection]
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

(g/defnk update-tree-view [_self ^TreeView tree-view root-cache active-resource active-outline open-resources selection selection-listener]
  (let [resource-set (set open-resources)
       root (get root-cache active-resource)
       new-root (when active-outline (sync-tree root (tree-item (pathify active-outline))))
       new-cache (assoc (map-filter (fn [[resource _]] (contains? resource-set resource)) root-cache) active-resource new-root)]
    (binding [*programmatic-selection* true]
      (when new-root
        (.setExpanded new-root true))
      (.setRoot tree-view new-root)
      (sync-selection tree-view new-root selection)
      (g/transact (g/set-property (g/node-id _self) :root-cache new-cache)))))

(g/defnode OutlineView
  (property tree-view TreeView)
  (property root-cache g/Any)
  (property selection-listener ListChangeListener)

  (input active-outline g/Any)
  (input active-resource (g/protocol workspace/Resource))
  (input open-resources g/Any)
  (input selection g/Any)

  (output tree-view TreeView :cached update-tree-view))

(ui/extend-menu ::outline-menu nil
                [{:label "Add"
                  :icon "icons/plus.png"
                  :command :add}
                 {:label "Add From File"
                  :icon "icons/plus.png"
                  :command :add-from-file}
                 {:label "Add Secondary From File"
                  :icon "icons/plus.png"
                  :command :add-secondary-from-file}
                 {:label "Delete"
                  :icon "icons/cross.png"
                  :command :delete}])

(handler/defhandler :delete :global
    (enabled? [selection] (= 1 (count selection)))
    (run [selection] (g/transact
                       (concat
                         (g/operation-label "Delete")
                         (g/delete-node (first selection))))))

(defn- roots [items]
  (let [items (into {} (map #(do [(:path %) %]) items))
        roots (loop [paths (keys items)
                     roots []]
                (if-let [path (first paths)]
                  (let [ancestors (filter #(= (subvec path 0 (count %)) %) roots)
                        roots (if (empty? ancestors)
                                (conj roots path)
                                roots)]
                    (recur (rest paths) roots))
                  roots))]
    (vals (into {} (map #(let [item (items %)]
                           [(:node-id item) item]) roots)))))

(def resource? (partial g/node-instance? project/ResourceNode))

(defrecord ResourceReference [path label])

(defn make-reference [node label]
  #_{:path (workspace/proj-path (:resource node)) :label label}
  (ResourceReference. (workspace/proj-path (:resource node)) label))

(defn resolve-reference [workspace project reference]
  (let [resource      (workspace/resolve-workspace-resource workspace (:path reference))
        resource-node (project/get-resource-node project resource)]
    [resource-node (:label reference)]))

(def data-format-fn (fn []
                      (let [format "graph-format"]
                        (or (DataFormat/lookupMimeType format)
                            (DataFormat. (into-array String [format]))))))

(defn- drag-detected [e selection]
  (let [items (roots selection)
        mode TransferMode/COPY #_(if (empty? (filter workspace/read-only? resources))
                    TransferMode/MOVE
                    TransferMode/COPY)
        db (.startDragAndDrop (.getSource e) (into-array TransferMode [mode]))
        content (ClipboardContent.)
        copy-data (g/copy (mapv :node-id items) {:continue? (comp not resource?)
                                                 :write-handlers {project/ResourceNode make-reference}})
        root-node (:node-id (.getValue (.getRoot (.getSource e))))
        graph-id (g/node-id->graph-id root-node)
        project (project/get-project root-node)
        workspace (project/workspace project)
        paste-data (g/paste graph-id copy-data {:read-handlers {ResourceReference (partial resolve-reference workspace project)}})]
    (prn "paste" paste-data)
    (let [copy-data (edn/read-string
                      {:readers {'editor.outline_view.ResourceReference map->ResourceReference
                                 'dynamo.graph.Endpoint g/map->Endpoint}}
                      (prn-str copy-data))
          #_paste-data #_(g/paste graph-id copy-data {:read-handlers {ResourceReference (partial resolve-reference workspace project)}})])
    #_(prn "rtr" (edn/read-string
                  {:readers {'editor.outline_view.ResourceReference map->ResourceReference
                             'dynamo.graph.Endpoint g/map->Endpoint}}
                  (prn-str copy-data)))
    #_(.putString content (prn-str copy-data))
    (.setContent db {(data-format-fn) copy-data})
    (.consume e)))

(defn- drag-done [e selection]
  #_(when (and
           (.isAccepted e)
           (= TransferMode/MOVE (.getTransferMode e)))
     (let [resources (roots selection)
           delete? (empty? (filter workspace/read-only? resources))]
       (when delete?
         (delete resources)))))

(defn- target [^Node node]
  (when node
    (if (instance? TreeCell node)
      node
      (target (.getParent node)))))

(defn- drag-over [^DragEvent e]
  (let [db (.getDragboard e)]
    (when-let [cell (target (.getTarget e))]
      ; Auto scrolling
      (let [view (.getTreeView cell)
            view-y (.getY (.sceneToLocal view (.getSceneX e) (.getSceneY e)))
            height (.getHeight (.getBoundsInLocal view))]
        (when (< view-y 15)
          (.scrollTo view (dec (.getIndex cell))))
        (when (> view-y (- height 15))
          (.scrollTo view (inc (.getIndex cell)))))
      (when (and (instance? TreeCell cell)
                 (not (.isEmpty cell))
                 (.hasContent db (data-format-fn)))
        (let [tgt-item (-> cell (.getTreeItem) (.getValue))
              graph-id (g/node-id->graph-id (:node-id tgt-item))
              project (project/get-project (:node-id (.getValue (.getRoot (.getTreeView cell)))))
              workspace (project/workspace project)]
          (try
            (let [fragment (edn/read-string
                             {:readers {'editor.outline_view.ResourceReference map->ResourceReference
                                        'dynamo.graph.Endpoint g/map->Endpoint}}
                             (.getContent db (data-format-fn)))
                  paste-data (g/paste graph-id fragment {:read-handlers {ResourceReference (partial resolve-reference workspace project)}})]
              (prn paste-data))
            #_(catch Throwable t
               (prn "unknown data" t)))
          ; TODO - handle target acceptance
          #_(prn "over" tgt-item)
          #_(when (not (workspace/read-only? tgt-resource))
             (let [tgt-path (-> tgt-resource resource/abs-path File. to-folder .getAbsolutePath ->path)
                   tgt-descendant? (not (empty? (filter (fn [^Path p] (or
                                                                        (.equals tgt-path (.getParent p))
                                                                        (.startsWith tgt-path p))) (map #(-> % .getAbsolutePath ->path) (.getFiles db)))))]
               (when (not tgt-descendant?)
                 (.acceptTransferModes e TransferMode/COPY_OR_MOVE)
                 (.consume e)))))))))

(defn- drag-dropped [^DragEvent e]
  #_(let [db (.getDragboard e)]
     (when (.hasFiles db)
       (let [resource (-> e (.getTarget) (target) (.getTreeItem) (.getValue))
             tgt-dir (to-folder (File. (workspace/abs-path resource)))]
         (doseq [src-file (.getFiles db)]
           (let [tgt-file (File. tgt-dir (FilenameUtils/getName (.toString src-file)))]
             (if (.isDirectory src-file)
               (FileUtils/copyDirectory src-file tgt-file)
               (FileUtils/copyFile src-file tgt-file))))
         ; TODO - notify move instead of ordinary sync
         (workspace/fs-sync (workspace/workspace resource))))
     (.setDropCompleted e true)
     (.consume e)))

(defn- setup-tree-view [^TreeView tree-view ^ListChangeListener selection-listener selection-provider]
  (-> tree-view
      (.getSelectionModel)
      (.setSelectionMode SelectionMode/MULTIPLE))
  (-> tree-view
      (.getSelectionModel)
      (.getSelectedItems)
      (.addListener selection-listener))
  (doto tree-view
    (.setOnDragDetected (ui/event-handler e (drag-detected e (workspace/selection tree-view))))
    (.setOnDragDone (ui/event-handler e (drag-done e (workspace/selection tree-view)))))
  (ui/register-context-menu tree-view ::outline-menu)
  (let [over-handler (ui/event-handler e (drag-over e))
        dropped-handler (ui/event-handler e (drag-dropped e))]
    (.setCellFactory tree-view (reify Callback (call ^TreeCell [this view]
                                                 (let [cell (proxy [TreeCell] []
                                                              (updateItem [item empty]
                                                                (let [this ^TreeCell this]
                                                                  (proxy-super updateItem item empty)
                                                                  (if empty
                                                                    (do
                                                                      (proxy-super setText nil)
                                                                      (proxy-super setGraphic nil)
                                                                      (proxy-super setContextMenu nil))
                                                                    (let [{:keys [label icon]} item]
                                                                      (proxy-super setText label)
                                                                      (proxy-super setGraphic (jfx/get-image-view icon 16)))))))]
                                                   (doto cell
                                                     (.setOnDragOver over-handler)
                                                     (.setOnDragDropped dropped-handler))))))))

(defn- propagate-selection [change selection-fn]
  (when-not *programmatic-selection*
    (when-let [changes (filter (comp not nil?) (and change (.getList change)))]
      ; TODO - handle selection order
      (selection-fn (map item->node-id changes)))))

(defn make-outline-view [graph tree-view selection-fn selection-provider]
  (let [selection-listener (reify ListChangeListener
                             (onChanged [this change]
                               (propagate-selection change selection-fn)))]
    (setup-tree-view tree-view selection-listener selection-provider)
    (first (g/tx-nodes-added (g/transact (g/make-node graph OutlineView :tree-view tree-view :selection-listener selection-listener))))))
