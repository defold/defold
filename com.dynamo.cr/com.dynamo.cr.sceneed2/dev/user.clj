(ns user
  (:require [clojure.osgi.core :refer [*bundle*]]
            [dynamo.camera :as c]
            [dynamo.geom :as g]
            [dynamo.project :as p]
            [dynamo.file :as f]
            [dynamo.image :refer :all]
            [dynamo.node :as n]
            [dynamo.system :as ds]
            [dynamo.types :as t]
            [dynamo.ui :as ui]
            [dynamo.ui.widgets :as widgets]
            [clojure.java.io :refer [file]]
            [internal.graph.dgraph :as dg]
            [internal.graph.lgraph :as lg]
            [internal.system :as is]
            [internal.ui.property :as iip]
            [internal.java :as j]
            [internal.node :as in]
            [clojure.repl :refer :all]
            [clojure.pprint :refer [pprint]]
            [schema.core :as s])
  (:import [java.awt Dimension]
           [javax.vecmath Matrix4d Matrix3d Point3d Vector4d Vector3d]
           [javax.swing JFrame JPanel]
           [org.eclipse.ui PlatformUI]
           [org.eclipse.e4.ui.workbench IWorkbench]
           [org.eclipse.swt SWT]
           [org.eclipse.swt.graphics Color RGB]
           [org.eclipse.swt.layout GridLayout GridData]
           [org.eclipse.swt.widgets Canvas Display Listener Shell Slider]
           [org.eclipse.ui.forms.widgets FormToolkit]))

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

(defn the-world       [] (-> is/the-system deref :world))
(defn the-world-state [] (-> (the-world) :state deref))
(defn the-cache       [] (-> (the-world-state) :cache))
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
  [id label & {:as opt-map}]
  (n/get-node-value (merge (node id) opt-map) label))

(defn cache-peek
  [id label]
  (if-let [cache-key (some-> (the-world-state) :cache-keys (get-in [id label]))]
    (if-let [x (get (the-cache) cache-key)]
      x
      :value-not-cached)
    :output-not-cacheable))

(defn who-has-cache-key
  [k]
  (first
    (for [[n m] (:cache-keys (the-world-state))
          [l ck] m
          :when (= ck k)]
     [n l])))

(defn decache
  [id label]
  (if-let [cache-key (some-> (the-world-state) :cache-keys (get-in [id label]))]
    (if (get (the-cache) cache-key)
      (dosync
        (alter (:state (the-world)) update-in [:cache] clojure.core.cache/evict cache-key)
        :ok)
      :value-not-cached)
    :output-not-cacheable))

(defn projects-in-memory
  []
  (keep #(when (.endsWith (.getName (second %)) "Project__") (node (first %))) (nodes-and-classes)))

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

(defn protobuf-fields
  [protobuf-msg-cls]
  (for [fld (.getFields (internal.java/invoke-no-arg-class-method protobuf-msg-cls "getDescriptor"))]
    {:name (.getName fld)
     :type (let [t (.getJavaType fld)]
             (if (not= com.google.protobuf.Descriptors$FieldDescriptor$JavaType/MESSAGE t)
               (str t)
               (.. fld getMessageType getName)))
     :resource? (.getExtension (.getOptions fld) com.dynamo.proto.DdfExtensions/resource)}))

(defn popup-ui [widgets]
  (let [r (promise)]
    (ui/swt-safe
      (let [shell (ui/shell)
            toolkit (FormToolkit. (.getDisplay shell))
            widgets (widgets/make-control toolkit shell widgets)]
        (deliver r widgets)
        (.pack shell)
        (.open shell)))
    r))

(defn popup-resource-selector
  [node & extensions]
  (let [p (promise)]
    (ui/swt-safe
     (deliver p
       (p/select-resources
        node
        extensions)))
    p))

(defn shell
  [display & styles]
  (Shell. display (reduce bit-and styles)))

(defn darker
  [c]
  (let [[h s b] (.. c getRGB getHSB)
        b       (* b 0.8)
        rgb     (RGB. (float h) (float s) (float b))]
    (Color. (.getDevice c) rgb)))

(defn outlined-rect
  [gc scale {:keys [x y width height]}]
  (.setAlpha gc 255)
  (.drawRectangle gc (* scale x) (* scale y) (* scale width) (* scale height))
  (.setAlpha gc 192)
  (.fillRectangle gc (* scale x) (* scale y) (* scale width) (* scale height)))

(defn pack-viz-draw
  [gc evt trace t]
  (let [trace-time (nth trace t)
        client-area (.. evt widget getClientArea)
        text-c      (Color. (.. evt display) 251 250 217)
        gray1-c     (Color. (.. evt display)  64  64  64)
        gray2-c     (Color. (.. evt display) 128 128 145)
        free-c      (Color. (.. evt display) 251 250 217)
        free-b      (darker free-c)
        placed-c    (Color. (.. evt display) 172   0  16)
        placed-b    (darker placed-c)
        scale       0.4]
    (.setForeground gc gray2-c)
    (.setBackground gc gray1-c)
    (.fillGradientRectangle gc (.x client-area) (.y client-area) (.width client-area) (.height client-area) true)
    (.setForeground gc placed-c)
    (.setBackground gc placed-b)
    (doseq [r (:placed trace-time)]
      (outlined-rect gc scale r))
    (.setForeground gc free-c)
    (.setBackground gc free-b)
    (doseq [r (:free-rects trace-time)]
      (outlined-rect gc scale r))
    (.setAlpha gc 255)
    (.setForeground gc text-c)
    (.setBackground gc placed-b)
    (.drawString gc (str "t = " t) 10 10)
    (.dispose placed-c)
    (.dispose placed-b)
    (.dispose free-c)
    (.dispose free-b)
    (.dispose gray1-c)
    (.dispose gray2-c)))

(defn pack-viz [trace]
  (let [min-t 0
        max-t (count trace)
        t     (atom min-t)]
    (ui/swt-safe
      (let [display (Display/getDefault)
            shell   (shell display SWT/SHELL_TRIM)
            layout  (GridLayout. 1 true)
            canvas  (Canvas. shell 0)
            slider  (Slider. shell SWT/HORIZONTAL)]
        (.setLayoutData canvas (GridData. SWT/FILL SWT/FILL true true))
        (.setLayoutData slider (GridData. SWT/FILL SWT/FILL true false))
        (.setMinimum slider min-t)
        (.setIncrement slider 1)
        (.setPageIncrement slider 1)
        (.setMaximum slider max-t)
        (.setLayout shell layout)
        (.addListener slider SWT/Selection
          (reify Listener
            (handleEvent [this evt]
              (try
                (reset! t (.getSelection slider))
                (.redraw canvas)
                (catch Exception e
                  (println e))))))
        (.addListener canvas SWT/Paint
          (reify Listener
            (handleEvent [this evt]
              (try
                (let [gc (.gc evt)]
                  (pack-viz-draw gc evt trace @t))
                (catch Exception e
                  (println e))))))
        (.pack shell)
        (.open shell)))))

(comment
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

