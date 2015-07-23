(ns editor.asset-browser
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.handler :as handler]
            [editor.jfx :as jfx]
            [editor.ui :as ui]
            [editor.resource :as resource]
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
           [javafx.scene.input Clipboard ClipboardContent]
           [javafx.scene.input DragEvent TransferMode]
           [javafx.scene Scene Node Parent]
           [javafx.scene.control Button ColorPicker Label TextField TitledPane TextArea TreeItem TreeCell TreeView Menu MenuItem SeparatorMenuItem MenuBar Tab ProgressBar ContextMenu SelectionMode]
           [javafx.scene.image Image ImageView WritableImage PixelWriter]
           [javafx.scene.input MouseEvent KeyCombination ContextMenuEvent]
           [javafx.scene.layout AnchorPane GridPane StackPane HBox Priority]
           [javafx.scene.paint Color]
           [javafx.stage Stage FileChooser]
           [javafx.util Callback]
           [java.io File]
           [java.nio.file Path Paths Files attribute.FileAttribute]
           [java.util.prefs Preferences]
           [javax.media.opengl GL GL2 GLContext GLProfile GLDrawableFactory GLCapabilities]
           [org.apache.commons.io FileUtils FilenameUtils IOUtils]))

(declare tree-item)

(def ^:private empty-string-array (into-array String []))

