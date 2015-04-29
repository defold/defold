(ns editor.outline-view
  (:require [dynamo.graph :as g]
            [editor.jfx :as jfx]
            [editor.menu :as menu]
            [editor.workspace :as workspace]
            [dynamo.types :as t])
  (:import [com.defold.editor Start]
           [com.jogamp.opengl.util.awt Screenshot]
           [javafx.application Platform]
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

; TreeItem creator
(defn- list-children [parent]
  (let [children (:children parent)]
    (if (empty? children)
      (FXCollections/emptyObservableList)
      (doto (FXCollections/observableArrayList)
        (.addAll (map tree-item children))))))

; NOTE: Without caching stack-overflow... WHY?
(defn tree-item [parent]
  (let [cached (atom false)]
    (proxy [TreeItem] [parent]
      (isLeaf []
        (empty? (:children parent)))
      (getChildren []
        (let [children (proxy-super getChildren)]
          (when-not @cached
            (reset! cached true)
            (.setAll children (list-children parent)))
          children)))))

(defn- map-filter [filter-fn m]
  (into {} (filter filter-fn m)))

(defn tree-item-seq [item]
  (if item
    (tree-seq 
      #(not (.isLeaf %))
      #(seq (.getChildren %))
      item)
    []))

(defn- sync-tree [old-root new-root]
  (let [item-seq (tree-item-seq old-root)
        expanded (zipmap (map #(:self (.getValue ^TreeItem %)) item-seq)
                         (map #(.isExpanded ^TreeItem %) item-seq))]
    (doseq [^TreeItem item (tree-item-seq new-root)]
      (when (get expanded (:self (.getValue item)))
        (.setExpanded item true))))
  new-root)

(defn- sync-selection [tree-view new-root selection]
  (let [selected-ids (set (map g/node-id selection))
        selected-items (filter #(selected-ids (g/node-id (:self (.getValue %)))) (tree-item-seq new-root))
        selected-indices (map #(.getRow tree-view %) selected-items)]
    (when (not (empty? selected-indices))
      (doto (-> tree-view (.getSelectionModel))
        (.selectIndices (int (first selected-indices)) (int-array (rest selected-indices)))))))

(g/defnk update-tree-view [self tree-view root-cache active-resource active-outline open-resources selection selection-listener]
  (let [resource-set (set open-resources)
       root (get root-cache active-resource)
       new-root (when active-outline (sync-tree root (tree-item active-outline)))
       new-cache (assoc (map-filter (fn [[resource _]] (contains? resource-set resource)) root-cache) active-resource new-root)]
    (doto (-> tree-view (.getSelectionModel) (.getSelectedItems))
      (.removeListener selection-listener))
    (.setRoot tree-view new-root)
    (sync-selection tree-view new-root selection)
    (doto (-> tree-view (.getSelectionModel) (.getSelectedItems))
      (.addListener selection-listener))
    (g/transact (g/set-property self :root-cache new-cache))))

(g/defnode OutlineView
  (property tree-view TreeView)
  (property root-cache t/Any)
  (property selection-listener ListChangeListener)

  (input active-outline t/Any)
  (input active-resource workspace/Resource)
  (input open-resources t/Any)
  (input selection t/Any)
  
  (output tree-view TreeView :cached update-tree-view))

(defn- setup-tree-view [tree-view selection-listener]
  (-> tree-view
      (.getSelectionModel)
      (.setSelectionMode SelectionMode/MULTIPLE))
  (-> tree-view
      (.getSelectionModel)
      (.getSelectedItems)
      (.addListener selection-listener))
  (.setCellFactory tree-view (reify Callback (call ^TreeCell [this view]
                                               (proxy [TreeCell] []
                                                 (updateItem [item empty]
                                                   (proxy-super updateItem item empty)
                                                   (if empty
                                                     (do
                                                       (proxy-super setText nil)
                                                       (proxy-super setGraphic nil)
                                                       (proxy-super setContextMenu nil))
                                                     (let [{:keys [label icon context-menu]} item]
                                                       (proxy-super setText label)
                                                       (proxy-super setGraphic (jfx/get-image-view icon))
                                                       (when context-menu (proxy-super setContextMenu (menu/make-context-menu context-menu)))))))))))

(defn make-outline-view [graph tree-view selection-fn]
  (let [selection-listener (reify ListChangeListener (onChanged [this change]
                                                       ; TODO - handle selection order
                                                       (selection-fn (map #(.getValue %1) (.getList change)))))]
    (setup-tree-view tree-view selection-listener)
    (first (g/tx-nodes-added (g/transact (g/make-node graph OutlineView :tree-view tree-view :selection-listener selection-listener))))))
