(ns user
  (:require [camel-snake-kebab :refer :all]
            [clojure.pprint :refer [pprint]]
            [clojure.repl :refer :all]
            [dynamo.graph :as g]
            [dynamo.image :refer :all]
            [dynamo.types :as t]
            [internal.graph.types :as gt]))

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
    [(gt/nref->gid node-id) (gt/nref->nid node-id) (class n)]))

(defn node
  ([nref]
   (g/node-by-id (g/now) nref))

  ([gid nid]
   (g/node-by-id (g/now) (gt/make-nref gid nid))))

(defn nodes-for-file
  [filename]
  (for [n (nodes)
        :let [f (some-> n :resource :file (.getPath))]
        :when (and f (.endsWith f filename))]
    n))

(defn node-type
  [gid nid]
  (-> (node gid nid) g/node-type :name))

(defn inputs-to
  ([gid nid label]
   (sort-by first
            (gt/sources (g/now) (gt/make-nref gid nid) label))))

(defn outputs-from
  ([node-id label]
   (sort-by first
            (gt/targets (g/now) node-id label)))
  ([gid nid label]
   (outputs-from (gt/make-nref gid nid) label)))

(defn get-value
  [gid nid label]
  (g/node-value (node gid nid) label))

(defn protobuf-fields
  [protobuf-msg-cls]
  (for [fld (.getFields (internal.java/invoke-no-arg-class-method protobuf-msg-cls "getDescriptor"))]
    {:name (.getName fld)
     :type (let [t (.getJavaType fld)]
             (if (not= com.google.protobuf.Descriptors$FieldDescriptor$JavaType/MESSAGE t)
               (str t)
               (.. fld getMessageType getName)))
     :resource? (.getExtension (.getOptions fld) com.dynamo.proto.DdfExtensions/resource)}))
