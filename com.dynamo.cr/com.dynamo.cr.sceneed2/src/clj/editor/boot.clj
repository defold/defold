(ns editor.boot
  (:require [clojure.java.io :as io]
            [clojure.tools.nrepl.server :as nrepl])
  (:import [com.defold.editor Start UIUtil]
           [javafx.application Platform]
           [javafx.fxml FXMLLoader]
           [javafx.collections FXCollections ObservableList]
           [javafx.scene Scene Node Parent]
           [javafx.stage Stage]
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

