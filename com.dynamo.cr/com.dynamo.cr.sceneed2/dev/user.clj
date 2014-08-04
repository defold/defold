(ns user
  (:require [dynamo.project :as p]
            [dynamo.file :as f]
            [dynamo.image :refer :all]
            [clojure.java.io :refer [file]]
            [internal.graph.dgraph :as dg]
            [internal.graph.lgraph :as lg]
            [internal.system :as sys]
            [clojure.repl :refer :all]
            [schema.core :as s])
  (:import [java.awt Dimension]
           [javax.swing JFrame JPanel]))


(defmacro tap [x] `(do (prn ~(str "**** " &form " ") ~x) ~x))

(defn current-project []
  (get-in @sys/the-system [:project :project-state]))

(defn macro-pretty-print
  [x]
  (clojure.pprint/write (macroexpand x) :dispatch clojure.pprint/code-dispatch))

(defn nodes-and-classes-in-project
  []
  (map (fn [n v] [n (class v)]) (keys (get-in @(current-project) [:graph :nodes])) (vals (get-in @(current-project) [:graph :nodes]))))

(defn node-in-project
  [id]
  (dg/node (:graph @(current-project)) id))

(defn load-resource-in-project
  [res]
  (p/load-resource (current-project) (f/project-path (current-project) res)))

(defn get-value-in-project
  [id label]
  (p/get-resource-value (current-project) (node-in-project id) label))

(defn update-node-in-project
  [id f & args]
  (p/transact (current-project) (apply p/update-resource {:_id id} f args)))

(defn images-from-dir
  [d]
  (map load-image (filter #(.endsWith (.getName %) ".png") (file-seq (file d)))))

(defn test-resource-dir
  [& [who]]
  (let [who (or who (System/getProperty "user.name"))]
    (str
     (cond
       (= who "mtnygard")   "/Users/mtnygard/src/defold"
       (= who "ben")        "/Users/ben/projects/eleiko")
     "/com.dynamo.cr/com.dynamo.cr.sceneed2/test/resources")))

(images-from-dir (test-resource-dir))

(defn image-panel [img]
  (doto (proxy [JPanel] []
          (paint [g]
            (.drawImage g img 0 0 nil)))
    (.setPreferredSize (new Dimension
                            (.getWidth img)
                            (.getHeight img)))))

(defn imshow [img]
  (let [panel (image-panel img)]
    (doto (JFrame.) (.add panel) .pack .show)))

(comment
  (import '[com.dynamo.atlas.proto AtlasProto AtlasProto$Atlas AtlasProto$AtlasAnimation AtlasProto$AtlasImage])
  (p/register-loader (current-project) "atlas" (f/protocol-buffer-loader AtlasProto$Atlas atlas.core/on-load))
)
