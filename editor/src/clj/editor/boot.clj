(ns editor.boot
  (:require [camel-snake-kebab :as camel]
            [clojure.java.io :as io]
            [dynamo.graph :as g]
            [dynamo.node :as n]
            [dynamo.system :as ds]
            [dynamo.types :as t]
            [editor.core :as core]
            [editor.ui :as ui]
            [editor.jfx :as jfx]
            [editor.workspace :as workspace]
            [editor.project :as project]
            [editor.outline-view :as outline-view]
            [editor.properties-view :as properties-view]
            [editor.graph-view :as graph-view]
            [editor.scene :as scene]
            [editor.image :as image]
            [editor.collection :as collection]
            [editor.game-object :as game-object]
            [editor.atlas :as atlas]
            [editor.cubemap :as cubemap]
            [editor.platformer :as platformer]
            [editor.switcher :as switcher]
            [editor.sprite :as sprite]
            [internal.clojure :as clojure]
            [internal.disposal :as disp]
            [internal.graph.types :as gt]
            [service.log :as log])
  (:import [com.defold.editor Start]
           [com.jogamp.opengl.util.awt Screenshot]
           [javafx.application Platform]
           [javafx.collections FXCollections ObservableList]
           [javafx.embed.swing SwingFXUtils]
           [javafx.event ActionEvent EventHandler]
           [javafx.fxml FXMLLoader]
           [javafx.geometry Insets]
           [javafx.scene Scene Node Parent]
           [javafx.scene.control Button ColorPicker Label TextField TitledPane TextArea TreeItem TreeCell Menu MenuItem MenuBar Tab ProgressBar]
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

(defn- fill-control [control]
  (AnchorPane/setTopAnchor control 0.0)
  (AnchorPane/setBottomAnchor control 0.0)
  (AnchorPane/setLeftAnchor control 0.0)
  (AnchorPane/setRightAnchor control 0.0))

(defn- setup-console [root]
  (.appendText (.lookup root "#console") "Hello Console"))

; Editors
(g/defnode CurveEditor
  (inherits core/Scope)
  (on :create
      (let [btn (Button.)]
        (.setText btn "Curve Editor WIP!")
        (.add (.getChildren (:parent event)) btn)))

  t/IDisposable
  (dispose [this]))

(g/defnode TextEditor
  (inherits core/Scope)
  (inherits core/ResourceNode)

  (input text t/Str)

  (on :create
      (let [textarea (TextArea.)]
        (fill-control textarea)
        (.appendText textarea (slurp (:file event)))
        (.add (.getChildren (:parent event)) textarea)))
  t/IDisposable
  (dispose [this]
           (println "Dispose TextEditor")))

(g/defnode TextNode
  (inherits core/Scope)
  (inherits core/ResourceNode)

  (property text t/Str)
  (property a-vector t/Vec3 (default [1 2 3]))
  (property a-color t/Color (default [1 0 0 1]))


  (on :load
      (g/set-property self :text (slurp (:filename self)))))

(defn on-edit-text
  [project-node text-node]
  (g/graph-with-nodes
   10
   [editor TextEditor]
   (g/connect text-node :text editor :text)))

(defn- create-editor [workspace project resource root]
  (let [resource-node (project/get-resource-node project resource)
        resource-type (project/get-resource-type resource-node)]
    (when (and resource-type (:setup-rendering-fn resource-type))
      (let [setup-view-fn (:setup-view-fn resource-type)
            setup-rendering-fn (:setup-rendering-fn resource-type)
            tab-pane   (.lookup root "#editor-tabs")
            parent     (AnchorPane.)
            tab        (doto (Tab. (workspace/resource-name resource)) (.setContent parent))
            tabs       (doto (.getTabs tab-pane) (.add tab))
            ;; TODO Delete this graph when the tab is closed.
            view-graph (ds/attach-graph (g/make-graph :volatility 100))
            view       (scene/make-scene-view view-graph parent tab)]
        (.setGraphic tab (jfx/get-image-view "cog.png"))
        (.select (.getSelectionModel tab-pane) tab)
        (ds/transact (setup-view-fn resource-node view))
        (let [view (g/refresh view)]
          (ds/transact (setup-rendering-fn resource-node view)))
        (outline-view/setup (.lookup root "#outline") resource-node)
        (properties-view/setup (.lookup root "#properties") resource-node)))))

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

(defn- setup-asset-browser [workspace project root]
  (let [tree (.lookup root "#assets")
        tab-pane (.lookup root "#editor-tabs")
        handler (reify EventHandler
                  (handle [this e]
                    (when (= 2 (.getClickCount e))
                      (let [item (-> tree (.getSelectionModel) (.getSelectedItem))
                            resource (.getValue item)]
                        (when (= :file (workspace/source-type resource))
                          (create-editor workspace project resource root))))))]
    (.setOnMouseClicked tree handler)
    (.setCellFactory tree (reify Callback (call ^TreeCell [this view]
                                            (proxy [TreeCell] []
                                              (updateItem [resource empty]
                                                (proxy-super updateItem resource empty)
                                                (let [name (or (and (not empty) (not (nil? resource)) (workspace/resource-name resource)) nil)]
                                                  (proxy-super setText name))
                                                (proxy-super setGraphic (jfx/get-image-view (workspace/resource-icon resource))))))))
    (.setRoot tree (tree-item (g/node-value workspace :resource-tree)))))

(defn- bind-menus [menu handler]
  (cond
    (instance? MenuBar menu) (doseq [m (.getMenus menu)] (bind-menus m handler))
    (instance? Menu menu) (doseq [m (.getItems menu)]
                            (.addEventHandler m ActionEvent/ACTION handler))))

(def ^:dynamic *workspace-graph*)
(def ^:dynamic *project-graph*)

(def the-root (atom nil))

(defn load-stage [workspace project]
  (let [root (FXMLLoader/load (io/resource "editor.fxml"))
        stage (Stage.)
        scene (Scene. root)]
    (.setUseSystemMenuBar (.lookup root "#menu-bar") true)
    (.setTitle stage "Defold Editor 2.0!")
    (.setScene stage scene)

    (.show stage)
    (let [handler (ui/event-handler event (println event))]
      (bind-menus (.lookup root "#menu-bar") handler))

    (let [close-handler (ui/event-handler event
                          (ds/transact
                            (g/delete-node project))
                          (ds/dispose-pending))
          dispose-handler (ui/event-handler event (ds/dispose-pending))]
      (.addEventFilter stage MouseEvent/MOUSE_MOVED dispose-handler)
      (.setOnCloseRequest stage close-handler))
    (setup-console root)
    (setup-asset-browser workspace project root)
    (graph-view/setup-graph-view root *project-graph*)
    (reset! the-root root)
    root))

(defn- create-view [game-project root place node-type]
  (let [node (first
              (ds/tx-nodes-added
               (ds/transact
                (g/make-node (g/nref->gid (g/node-id game-project)) node-type))))]
    (n/dispatch-message (ds/now) node :create :parent (.lookup root place))))

(defn setup-workspace [project-path]
  (first
    (ds/tx-nodes-added
      (ds/transact
        (g/make-nodes
          *workspace-graph*
          [workspace [workspace/Workspace :root project-path]]
          (collection/register-resource-types workspace)
          (game-object/register-resource-types workspace)
          (cubemap/register-resource-types workspace)
          (image/register-resource-types workspace)
          (atlas/register-resource-types workspace)
          (platformer/register-resource-types workspace)
          (switcher/register-resource-types workspace)
          (sprite/register-resource-types workspace))))))

(defn open-project
  [^File game-project-file]
  (let [progress-bar nil
        project-path (.getPath (.getParentFile game-project-file))
        workspace    (setup-workspace project-path)
        project      (first
                      (ds/tx-nodes-added
                       (ds/transact
                        (g/make-nodes
                         *project-graph*
                         [project [project/Project :workspace workspace]]
                         (g/connect workspace :resource-list project :resources)
                         (g/connect workspace :resource-types project :resource-types)))))
        resources    (g/node-value workspace :resource-list)
        project      (project/load-project project resources)
        root         (load-stage workspace project)
        curve        (create-view project root "#curve-editor-container" CurveEditor)]))

(defn get-preference [key]
  (let [prefs (.node (Preferences/userRoot) "defold")]
    (.get prefs key nil)))

(defn set-preference [key value]
  (let [prefs (.node (Preferences/userRoot) "defold")]
    (.put prefs key value)))

(Platform/runLater
  (fn []
    (when (nil? @the-root)
      (ds/initialize {:initial-graph (workspace/workspace-graph)})
      (alter-var-root #'*workspace-graph* (fn [_] (ds/last-graph-added)))
      (alter-var-root #'*project-graph*   (fn [_] (ds/attach-graph-with-history (g/make-graph :volatility 1)))))
    (let [pref-key "default-project-file"
          project-file (or (get-preference pref-key) (jfx/choose-file "Open Project" "~" "game.project" "Project Files" ["*.project"]))]
      (when project-file
        (set-preference pref-key project-file)
        (open-project (io/file project-file))))))