(defn- ->path [s]
  (Paths/get s empty-string-array))

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
        (or (not= :folder (workspace/source-type (.getValue ^TreeItem this)))
            (empty? (:children (.getValue ^TreeItem this)))))
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
                 {:label "Copy"
                  :command :copy
                  :acc "Shortcut+C"}
                 {:label "Cut"
                  :command :cut
                  :acc "Shortcut+X"}
                 {:label "Paste"
                  :command :paste
                  :acc "Shortcut+V"}
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
(defn- is-deletable-resource [x] (and (satisfies? workspace/Resource x)
                                      (not (workspace/read-only? x))
                                      (not (#{"/" "/game.project"} (workspace/proj-path x)))))
(defn- is-file-resource [x] (and (satisfies? workspace/Resource x)
                                 (= :file (workspace/source-type x))))

(handler/defhandler :open :asset-browser
  (enabled? [selection] (every? is-file-resource selection))
  (run [selection open-fn]
       (doseq [resource selection]
         (open-fn resource))))

(defn- roots [resources]
  (let [resources (into {} (map (fn [resource] [(->path (workspace/proj-path resource)) resource]) resources))
        roots (loop [paths (keys resources)
                     roots []]
                (if-let [path (first paths)]
                  (let [roots (if (empty? (filter #(.startsWith path %) roots))
                                (conj roots path)
                                roots)]
                    (recur (rest paths) roots))
                  roots))]
    (mapv #(resources %) roots)))

(defn tmp-file [^File dir resource]
  (let [f (File. dir (workspace/resource-name resource))]
    (if (= :file (workspace/source-type resource))
      (with-open [out (io/writer f)]
        (IOUtils/copy (io/input-stream resource) out))
      (do
        (.mkdirs f)
        (doseq [c (:children resource)]
          (tmp-file f c))))
    f))

(defn- fileify [resources]
  (let [tmp (doto (-> (Files/createTempDirectory "asset-dnd" (into-array FileAttribute []))
                    (.toFile))
              (.deleteOnExit))]
    (mapv (fn [r]
            (let [^String abs-path (or (workspace/abs-path r) (.getAbsolutePath ^File (tmp-file tmp r)))]
              (File. abs-path)))
          resources)))

(defn- delete [resources]
  (when (not (empty? resources))
    (let [workspace (workspace/workspace (first resources))]
      (doseq [resource resources]
        (let [f (File. (workspace/abs-path resource))]
          (if (.isDirectory f)
            (FileUtils/deleteDirectory f)
            (.delete (File. (workspace/abs-path resource))))))
      (workspace/fs-sync workspace))))

(defn- copy [files]
  (let [cb (Clipboard/getSystemClipboard)
        content (ClipboardContent.)]
    (.putFiles content files)
    (.setContent cb content)))

(handler/defhandler :copy :asset-browser
  (enabled? [selection] (not (empty? selection)))
  (run [selection]
       (copy (-> selection roots fileify))))

(handler/defhandler :cut :asset-browser
  (enabled? [selection] (every? is-deletable-resource selection))
  (run [selection]
       (let [tmp (doto (-> (Files/createTempDirectory "asset-cut" (into-array FileAttribute []))
                         (.toFile))
                   (.deleteOnExit))]
         (copy (mapv #(tmp-file tmp %) (roots selection))))
       (delete selection)))

(defn- unique [^File f]
  (if (.exists f)
    (let [path (.getAbsolutePath f)
          ext (FilenameUtils/getExtension path)
          path (str (FilenameUtils/removeExtension path) "_copy")
          path (if (not (empty? ext))
                 (str path "." ext)
                 path)]
      (recur (File. path)))
    f))

(handler/defhandler :paste :asset-browser
  (enabled? [selection]
            (let [cb (Clipboard/getSystemClipboard)]
              (and (.hasFiles cb)
                   (= 1 (count selection))
                   (empty? (filter workspace/read-only? selection)))))
  (run [selection]
       (let [resource (first selection)
             tgt-dir (to-folder (File. (workspace/abs-path resource)))]
         (doseq [src-file (.getFiles (Clipboard/getSystemClipboard))
                 :let [tgt-dir (if (= tgt-dir src-file)
                                 (.getParent tgt-dir)
                                 tgt-dir)]]
           (let [tgt-file (unique (File. tgt-dir (FilenameUtils/getName (.toString src-file))))]
             (if (.isDirectory src-file)
               (FileUtils/copyDirectory src-file tgt-file)
               (FileUtils/copyFile src-file tgt-file))))
         ; TODO - notify move instead of ordinary sync
         (workspace/fs-sync (workspace/workspace resource)))))

(handler/defhandler :delete :asset-browser
  (enabled? [selection] (every? is-deletable-resource selection))
  (run [selection]
       (delete selection)))

(defn- to-folder [file]
  (if (.isFile file) (.getParentFile file) file))

(handler/defhandler :show-in-desktop :asset-browser
  (enabled? [selection] (and (= 1 (count selection)) (not= nil (workspace/abs-path (first selection)))) )
  (run [selection] (let [f (File. ^String (workspace/abs-path (first selection)))]
                     (.open (Desktop/getDesktop) (to-folder f)))))

(def update-tree-view nil)

(handler/defhandler :new-file :asset-browser
  (label [user-data] (if-not user-data
                       "New"
                       (let [rt (:resource-type user-data)]
                         (or (:label rt) (:ext rt)))))
  (enabled? [selection] (and (= (count selection) 1) (not= nil (workspace/abs-path (first selection)))))
  (run [selection user-data workspace tree-view open-fn]
       (let [resource (first selection)
             f (File. ^String (workspace/abs-path resource))
             base-folder (to-folder f)
             rt (:resource-type user-data)]
         (when-let [f (dialogs/make-new-file-dialog (File. (:root workspace)) base-folder (or (:label rt) (:ext rt)) (:ext rt))]
           (spit f (workspace/template rt))
           (workspace/fs-sync workspace)
           (let [resource (FileResource. workspace f [])]
             (update-tree-view tree-view (g/node-value workspace :resource-tree) [resource])
             (open-fn resource)))))
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
  (run [selection workspace] (let [f (File. ^String (workspace/abs-path (first selection)))
                                   base-folder (to-folder f)]
                               (when-let [new-folder-name (dialogs/make-new-folder-dialog base-folder)]
                                 (.mkdir (resolve-sub-folder base-folder new-folder-name))
                                 (workspace/fs-sync workspace)))))

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

(defn update-tree-view [tree-view resource-tree selection]
  (let [root (.getRoot tree-view)
        new-root (->>
                   (tree-item resource-tree)
                   (sync-tree root))]
    (when new-root
      (.setExpanded new-root true))
    (.setRoot tree-view new-root)
    (sync-selection tree-view new-root (mapv workspace/proj-path selection))
    tree-view))

(g/defnk produce-tree-view [tree-view resource-tree]
  (update-tree-view tree-view resource-tree (workspace/selection tree-view)))

(defn- drag-detected [e selection]
  (let [resources (roots selection)
        files (fileify resources)
        mode (if (empty? (filter workspace/read-only? resources))
               TransferMode/MOVE
               TransferMode/COPY)
        db (.startDragAndDrop (.getSource e) (into-array TransferMode [mode]))
        content (ClipboardContent.)]
    (.putFiles content files)
    (.setContent db content)
    (.consume e)))

(defn- drag-done [e selection]
  (when (and
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
      (when (and (instance? TreeCell cell)
                 (not (.isEmpty cell))
                 (.hasFiles db))
        ; Auto scrolling
        (let [view (.getTreeView cell)
              view-y (.getY (.sceneToLocal view (.getSceneX e) (.getSceneY e)))
              height (.getHeight (.getBoundsInLocal view))]
          (when (< view-y 15)
            (.scrollTo view (dec (.getIndex cell))))
          (when (> view-y (- height 15))
            (.scrollTo view (inc (.getIndex cell)))))
        (let [tgt-resource (-> cell (.getTreeItem) (.getValue))]
          (when (not (workspace/read-only? tgt-resource))
            (let [tgt-path (-> tgt-resource resource/abs-path File. to-folder .getAbsolutePath ->path)
                  tgt-descendant? (not (empty? (filter (fn [^Path p] (or
                                                                       (.equals tgt-path (.getParent p))
                                                                       (.startsWith tgt-path p))) (map #(-> % .getAbsolutePath ->path) (.getFiles db)))))]
              (when (not tgt-descendant?) 
                (.acceptTransferModes e TransferMode/COPY_OR_MOVE)
                (.consume e)))))))))

(defn- drag-dropped [^DragEvent e]
  (let [db (.getDragboard e)]
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

(defn- setup-asset-browser [workspace ^TreeView tree-view open-resource-fn]
  (.setSelectionMode (.getSelectionModel tree-view) SelectionMode/MULTIPLE)
  (let [handler (reify EventHandler
                  (handle [this e]
                    (when (= 2 (.getClickCount ^MouseEvent e))
                      (let [item (-> tree-view (.getSelectionModel) (.getSelectedItem))
                            resource (.getValue ^TreeItem item)]
                        (when (= :file (workspace/source-type resource))
                          (open-resource-fn resource))))))
        over-handler (ui/event-handler e (drag-over e))
        done-handler (ui/event-handler e (drag-done e (workspace/selection tree-view)))
        dropped-handler (ui/event-handler e (drag-dropped e))
        detected-handler (ui/event-handler e (drag-detected e (workspace/selection tree-view)))]
    (.setOnDragDetected tree-view detected-handler)
    (.setOnDragDone tree-view done-handler)
    (.setOnMouseClicked tree-view handler)
    (.setCellFactory tree-view (reify Callback (call ^TreeCell [this view]
                                                 (let [cell (proxy [TreeCell] []
                                                            (updateItem [resource empty]
                                                              (let [this ^TreeCell this]
                                                                (proxy-super updateItem resource empty)
                                                                   (let [name (or (and (not empty) (not (nil? resource)) (workspace/resource-name resource)) nil)]
                                                                     (proxy-super setText name))
                                                                   (proxy-super setGraphic (jfx/get-image-view (workspace/resource-icon resource) 16)))))]
                                                   (doto cell
                                                     (.setOnDragOver over-handler)
                                                     (.setOnDragDropped dropped-handler))))))

    (ui/register-context-menu tree-view ::resource-menu)))

(g/defnode AssetBrowser
  (property tree-view TreeView)

  (input resource-tree FileResource)

  (output tree-view TreeView :cached produce-tree-view))

(defn make-asset-browser [graph workspace tree-view open-resource-fn]
  (let [asset-browser (first
                        (g/tx-nodes-added
                          (g/transact
                            (g/make-nodes graph
                                          [asset-browser [AssetBrowser :tree-view tree-view]]
                                          (g/connect workspace :resource-tree asset-browser :resource-tree)))))]
    (ui/context! tree-view :asset-browser {:tree-view tree-view :workspace workspace :open-fn open-resource-fn} tree-view)
    (setup-asset-browser workspace tree-view open-resource-fn)
    asset-browser))
