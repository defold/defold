(ns editor.asset-browser
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.handler :as handler]
            [editor.jfx :as jfx]
            [editor.ui :as ui]
            [editor.resource :as resource]
            [editor.workspace :as workspace]
            [editor.dialogs :as dialogs]
            [editor.util :as util])
  (:import [com.defold.editor Start]
           [editor.resource FileResource]
           [java.awt Desktop]
           [javafx.application Platform]
           [javafx.collections FXCollections ObservableList]
           [javafx.embed.swing SwingFXUtils]
           [javafx.event ActionEvent Event EventHandler]
           [javafx.fxml FXMLLoader]
           [javafx.geometry Insets]
           [javafx.scene.input Clipboard ClipboardContent]
           [javafx.scene.input DragEvent TransferMode MouseEvent]
           [javafx.scene Scene Node Parent]
           [javafx.scene.control Button ColorPicker Label TextField TitledPane TextArea TreeItem TreeView Menu MenuItem SeparatorMenuItem MenuBar Tab ProgressBar ContextMenu SelectionMode]
           [javafx.scene.image Image ImageView WritableImage PixelWriter]
           [javafx.scene.input MouseEvent KeyCombination ContextMenuEvent]
           [javafx.scene.layout AnchorPane GridPane StackPane HBox Priority]
           [javafx.scene.paint Color]
           [javafx.stage Stage FileChooser]
           [javafx.util Callback]
           [java.io File]
           [java.nio.file Path Paths Files attribute.FileAttribute]
           [java.util.prefs Preferences]
           [com.jogamp.opengl GL GL2 GLContext GLProfile GLDrawableFactory GLCapabilities]
           [org.apache.commons.io FileUtils FilenameUtils IOUtils]
           [com.defold.control TreeCell]))

(set! *warn-on-reflection* true)

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
        (or (not= :folder (resource/source-type (.getValue ^TreeItem this)))
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
                  :icon "icons/32/Icons_S_14_linkarrow.png"
                  :command :open}
                 {:label "Open As"
                  :icon "icons/32/Icons_S_14_linkarrow.png"
                  :command :open-as}
                 {:label "Show in Desktop"
                  :icon "icons/32/Icons_S_14_linkarrow.png"
                  :command :show-in-desktop}
                 {:label :separator}
                 {:label "Copy"
                  :command :copy
                  :acc "Shortcut+C"}
                 {:label "Cut"
                  :command :cut
                  :acc "Shortcut+X"}
                 {:label "Paste"
                  :command :paste
                  :acc "Shortcut+V"}
                 {:label "Rename"
                  :command :rename}
                 {:label "Delete"
                  :command :delete
                  :icon "icons/32/Icons_M_06_trash.png"
                  :acc "Shortcut+BACKSPACE"}
                 {:label :separator}
                 {:label "New"
                  :command :new-file
                  :icon "icons/64/Icons_29-AT-Unknown.png"}
                 {:label "New Folder"
                  :command :new-folder
                  :icon "icons/32/Icons_01-Folder-closed.png"}])

