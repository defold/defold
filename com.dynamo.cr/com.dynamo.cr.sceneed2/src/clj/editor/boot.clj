(ns editor.boot
  (:require [clojure.java.io :as io]
            [schema.core :as s]
            
            [dynamo.node :as n]
            [dynamo.system :as ds]
            [internal.system :as is]
            [internal.transaction :as it]
            )
  (:import  [com.defold.editor Start UIUtil]
            [java.io File]
            [javafx.application Platform]
            [javafx.fxml FXMLLoader]
            [javafx.collections FXCollections ObservableList]
            [javafx.scene Scene Node Parent]
            [javafx.stage Stage]
            [javafx.scene.image Image ImageView]
            [javafx.event ActionEvent EventHandler]
            [javafx.scene.control Button TitledPane TextArea TreeItem Menu MenuItem MenuBar Tab]
            [javafx.scene.layout AnchorPane StackPane HBox Priority]))

(declare tree-item)

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

(defn place-control [root id control]
  (let [parent (.lookup root id)]
    (AnchorPane/setTopAnchor control 0.0)
    (AnchorPane/setBottomAnchor control 0.0)
    (AnchorPane/setLeftAnchor control 0.0)
    (AnchorPane/setRightAnchor control 0.0)
    (.add (.getChildren parent) control)))

(defn- setup-console [root]
  (.appendText (.lookup root "#console") "Hello Console"))

(defonce cached-image-views (atom {}))
;(ds/current-scope)

(n/defnode GameProject
  (inherits n/Scope)
  
  ;TODO: Resource type instead of string?
  (property content-root File)
  ;(property presenter-registry t/Registry)
  ;(property node-types         {s/Str s/Symbol})
  ;(property handlers           {s/Keyword {s/Str s/fn-schema}})

  (on :destroy
      (println "DESTORY!!!")
      (ds/delete self)))

(defn load-project
  [^File game-project]
  (prn "FOOO" game-project)
  (ds/transactional
    (ds/add
      (n/construct GameProject
        :content-root (.getParentFile game-project)))))

#_(comment 
(def the-system (atom (is/system)))
(prn (keys @the-system))
(prn (:state (:world @the-system)))
(prn it/*transaction*)

(defn- started?     [s] (-> s deref :world :started))

(is/start the-system)
(is/stop the-system)
(started? the-system)
)

(def the-system (is/start))
(load-project (io/file "/Applications/Defold/branches/1630/3/p/game.project"))

(prn (:world the-system))


(defn- load-image-view [name]
  (if-let [url (io/resource (str "icons/" name))]
    (ImageView. (Image. (str url)))
    (ImageView.)))

(defn- get-image-view [name]
  (if-let [image-view (:name @cached-image-views)]
    image-view
    (let [image-view (load-image-view name)]
      ((swap! cached-image-views assoc name image-view) name))))

(defn- setup-assets-browser [root]
  (let [tree (.lookup root "#assets")
        tab-pane (.lookup root "#editor-tabs")
        handler (reify EventHandler
                  (handle [this e]
                    (when (= 2 (.getClickCount e))
                      (let [item (-> tree (.getSelectionModel) (.getSelectedItem))
                            file (.getValue item)]
                        (when (.isFile file)
                          (let [tab (Tab. (str file))
                                editor (TextArea.)]
                            (.setGraphic tab (get-image-view "cog.png"))
                            (.appendText editor (slurp file))
                            (.setContent tab editor)
                            (.add (.getTabs tab-pane) tab)
                            (.select (.getSelectionModel tab-pane) tab)))))))]
    (.setOnMouseClicked tree handler)
    (.setCellFactory tree (UIUtil/newFileCellFactory))
    (.setRoot tree (tree-item (io/file ".")))))

(defn- bind-menus [menu handler]
  (cond
    (instance? MenuBar menu) (doseq [m (.getMenus menu)] (bind-menus m handler))
    (instance? Menu menu) (doseq [m (.getItems menu)]
                            (.addEventHandler m ActionEvent/ACTION handler))))

(s/defrecord ResourceType
  [name :- s/Str
   ext :- s/Str
   proto-class :- s/Any
   template-data :- s/Str
   refactor-participant :- s/Any
   icon :- s/Str])

(s/validate ResourceType (ResourceType. "Lua" "lua" nil "" nil ""))

(type (.getClass ""))

#_ "
node-type:
  * id
  * class
  * presenter
  * renderer
  * resource-type
  * loader

resource-type:
  * id
  * name
  * ext
  * proto-class
  * template-data
  * embeddable
  * edit-support-class (ddf)
  (* type-class, .e.g. gameobject)
  * refactor-partisipant
  * icon
"

(def the-root (atom nil))

(defn load-scene []
  (let [root (FXMLLoader/load (io/resource "editor.fxml"))
        stage (Stage.)
        scene (Scene. root)]
    (.setUseSystemMenuBar (.lookup root "#menu-bar") true)
    (.setTitle stage "Defold Editor 2.0!")
    (.setScene stage scene)

    (reset! the-root root)
    (.show stage)
    (let [handler (reify EventHandler
                    (handle [this e] (println "EVENT!" e)))]
      (bind-menus (.lookup root "#menu-bar") handler))
    
    (setup-console root)
    (setup-assets-browser root)))

(Platform/runLater load-scene)

