(ns editor.boot
  (:require [clojure.java.io :as io])
  (:import [com.defold.editor Start UIUtil]
           [javafx.application Platform]
           [javafx.fxml FXMLLoader]
           [javafx.collections FXCollections ObservableList]
           [javafx.scene Scene]
           [javafx.stage Stage]
           [javafx.scene.control Button TitledPane TextArea TreeItem]
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

;(tree-item (io/file "/tmp/JOGL-FX"))
;(.isLeaf (nth (.getChildren (tree-item (io/file "/tmp/JOGL-FX"))) 1))
;(.getChildren (tree-item (io/file "/tmp/JOGL-FX")))

#_(let [l (FXCollections/observableArrayList)]
   (doto l
     (.addAll [1 2 3])))

(defn place-control [root id control]
  (let [parent (.lookup root id)]
    (AnchorPane/setTopAnchor control 0.0)
    (AnchorPane/setBottomAnchor control 0.0)
    (AnchorPane/setLeftAnchor control 0.0)
    (AnchorPane/setRightAnchor control 0.0)
    (.add (.getChildren parent) control)))

(defn load-scene []
  (let [root (FXMLLoader/load (io/resource "editor.fxml"))
        stage (Stage.)
        scene (Scene. root)]
    (.setUseSystemMenuBar (.lookup root "#menu-bar") true)
    (.setTitle stage "Defold Editor 2.0!")
    (.setScene stage scene)
    (.show stage)
    (.setCellFactory (.lookup root "#assets") (UIUtil/newFileCellFactory))
    (.setRoot (.lookup root "#assets") (tree-item (io/file ".")))   
    (.appendText (.lookup root "#console") "Hello Console")))

(time (count (file-seq (io/file "/tmp"))))

(Platform/runLater load-scene)
