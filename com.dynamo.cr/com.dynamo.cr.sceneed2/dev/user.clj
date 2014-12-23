(ns user
  (:require [clojure.osgi.core :refer [*bundle*]]
            [dynamo.camera :as c]
            [dynamo.geom :as g]
            [dynamo.project :as p]
            [dynamo.file :as f]
            [dynamo.image :refer :all]
            [dynamo.node :as n]
            [dynamo.system :as ds]
            [dynamo.ui :as ui]
            [clojure.java.io :refer [file]]
            [internal.graph.dgraph :as dg]
            [internal.graph.lgraph :as lg]
            [internal.system :as is]
            [internal.ui.property :as iip]
            [clojure.repl :refer :all]
            [schema.core :as s])
  (:import [java.awt Dimension]
           [javax.vecmath Matrix4d Matrix3d Point3d Vector4d Vector3d]
           [javax.swing JFrame JPanel]
           [org.eclipse.e4.ui.workbench IWorkbench]))

(defn method->function [m]
  (list (symbol (.getName m)) (into ['this] (.getParameterTypes m))))

(defn all-types [cls]
  (cons cls (supers cls)))

(defn skeletor [iface]
  (->> iface
    all-types
    (mapcat #(.getDeclaredMethods %))
    (map method->function)))

(defmacro tap [x] `(do (prn ~(str "**** " &form " ") ~x) ~x))

(defn macro-pretty-print
  [x]
  (clojure.pprint/write (macroexpand x) :dispatch clojure.pprint/code-dispatch))

(defn the-world [] (-> is/the-system deref :world))
(defn the-graph [] (-> (the-world) :state is/graph))

(defn nodes-and-classes
  []
  (let [gnodes (:nodes (the-graph))]
    (sort (map (fn [n v] [n (class v)]) (keys gnodes) (vals gnodes)))))

(defn node
  [id]
  (dg/node (the-graph) id))

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
  [id label & {:as opt-map}]
  (n/get-node-value (merge (node id) opt-map) label))

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

(defn application-model
  []
  (let [bc   (.getBundleContext *bundle*)
        refs (.getServiceReferences bc IWorkbench nil)
        so   (.getServiceObjects bc (first refs))]
    (.getApplication (.getService so))))

(defn popup-ui [widgets]
  (let [r (promise)]
    (ui/swt-safe
      (let [shell (ui/shell)
            widgets (ui/make-control shell widgets)]
        (deliver r widgets)
        (.pack shell)
        (.open shell)))
    r))

(comment
  (import '[com.dynamo.atlas.proto AtlasProto AtlasProto$Atlas AtlasProto$AtlasAnimation AtlasProto$AtlasImage])
  (p/register-loader (current-project) "atlas" (f/protocol-buffer-loader AtlasProto$Atlas atlas.core/on-load))

  (use 'criterium-core)
  (require '[dynamo.geom :refer [aabb-union]])
  (require '[clojure.test.check.generators :as gen])
  (bench (reduce aabb-union (gen/sample dynamo.geom-test/gen-aabb 10000)))
)

(comment
  ;; menu demo code
  (require '[dynamo.ui :refer [defcommand defhandler]])
  (import '[org.eclipse.ui.commands ICommandService])
  (import '[org.eclipse.core.commands ExecutionEvent])
  (defcommand speak-command "com.dynamo.cr.menu-items.EDIT" "com.dynamo.cr.clojure-eclipse.commands.speak" "Speak!")
  (defhandler handle-speak-command speak-command (fn [^ExecutionEvent ev & args] (prn "Arf Arf! - " args)) "w/args")
)
