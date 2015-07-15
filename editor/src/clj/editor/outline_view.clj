(ns editor.outline-view
  (:require [dynamo.graph :as g]
            [editor.handler :as handler]
            [editor.jfx :as jfx]
            [editor.ui :as ui]
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

(defn tree-item-seq [item]
  (if item
    (tree-seq
      #(not (.isLeaf ^TreeItem %))
      #(seq (.getChildren ^TreeItem %))
      item)
    []))

(defn- item->node-id [^TreeItem item]
  (:node-id (.getValue item)))

(defn- sync-tree [old-root new-root]
  (let [item-seq (tree-item-seq old-root)
        expanded (zipmap (map item->node-id item-seq)
                         (map #(.isExpanded ^TreeItem %) item-seq))]
    (doseq [^TreeItem item (tree-item-seq new-root)]
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

(g/defnk update-tree-view [_self ^TreeView tree-view root-cache active-resource active-outline open-resources selection selection-listener]
  (let [resource-set (set open-resources)
       root (get root-cache active-resource)
       new-root (when active-outline (sync-tree root (tree-item active-outline)))
       new-cache (assoc (map-filter (fn [[resource _]] (contains? resource-set resource)) root-cache) active-resource new-root)]
    (binding [*programmatic-selection* true]
      (when new-root
        (.setExpanded new-root true))
      (.setRoot tree-view new-root)
      (sync-selection tree-view new-root selection)
      (g/transact (g/set-property _self :root-cache new-cache)))))

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

(defn- setup-tree-view [^TreeView tree-view ^ListChangeListener selection-listener selection-provider]
  (-> tree-view
      (.getSelectionModel)
      (.setSelectionMode SelectionMode/MULTIPLE))
  (-> tree-view
      (.getSelectionModel)
      (.getSelectedItems)
      (.addListener selection-listener))
  (ui/register-context-menu tree-view ::outline-menu)
  (.setCellFactory tree-view (reify Callback (call ^TreeCell [this view]
                                               (proxy [TreeCell] []
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
                                                         (proxy-super setGraphic (jfx/get-image-view icon)))))))))))

(defn- propagate-selection [change selection-fn]
  (when-not *programmatic-selection*
    (when-let [changes (filter (comp not nil?) (and change (.getList change)))]
      ; TODO - handle selection order
      (selection-fn (map #(g/node-by-id (item->node-id %)) changes)))))

(defn make-outline-view [graph tree-view selection-fn selection-provider]
  (let [selection-listener (reify ListChangeListener
                             (onChanged [this change]
                               (propagate-selection change selection-fn)))]
    (setup-tree-view tree-view selection-listener selection-provider)
    (first (g/tx-nodes-added (g/transact (g/make-node graph OutlineView :tree-view tree-view :selection-listener selection-listener))))))
