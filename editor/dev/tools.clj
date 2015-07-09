(ns user
  (:require [clojure.pprint :refer [pprint]]
            [dynamo.graph :as g]
            [internal.graph.types :as gt]
            [internal.java :as java]))

(defmacro tap [x] `(do (prn ~(str "**** " &form " ") ~x) ~x))

(defn macro-pretty-print
  [x]
  (clojure.pprint/write (macroexpand x) :dispatch clojure.pprint/code-dispatch))

(defn nodes
  []
  (mapcat (comp vals :nodes) (vals (:graphs (g/now)))))

(defn nodes-and-classes
  []
  (for [n (nodes)
        :let [node-id (g/node-id n)]]
    [(gt/node-id->graph-id node-id) (gt/node-id->nid node-id) (class n)]))

(defn node
  ([node-id]
   (g/node-by-id (g/now) node-id))

  ([gid nid]
   (g/node-by-id (g/now) (gt/make-node-id gid nid))))

(defn node-at
  ([basis node-id]
   (g/node-by-id basis node-id))

  ([basis gid nid]
   (g/node-by-id basis (gt/make-node-id gid nid))))

(defn nodes-for-file
  [filename]
  (for [n (nodes)
        :let [f (some-> n :resource :file (.getPath))]
        :when (and f (.endsWith f filename))]
    n))

(defn node-type
  ([node-id]
   (-> (node node-id) g/node-type :name))
  ([gid nid]
   (-> (node gid nid) g/node-type :name)))

(defn inputs-to
  ([node-id label]
   (sort-by first
            (gt/sources (g/now) node-id label)))
  ([gid nid label]
   (inputs-to (gt/make-node-id gid nid) label)))

(defn inputs-to-at
  ([basis node-id label]
   (sort-by first
            (gt/sources basis node-id label)))
  ([basis gid nid label]
   (inputs-to basis (gt/make-node-id gid nid) label)))

(defn outputs-from
  ([node-id label]
   (sort-by first
            (gt/targets (g/now) node-id label)))
  ([gid nid label]
   (outputs-from (gt/make-node-id gid nid) label)))

(defn all-outputs
  ([node-id]
   (map #(outputs-from node-id %) (-> (node node-id) g/node-type g/output-labels)))
  ([gid nid]
   (all-outputs (gt/make-node-id gid nid))))

(defn get-value
  [gid nid label]
  (g/node-value (node gid nid) label))

(defn protobuf-fields
  [protobuf-msg-cls]
  (for [fld (.getFields (java/invoke-no-arg-class-method protobuf-msg-cls "getDescriptor"))]
    {:name (.getName fld)
     :type (let [t (.getJavaType fld)]
             (if (not= com.google.protobuf.Descriptors$FieldDescriptor$JavaType/MESSAGE t)
               (str t)
               (.. fld getMessageType getName)))
     :resource? (.getExtension (.getOptions fld) com.dynamo.proto.DdfExtensions/resource)}))

(defn main-scene [] (.getScene @editor.ui/*main-stage*))

(defn refresh-css
  []
  (let [sheets (.getStylesheets (.getRoot (main-scene)))]
    (editor.ui/run-now
     (.clear sheets)
     (.add sheets "file:/Users/mtnygard/src/defold/editor/resources/editor.css"))))
