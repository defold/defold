(ns editor.asset-browser
  (:require [clojure.java.io :as io]
            [clojure.string :as string]
            [dynamo.graph :as g]
            [editor.error-reporting :as error-reporting]
            [editor.handler :as handler]
            [editor.jfx :as jfx]
            [editor.ui :as ui]
            [editor.resource :as resource]
            [editor.workspace :as workspace]
            [editor.dialogs :as dialogs]
            [editor.util :as util]
            [editor.app-view :as app-view])
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
                 {:label "Show In Desktop"
                  :icon "icons/32/Icons_S_14_linkarrow.png"
                  :command :show-in-desktop}
                 {:label "Referencing Files"
                  :command :referencing-files}
                 {:label "Dependencies"
                  :command :dependencies}
                 {:label :separator}
                 {:label "New"
                  :command :new-file
                  :icon "icons/64/Icons_29-AT-Unknown.png"}
                 {:label "New Folder"
                  :command :new-folder
                  :icon "icons/32/Icons_01-Folder-closed.png"}
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
                  :acc "Shortcut+BACKSPACE"}])

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

(defn- moved-files
  [^File src-dir ^File dest-dir files]
  (let [src-path (.toPath src-dir)]
    (mapv (fn [^File f]
            (let [dest-path (.relativize src-path (.toPath f))]
              [f (io/file dest-dir (.toFile dest-path))]))
          files)))

(defn rename [resource ^String new-name]
  (when resource
    (let [workspace (resource/workspace resource)
          src-file  (File. (resource/abs-path resource))
          src-files (vec (file-seq src-file))
          dest-file (and new-name (File. (.getParent src-file) new-name))]
      (when dest-file
        (if (.isDirectory src-file)
          (FileUtils/moveDirectory src-file dest-file)
          (FileUtils/moveFile src-file dest-file))
        (workspace/resource-sync! workspace true (moved-files src-file dest-file src-files))))))

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

(defn- select-resource! [asset-browser resource]
  ;; This is a hack!
  ;; The reason is that the FileResource 'next' fetched prior to the deletion
  ;; may have changed after the deletion, due to the 'children' field in the record.
  ;; This is why we can't use ui/select! directly, but do our own implementation based on path.
  (let [^TreeView tree-view (g/node-value asset-browser :tree-view)
        tree-items (ui/tree-item-seq (.getRoot tree-view))
        path (resource/resource->proj-path resource)]
    (when-let [tree-item (some (fn [^TreeItem tree-item] (and (= path (resource/resource->proj-path (.getValue tree-item))) tree-item)) tree-items)]
      (doto (.getSelectionModel tree-view)
        (.clearSelection)
        (.select tree-item)))))

