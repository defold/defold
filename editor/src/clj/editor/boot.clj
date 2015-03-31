(ns editor.boot
  (:require [camel-snake-kebab :as camel]
            [clojure.java.io :as io]
            [dynamo.graph :as g]
            [dynamo.node :as n]
            [dynamo.system :as ds]
            [dynamo.types :as t]
            [editor.atlas :as atlas]
            [editor.core :as core]
            [editor.cubemap :as cubemap]
            [editor.graph-view :as graph-view]
            [editor.jfx :as jfx]
            [editor.image :as image]
            [editor.platformer :as platformer]
            [editor.project :as p]
            [editor.switcher :as switcher]
            [editor.ui :as ui]
            [editor.workspace :as workspace]
            [editor.project :as project]
            [editor.scene :as scene]
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

; ImageView cache
(defonce cached-image-views (atom {}))
(defn- load-image-view [name]
  (if-let [url (io/resource (str "icons/" name))]
    (ImageView. (Image. (str url)))
    (ImageView.)))

(defn- get-image-view [name]
  (if-let [image-view (:name @cached-image-views)]
    image-view
    (let [image-view (load-image-view name)]
      ((swap! cached-image-views assoc name image-view) name))))

(defn- setup-console [root]
  (.appendText (.lookup root "#console") "Hello Console"))


; From https://github.com/mikera/clojure-utils/blob/master/src/main/clojure/mikera/cljutils/loops.clj
(defmacro doseq-indexed
  "loops over a set of values, binding index-sym to the 0-based index of each value"
  ([[val-sym values index-sym] & code]
  `(loop [vals# (seq ~values)
          ~index-sym (long 0)]
     (if vals#
       (let [~val-sym (first vals#)]
             ~@code
             (recur (next vals#) (inc ~index-sym)))
       nil))))

(def create-property-control! nil)

(defmulti create-property-control! (fn [t _] t))

(defmethod create-property-control! String [_ on-new-value]
  (let [text (TextField.)
        setter #(.setText text (str %))]
    (.setOnAction text (ui/event-handler event (on-new-value (.getText text))))
    [text setter]))

(defn- to-double [s]
  (try
    (Double/parseDouble s)
    (catch Throwable _
      nil)))

(defmethod create-property-control! t/Vec3 [_ on-new-value]
  (let [x (TextField.)
        y (TextField.)
        z (TextField.)
        box (HBox.)
        setter (fn [vec]
                 (doseq-indexed [t [x y z] i]
                   (.setText t (str (nth vec i)))))
        handler (ui/event-handler event (on-new-value (mapv #(to-double (.getText %)) [x y z])))]

    (doseq [t [x y z]]
      (.setOnAction t handler)
      (HBox/setHgrow t Priority/SOMETIMES)
      (.setPrefWidth t 60)
      (.add (.getChildren box) t))
    [box setter]))

(defmethod create-property-control! t/Color [_ on-new-value]
 (let [color-picker (ColorPicker.)
       handler (ui/event-handler event
                                 (let [c (.getValue color-picker)]
                                   (on-new-value [(.getRed c) (.getGreen c) (.getBlue c) (.getOpacity c)])))
       setter #(.setValue color-picker (Color. (nth % 0) (nth % 1) (nth % 2) (nth % 3)))]
   (.setOnAction color-picker handler)
   [color-picker setter]))

(defmethod create-property-control! :default [_ on-new-value]
  (let [text (TextField.)
        setter #(.setText text (str %))]
    (.setDisable text true)
    [text setter]))

(defn- niceify-label
  [k]
  (-> k
    name
    camel/->Camel_Snake_Case_String
    (clojure.string/replace "_" " ")))

(defn- create-properties-row [grid node key property row]
  (let [label (Label. (niceify-label key))
        ; TODO: Possible to solve mutual references without an atom here?
        setter-atom (atom nil)
        on-new-value (fn [new-val]
                       (let [old-val (key (ds/refresh node))]
                         (when-not (= new-val old-val)
                           (if (t/property-valid-value? property new-val)
                             (g/transactional
                               (g/set-property node key new-val)
                               (@setter-atom new-val))
                             (@setter-atom old-val)))))
        [control setter] (create-property-control! (t/property-value-type property) on-new-value)]
    (reset! setter-atom setter)
    (setter (get node key))
    (GridPane/setConstraints label 1 row)
    (GridPane/setConstraints control 2 row)
    (.add (.getChildren grid) label)
    (.add (.getChildren grid) control)))

(defn- setup-properties [root node]
  (let [properties (g/properties node)
        parent (.lookup root "#properties")
        grid (GridPane.)]
    (.clear (.getChildren parent))
    (.setPadding grid (Insets. 10 10 10 10))
    (.setHgap grid 4)
    (doseq [[key p] properties]
      (let [row (/ (.size (.getChildren grid)) 2)]
        (create-properties-row grid node key p row)))

    (.add (.getChildren parent) grid)))

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
  (let [editor (n/construct TextEditor)]
    (ds/in (g/add editor)
           (g/connect text-node :text editor :text)
           editor)))

(defn- create-editor [workspace project resource root]
  (let [resource-node (project/get-resource-node project resource)
        resource-type (project/get-resource-type resource-node)]
    (when-let [setup-rendering-fn (and resource-type (:setup-rendering-fn resource-type))]
      (let [tab-pane (.lookup root "#editor-tabs")
            parent (AnchorPane.)
            tab (doto (Tab. (workspace/resource-name resource)) (.setContent parent))
            tabs (doto (.getTabs tab-pane) (.add tab))
            view (scene/make-scene-view parent tab)]
        (.setGraphic tab (get-image-view "cog.png"))
        (.select (.getSelectionModel tab-pane) tab)
        (g/transactional (setup-rendering-fn resource-node view))
        (setup-properties root resource-node)))))

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
                                                  (proxy-super setText name)))))))
    (.setRoot tree (tree-item (g/node-value workspace :resource-tree)))))

(defn- bind-menus [menu handler]
  (cond
    (instance? MenuBar menu) (doseq [m (.getMenus menu)] (bind-menus m handler))
    (instance? Menu menu) (doseq [m (.getItems menu)]
                            (.addEventHandler m ActionEvent/ACTION handler))))

(ds/initialize {:initial-graph (core/make-graph)})
(ds/start)

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
                                          (g/transactional
                                            (g/delete project))
                                          (ds/dispose-pending))
          dispose-handler (ui/event-handler event (ds/dispose-pending))]
      (.addEventFilter stage MouseEvent/MOUSE_MOVED dispose-handler)
      (.setOnCloseRequest stage close-handler))
    (setup-console root)
    (setup-asset-browser workspace project root)
    (graph-view/setup-graph-view root)
    (reset! the-root root)
    root))

(defn- create-view [game-project root place node-type]
  (let [node (g/transactional ()
               (ds/in game-project
                      (g/add
                        (n/construct node-type))))]
    (n/dispatch-message node :create :parent (.lookup root place))))

(defn setup-workspace [project-path]
  (g/transactional
    (let [workspace (g/add (n/construct workspace/Workspace :root project-path))]
      (cubemap/register-resource-types workspace)
      (image/register-resource-types workspace)
      (atlas/register-resource-types workspace)
      (platformer/register-resource-types workspace)
      (switcher/register-resource-types workspace)
      workspace)))

(defn open-project
  [^File game-project-file]
  (let [progress-bar nil
        project-path (.getPath (.getParentFile game-project-file))
        workspace (setup-workspace project-path)
        project (g/transactional (let [project (g/add (n/construct project/Project))]
                                   (g/connect workspace :resource-list project :resources)
                                   project))
        resources (g/node-value workspace :resource-list)
        project (project/load-project project resources)
        root (load-stage workspace project)
        curve (create-view project root "#curve-editor-container" CurveEditor)]))

(defn get-preference [key]
  (let [prefs (.node (Preferences/userRoot) "defold")]
    (.get prefs key nil)))

(defn set-preference [key value]
  (let [prefs (.node (Preferences/userRoot) "defold")]
    (.put prefs key value)))

(Platform/runLater
  (fn []
    (let [pref-key "default-project-file"
          project-file (or (get-preference pref-key) (jfx/choose-file "Open Project" "~" "game.project" "Project Files" ["*.project"]))]
      (when project-file
        (set-preference pref-key project-file)
        (open-project (io/file project-file))))))
