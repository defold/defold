(ns editor.graph-view
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.gviz :as gviz]
            [editor.ui :as ui])
  (:import [javafx.application Platform]
           [javafx.scene Parent]
           [javafx.scene.control Button ScrollPane]
           [javafx.scene.image Image ImageView]
           [java.io File]))

(set! *warn-on-reflection* true)

(defonce ^:private ^:dynamic *root-id* nil)
(defonce ^:private ^:dynamic *input-fn* (constantly false))
(defonce ^:private ^:dynamic *output-fn* (constantly false))

(defn- update-graph-view [^Parent root]
  (when-let [dot-image (-> (gviz/subgraph->dot (g/now) :root-id *root-id* :input-fn *input-fn* :output-fn *output-fn*)
                         (gviz/dot->image))]
    (Platform/runLater
      (fn []
        (let [^ImageView image-view  (.lookup root "#graph-image")
              ^ScrollPane scroll-pane (.getParent image-view)
              image       (Image. (format "file://%s" (.getAbsolutePath dot-image)))
              width       (.getWidth image)
              height      (.getHeight image)]
          (.setOnMousePressed scroll-pane
            (ui/event-handler event
                	           (if (= width (.getFitWidth image-view))
                              (do
                                (.setFitWidth image-view (max (.getWidth scroll-pane) (* 0.5 width)))
                                (.setFitHeight image-view (max (.getHeight scroll-pane) (* 0.5 height))))
                              (do
                                (.setFitWidth image-view width)
                                (.setFitHeight image-view height)))))
          (.setFitWidth image-view width)
          (.setFitHeight image-view height)
          (.setImage image-view image))))))

(defn setup-graph-view [^Parent root]
  (let [^Button button  (.lookup root "#graph-refresh")
        handler (ui/event-handler event (update-graph-view root))]
    (.setOnAction button handler)))
