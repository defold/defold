(ns user
  (:require [dynamo.camera :as c]
            [dynamo.geom :as g]
            [dynamo.project :as p]
            [dynamo.file :as f]
            [dynamo.image :refer :all]
            [clojure.java.io :refer [file]]
            [internal.graph.dgraph :as dg]
            [internal.graph.lgraph :as lg]
            [internal.system :as sys]
            [clojure.repl :refer :all]
            [schema.core :as s])
  (:import [java.awt Dimension]
           [javax.vecmath Matrix4d Matrix3d Point3d Vector4d Vector3d]
           [javax.swing JFrame JPanel]))


;; want:

(defn method->function [m]
  (list (symbol (.getName m)) (into ['this] (.getParameterTypes m))))

(defn all-types [cls ]
  (cons cls (supers cls)))

(defn skeletor [iface]
  (->> iface
    all-types
    (mapcat #(.getDeclaredMethods %))
    (map method->function)))

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
  [id label & {:as opt-map}]
  (p/get-resource-value (current-project) (merge (node-in-project id) opt-map) label))

(defn update-node-in-project
  [id f & args]
  (p/transact (current-project) (apply p/update-resource {:_id id} f args)))

(defn images-from-dir
  [d]
  (map load-image (filter #(.endsWith (.getName %) ".png") (file-seq (file d)))))

(defn camera-fov-from-aabb
  [camera aabb]
  (let [viewport    (:viewport camera)
        min-proj    (c/camera-project camera viewport (.. aabb min))
        max-proj    (c/camera-project camera viewport (.. aabb max))
        proj-width  (Math/abs (- (.x max-proj) (.x min-proj)))
        proj-height (Math/abs (- (.y max-proj) (.x min-proj)))
        factor-x (/ proj-width  (- (.right viewport) (.left viewport)))
        factor-y (/ proj-height (- (.top viewport) (.bottom viewport)))
        factor-y (* factor-y (:aspect camera))
        fov-x-prim (* factor-x (:fov camera))
        fov-y-prim (* factor-y (:fov camera))]
    (* 1.1 (Math/max (/ fov-y-prim (:aspect camera)) fov-x-prim))))

(defn aabb-ortho-framing-fn
  [camera aabb]
  (assert (= :orthographic (:type camera)))
  (let [fov (camera-fov-from-aabb camera aabb)]
    (fn [old-cam]
      (-> old-cam
        (c/set-orthographic fov (:aspect camera) (:z-near camera) (:z-far camera))
        (c/camera-set-center aabb)))))

#_(defn ortho-frame-aabb
   [camera-node-id aabb]
   (let [cam          (get-value-in-project camera-node-id :camera)]
     (assert (= :orthographic (:type cam)))
     (let [viewport (:viewport cam)
           fov      (camera-fov-from-aabb cam aabb)]
       (update-node-in-project camera-node-id update-in [:camera]
                               (fn [old-cam]
                                 (-> old-cam
                                   (c/set-orthographic fov (:aspect cam) (:z-near cam) (:z-far cam))
                                   (c/camera-set-center aabb)
                                 ))))))

(defn rect->aabb
  [bounds]
  (-> (g/null-aabb)
      (g/aabb-incorporate (Point3d. (.x bounds) (.y bounds) 0))
      (g/aabb-incorporate (Point3d. (.width bounds) (.height bounds) 0))))

(defn ortho-frame-texture
  [atlas-node-id camera-node-id]
  (let [txt    (get-value-in-project atlas-node-id :textureset)
        camera (get-value-in-project camera-node-id :camera)]
    (update-node-in-project camera-node-id update-in [:camera]
                              (aabb-ortho-framing-fn camera (rect->aabb (:aabb txt))))))

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
