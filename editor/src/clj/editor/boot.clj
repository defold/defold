(ns editor.boot
  (:require [camel-snake-kebab :as camel]
            [clojure.java.io :as io]
            [dynamo.file :as f]
            [dynamo.graph :as g]
            [dynamo.node :as n]
            [dynamo.system :as ds]
            [dynamo.types :as t]
            [editor.atlas :as atlas]
            [editor.core :as core]
            [editor.cubemap :as cubemap]
            [editor.graph-view :as graph-view]
            [editor.image-node :as ein]
            [editor.jfx :as jfx]
            [editor.platformer :as platformer]
            [editor.project :as p]
            [editor.switcher :as switcher]
            [editor.ui :as ui]
            [internal.clojure :as clojure]
            [internal.disposal :as disp]
            [internal.graph.types :as gt]
            [service.log :as log])
  (:import [com.defold.editor Start UIUtil]
           [com.jogamp.opengl.util.awt Screenshot]
           [javafx.application Platform]
           [javafx.collections FXCollections ObservableList]
           [javafx.embed.swing SwingFXUtils]
           [javafx.event ActionEvent EventHandler]
           [javafx.fxml FXMLLoader]
           [javafx.geometry Insets]
           [javafx.scene Scene Node Parent]
           [javafx.scene.control Button ColorPicker Label TextField TitledPane TextArea TreeItem Menu MenuItem MenuBar Tab ProgressBar]
           [javafx.scene.image Image ImageView WritableImage PixelWriter]
           [javafx.scene.input MouseEvent]
           [javafx.scene.layout AnchorPane GridPane StackPane HBox Priority]
           [javafx.scene.paint Color]
           [javafx.stage Stage FileChooser]
           [java.io File]
           [java.nio.file Paths]
           [java.util.prefs Preferences]
           [javax.media.opengl GL GL2 GLContext GLProfile GLDrawableFactory GLCapabilities]))

(def ^:dynamic *project-graph* nil)
(def ^:dynamic *game-project*  nil)

(defn- split-ext [f]
  (let [n (.getPath f)
        i (.lastIndexOf n ".")]
    (if (== i -1)
      [n nil]
      [(.substring n 0 i)
       (.substring n (inc i))])))

(defn- relative-path [f1 f2]
  (.toString (.relativize (.toPath f1) (.toPath f2))))

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

(declare tree-item)

; TreeItem creator
(defn- list-children [parent]
  (if (.isDirectory parent)
    (doto (FXCollections/observableArrayList)
      (.addAll (map tree-item (.listFiles parent))))
    (FXCollections/emptyObservableList)))

; NOTE: Without caching stack-overflow... WHY?
(defn tree-item [parent]
  (let [cached (atom false)]
    (proxy [TreeItem] [parent]
    (isLeaf []
      (.isFile (.getValue this)))
    (getChildren []
      (let [children (proxy-super getChildren)]
        (when-not @cached
          (reset! cached true)
          (.setAll children (list-children (.getValue this))))
        children)))))

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
  (g/graph-with-nodes
   10
   [editor TextEditor]
   (g/connect text-node :text editor :text)))

(defrecord ProjectPath [project-ref ^String path ^String ext]
  t/PathManipulation
  (extension         [this]         ext)
  (replace-extension [this new-ext] (ProjectPath. project-ref path new-ext))
  (local-path        [this]         (if ext (str path "." ext) path))
  (local-name        [this]         (str (last (clojure.string/split path (java.util.regex.Pattern/compile java.io.File/separator))) "." ext))

  f/ProjectRelative
  (project-file          [this]      (io/file (str (:content-root project-ref) "/" (t/local-path this))))

  io/IOFactory
  (io/make-input-stream  [this opts] (io/make-input-stream (f/project-file this) opts))
  (io/make-reader        [this opts] (io/make-reader (io/make-input-stream this opts) opts))
  (io/make-output-stream [this opts] (io/make-output-stream (f/project-file this) opts))
  (io/make-writer        [this opts] (io/make-writer (io/make-output-stream this opts) opts)))

(defn- make-project-path [game-project name]
  (let [f (io/file name)
        [path ext] (split-ext f)]
    (ProjectPath. game-project path ext)))