(defn- is-resource [x] (satisfies? resource/Resource x))
(defn- is-deletable-resource [x] (and (satisfies? resource/Resource x)
                                      (not (resource/read-only? x))
                                      (not (#{"/" "/game.project"} (resource/proj-path x)))))
(defn- is-resource-file [x] (and (satisfies? resource/Resource x)
                                 (= :file (resource/source-type x))))

(defn- roots [resources]
  (let [resources (into {} (map (fn [resource] [(->path (resource/proj-path resource)) resource]) resources))
        roots (loop [paths (keys resources)
                     roots []]
                (if-let [^Path path (first paths)]
                  (let [roots (if (empty? (filter (fn [^Path p] (.startsWith path p)) roots))
                                (conj roots path)
                                roots)]
                    (recur (rest paths) roots))
                  roots))]
    (mapv #(resources %) roots)))

(defn tmp-file [^File dir resource]
  (let [f (File. dir (resource/resource-name resource))]
    (if (= :file (resource/source-type resource))
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
            (let [abs-path (or (resource/abs-path r) (.getAbsolutePath ^File (tmp-file tmp r)))]
              (File. abs-path)))
          resources)))

(defn rename [resource ^String new-name]
  (when resource
    (let [workspace        (resource/workspace resource)
          srcf             (File. (resource/abs-path resource))
          dstf             (and new-name (File. (.getParent srcf) new-name))]
      (when dstf
        (if (.isDirectory srcf)
          (FileUtils/moveDirectory srcf dstf)
          (FileUtils/moveFile srcf dstf))
        (workspace/resource-sync! workspace true [[srcf dstf]])))))

(defn delete [resources]
  (when (not (empty? resources))
    (let [workspace (resource/workspace (first resources))]
      (doseq [resource resources]
        (let [f (File. (resource/abs-path resource))]
          (if (.isDirectory f)
            (FileUtils/deleteDirectory f)
            (.delete (File. (resource/abs-path resource))))))
      (workspace/resource-sync! workspace))))

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
  (enabled? [selection] (and (seq selection) (every? is-deletable-resource selection)))
  (run [selection]
       (let [tmp (doto (-> (Files/createTempDirectory "asset-cut" (into-array FileAttribute []))
                         (.toFile))
                   (.deleteOnExit))]
         (copy (mapv #(tmp-file tmp %) (roots selection))))
    (delete selection)))

(defn- unique
  [^File f exists-fn name-fn]
  (if (exists-fn f)
    (let [path (.getAbsolutePath f)
          ext (FilenameUtils/getExtension path)
          new-path (str (FilenameUtils/getFullPath path)
                        (name-fn (FilenameUtils/getBaseName path))
                        (when (seq ext) (str "." ext)))]
      (recur (File. new-path) exists-fn name-fn))
    f))

(defn- ensure-unique-dest-files
  [make-name-fn src-dest-pairs]
  (loop [[[src dest :as pair] & rest] src-dest-pairs
         new-names #{}
         ret []]
    (if pair
      (let [new-dest (unique dest #(or (.exists ^File %) (new-names %)) (make-name-fn dest))]
        (recur rest (conj new-names new-dest) (conj ret [src new-dest])))
      ret)))

(defn- numbering-name-fn
  [^File f]
  (let [basename (FilenameUtils/getBaseName (str f))
        num (java.util.concurrent.atomic.AtomicInteger. 1)]
    (fn [_] (str basename (.getAndIncrement num)))))

(defn- select-files! [workspace tree-view files]
  (let [selected-paths (mapv (partial resource/file->proj-path (workspace/project-path workspace)) files)]
    (ui/user-data! tree-view ::pending-selection selected-paths)))

(defn- allow-resource-transfer? [tgt-resource src-files]
  (and (not (resource/read-only? tgt-resource))
       (let [^Path tgt-path (-> tgt-resource resource/abs-path File. util/to-folder .getAbsolutePath ->path)
             src-paths (map (fn [^File f] (-> f .getAbsolutePath ->path))
                            src-files)
             descendant (some (fn [^Path p] (or (.equals tgt-path (.getParent p))
                                                (.startsWith tgt-path p)))
                              src-paths)]
         (nil? descendant))))

(handler/defhandler :paste :asset-browser
  (enabled? [selection]
            (let [cb (Clipboard/getSystemClipboard)]
              (and (.hasFiles cb)
                   (= 1 (count selection))
                   (allow-resource-transfer? (first selection) (.getFiles cb)))))
  (run [selection workspace tree-view]
       (let [resource (first selection)
             src-files (.getFiles (Clipboard/getSystemClipboard))
             ^File tgt-dir (reduce (fn [^File tgt ^File src]
                                     (if (= tgt src)
                                       (.getParentFile ^File tgt)
                                       tgt))
                                   (util/to-folder (File. (resource/abs-path resource))) src-files)
             pairs (->> src-files
                        (map (fn [^File f] [f (File. tgt-dir (FilenameUtils/getName (.toString f)))]))
                        (ensure-unique-dest-files (constantly #(str % "_copy"))))]
         (doseq [[^File src-file ^File tgt-file] pairs]
           (if (.isDirectory src-file)
             (FileUtils/copyDirectory src-file tgt-file)
             (FileUtils/copyFile src-file tgt-file)))
         (select-files! workspace tree-view (mapv second pairs))
         (workspace/resource-sync! (resource/workspace resource)))))

(handler/defhandler :rename :asset-browser
  (enabled? [selection]
            (and (= 1 (count selection))
                 (not (resource/read-only? (first selection)))))
  (run [selection]
    (let [resource (first selection)
          dir? (= :folder (resource/source-type resource))
          ext (:ext (resource/resource-type resource))
          ^String new-name (dialogs/make-rename-dialog
                             (if dir? "Rename folder" "Rename file")
                             (if dir? "New folder name" "New file name")
                             (if dir?
                               (resource/resource-name resource)
                               (string/replace (resource/resource-name resource)
                                               (re-pattern (str "." ext))
                                               ""))
                             ext)]
      (rename resource new-name))))

(handler/defhandler :delete :asset-browser
  (enabled? [selection] (and (seq selection) (every? is-deletable-resource selection)))
  (run [selection]
    (let [names (apply str (interpose ", " (map resource/resource-name selection)))]
      (and (dialogs/make-confirm-dialog (format "Are you sure you want to delete %s?" names))
           (delete selection)))))

(def update-tree-view nil)

(defn- resolve-sub-folder [^File base-folder ^String new-folder-name]
  (.toFile (.resolve (.toPath base-folder) new-folder-name)))

(handler/defhandler :new-folder :asset-browser
  (enabled? [selection] (and (= (count selection) 1) (not= nil (resource/abs-path (first selection)))))
  (run [selection workspace] (let [f (File. (resource/abs-path (first selection)))
                                   base-folder (util/to-folder f)]
                               (when-let [new-folder-name (dialogs/make-new-folder-dialog base-folder)]
                                 (.mkdir ^File (resolve-sub-folder base-folder new-folder-name))
                                 (workspace/resource-sync! workspace)))))

(defn- item->path [^TreeItem item]
  (-> item (.getValue) (resource/proj-path)))

(defn- sync-tree [old-root new-root]
  (let [item-seq (ui/tree-item-seq old-root)
        expanded (zipmap (map item->path item-seq)
                         (map #(.isExpanded ^TreeItem %) item-seq))]
    (doseq [^TreeItem item (ui/tree-item-seq new-root)]
      (when (get expanded (item->path item))
        (.setExpanded item true))))
  new-root)

(defn- auto-expand [items selected-paths]
  (reduce #(or %1 %2) false (map (fn [^TreeItem item] (let [path (item->path item)
                                                            selected (boolean (selected-paths path))
                                                            expanded (auto-expand (.getChildren item) selected-paths)]
                                                        (when expanded (.setExpanded item expanded))
                                                        selected)) items)))

(defn- sync-selection [^TreeView tree-view ^TreeItem root selected-paths]
  (when (and root (seq selected-paths))
    (let [selected-paths (set selected-paths)]
      (auto-expand (.getChildren root) selected-paths)
      (let [count (.getExpandedItemCount tree-view)
            selected-indices (filter #(selected-paths (item->path (.getTreeItem tree-view %))) (range count))]
        (when (not (empty? selected-indices))
          (doto (-> tree-view (.getSelectionModel))
            (.selectIndices (int (first selected-indices)) (int-array (rest selected-indices)))))))))

(defn update-tree-view [^TreeView tree-view resource-tree selected-paths]
  (let [root (.getRoot tree-view)
        ^TreeItem new-root (->>
                             (tree-item resource-tree)
                             (sync-tree root))]
    (when new-root
      (.setExpanded new-root true))
    (.setRoot tree-view new-root)
    (sync-selection tree-view new-root selected-paths)
    tree-view))

(g/defnk produce-tree-view [_node-id tree-view resource-tree]
  (let [selected-paths (or (ui/user-data tree-view ::pending-selection)
                           (mapv resource/proj-path (handler/selection tree-view)))]
    (update-tree-view tree-view resource-tree selected-paths)
    (ui/user-data! tree-view ::pending-selection nil)
    tree-view))

(defn- drag-detected [^MouseEvent e selection]
  (let [resources (roots selection)
        files (fileify resources)
        mode (if (empty? (filter resource/read-only? resources))
               TransferMode/MOVE
               TransferMode/COPY)
        db (.startDragAndDrop ^Node (.getSource e) (into-array TransferMode [mode]))
        content (ClipboardContent.)]
    (when (= 1 (count resources))
      (.setDragView db (jfx/get-image (workspace/resource-icon (first resources)) 16)
                    0 16))
    (.putFiles content files)
    (.setContent db content)
    (.consume e)))

(defn- target [^Node node]
  (when node
    (if (instance? TreeCell node)
      node
      (target (.getParent node)))))

(defn- drag-done [^DragEvent e selection])

(defn- drag-entered [^DragEvent e]
  (let [^TreeCell cell (target (.getTarget e))]
    (when (and cell (not (.isEmpty cell)))
      (let [resource (.getValue (.getTreeItem cell))]
        (when (and (= :folder (resource/source-type resource))
                   (not (resource/read-only? resource)))
          (ui/add-style! cell "drop-target"))))))

(defn- drag-exited [^DragEvent e]
  (when-let [cell (target (.getTarget e))]
    (ui/remove-style! cell "drop-target")))

(defn- drag-over [^DragEvent e]
  (let [db (.getDragboard e)]
    (when-let [^TreeCell cell (target (.getTarget e))]
      (when (and (not (.isEmpty cell))
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
         (when (allow-resource-transfer? tgt-resource (.getFiles db))
           (.acceptTransferModes e TransferMode/COPY_OR_MOVE)
           (.consume e)))))))

(defn- find-files [^File src ^File tgt]
  (if (.isDirectory src)
    (let [base (.getAbsolutePath src)
          base-count (inc (count base))]
      (mapv (fn [^File f]
              (let [rel-path (subs (.getAbsolutePath f) base-count)]
                [f (File. tgt rel-path)]))
            (filter (fn [^File f] (.isFile f)) (file-seq src))))
    [[src tgt]]))


(defmulti resolve-conflicts (fn [strategy src-dest-pairs] strategy))

(defmethod resolve-conflicts :replace
  [_ src-dest-pairs]
  src-dest-pairs)

(defmethod resolve-conflicts :rename
  [_ src-dest-pairs]
  (ensure-unique-dest-files numbering-name-fn src-dest-pairs))

(defn- resolve-any-conflicts
  [src-dest-pairs]
  (let [files-by-existence (group-by (fn [[src ^File dest]] (.exists dest)) src-dest-pairs)
        conflicts (get files-by-existence true)
        non-conflicts (get files-by-existence false [])]
    (if (seq conflicts)
      (when-let [strategy (dialogs/make-resolve-file-conflicts-dialog conflicts)]
        (into non-conflicts (resolve-conflicts strategy conflicts)))
      non-conflicts)))

(defn- drag-dropped [^DragEvent e]
  (let [db (.getDragboard e)]
    (when (.hasFiles db)
      (let [target (-> e (.getTarget) ^TreeCell (target))
            tree-view (.getTreeView target)
            resource (-> target (.getTreeItem) (.getValue))
            ^File tgt-dir (util/to-folder (File. (resource/abs-path resource)))
            move? (and (= (.getGestureSource e) tree-view)
                       (= (.getTransferMode e) TransferMode/MOVE))
            pairs (->> (.getFiles db)
                       (mapv (fn [^File f] [f (File. tgt-dir (FilenameUtils/getName (.toString f)))]))
                       (resolve-any-conflicts)
                       (vec))
            moved (if move?
                    (vec (mapcat (fn [[^File src ^File tgt]]
                                   (find-files src tgt)) pairs))
                    [])
            workspace (resource/workspace resource)]
        (when pairs
          (doseq [[^File src-file ^File tgt-file] pairs]
            (if (= (.getTransferMode e) TransferMode/MOVE)
              (do
                (io/make-parents tgt-file)
                (.renameTo src-file tgt-file))
              (if (.isDirectory src-file)
                (FileUtils/copyDirectory src-file tgt-file)
                (FileUtils/copyFile src-file tgt-file))))
          (select-files! workspace tree-view (mapv second pairs))
          (workspace/resource-sync! workspace true moved))))
    (.setDropCompleted e true)
    (.consume e)))

(defn- setup-asset-browser [workspace ^TreeView tree-view open-resource-fn]
  (.setSelectionMode (.getSelectionModel tree-view) SelectionMode/MULTIPLE)
  (let [over-handler (ui/event-handler e (drag-over e))
        done-handler (ui/event-handler e (drag-done e (handler/selection tree-view)))
        dropped-handler (ui/event-handler e (drag-dropped e))
        detected-handler (ui/event-handler e (drag-detected e (handler/selection tree-view)))
        entered-handler (ui/event-handler e (drag-entered e))
        exited-handler (ui/event-handler e (drag-exited e))]
    (ui/bind-double-click! tree-view :open)
    (.setOnDragDetected tree-view detected-handler)
    (.setOnDragDone tree-view done-handler)
    (.setCellFactory tree-view (reify Callback (call ^TreeCell [this view]
                                                 (let [cell (proxy [TreeCell] []
                                                            (updateItem [resource empty]
                                                              (let [this ^TreeCell this]
                                                                (proxy-super updateItem resource empty)
                                                                (ui/update-tree-cell-style! this)
                                                                (let [name (or (and (not empty) (not (nil? resource)) (resource/resource-name resource)) nil)]
                                                                  (proxy-super setText name))
                                                                   (proxy-super setGraphic (jfx/get-image-view (workspace/resource-icon resource) 16)))))]
                                                   (doto cell
                                                     (.setOnDragOver over-handler)
                                                     (.setOnDragDropped dropped-handler)
                                                     (.setOnDragEntered entered-handler)
                                                     (.setOnDragExited exited-handler))))))

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
