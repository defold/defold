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

(defn nodes-and-classes
  []
  (let [now (g/now)]
   (for [graphid (sort (keys (:graphs now)))
         :let [graph (get-in now [:graphs graphid])]
         nodeid  (sort (keys (:nodes graph)))
         :let [node (get (:nodes graph) nodeid)]]
     [graphid (gt/nref->nid nodeid) (class node)])))

(defn node
  ([nref]
   (g/node-by-id (g/now) nref))

  ([gid nid]
   (g/node-by-id (g/now) (gt/make-nref gid nid))))

(defn node-type
  [gid nid]
  (-> (node gid nid) g/node-type :name))

(defn inputs-to
  ([gid nid label]
   (sort-by first
            (gt/sources (g/now) (gt/make-nref gid nid) label))))

(defn outputs-from
  [gid nid label]
  (sort-by first
            (gt/targets (g/now) (gt/make-nref gid nid) label)))

(defn sarc-reciprocated
  [basis source-arc]
  (some #{(gt/head source-arc)} (apply gt/sources (g/now) (gt/tail source-arc))))

(defn tarc-reciprocated
  [basis target-arc]
  (some #{(gt/tail target-arc)} (apply gt/targets (g/now) (gt/head target-arc))))

(defn unreciprocated-arcs
  [basis]
  {:source-arcs
   (remove (partial sarc-reciprocated basis) (mapcat :sarcs (vals (:graphs basis))))
   :target-arcs
   (remove (partial tarc-reciprocated basis) (mapcat :tarcs (vals (:graphs basis))))})

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
