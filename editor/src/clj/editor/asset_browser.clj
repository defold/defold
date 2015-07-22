(ns editor.asset-browser
  (:require [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.handler :as handler]
            [editor.jfx :as jfx]
            [editor.ui :as ui]
            [editor.workspace :as workspace]
            [editor.dialogs :as dialogs])
  (:import [com.defold.editor Start]
           [editor.resource FileResource]
           [com.jogamp.opengl.util.awt Screenshot]
           [java.awt Desktop]
           [javafx.application Platform]
           [javafx.collections FXCollections ObservableList]
           [javafx.embed.swing SwingFXUtils]
           [javafx.event ActionEvent EventHandler]
           [javafx.fxml FXMLLoader]
           [javafx.geometry Insets]
           [javafx.scene Scene Node Parent]
           [javafx.scene.control Button ColorPicker Label TextField TitledPane TextArea TreeItem TreeCell TreeView Menu MenuItem SeparatorMenuItem MenuBar Tab ProgressBar ContextMenu SelectionMode]
           [javafx.scene.image Image ImageView WritableImage PixelWriter]
           [javafx.scene.input MouseEvent KeyCombination ContextMenuEvent]
           [javafx.scene.layout AnchorPane GridPane StackPane HBox Priority]
           [javafx.scene.paint Color]
           [javafx.stage Stage FileChooser]
           [javafx.util Callback]
           [java.io File]
           [java.nio.file Paths]
           [java.util.prefs Preferences]
           [javax.media.opengl GL GL2 GLContext GLProfile GLDrawableFactory GLCapabilities]
           [org.apache.commons.io FilenameUtils]))

(declare tree-item)


; TreeItem creator
(defn-  ^ObservableList list-children [parent]
  (let [children (:children parent)
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
        (not= :folder (workspace/source-type (.getValue ^TreeItem this))))
      (getChildren []
        (let [this ^TreeItem this
              ^ObservableList children (proxy-super getChildren)]
          (when-not @cached
            (reset! cached true)
            (.setAll children (list-children (.getValue this))))
          children)))))

(ui/extend-menu ::resource-menu nil
                [{:label "Open"
                  :command :open}
                 {:label "Delete"
                  :command :delete
                  :icon "icons/cross.png"
                  :acc "Shortcut+BACKSPACE"}
                 {:label "Show in Desktop"
                  :command :show-in-desktop}
                 {:label :separator}
                 {:label "New"
                  :command :new-file
                  :icon "icons/32/Icons_29-AT-Unkown.png"}
                 {:label "New Folder"
                  :command :new-folder
                  :icon "icons/32/Icons_01-Folder-closed.png"}])

(defn- is-resource [x] (satisfies? workspace/Resource x))
(defn- is-deletable-resource [x] (and (satisfies? workspace/Resource x) (not (workspace/read-only? x))))
(defn- is-file-resource [x] (and (satisfies? workspace/Resource x)
                                 (= :file (workspace/source-type x))))

(handler/defhandler :open :asset-browser
  (enabled? [selection] (every? is-file-resource selection))
  (run [selection open-fn]
       (doseq [resource selection]
         (open-fn resource))))

(handler/defhandler :delete :asset-browser
  (enabled? [selection] (every? is-deletable-resource selection))
  (run [selection workspace]
       (doseq [resource selection]
         (.delete (File. (workspace/abs-path resource))))
       (workspace/fs-sync workspace)))

(handler/defhandler :show-in-desktop :asset-browser
  (enabled? [selection] (and (= 1 (count selection)) (not= nil (workspace/abs-path (first selection)))) )
  (run [selection] (let [f (File. ^String (workspace/abs-path (first selection)))
                         dir (to-folder f)]
                     (.open (Desktop/getDesktop) dir))))

(defn- to-folder [file]
  (if (.isFile file) (.getParentFile file) file))

(handler/defhandler :new-file :asset-browser
  (label [user-data] (if-not user-data
                       "New"
                       (let [rt (:resource-type user-data)]
                         (or (:label rt) (:ext rt)))))
  (enabled? [selection] (and (= (count selection) 1) (not= nil (workspace/abs-path (first selection)))))
  (run [selection user-data workspace]
       (let [resource (first selection)
             f (File. ^String (workspace/abs-path resource))
             base-folder (to-folder f)
             rt (:resource-type user-data)
             name (dialogs/make-new-file-dialog base-folder (format "untitled.%s" (:ext rt)))
             f (File. base-folder (format "%s.%s" (FilenameUtils/removeExtension name) (:ext rt)))]
         (spit f (workspace/template rt))
         (workspace/fs-sync workspace)))
  (options [workspace selection user-data]
           (when (not user-data)
             (let [resource-types (filter (fn [rt] (workspace/template rt)) (workspace/get-resource-types workspace))]
               (sort-by (fn [rt] (string/lower-case (:label rt))) (map (fn [res-type] {:label (or (:label res-type) (:ext res-type))
                                                                                       :icon (:icon res-type)
                                                                                       :command :new-file
                                                                                       :user-data {:resource-type res-type}}) resource-types))))))

