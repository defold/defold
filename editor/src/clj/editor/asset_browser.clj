(ns editor.asset-browser
  (:require [dynamo.graph :as g]
            [editor.jfx :as jfx]
            [editor.workspace :as workspace]
            [editor.ui :as ui]
            [editor.handler :as handler])
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
           [javafx.scene.control Button ColorPicker Label TextField TitledPane TextArea TreeItem TreeCell Menu MenuItem MenuBar Tab ProgressBar ContextMenu SelectionMode]
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
        (not= :folder (workspace/source-type (.getValue this))))
      (getChildren []
        (let [children (proxy-super getChildren)]
          (when-not @cached
            (reset! cached true)
            (.setAll children (list-children (.getValue this))))
          children)))))

(def asset-context-menu
  [{:label "Open"
    :command :open}
   {:label "Delete"
    :command :delete
    :icon "icons/cross.png"}])

(defn- is-resource [x] (satisfies? workspace/Resource x))
(defn- is-file-resource [x] (and (satisfies? workspace/Resource x)
                                 (= :file (workspace/source-type x))))

(handler/defhandler open-resource :open :project
  (visible? [instances] (every? is-file-resource instances))
  (enabled? [instances] (every? is-file-resource instances))
  (run [instances] (prn "Open" instances "NOW!")))

(handler/defhandler delete-resource :delete :project
  (visible? [instances] (every? is-resource instances))
  (enabled? [instances] (every? is-resource instances))
  (run [instances] (prn "Delete" instances "NOW!")))

; TODO: Context menu etc is temporary and should be moved elsewhere (and reused)
(defn- populate [context-menu tree-view menu-var]
  (.clear (.getItems context-menu))
  (doseq [item @menu-var]
    (let [tree-items (-> tree-view (.getSelectionModel) (.getSelectedItems))
          resources (map #(.getValue %) tree-items)
          command (:command item)
          context :project ; TODO
          arg-map {:instances resources}
          menu-item (MenuItem. (:label item))]
      (when (handler/visible? command context arg-map)
        (when (:icon item) (.setGraphic menu-item (jfx/get-image-view (:icon item))))
        (.setDisable menu-item (not (handler/enabled? command context arg-map)))
        (.setOnAction menu-item (ui/event-handler event (handler/run command context arg-map) ))
        (.add (.getItems context-menu) menu-item))))
  (when (empty? (.getItems context-menu))
    (.add (.getItems context-menu) (MenuItem. "Empty Menu"))))

(defn- make-context-menu [tree-view menu-var]
  (let [context-menu (ContextMenu.)]
    (populate context-menu tree-view menu-var)
    (.setOnShowing context-menu (ui/event-handler event (populate context-menu tree-view menu-var)))
    context-menu))

(defn- setup-asset-browser [workspace tree-view open-resource-fn]
  (.setSelectionMode (.getSelectionModel tree-view) SelectionMode/MULTIPLE)
  (let [handler (reify EventHandler
                  (handle [this e]
                    (when (= 2 (.getClickCount e))
                      (let [item (-> tree-view (.getSelectionModel) (.getSelectedItem))
                            resource (.getValue item)]
                        (when (= :file (workspace/source-type resource))
                          (open-resource-fn resource))))))]
    (.setOnMouseClicked tree-view handler)
    (.setCellFactory tree-view (reify Callback (call ^TreeCell [this view]
                                                 (proxy [TreeCell] []
                                                   (updateItem [resource empty]
                                                     (proxy-super updateItem resource empty)
                                                     (let [name (or (and (not empty) (not (nil? resource)) (workspace/resource-name resource)) nil)]
                                                       (proxy-super setText name))
                                                     (proxy-super setGraphic (jfx/get-image-view (workspace/resource-icon resource))))))))
    (.setContextMenu tree-view (make-context-menu tree-view #'asset-context-menu))
    (.setRoot tree-view (tree-item (g/node-value workspace :resource-tree)))))

(defn make-asset-browser [workspace tree-view open-resource-fn]
  (setup-asset-browser workspace tree-view open-resource-fn))
