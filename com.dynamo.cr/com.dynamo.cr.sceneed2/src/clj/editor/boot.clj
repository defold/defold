(ns editor.boot
  (:require [clojure.java.io :as io]
            [schema.core :as s]
            
            [dynamo.node :as n]
            [dynamo.system :as ds]
            [dynamo.types :as t]
            [internal.system :as is]
            [internal.transaction :as it]
            [internal.disposal :as disp]
            )
  (:import  [com.defold.editor Start UIUtil]
            [java.io File]
            [javafx.application Platform]
            [javafx.fxml FXMLLoader]
            [javafx.collections FXCollections ObservableList]
            [javafx.scene Scene Node Parent]
            [javafx.stage Stage FileChooser]
            [javafx.scene.image Image ImageView]
            [javafx.scene.input MouseEvent]
            [javafx.event ActionEvent EventHandler]
            [javafx.scene.control Button TitledPane TextArea TreeItem Menu MenuItem MenuBar Tab]
            [javafx.scene.layout AnchorPane StackPane HBox Priority]))

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

; Events
(defmacro event-handler [event & body]
  `(reify EventHandler
     (handle [this ~event]
       ~@body)))

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
  (on :create
      (let [textarea (TextArea.)]
        (AnchorPane/setTopAnchor textarea 0.0)
        (AnchorPane/setBottomAnchor textarea 0.0)
        (AnchorPane/setLeftAnchor textarea 0.0)
        (AnchorPane/setRightAnchor textarea 0.0)        
        (.appendText textarea (slurp (:file event)))
        (.add (.getChildren (:parent event)) textarea)))
  t/IDisposable
  (dispose [this]
           (println "Dispose TextEditor")))

(n/defnode GameProject
  (inherits n/Scope)
  
  ;TODO: Resource type instead of string?
  (property content-root File)

  t/IDisposable
  (dispose [this]
           (println "Dispose GameProject"))
  
  (on :destroy
      (println "Destory GameProject")
      (ds/delete self)))

(defn- create-editor [game-project file root node-type]
  (let [tab (Tab. (.getName file))
        tab-pane (.lookup root "#editor-tabs")
        parent (AnchorPane.)
        node (ds/transactional (ds/in game-project
                                      (ds/add
                                        (n/construct node-type))))
        close-handler (event-handler event
                        (ds/transactional 
                          (ds/delete node)))]
    (.setOnClosed tab close-handler)
    (.setGraphic tab (get-image-view "cog.png"))
    (n/dispatch-message node :create :parent parent :file file)
    (.setContent tab parent)
    (.add (.getTabs tab-pane) tab)
    (.select (.getSelectionModel tab-pane) tab)))

(defn- setup-assets-browser [game-project root]
  (let [tree (.lookup root "#assets")
        tab-pane (.lookup root "#editor-tabs")
        handler (reify EventHandler
                  (handle [this e]
                    (when (= 2 (.getClickCount e))
                      (let [item (-> tree (.getSelectionModel) (.getSelectedItem))
                            file (.getValue item)]
                        (when (.isFile file)
                          (create-editor game-project file root TextEditor))))))]
    (.setOnMouseClicked tree handler)
    (.setCellFactory tree (UIUtil/newFileCellFactory))
    (.setRoot tree (tree-item (:content-root game-project)))))

(defn- bind-menus [menu handler]
  (cond
    (instance? MenuBar menu) (doseq [m (.getMenus menu)] (bind-menus m handler))
    (instance? Menu menu) (doseq [m (.getItems menu)]
                            (.addEventHandler m ActionEvent/ACTION handler))))

(def the-system (is/start))

(defn load-stage [game-project]
  (let [root (FXMLLoader/load (io/resource "editor.fxml"))
        stage (Stage.)
        scene (Scene. root)]
    (.setUseSystemMenuBar (.lookup root "#menu-bar") true)
    (.setTitle stage "Defold Editor 2.0!")
    (.setScene stage scene)

    (.show stage)
    (let [handler (event-handler event (println event))]
      (bind-menus (.lookup root "#menu-bar") handler))
    
    (let [close-handler (event-handler event
                          (ds/transactional 
                            (ds/delete game-project))
                          (disp/dispose-pending (:state (:world the-system))))
          dispose-handler (event-handler event (disp/dispose-pending (:state (:world  the-system))))]
      (.addEventFilter stage MouseEvent/MOUSE_MOVED dispose-handler)
      (.setOnCloseRequest stage close-handler))
    (setup-console root)
    (setup-assets-browser game-project root)
    root))

(defn- create-view [game-project root place node-type]
  (let [node (ds/transactional
               (ds/in game-project
                      (ds/add
                        (n/construct node-type))))]
    (n/dispatch-message node :create :parent (.lookup root place))))

(defn load-project
  [^File game-project-file]
  (let [game-project (ds/transactional
                       (ds/add
                         (n/construct GameProject
                           :content-root (.getParentFile game-project-file))))
        root (load-stage game-project)
        curve (create-view game-project root "#curve-editor-container" CurveEditor)]))

(Platform/runLater 
  (fn [] 
    (load-project (io/file "/Applications/Defold/branches/1630/3/p/game.project"))))