(defn- resolve-sub-folder [base-folder new-folder-name]
  (.toFile (.resolve (.toPath base-folder) new-folder-name)))

(handler/defhandler :new-folder :asset-browser
  (enabled? [selection] (and (= (count selection) 1) (not= nil (workspace/abs-path (first selection)))))
  (run [selection] (let [f (File. ^String (workspace/abs-path (first selection)))
                         base-folder (to-folder f)]
                     (when-let [new-folder-name (dialogs/make-new-folder-dialog base-folder)]
                       (.mkdir (resolve-sub-folder base-folder new-folder-name))))))

(defn- item->path [^TreeItem item]
  (-> item (.getValue) (workspace/proj-path)))

(defn- sync-tree [old-root new-root]
  (let [item-seq (ui/tree-item-seq old-root)
        expanded (zipmap (map item->path item-seq)
                         (map #(.isExpanded ^TreeItem %) item-seq))]
    (doseq [^TreeItem item (ui/tree-item-seq new-root)]
      (when (get expanded (item->path item))
        (.setExpanded item true))))
  new-root)

(defn- auto-expand [items selected-paths]
  (reduce #(or %1 %2) false (map (fn [item] (let [path (item->path item)
                                                  selected (boolean (selected-paths path))
                                                  expanded (auto-expand (.getChildren item) selected-paths)]
                                              (when expanded (.setExpanded item expanded))
                                              selected)) items)))

(defn- sync-selection [^TreeView tree-view root selection]
  (when (and root (not (empty? selection)))
    (let [selected-paths (set selection)]
      (auto-expand (.getChildren root) selected-paths)
      (let [count (.getExpandedItemCount tree-view)
            selected-indices (filter #(selected-paths (item->path (.getTreeItem tree-view %))) (range count))]
        (when (not (empty? selected-indices))
          (doto (-> tree-view (.getSelectionModel))
            (.selectIndices (int (first selected-indices)) (int-array (rest selected-indices)))))))))

(g/defnk update-tree-view [tree-view resource-tree]
  (let [selection (workspace/selection tree-view)
        root (.getRoot tree-view)
        new-root (->>
                   (tree-item resource-tree)
                   (sync-tree root))]
    (when new-root
      (.setExpanded new-root true))
    (.setRoot tree-view new-root)
    (sync-selection tree-view new-root (mapv workspace/proj-path selection))
    tree-view))

(defn- setup-asset-browser [workspace ^TreeView tree-view open-resource-fn]
  (.setSelectionMode (.getSelectionModel tree-view) SelectionMode/MULTIPLE)
  (let [handler (reify EventHandler
                  (handle [this e]
                    (when (= 2 (.getClickCount ^MouseEvent e))
                      (let [item (-> tree-view (.getSelectionModel) (.getSelectedItem))
                            resource (.getValue ^TreeItem item)]
                        (when (= :file (workspace/source-type resource))
                          (open-resource-fn resource))))))]
    (.setOnMouseClicked tree-view handler)
    (.setCellFactory tree-view (reify Callback (call ^TreeCell [this view]
                                                 (proxy [TreeCell] []
                                                   (updateItem [resource empty]
                                                     (let [this ^TreeCell this]
                                                       (proxy-super updateItem resource empty)
                                                          (let [name (or (and (not empty) (not (nil? resource)) (workspace/resource-name resource)) nil)]
                                                            (proxy-super setText name))
                                                          (proxy-super setGraphic (jfx/get-image-view (workspace/resource-icon resource) 16))))))))

    (ui/register-context-menu tree-view ::resource-menu)))

(g/defnode AssetBrowser
  (property tree-view TreeView)

  (input resource-tree FileResource)

  (output tree-view TreeView :cached update-tree-view))

(defn make-asset-browser [graph workspace tree-view open-resource-fn]
  (let [asset-browser (first
                        (g/tx-nodes-added
                          (g/transact
                            (g/make-nodes graph
                                          [asset-browser [AssetBrowser :tree-view tree-view]]
                                          (g/connect (g/node-id workspace) :resource-tree asset-browser :resource-tree)))))]
    (ui/context! tree-view :asset-browser {:tree-view tree-view :workspace workspace :open-fn open-resource-fn} tree-view)
    (setup-asset-browser workspace tree-view open-resource-fn)
    asset-browser))
