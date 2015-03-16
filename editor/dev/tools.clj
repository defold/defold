(ns user
  (:require [camel-snake-kebab :refer :all]
            [clojure.java.io :as io]
            [clojure.java.io :refer [file]]
            [clojure.pprint :refer [pprint]]
            [clojure.repl :refer :all]
            [dynamo.cache :as cache]
            [dynamo.file :as f]
            [dynamo.graph :as g]
            [dynamo.image :refer :all]
            [dynamo.node :as n]
            [dynamo.project :as p]
            [dynamo.system :as ds]
            [dynamo.types :as t]
            [dynamo.ui :as ui]
            [internal.graph.dgraph :as dg]
            [internal.graph.lgraph :as lg]
            [internal.java :as j]
            [internal.node :as in]
            [internal.system :as is])
  (:import [java.awt Dimension]
           [javafx.application Platform]
           [javafx.embed.swing JFXPanel]
           [javafx.fxml FXMLLoader]
           [javafx.scene Scene Node Parent]
           [javafx.stage Stage FileChooser]
           [javax.swing JFrame JPanel]
           [javax.vecmath Matrix4d Matrix3d Point3d Vector4d Vector3d]))

(defmacro tap [x] `(do (prn ~(str "**** " &form " ") ~x) ~x))

(defn macro-pretty-print
  [x]
  (clojure.pprint/write (macroexpand x) :dispatch clojure.pprint/code-dispatch))

(defn the-world       [] (-> ds/the-system deref :world))
(defn the-world-state [] (-> (the-world) :state deref))
(defn the-cache       [] (-> ds/the-system deref :cache))
(defn the-graph       [] (-> (the-world-state) :graph))
(defn nodes           [] (-> (the-graph) :nodes vals))

(defn nodes-and-classes
  []
  (let [gnodes (:nodes (the-graph))]
    (sort (map (fn [n v] [n (class v)]) (keys gnodes) (vals gnodes)))))

(defn node
  [id]
  (dg/node (the-graph) id))

(defn node-type
  [id]
  (-> (node id) t/node-type :name))

(defn nodes-of-type
  [type-name]
  (filter #(= type-name (node-type (:_id %))) (nodes)))

(def image-nodes (partial nodes-of-type "ImageResourceNode"))

(defn node-for-file
  [file-name]
  (filter #(= file-name (t/local-path (:filename %))) (filter (comp not nil? :filename) (nodes))))

(defn inputs-to
  ([id]
    (group-by first
      (for [a (dg/arcs-to (the-graph) id)]
        [(get-in a [:target-attributes :label]) (:source a) (get-in a [:source-attributes :label]) ])))
  ([id label]
    (lg/sources (the-graph) id label)))

(defn outputs-from
  ([id]
    (group-by first
      (for [a (dg/arcs-from (the-graph) id)]
       [(get-in a [:source-attributes :label]) (:target a) (get-in a [:target-attributes :label])])))
  ([id label]
    (lg/targets (the-graph) id label)))

(defn get-value
  [id label]
  (g/node-value (node id) label))

(defn cache-peek
  [id label]
  (if-let [x (get (cache/cache-snapshot) [id label])]
    x
    :value-not-cached))

(defn decache
  [id label]
  (cache/cache-invalidate [[id label]])
  :ok)

(defn projects-in-memory
  []
  (keep #(when (.endsWith (.getName (second %)) "Project__") (node (first %))) (nodes-and-classes)))

(defn protobuf-fields
  [protobuf-msg-cls]
  (for [fld (.getFields (internal.java/invoke-no-arg-class-method protobuf-msg-cls "getDescriptor"))]
    {:name (.getName fld)
     :type (let [t (.getJavaType fld)]
             (if (not= com.google.protobuf.Descriptors$FieldDescriptor$JavaType/MESSAGE t)
               (str t)
               (.. fld getMessageType getName)))
     :resource? (.getExtension (.getOptions fld) com.dynamo.proto.DdfExtensions/resource)}))

(defn load-stage [fxml]
  (let [root (FXMLLoader/load (.toURL (io/file fxml)))
        stage (Stage.)
        scene (Scene. root)]
    (.setUseSystemMenuBar (.lookup root "#menu-bar") true)
    (.setTitle stage "GUI playground")
    (.setScene stage scene)
    (.show stage)
    root))
