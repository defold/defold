(ns editor.asset-browser
  (:require [dynamo.graph :as g]
            [editor.handler :as handler]
            [editor.jfx :as jfx]
            [editor.ui :as ui]
            [editor.workspace :as workspace]
            [editor.dialogs :as dialogs])
  (:import [com.defold.editor Start]
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
           [javax.media.opengl GL GL2 GLContext GLProfile GLDrawableFactory GLCapabilities]))

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
                 {:label "New Folder"
                  :command :new-folder
                  :icon "icons/folder.png"}
                 {:label :separator}
                 {:id ::refresh
                  :label "Refresh"
                  :command :refresh}])

(ui/extend-menu ::resource-menu-ext ::refresh
                [{:label "Show in Desktop"
                  :command :show-in-desktop}])

(defn- is-resource [x] (satisfies? workspace/Resource x))
(defn- is-deletable-resource [x] (and (satisfies? workspace/Resource x) (not (workspace/read-only? x))))
(defn- is-file-resource [x] (and (satisfies? workspace/Resource x)
                                 (= :file (workspace/source-type x))))

(handler/defhandler :open :asset-browser
  (enabled? [selection] (every? is-file-resource selection))
  (run [selection] (prn "Open" selection "NOW!")))

(handler/defhandler :delete :asset-browser
  (enabled? [selection] (every? is-deletable-resource selection))
  (run [selection] (prn "Delete" selection "NOW!")))

(defn- resolve-sub-folder [base-folder new-folder-name]
  (.toFile (.resolve (.toPath base-folder) new-folder-name)))

(defn- to-folder [file]
  (if (.isFile file) (.getParentFile file) file))

(handler/defhandler :new-folder :asset-browser
  (enabled? [selection] (and (= (count selection) 1) (not= nil (workspace/abs-path (first selection)))))
  (run [selection] (let [f (File. ^String (workspace/abs-path (first selection)))
                         base-folder (to-folder f)]
                     (when-let [new-folder-name (dialogs/make-new-folder-dialog base-folder)]
                       (.mkdir (resolve-sub-folder base-folder new-folder-name))))))
                     

(handler/defhandler :refresh :asset-browser
  (enabled? [] true)
  (run [] (prn "REFRESH NOW!") ))

(handler/defhandler :show-in-desktop :asset-browser
  (enabled? [selection] (and (= 1 (count selection)) (not= nil (workspace/abs-path (first selection)))) )
  (run [selection] (let [f (File. ^String (workspace/abs-path (first selection)))
                         dir (to-folder f)]
                     (.open (Desktop/getDesktop) dir))))

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
                                                          (proxy-super setGraphic (jfx/get-image-view (workspace/resource-icon resource)))))))))

    (ui/register-context-menu tree-view ::resource-menu)
    (.setRoot tree-view (tree-item (g/node-value workspace :resource-tree)))))

(defn make-asset-browser [workspace tree-view open-resource-fn]
  (ui/context! tree-view :asset-browser {} tree-view)
  (setup-asset-browser workspace tree-view open-resource-fn))
