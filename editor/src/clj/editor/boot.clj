(ns editor.boot
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [dynamo.node :as n]
            [dynamo.types :as t]
            [editor.app-view :as app-view]
            [editor.asset-browser :as asset-browser]
            [editor.atlas :as atlas]
            [editor.collection :as collection]
            [editor.core :as core]
            [editor.cubemap :as cubemap]
            [editor.game-object :as game-object]
            [editor.graph-view :as graph-view]
            [editor.image :as image]
            [editor.jfx :as jfx]
            [editor.menu :as menu]
            [editor.outline-view :as outline-view]
            [editor.platformer :as platformer]
            [editor.project :as project]
            [editor.properties-view :as properties-view]
            [editor.scene :as scene]
            [editor.sprite :as sprite]
            [editor.switcher :as switcher]
            [editor.ui :as ui]
            [editor.workspace :as workspace])
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

(defn on-outline-selection-fn [project items]
  (project/select project (map :self items)))

(def ^:dynamic *workspace-graph*)
(def ^:dynamic *project-graph*)
(def ^:dynamic *view-graph*)

(def the-root (atom nil))

(defn load-stage [workspace project]
  (let [root (FXMLLoader/load (io/resource "editor.fxml"))
        stage (Stage.)
        scene (Scene. root)]

    (.setScene stage scene)
    (.show stage)

    (let [close-handler (ui/event-handler event
                          (g/transact
                            (g/delete-node project))
                          (g/dispose-pending))
          dispose-handler (ui/event-handler event (g/dispose-pending))]
      (.addEventFilter stage MouseEvent/MOUSE_MOVED dispose-handler)
      (.setOnCloseRequest stage close-handler))
    (setup-console root)
    (let [app-view (app-view/make-app-view *view-graph* stage (.lookup root "#menu-bar") (.lookup root "#editor-tabs"))
          outline-view (outline-view/make-outline-view *view-graph* (.lookup root "#outline") (fn [items] (on-outline-selection-fn project items)))]
      (g/transact
        (concat
          (g/connect project :menu app-view :main-menus)
          (for [label [:active-resource :active-outline :open-resources]]
            (g/connect app-view label outline-view label))
          (g/update-property app-view :auto-pulls conj [outline-view :tree-view])))
      (asset-browser/make-asset-browser workspace (.lookup root "#assets") (fn [resource] (app-view/open-resource app-view project resource))))
    (graph-view/setup-graph-view root *project-graph*)
    (reset! the-root root)
    root))

(defn- create-view [game-project root place node-type]
  (let [node (g/make-node! (g/node->graph-id game-project) node-type)]
    (n/dispatch-message (g/now) node :create :parent (.lookup root place))))

(defn setup-workspace [project-path]
  (let [workspace (workspace/make-workspace *workspace-graph* project-path)]
    (g/transact
      (concat
        (scene/register-view-types workspace)))
    (let [workspace (g/refresh workspace)]
      (g/transact
        (concat
          (collection/register-resource-types workspace)
          (game-object/register-resource-types workspace)
          (cubemap/register-resource-types workspace)
          (image/register-resource-types workspace)
          (atlas/register-resource-types workspace)
          (platformer/register-resource-types workspace)
          (switcher/register-resource-types workspace)
          (sprite/register-resource-types workspace))))
    (g/refresh workspace)))

(defn open-project
  [^File game-project-file]
  (let [progress-bar nil
        project-path (.getPath (.getParentFile game-project-file))
        workspace    (setup-workspace project-path)
        project      (first 
                      (g/tx-nodes-added
                       (g/transact
                        (g/make-nodes
                         *project-graph*
                         [project [project/Project :workspace workspace]
                          selection project/Selection]
                         (g/connect selection :self project :selection)
                         (g/connect workspace :resource-list project :resources)
                         (g/connect workspace :resource-types project :resource-types)))))
        project      (project/load-project project)
        root         (load-stage workspace project)
        curve        (create-view project root "#curve-editor-container" CurveEditor)
        properties   (properties-view/make-properties-view *view-graph* (.lookup root "#properties"))
        selection    (g/node-value project :selection)]
    (g/transact (g/connect selection :selection properties :selection))
    (g/reset-undo! *project-graph*)))

(defn get-preference [key]
  (let [prefs (.node (Preferences/userRoot) "defold")]
    (.get prefs key nil)))

(defn set-preference [key value]
  (let [prefs (.node (Preferences/userRoot) "defold")]
    (.put prefs key value)))

(Platform/runLater
  (fn []
    (when (nil? @the-root)
      (g/initialize {})
      (alter-var-root #'*workspace-graph* (fn [_] (g/last-graph-added)))
      (alter-var-root #'*project-graph*   (fn [_] (g/make-graph! :history true  :volatility 1)))
      (alter-var-root #'*view-graph*      (fn [_] (g/make-graph! :history false :volatility 2))))
    (let [pref-key "default-project-file"
          project-file (or (get-preference pref-key) (jfx/choose-file "Open Project" "~" "game.project" "Project Files" ["*.project"]))]
      (when project-file
        (set-preference pref-key project-file)
        (open-project (io/file project-file))))))
