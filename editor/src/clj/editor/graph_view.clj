(ns editor.graph-view
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.ui :as ui])
  (:import [javafx.application Platform]
           [javafx.scene Parent]
           [javafx.scene.control Button ScrollPane]
           [javafx.scene.image Image ImageView]
           [java.io File]))

(defn- ^String node-label [^Object node]
  (.getSimpleName (.getClass node)))

; TODO: How to check for interface instead?
(defn- include-node? [node]
  (and (= -1 (.indexOf (node-label node) "Placeholder"))
       (= -1 (.indexOf (node-label node) "ImageResource"))
       (= -1 (.indexOf (node-label node) "TextNode"))))

(defn- conj-set [set val]
  (if set
    (conj set val)
    #{val}))

(defn- write-dot-graph [graph]
  (let [arcs (:arcs graph)
        nodes (:nodes graph)
        dot-arcs (->> arcs
                   (filter (fn [a] (include-node? (nodes (:source a)))))
                   (map (fn [a] {:source (nodes (:source a))
                                 :source-label (get-in a [:source-attributes :label])
                                 :target (nodes (:target a))
                                 :target-label (get-in a [:target-attributes :label])})))
        included-nodes (reduce (fn [acc a] (-> acc
                                             (conj (:source a))
                                             (conj (:target a)))) #{} dot-arcs)
        node-sockets (->> dot-arcs
                       (reduce (fn [acc a] (-> acc
                                             (update-in [(:source a)] conj-set (:source-label a))
                                             (update-in [(:target a)] conj-set (:target-label a)))) {}))
        dot-file (File/createTempFile "graph" ".dot")]

    (.deleteOnExit dot-file)

    (with-open [w (io/writer dot-file)]
      (.write w "digraph G {\n")
      (.write w "node [label=\"\\N\"];\n")

      (doseq [n included-nodes]
        (let [props (map name (node-sockets n))]
           (.write w (format "%s [shape=record, label=\"<%s> %s|" (g/node-id n) (node-label n) (node-label n)) )
           (.write w (clojure.string/join "|" (map (fn [x] (format "<%s> %s" x x)) props)))
           (.write w "\"];\n")))

      (doseq [a dot-arcs]
        (let [source       (:source a)
              target       (:target a)
              source-label (name (:source-label a))
              target-label (name (:target-label a))]
          (.write w (format "%s:\"%s\" -> %s:\"%s\";\n" (g/node-id source) source-label (g/node-id target) target-label))))
      (.write w "}\n"))
    dot-file))

(defn- update-graph-view [^Parent root graph-id]
  (let [graph     (g/graph graph-id)
        dot-file  (write-dot-graph graph)
        dot-image (File/createTempFile "graph" ".png")
        process   (.exec (Runtime/getRuntime) (format "dot %s -Tpng -o%s" dot-file dot-image))]
    (.waitFor process)
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

(defn setup-graph-view [^Parent root graph-id]
  (let [^Button button  (.lookup root "#graph-refresh")
        handler (ui/event-handler event (update-graph-view root graph-id))]
    (.setOnAction button handler)))