(handler/defhandler :cut :asset-browser
  (enabled? [selection] (and (seq selection) (every? is-deletable-resource selection)))
  (run [selection selection-provider asset-browser]
       (let [next (-> (handler/succeeding-selection selection-provider)
                    (handler/adapt-single resource/Resource))
             tmp (doto (-> (Files/createTempDirectory "asset-cut" (into-array FileAttribute []))
                         (.toFile))
                   (.deleteOnExit))]
         (copy (mapv #(tmp-file tmp %) (roots selection)))
         (delete selection)
         (when next
           (select-resource! asset-browser next)))))

(defn- unique
  [^File original exists-fn name-fn]
  (let [original-basename (FilenameUtils/getBaseName (.getAbsolutePath original))]
    (loop [^File f original]
      (if (exists-fn f)
        (let [path (.getAbsolutePath f)
              ext (FilenameUtils/getExtension path)
              new-path (str (FilenameUtils/getFullPath path)
                            (name-fn original-basename (FilenameUtils/getBaseName path))
                            (when (seq ext) (str "." ext)))]
          (recur (File. new-path)))
        f))))

(defn- ensure-unique-dest-files
  [name-fn src-dest-pairs]
  (loop [[[src dest :as pair] & rest] src-dest-pairs
         new-names #{}
         ret []]
    (if pair
      (let [new-dest (unique dest #(or (.exists ^File %) (new-names %)) name-fn)]
        (recur rest (conj new-names new-dest) (conj ret [src new-dest])))
      ret)))

(defn- numbering-name-fn
  [original-basename basename]
  (let [suffix (string/replace basename original-basename "")]
    (if (.isEmpty suffix)
      (str original-basename "1")
      (str original-basename (str (inc (Integer/parseInt suffix)))))))

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
  (run [selection workspace asset-browser]
       (let [tree-view (g/node-value asset-browser :tree-view)
             resource (first selection)
             src-files (.getFiles (Clipboard/getSystemClipboard))
             ^File tgt-dir (reduce (fn [^File tgt ^File src]
                                     (if (= tgt src)
                                       (.getParentFile ^File tgt)
                                       tgt))
                                   (util/to-folder (File. (resource/abs-path resource))) src-files)
             pairs (->> src-files
                        (map (fn [^File f] [f (File. tgt-dir (FilenameUtils/getName (.toString f)))]))
                        (ensure-unique-dest-files (fn [_ basename] (str basename "_copy"))))]
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
  (run [selection asset-browser selection-provider]
    (let [names (apply str (interpose ", " (map resource/resource-name selection)))
          next (-> (handler/succeeding-selection selection-provider)
                 (handler/adapt-single resource/Resource))]
      (when (dialogs/make-confirm-dialog (format "Are you sure you want to delete %s?" names))
        (when (and (delete selection) next)
          (select-resource! asset-browser next))))))

(handler/defhandler :new-file :global
  (label [user-data] (if-not user-data
                       "New..."
                       (let [rt (:resource-type user-data)]
                         (or (:label rt) (:ext rt)))))
  (active? [selection selection-context] (or (= :global selection-context) (and (= :asset-browser selection-context)
                                                                                (= (count selection) 1)
                                                                                (not= nil (some-> (handler/adapt-single selection resource/Resource)
                                                                                            resource/abs-path)))))
  (run [selection user-data asset-browser app-view workspace project]
       (let [project-path (workspace/project-path workspace)
             base-folder (-> (or (some-> (handler/adapt-every selection resource/Resource)
                                   first
                                   resource/abs-path
                                   (File.))
                                 project-path)
                           util/to-folder)
             rt (:resource-type user-data)]
         (when-let [new-file (dialogs/make-new-file-dialog project-path base-folder (or (:label rt) (:ext rt)) (:ext rt))]
           (spit new-file (workspace/template rt))
           (workspace/resource-sync! workspace)
           (let [resource-map (g/node-value workspace :resource-map)
                 new-resource-path (resource/file->proj-path project-path new-file)
                 resource (resource-map new-resource-path)]
             (app-view/open-resource app-view workspace project resource)
             (select-resource! asset-browser resource)))))
  (options [workspace selection user-data]
           (when (not user-data)
             (let [resource-types (filter (fn [rt] (workspace/template rt)) (workspace/get-resource-types workspace))]
               (sort-by (fn [rt] (string/lower-case (:label rt))) (map (fn [res-type] {:label (or (:label res-type) (:ext res-type))
                                                                                       :icon (:icon res-type)
                                                                                       :command :new-file
                                                                                       :user-data {:resource-type res-type}}) resource-types))))))

(defn- resolve-sub-folder [^File base-folder ^String new-folder-name]
  (.toFile (.resolve (.toPath base-folder) new-folder-name)))

(handler/defhandler :new-folder :asset-browser
  (enabled? [selection] (and (= (count selection) 1) (not= nil (resource/abs-path (first selection)))))
  (run [selection workspace asset-browser]
       (let [f (File. (resource/abs-path (first selection)))
             base-folder (util/to-folder f)]
         (when-let [new-folder-name (dialogs/make-new-folder-dialog base-folder)]
           (let [^File f (resolve-sub-folder base-folder new-folder-name)]
             (.mkdir f)
             (workspace/resource-sync! workspace)
             (select-resource! asset-browser (workspace/file-resource workspace f)))))))

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
          (ui/select-indices! tree-view selected-indices))))))

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

