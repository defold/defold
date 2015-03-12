(ns editor.boot
  (:require [clojure.java.io :as io]
            [schema.core :as s]
            [internal.clojure :as clojure]
            [dynamo.node :as n]
            [dynamo.system :as ds]
            [dynamo.types :as t]
            [dynamo.file :as f]
            [dynamo.project :as p]
            [internal.system :as is]
            [internal.transaction :as it]
            [internal.disposal :as disp]
            [camel-snake-kebab :as camel]
            [service.log :as log]
            [editor.atlas :as atlas]
            [editor.cubemap :as cubemap]
            [editor.jfx :as jfx]
            [editor.image-node :as ein]
            [editor.ui :as ui]
            [editor.graph_view :as graph_view])
  (:import  [com.defold.editor Start UIUtil]
            [java.io File]
            [java.nio.file Paths]
            [java.util.prefs Preferences]
            [javafx.application Platform]
            [javafx.fxml FXMLLoader]
            [javafx.collections FXCollections ObservableList]
            [javafx.scene Scene Node Parent]
            [javafx.geometry Insets]
            [javafx.stage Stage FileChooser]
            [javafx.scene.image Image ImageView WritableImage PixelWriter]
            [javafx.scene.input MouseEvent]
            [javafx.event ActionEvent EventHandler]
            [javafx.scene.paint Color]
            [javafx.scene.control Button ColorPicker Label TextField TitledPane TextArea TreeItem Menu MenuItem MenuBar Tab ProgressBar]
            [javafx.scene.layout AnchorPane GridPane StackPane HBox Priority]
            [javafx.embed.swing SwingFXUtils]
            [javax.media.opengl GL GL2 GLContext GLProfile GLDrawableFactory GLCapabilities]
            [com.jogamp.opengl.util.awt Screenshot]))

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
                             (ds/transactional
                               (ds/set-property node key new-val)
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
  (let [properties (t/properties node)
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
(n/defnode CurveEditor
  (inherits n/Scope)
  (on :create
      (let [btn (Button.)]
        (.setText btn "Curve Editor WIP!")
        (.add (.getChildren (:parent event)) btn)))

  t/IDisposable
  (dispose [this]))

(n/defnode TextEditor
  (inherits n/Scope)
  (inherits n/ResourceNode)

  (input text s/Str )

  (on :create
      (let [textarea (TextArea.)]
        (fill-control textarea)
        (.appendText textarea (slurp (:file event)))
        (.add (.getChildren (:parent event)) textarea)))
  t/IDisposable
  (dispose [this]
           (println "Dispose TextEditor")))

(n/defnode TextNode
  (inherits n/Scope)
  (inherits n/ResourceNode)

  (property text s/Str)
  (property a-vector t/Vec3 (default [1 2 3]))
  (property a-color t/Color (default [1 0 0 1]))


  (on :load
      (ds/set-property self :text (slurp (:filename self)))))

(defn on-edit-text
  [project-node text-node]
  (let [editor (n/construct TextEditor)]
    (ds/in (ds/add editor)
           (ds/connect text-node :text editor :text)
           editor)))

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

(n/defnode GameProject
  (inherits n/Scope)

  (property node-types         {s/Str s/Symbol})
  ;TODO: Resource type instead of string?
  (property content-root File)

  ; TODO: Couldn't get ds/query to work
   t/NamingContext
  (lookup [this name]
    (let [path (if (instance? ProjectPath name) name (make-project-path this name))]
      (->> (:nodes (:graph @(:world-ref this)))
        (vals)
        (filter #(= path (:filename %)))
        (first))))

  t/IDisposable
  (dispose [this]
           (println "Dispose GameProject"))

  (on :destroy
      (println "Destory GameProject")
      (ds/delete self)))

(def editor-fns {:atlas atlas/construct-atlas-editor
                 :cubemap cubemap/construct-cubemap-editor})

(defn- find-editor-fn [file]
  (let [ext (last (.split file "\\."))
        editor-fn (if ext ((keyword ext) editor-fns) nil)]
    (or editor-fn on-edit-text)))

(defn- create-editor [game-project file root]
  (let [tab-pane (.lookup root "#editor-tabs")
        parent (AnchorPane.)
        path (relative-path (:content-root game-project) file)
        resource-node (t/lookup game-project path)
        node (ds/transactional
               (ds/in game-project
                      (let [editor-fn (find-editor-fn (.getName file))]
                        (editor-fn game-project resource-node))))
        close-handler (ui/event-handler event
                        (ds/transactional
                          (ds/delete node)))]

    (if (satisfies? t/MessageTarget node)
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

(def system nil)
;(is/stop)
(def the-system (is/start))
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
                          (ds/transactional
                            (ds/delete game-project))
                          (disp/dispose-pending (:state (:world the-system))))
          dispose-handler (ui/event-handler event (disp/dispose-pending (:state (:world  the-system))))]
      (.addEventFilter stage MouseEvent/MOUSE_MOVED dispose-handler)
      (.setOnCloseRequest stage close-handler))
    (setup-console root)
    (setup-assets-browser game-project root)
    (graph_view/setup-graph-view root (:world-ref game-project))
    (reset! the-root root)
    root))

;(.setProgress (.lookup @the-root "#progress-bar") 1.0)

(defn- create-view [game-project root place node-type]
  (let [node (ds/transactional
               (ds/in game-project
                      (ds/add
                        (n/construct node-type))))]
    (n/dispatch-message node :create :parent (.lookup root place))))

(defn load-resource-nodes
  [game-project paths ^ProgressBar progress-bar]
  (ds/transactional
    (ds/in game-project
        [game-project (doall
                        (for [p paths]
                          (p/load-resource game-project p)))])))

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
    (log/logging-exceptions (str message (:filename resource-node))
                            (when (satisfies? t/MessageTarget resource-node)
                              (ds/in project-node
                                     (t/process-one-event resource-node {:type :load :project project-node}))))))

(defn load-project
  [^File game-project-file]
  (let [progress-bar nil
        content-root (.getParentFile game-project-file)
        game-project (ds/transactional
                       (ds/add
                         (n/construct GameProject
                                      :node-types {"script" TextNode
                                                   "clj" clojure/ClojureSourceNode
                                                   "jpg" ein/ImageResourceNode
                                                   "png" ein/ImageResourceNode
                                                   "atlas" atlas/AtlasNode
                                                   "cubemap" cubemap/CubemapNode}
                                      :content-root content-root)))
        resources       (get-project-paths game-project content-root)
        _ (apply post-load "Loading" (load-resource-nodes game-project resources progress-bar))
        root (load-stage game-project)
        curve (create-view game-project root "#curve-editor-container" CurveEditor)]))

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
        (load-project (io/file project-file))))))