(g/defnode GameProject
  (inherits core/Scope)

  (property node-types   {t/Str t/Symbol})
  ;; TODO: Resource type instead of string?
  (property content-root File)

  ;; TODO: Couldn't get ds/query to work
  t/NamingContext
  (lookup
   [this name]
   (let [path  (if (instance? ProjectPath name) name (make-project-path this name))
         nodes (g/node-value this :nodes)]
     (first (filter #(= path (:filename %)) nodes))))

  t/IDisposable
  (dispose
   [this]
   (println "Dispose GameProject"))

  (on :destroy
      (println "Destroy GameProject")
      (g/transact
       (g/delete-node self))))

(def editor-fns {:atlas      atlas/construct-atlas-editor
                 :cubemap    cubemap/construct-cubemap-editor
                 :switcher   switcher/construct-switcher-editor
                 :platformer platformer/construct-platformer-editor})

(defn- find-editor-fn
  [file]
  (let [ext (last (.split file "\\."))
        editor-fn (if ext ((keyword ext) editor-fns) nil)]
    (or editor-fn on-edit-text)))

(defn- create-editor
  [game-project file root]
  (let [tab-pane      (.lookup root "#editor-tabs")
        parent        (AnchorPane.)
        path          (relative-path (:content-root game-project) file)
        resource-node (t/lookup game-project path)
        node          (g/transact
                       ((find-editor-fn game-project (.getName file)) game-project resource-node))
        close-handler (ui/event-handler event
                                        (g/transact (g/delete-node node)))]

    (if (satisfies? g/MessageTarget node)
      (let [tab (Tab. (.getName file))]
        (setup-properties root resource-node)

        (.setOnClosed tab close-handler)
        (.setGraphic tab (get-image-view "cog.png"))
        (.add (.getTabs tab-pane) tab)
        (.setContent tab parent)
        (n/dispatch-message node :create :parent parent :file file :tab tab)
        (.select (.getSelectionModel tab-pane) tab))
      (println "No editor for " node))))

(defn- setup-assets-browser [game-project root]
  (let [tree (.lookup root "#assets")
        tab-pane (.lookup root "#editor-tabs")
        handler (reify EventHandler
                  (handle [this e]
                    (when (= 2 (.getClickCount e))
                      (let [item (-> tree (.getSelectionModel) (.getSelectedItem))
                            file (.getValue item)]
                        (when (.isFile file)
                          (create-editor game-project file root))))))]
    (.setOnMouseClicked tree handler)
    (.setCellFactory tree (UIUtil/newFileCellFactory))
    (.setRoot tree (tree-item (:content-root game-project)))))

(defn- bind-menus [menu handler]
  (cond
    (instance? MenuBar menu) (doseq [m (.getMenus menu)] (bind-menus m handler))
    (instance? Menu menu) (doseq [m (.getItems menu)]
                            (.addEventHandler m ActionEvent/ACTION handler))))

(ds/initialize {:initial-graph (p/project-graph)})

(def the-root (atom nil))

(defn load-stage [game-project]
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
                          (g/transact
                            (g/delete-node game-project))
                          (ds/dispose-pending))
          dispose-handler (ui/event-handler event (ds/dispose-pending))]
      (.addEventFilter stage MouseEvent/MOUSE_MOVED dispose-handler)
      (.setOnCloseRequest stage close-handler))
    (setup-console root)
    (setup-assets-browser game-project root)
    (graph-view/setup-graph-view root)
    (reset! the-root root)
    root))

;(.setProgress (.lookup @the-root "#progress-bar") 1.0)

(defn- create-view [game-project root place node-type]
  (let [node (g/transact
              (g/make-node (:_gid game-project) node-type))]
    (n/dispatch-message node :create :parent (.lookup root place))))

(defn resource-nodes
  [project-graph game-project paths ^ProgressBar progress-bar]
  (for [p paths]
    (p/load-resource project-graph game-project p)))

(defn get-project-paths [game-project content-root]
  (->> (file-seq content-root)
    (filter #(.isFile %))
    (map #(io/file (relative-path content-root %)))
    (remove #(.startsWith (.getPath %) "."))
    (remove #(.startsWith (.getPath %) "build"))
    (map (fn [f] (make-project-path game-project (.getPath f))))))

(defn- post-load
  [message project-node resource-nodes]
  (doseq [resource-node resource-nodes]
    (log/logging-exceptions
     (str message (:filename resource-node))
     (when (satisfies? g/MessageTarget resource-node)
       (g/process-one-event resource-node {:type :load :project project-node})))))

(defn load-project
  [^File game-project-file]
  (let [progress-bar    nil
        project-graph   (ds/attach-graph-with-history (g/make-graph :volatility 10))
        content-root    (.getParentFile game-project-file)
        game-project-tx (ds/transact
                         (g/make-node project-graph GameProject
                                      :node-types {"script"     TextNode
                                                   "clj"        clojure/ClojureSourceNode
                                                   "jpg"        ein/ImageResourceNode
                                                   "png"        ein/ImageResourceNode
                                                   "atlas"      atlas/AtlasNode
                                                   "cubemap"    cubemap/CubemapNode
                                                   "switcher"   switcher/SwitcherNode
                                                   "platformer" platformer/PlatformerNode}
                                      :content-root content-root))
        game-project    (first (ds/tx-nodes-added game-project-tx))
        resources       (get-project-paths game-project content-root)
        tx-result       (ds/transact (resource-nodes project-graph game-project resources progress-bar))
        resource-nodes  (ds/tx-nodes-added tx-result)]
    (post-load "Loading" project-graph resource-nodes)
    (load-stage game-project)
    (create-view game-project root "#curve-editor-container" CurveEditor)
    [project-graph game-project]))

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
        (let [[project-graph game-project] (load-project (io/file project-file))]
          (alter-var-root! #'*project-graph* project-graph)
          (alter-var-root! #'*game-project*  game-project))))))