(g/defnk produce-tree-view [_node-id raw-tree-view resource-tree]
  (let [selected-paths (or (ui/user-data raw-tree-view ::pending-selection)
                           (mapv resource/proj-path (ui/selection raw-tree-view)))]
    (update-tree-view raw-tree-view resource-tree selected-paths)
    (ui/user-data! raw-tree-view ::pending-selection nil)
    raw-tree-view))

(defn- drag-detected [^MouseEvent e selection]
  (let [resources (roots selection)
        files (fileify resources)
        ;; Note: It would seem we should use the TransferMode/COPY_OR_MOVE mode
        ;; here in order to support making copies of non-readonly files, but
        ;; that results in every drag operation becoming a copy on macOS due to
        ;; https://bugs.openjdk.java.net/browse/JDK-8148025
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
       ;; Auto scrolling
       (let [view (.getTreeView cell)
             view-y (.getY (.sceneToLocal view (.getSceneX e) (.getSceneY e)))
             height (.getHeight (.getBoundsInLocal view))]
         (when (< view-y 15)
           (.scrollTo view (dec (.getIndex cell))))
         (when (> view-y (- height 15))
           (.scrollTo view (inc (.getIndex cell)))))
       (let [tgt-resource (-> cell (.getTreeItem) (.getValue))]
         (when (allow-resource-transfer? tgt-resource (.getFiles db))
           ;; Allow move only if the drag source was also the tree view.
           (if (= (.getTreeView cell) (.getGestureSource e))
             (.acceptTransferModes e TransferMode/COPY_OR_MOVE)
             (.acceptTransferModes e (into-array TransferMode [TransferMode/COPY])))
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

(defrecord SelectionProvider [asset-browser]
  handler/SelectionProvider
  (selection [this]
    (ui/selection (g/node-value asset-browser :tree-view)))
  (succeeding-selection [this]
    (let [tree-view (g/node-value asset-browser :tree-view)
          path-fn (comp #(string/split % #"/") item->path)]
      (->> (ui/selection-root-items tree-view path-fn item->path)
        (ui/succeeding-selection tree-view))))
  (alt-selection [this] []))

(defn- setup-asset-browser [asset-browser workspace ^TreeView tree-view]
  (.setSelectionMode (.getSelectionModel tree-view) SelectionMode/MULTIPLE)
  (let [selection-provider (SelectionProvider. asset-browser)
        over-handler (ui/event-handler e (drag-over e))
        dropped-handler (ui/event-handler e (error-reporting/catch-all! (drag-dropped e)))
        detected-handler (ui/event-handler e (drag-detected e (handler/selection selection-provider)))
        entered-handler (ui/event-handler e (drag-entered e))
        exited-handler (ui/event-handler e (drag-exited e))]
    (doto tree-view
      (ui/bind-double-click! :open)
      (.setOnDragDetected detected-handler)
      (ui/cell-factory! (fn [resource]
                          (if (nil? resource)
                            {}
                            {:text (resource/resource-name resource)
                             :icon (workspace/resource-icon resource)
                             :style (resource/style-classes resource)
                             :over-handler over-handler
                             :dropped-handler dropped-handler
                             :entered-handler entered-handler
                             :exited-handler exited-handler})))
      (ui/register-context-menu ::resource-menu)
      (ui/context! :asset-browser {:workspace workspace :asset-browser asset-browser} selection-provider))))

(g/defnode AssetBrowser
  (property raw-tree-view TreeView)

  (input resource-tree FileResource)

  (output tree-view TreeView :cached produce-tree-view))

(defn make-asset-browser [graph workspace tree-view]
  (let [asset-browser (first
                        (g/tx-nodes-added
                          (g/transact
                            (g/make-nodes graph
                                          [asset-browser [AssetBrowser :raw-tree-view tree-view]]
                                          (g/connect workspace :resource-tree asset-browser :resource-tree)))))]
    (setup-asset-browser asset-browser workspace tree-view)
    asset-browser))
