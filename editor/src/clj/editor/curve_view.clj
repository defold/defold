(ns editor.curve-view
  (:require [clojure.set :as set]
            [dynamo.graph :as g]
            [editor.app-view :as app-view]
            [editor.background :as background]
            [editor.camera :as c]
            [editor.scene-selection :as selection]
            [editor.colors :as colors]
            [editor.core :as core]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.shader :as shader]
            [editor.gl.vertex :as vtx]
            [editor.curve-grid :as curve-grid]
            [editor.input :as i]
            [editor.math :as math]
            [util.profiler :as profiler]
            [editor.scene-cache :as scene-cache]
            [editor.scene-tools :as scene-tools]
            [editor.types :as types]
            [editor.ui :as ui]
            [editor.handler :as handler]
            [editor.gl.pass :as pass]
            [editor.ui :as ui]
            [editor.scene :as scene]
            [editor.properties :as properties]
            [editor.camera :as camera]
            [editor.rulers :as rulers]
            [util.id-vec :as iv]
            [service.log :as log])
  (:import [com.defold.editor Start UIUtil]
           [com.jogamp.opengl.util GLPixelStorageModes]
           [com.jogamp.opengl.util.awt TextRenderer]
           [editor.types Camera AABB Region Rect]
           [editor.properties Curve CurveSpread]
           [java.awt Font]
           [java.awt.image BufferedImage DataBufferByte DataBufferInt]
           [javafx.animation AnimationTimer]
           [javafx.application Platform]
           [javafx.beans.property SimpleBooleanProperty]
           [javafx.beans.value ChangeListener]
           [javafx.collections FXCollections ObservableList]
           [javafx.embed.swing SwingFXUtils]
           [javafx.event ActionEvent EventHandler]
           [javafx.geometry BoundingBox Pos VPos HPos]
           [javafx.scene Scene Group Node Parent]
           [javafx.scene.control Tab Button ListView ListCell SelectionMode]
           [javafx.scene.control.cell CheckBoxListCell]
           [javafx.scene.image Image ImageView WritableImage PixelWriter]
           [javafx.scene.input MouseEvent]
           [javafx.scene.layout AnchorPane Pane StackPane]
           [javafx.scene.paint Color]
           [javafx.util Callback StringConverter]
           [java.lang Runnable Math]
           [java.nio IntBuffer ByteBuffer ByteOrder]
           [com.jogamp.opengl GL GL2 GL2GL3 GLContext GLProfile GLAutoDrawable GLOffscreenAutoDrawable GLDrawableFactory GLCapabilities]
           [com.jogamp.opengl.glu GLU]
           [javax.vecmath Point2i Point3d Quat4d Matrix4d Vector4d Matrix3d Vector3d]
           [sun.awt.image IntegerComponentRaster]
           [java.util.concurrent Executors]
           [com.defold.editor AsyncCopier]))

(set! *warn-on-reflection* true)

(def ^:private ^:dynamic *programmatic-selection* nil)

; Line shader

(vtx/defvertex color-vtx
  (vec3 position)
  (vec4 color))

(shader/defshader line-vertex-shader
  (attribute vec4 position)
  (attribute vec4 color)
  (varying vec4 var_color)
  (defn void main []
    (setq gl_Position (* gl_ModelViewProjectionMatrix position))
    (setq var_color color)))

(shader/defshader line-fragment-shader
  (varying vec4 var_color)
  (defn void main []
    (setq gl_FragColor var_color)))

(def line-shader (shader/make-shader ::line-shader line-vertex-shader line-fragment-shader))

(defn gl-viewport [^GL2 gl viewport]
  (.glViewport gl (:left viewport) (:top viewport) (- (:right viewport) (:left viewport)) (- (:bottom viewport) (:top viewport))))

(defn render-curves [^GL2 gl render-args renderables rcount]
  (let [camera (:camera render-args)
        viewport (:viewport render-args)]
    (doseq [renderable renderables
            :let [screen-tris (get-in renderable [:user-data :screen-tris])
                  world-lines (get-in renderable [:user-data :world-lines])]]
      (when world-lines
        (let [vcount (count world-lines)
              vertex-binding (vtx/use-with ::lines world-lines line-shader)]
          (gl/with-gl-bindings gl render-args [line-shader vertex-binding]
            (gl/gl-draw-arrays gl GL/GL_LINES 0 vcount))))
      (when screen-tris
        (let [vcount (count screen-tris)
              vertex-binding (vtx/use-with ::tris screen-tris line-shader)]
          (gl/with-gl-bindings gl render-args [line-shader vertex-binding]
            (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 vcount)))))))

(defn- curve? [[_ p]]
  (let [t (g/value-type-dispatch-value (:type p))
        v (:value p)]
    (when (or (= t Curve) (= t CurveSpread))
      (< 1 (count (properties/curve-vals v))))))

(def ^:private x-steps 100)
(def ^:private xs (reduce (fn [res s] (conj res (double (/ s (- x-steps 1))))) [] (range x-steps)))

(g/defnk produce-curve-renderables [visible-curves]
  (let [splines+colors (mapv (fn [{:keys [node-id property curve hue]}]
                               (let [spline (->> curve
                                              (mapv second)
                                              (properties/->spline))
                                     color (colors/hsl->rgba hue 1.0 0.7)]
                                 [spline color])) visible-curves)
        curve-vs (reduce (fn [res [spline color]]
                           (let [[r g b a] color]
                             (->> (map #(properties/spline-cp spline %) xs)
                               (partition 2 1)
                               (reduce (fn [res [[x0 y0] [x1 y1]]]
                                         (-> res
                                           (conj [x0 y0 0.0 r g b a])
                                           (conj [x1 y1 0.0 r g b a]))) res))))
                         [] splines+colors)
        curve-vcount (count curve-vs)
        world-lines (when (< 0 curve-vcount)
                      (let [vb (->color-vtx curve-vcount)]
                        (doseq [v curve-vs]
                          (conj! vb v))
                        (persistent! vb)))
        renderables [{:render-fn render-curves
                      :batch-key nil
                      :user-data {:world-lines world-lines}}]]
    (into {} (map #(do [% renderables]) [pass/transparent]))))

(def ^:private tangent-length 40)

(g/defnk produce-cp-renderables [visible-curves viewport camera sub-selection-map]
  (let [sub-sel sub-selection-map
        scale (camera/scale-factor camera viewport)
        splines (mapv (fn [{:keys [node-id property curve]}]
                        (let [sel (get sub-sel [node-id property])
                              control-points (->> curve
                                               (sort-by (comp first second)))
                              order (into {} (map-indexed (fn [i [idx _]] [idx i]) control-points))]
                          [(properties/->spline (map second control-points)) (into #{} (map order sel))])) visible-curves)
        scount (count splines)
        color-hues (mapv :hue visible-curves)
        cp-r 4.0
        quad (let [[v0 v1 v2 v3] (vec (for [x [(- cp-r) cp-r]
                                            y [(- cp-r) cp-r]]
                                        [x y 0.0]))]
               (geom/scale scale [v0 v1 v2 v2 v1 v3]))
        cp-vs (mapcat (fn [[spline sel] hue]
                        (let [s 0.7
                              l 0.5]
                          (->> spline
                            (map-indexed (fn [i [x y tx ty]]
                                           (let [v (geom/transl [x y 0.0] quad)
                                                 selected? (contains? sel i)
                                                 s (if selected? 1.0 s)
                                                 l (if selected? 0.9 l)
                                                 c (colors/hsl->rgba hue s l)]
                                             (mapv (fn [v] (reduce conj v c)) v))))
                            (mapcat identity))))
                      splines color-hues)
        [sx sy] scale
        [tangent-vs line-vs] (let [s 1.0
                                l 0.9
                                cps (mapcat (fn [[spline sel] hue]
                                          (let [first-i 0
                                                last-i (dec (count spline))
                                                c (colors/hsl->rgba hue s l)]
                                            (->> sel
                                              (map (fn [i]
                                                     (if-let [[x y tx ty] (get spline i)]
                                                       (let [[tx ty] (let [v (doto (Vector3d. (/ tx sx) (/ ty sy) 0.0)
                                                                               (.normalize)
                                                                               (.scale tangent-length))]
                                                                       [(* (.x v) sx) (* (.y v) sy)])]
                                                         (cond-> []
                                                           (< i last-i) (conj [[x y 0.0] [(+ x tx) (+ y ty) 0.0] c])
                                                           (> i first-i) (conj [[x y 0.0] [(- x tx) (- y ty) 0.0] c])))
                                                       [])))
                                              (mapcat identity))))
                                       splines color-hues)]
                            [(mapcat (fn [[s cp c]]
                                      (->> (geom/transl cp quad)
                                        (mapv (fn [v] (reduce conj v c)))))
                                    cps)
                             (mapcat (fn [[s cp c]] [(reduce conj s c) (reduce conj cp c)]) cps)])
        cp-vs (into cp-vs tangent-vs)
        cp-vcount (count cp-vs)
        screen-tris (when (< 0 cp-vcount)
                      (let [vb (->color-vtx cp-vcount)]
                        (doseq [v cp-vs]
                          (conj! vb v))
                        (persistent! vb)))
        line-vcount (count line-vs)
        world-lines (when (< 0 line-vcount)
                      (let [vb (->color-vtx line-vcount)]
                        (doseq [v line-vs]
                          (conj! vb v))
                        (persistent! vb)))
        renderables [{:render-fn render-curves
                     :batch-key nil
                     :user-data {:screen-tris screen-tris
                                 :world-lines world-lines}}]]
    (into {} (map #(do [% renderables]) [pass/transparent]))))

(g/defnk produce-curves [selected-node-properties]
  (let [curves (mapcat (fn [p] (->> (:properties p)
                                 (filter curve?)
                                 (map (fn [[k p]] {:node-id (:node-id p)
                                                   :property k
                                                   :curve (iv/iv-mapv identity (:points (:value p)))}))))
                       selected-node-properties)
        ccount (count curves)
        hue-f (/ 360.0 ccount)
        curves (map-indexed (fn [i c] (assoc c :hue (* (+ i 0.5) hue-f))) curves)]
    curves))

(defn- aabb-contains? [^AABB aabb ^Point3d p]
  (let [min-p (types/min-p aabb)
        max-p (types/max-p aabb)]
    (and (<= (.x min-p) (.x p) (.x max-p))
         (<= (.y min-p) (.y p) (.y max-p)))))

(defn- reset-controller! [controller op-seq]
  (g/transact
    (concat
      (g/operation-sequence op-seq)
      (g/set-property controller :start nil)
      (g/set-property controller :current nil)
      (g/set-property controller :op-seq nil)
      (g/set-property controller :handle nil)
      (g/set-property controller :initial-evaluation-context nil))))

(defn handle-input [self action user-data]
  (let [^Point3d start      (g/node-value self :start)
        ^Point3d current    (g/node-value self :current)
        op-seq     (g/node-value self :op-seq)
        handle    (g/node-value self :handle)
        sub-selection (g/node-value self :sub-selection)
        ^Point3d cursor-pos (:world-pos action)]
    (case (:type action)
      :mouse-pressed (let [handled? (when-let [[handle data] (g/node-value self :curve-handle)]
                                      (if (and (= 2 (:click-count action))
                                            (or (= handle :control-point) (= handle :curve)))
                                        (do
                                          (g/transact
                                            (concat
                                              (g/operation-sequence op-seq)
                                              (case handle
                                                :control-point
                                                (let [[nid property id] data]
                                                  (g/update-property nid property types/geom-delete [id]))

                                                :curve
                                                (let [[nid property ^Point3d p] data
                                                      p [(.x p) (.y p) (.z p)]
                                                      new-curve (-> (g/node-value nid property)
                                                                  (types/geom-insert [p]))
                                                      id (last (iv/iv-ids (:points new-curve)))
                                                      select-fn (g/node-value self :select-fn)]
                                                  (select-fn [[nid property id]] op-seq)
                                                  (g/set-property nid property new-curve)))))
                                          (reset-controller! self op-seq)
                                          true)
                                        (when (or (= handle :control-point) (= handle :tangent))
                                          (let [op-seq (gensym)
                                                sel-mods? (some #(get action %) selection/toggle-modifiers)]
                                            (when (not sel-mods?)
                                              (when (and (= handle :control-point)
                                                         (not (contains? (set sub-selection) data)))
                                                (let [select-fn (g/node-value self :select-fn)
                                                      sub-selection [data]]
                                                  (select-fn sub-selection op-seq)))
                                              (g/transact
                                                (concat
                                                  (g/operation-sequence op-seq)
                                                  (g/set-property self :op-seq op-seq)
                                                  (g/set-property self :start cursor-pos)
                                                  (g/set-property self :current cursor-pos)
                                                  (g/set-property self :handle handle)
                                                  (g/set-property self :handle-data data)
                                                  (g/set-property self :initial-evaluation-context (atom (g/make-evaluation-context)))))
                                              true)))))]
                       (if handled? nil action))
      :mouse-released (do
                        (reset-controller! self op-seq)
                        (if handle
                          nil
                          action))
      :mouse-moved (case handle
                     :control-point (let [evaluation-context @(g/node-value self :initial-evaluation-context)
                                          delta (doto (Vector3d.)
                                                  (.sub cursor-pos start))
                                          trans (doto (Matrix4d.)
                                                  (.set delta))
                                          selected-ids (reduce (fn [ids [nid prop idx]]
                                                                 (update ids [nid prop] (fn [v] (conj (or v []) idx))))
                                                               {} sub-selection)]
                                      (g/transact
                                        (concat
                                          (g/operation-sequence op-seq)
                                          (g/set-property self :current cursor-pos)
                                          (for [[[nid prop] ids] selected-ids
                                                :let [curve (g/node-value nid prop evaluation-context)]]
                                            (g/set-property nid prop (types/geom-transform curve ids trans)))))
                                      nil)
                     :tangent (let [evaluation-context @(g/node-value self :initial-evaluation-context)
                                    [nid prop idx] (g/node-value self :handle-data)
                                    new-curve (-> (g/node-value nid prop evaluation-context)
                                                (types/geom-update [idx]
                                                                   (fn [cp]
                                                                     (let [[x y tx ty] cp
                                                                           t (doto (Vector3d. cursor-pos)
                                                                               (.sub (Vector3d. x y 0.0))
                                                                               (.setZ 0.0))]
                                                                       (when (< (.x t) 0.0)
                                                                         (.negate t))
                                                                       (when (< (.x t) 0.001)
                                                                         (.setX t 0.001))
                                                                       (.normalize t)
                                                                       [x y (.x t) (.y t)]))))]
                                (g/transact
                                  (concat
                                    (g/operation-sequence op-seq)
                                    (g/set-property nid prop new-curve)))
                                nil)
                     action)
      action)))

(g/defnode CurveController
  (property handle g/Keyword)
  (property handle-data g/Any)
  (property start Point3d)
  (property current Point3d)
  (property op-seq g/Any)
  (property initial-evaluation-context g/Any)
  (property select-fn Runnable)
  (input sub-selection g/Any)
  (input curve-handle g/Any)
  (output input-handler Runnable :cached (g/constantly handle-input)))

(defn- pick-control-points [curves picking-rect camera viewport]
  (let [aabb (geom/centered-rect->aabb picking-rect)]
    (->> curves
      (mapcat (fn [c]
                (->> (:curve c)
                  (filterv (fn [[idx cp]]
                             (let [[x y] cp
                                   p (doto (->> (Point3d. x y 0.0)
                                             (camera/camera-project camera viewport))
                                       (.setZ 0.0))]
                               (aabb-contains? aabb p))))
                  (mapv (fn [[idx _]] [(:node-id c) (:property c) idx])))))
      (keep identity))))

(defn- pick-tangent [curves ^Rect picking-rect camera viewport sub-selection-map]
  (let [aabb (geom/centered-rect->aabb picking-rect)
        [scale-x scale-y] (camera/scale-factor camera viewport)]
    (some (fn [c]
            (when-let [sel (get sub-selection-map [(:node-id c) (:property c)])]
              (let [cps (filterv (comp sel first) (:curve c))]
                (some (fn [[idx cp]]
                        (let [[x y tx ty] cp
                              t (doto (Vector3d. (/ tx scale-x) (/ (- ty) scale-y) 0.0)
                                  (.normalize)
                                  (.scale tangent-length))
                              p (doto (camera/camera-project camera viewport (Point3d. x y 0.0))
                                  (.setZ 0.0))
                              p0 (doto (Point3d. p)
                                     (.add t))
                              p1 (doto (Point3d. p)
                                     (.sub t))]
                          (when (or (aabb-contains? aabb p0)
                                    (aabb-contains? aabb p1))
                            [(:node-id c) (:property c) idx])))
                      cps))))
          curves)))

(defn- pick-closest-curve [curves ^Rect picking-rect camera viewport]
  (let [p (let [p (camera/camera-unproject camera viewport (.x picking-rect) (.y picking-rect) 0.0)]
            (Point3d. (.x p) (.y p) 0.0))
        min-distance (Double/MAX_VALUE)
        curve (second
                (reduce (fn [[min-dist closest-curve] {:keys [node-id property curve]}]
                          (let [s (-> (mapv second curve)
                                    (properties/->spline))
                                cp (properties/spline-cp s (.x p))
                                [x y] cp
                                closest (Point3d. x y 0.0)
                                dist (.distanceSquared p closest)]
                            (if (< dist min-dist)
                              [dist [node-id property closest]]
                              [min-dist closest-curve])))
                        [min-distance nil] curves))
        [_ _ ^Point3d closest] curve
        screen-p (and closest (camera/camera-project camera viewport closest))]
    (when (and screen-p (< (.distanceSquared screen-p (Point3d. (.x picking-rect) (.y picking-rect) 0.0))
                           (* selection/min-pick-size selection/min-pick-size)))
      curve)))

(g/defnk produce-picking-selection [visible-curves picking-rect camera viewport]
  (pick-control-points visible-curves picking-rect camera viewport))

(defn- sub-selection->map [sub-selection]
  (reduce (fn [sel [nid prop idx]] (update sel [nid prop] (fn [v] (conj (or v #{}) idx))))
          {} sub-selection))

(defn- curve-aabb [aabb geom-aabbs]
  (let [aabbs (vals geom-aabbs)]
    (when (> (count aabbs) 1)
      (loop [aabbs aabbs
             aabb aabb]
        (if-let [[min max] (first aabbs)]
          (let [aabb (apply geom/aabb-incorporate aabb min)
                aabb (apply geom/aabb-incorporate aabb max)]
            (recur (rest aabbs) aabb))
          aabb)))))

(defn- geom-cloud [v]
  (when (satisfies? types/GeomCloud v)
    v))

(g/defnk produce-aabb [selected-node-properties]
  (reduce (fn [aabb props]
            (loop [props (:properties props)
                   aabb aabb]
              (if-let [[kw p] (first props)]
                (recur (rest props)
                  (or (some->> p
                        :value
                        geom-cloud
                        types/geom-aabbs
                        (curve-aabb aabb))
                    aabb))
                aabb)))
    (geom/null-aabb) selected-node-properties))

(g/defnk produce-selected-aabb [sub-selection-map selected-node-properties]
  (reduce (fn [aabb props]
            (loop [props (:properties props)
                   aabb aabb]
              (if-let [[kw p] (first props)]
                (let [aabb (or (when-let [ids (sub-selection-map [(:node-id p) kw])]
                                 (curve-aabb aabb (types/geom-aabbs (:value p) ids)))
                             aabb)]
                  (recur (rest props) aabb))
                aabb)))
          (geom/null-aabb) selected-node-properties))

(g/defnode CurveView
  (inherits scene/SceneRenderer)

  (property image-view ImageView)
  (property viewport Region (default (types/->Region 0 0 0 0)))
  (property play-mode g/Keyword)
  (property drawable GLAutoDrawable)
  (property async-copier AsyncCopier)
  (property cursor-pos types/Vec2)
  (property tool-picking-rect Rect)
  (property list ListView)
  (property hidden-curves g/Any)
  (property tool-user-data g/Any (default (atom [])))
  (property input-action-queue g/Any (default []))
  (property updatable-states g/Any (default (atom {})))

  (input camera-id g/NodeID :cascade-delete)
  (input grid-id g/NodeID :cascade-delete)
  (input background-id g/NodeID :cascade-delete)
  (input input-handlers Runnable :array)
  (input selected-node-properties g/Any)
  (input tool-renderables pass/RenderData :array)
  (input picking-rect Rect)
  (input sub-selection g/Any)

  (output all-renderables g/Any :cached (g/fnk [renderables curve-renderables cp-renderables tool-renderables]
                                          (reduce (partial merge-with into) renderables (into [curve-renderables cp-renderables] tool-renderables))))
  (output curves g/Any :cached produce-curves)
  (output visible-curves g/Any :cached (g/fnk [curves hidden-curves] (remove #(contains? hidden-curves (:property %)) curves)))
  (output curve-renderables g/Any :cached produce-curve-renderables)
  (output cp-renderables g/Any :cached produce-cp-renderables)
  (output picking-selection g/Any :cached produce-picking-selection)
  (output selected-tool-renderables g/Any :cached (g/fnk [] {}))
  (output sub-selection-map g/Any :cached (g/fnk [sub-selection]
                                                 (sub-selection->map sub-selection)))
  (output aabb AABB :cached produce-aabb)
  (output selected-aabb AABB :cached produce-selected-aabb)
  (output curve-handle g/Any :cached (g/fnk [curves tool-picking-rect camera viewport sub-selection-map]
                                            (if-let [cp (first (pick-control-points curves tool-picking-rect camera viewport))]
                                              [:control-point cp]
                                              (if-let [tangent (pick-tangent curves tool-picking-rect camera viewport sub-selection-map)]
                                                [:tangent tangent]
                                                (if-let [curve (pick-closest-curve curves tool-picking-rect camera viewport)]
                                                  [:curve curve]
                                                  nil)))))
  (output update-list-view g/Any :cached (g/fnk [curves ^ListView list selected-node-properties sub-selection-map]
                                                (let [p (:properties (properties/coalesce selected-node-properties))
                                                      new-items (reduce (fn [res c]
                                                                          (if (contains? p (:property c))
                                                                            (conj res {:keyword (:property c)
                                                                                      :property (get p (:property c))
                                                                                      :hue (:hue c)})
                                                                            res))
                                                                        [] curves)
                                                      old-items (ui/user-data list ::items)
                                                      old-sel (ui/user-data list ::sub-selection)]
                                                  (when (or (not= new-items old-items)
                                                            (not= sub-selection-map old-sel))
                                                    (binding [*programmatic-selection* true]
                                                      (ui/user-data! list ::items new-items)
                                                      (ui/user-data! list ::sub-selection sub-selection-map)
                                                      (let [selected (reduce conj #{} (map second (keys sub-selection-map)))
                                                            selected-indices (reduce (fn [res [i v]]
                                                                                       (if (selected v) (conj res (int i)) res))
                                                                                     [] (map-indexed (fn [i v] [i (:keyword v)]) new-items))]
                                                        (ui/items! list new-items)
                                                        (if (empty? selected-indices)
                                                          (doto (.getSelectionModel list)
                                                                  (.selectRange 0 0))
                                                          (let [index (int (first selected-indices))
                                                                rest-indices (next selected-indices)
                                                                indices ^ints (int-array (or rest-indices 0))]
                                                            (doto (.getSelectionModel list)
                                                              (.selectIndices index indices))))))))))
  (output active-updatables g/Any (g/constantly [])))

(defonce view-state (atom nil))

(defn frame-selection [view selection animate?]
  (let [aabb (if (empty? selection)
               (g/node-value view :aabb)
               (g/node-value view :selected-aabb))]
    (when (not= aabb (geom/null-aabb))
      (let [graph (g/node-id->graph-id view)
            camera (g/node-feeding-into view :camera)
            viewport (g/node-value view :viewport)
            local-cam (g/node-value camera :local-camera)
            end-camera (c/camera-orthographic-frame-aabb-y local-cam viewport aabb)]
        (scene/set-camera! camera local-cam end-camera animate?)))))

(defn destroy-view! [parent ^AnchorPane view view-id ^ListView list]
  (when-let [repainter (ui/user-data view ::repainter)]
    (ui/timer-stop! repainter)
    (ui/user-data! view ::repainter nil))
  (when view-id
    (when-let [scene (g/node-by-id view-id)]
      (when-let [^GLAutoDrawable drawable (g/node-value view-id :drawable)]
        (let [gl (.getGL drawable)]
          (when-let [^AsyncCopier copier (g/node-value view-id :async-copier)]
            (.dispose copier gl))
          (scene-cache/drop-context! gl false)
          (.destroy drawable))))
    (g/transact (g/delete-node view-id))
    (ui/children! view [])
    (ui/remove-list-observers list (ui/selection list))))

(defn- camera-filter-fn [camera]
  (let [^Point3d p (:position camera)
        y (.y p)
        z (.z p)]
    (assoc camera
           :position (Point3d. 0.5 y z)
           :focus-point (Vector4d. 0.5 y 0.0 1.0)
           :fov-x 1.2)))

(defrecord SubSelectionProvider [app-view]
  handler/SelectionProvider
  (selection [this] (g/node-value app-view :sub-selection))
  (succeeding-selection [this] [])
  (alt-selection [this] []))

(defn- on-list-selection [app-view values]
  (when-not *programmatic-selection*
    (let [selection (loop [values values
                           res []]
                      (if-let [v (first values)]
                        (let [p (:property v)
                              kw (:key p)
                              res (loop [curves (map vector (:node-ids p) (:values p))
                                         res res]
                                    (if-let [[nid curve] (first curves)]
                                      (recur (rest curves)
                                             (reduce (fn [res id] (conj res [nid kw id]))
                                                     res (iv/iv-ids (:points curve))))
                                      res))]
                          (recur (rest values)
                                 res))
                        res))]
      (app-view/sub-select! app-view selection))))

(defn make-view!
  ([app-view graph ^Parent parent ^ListView list ^AnchorPane view opts]
    (let [view-id (make-view! app-view graph parent list view opts false)]
      (reset! view-state {:app-view app-view :graph graph :parent parent :list list :view view :opts opts :view-id view-id})
      view-id))
  ([app-view graph ^Parent parent ^ListView list ^AnchorPane view opts reloading?]
    (let [[node-id] (g/tx-nodes-added
                      (g/transact (g/make-nodes graph [view-id    [CurveView :list list :hidden-curves #{} :frame-version (atom 0)]
                                                       controller [CurveController :select-fn (fn [selection op-seq] (app-view/sub-select! app-view selection op-seq))]
                                                       selection  [selection/SelectionController :select-fn (fn [selection op-seq] (app-view/sub-select! app-view selection op-seq))]
                                                       background background/Background
                                                       camera     [c/CameraController :local-camera (or (:camera opts) (c/make-camera :orthographic camera-filter-fn))]
                                                       grid       curve-grid/Grid
                                                       rulers     [rulers/Rulers]]
                                                (g/update-property camera :movements-enabled disj :tumble) ; TODO - pass in to constructor

                                                (g/connect camera :_node-id view-id :camera-id)
                                                (g/connect grid :_node-id view-id :grid-id)
                                                (g/connect camera :camera view-id :camera)
                                                (g/connect camera :camera grid :camera)
                                                (g/connect camera :input-handler view-id :input-handlers)
                                                (g/connect view-id :viewport camera :viewport)
                                                (g/connect grid :renderable view-id :aux-renderables)
                                                (g/connect background :_node-id view-id :background-id)
                                                (g/connect background :renderable view-id :aux-renderables)

                                                (g/connect app-view :selected-node-properties view-id :selected-node-properties)
                                                (g/connect app-view :sub-selection view-id :sub-selection)

                                                (g/connect view-id :curve-handle controller :curve-handle)
                                                (g/connect app-view :sub-selection controller :sub-selection)
                                                (g/connect controller :input-handler view-id :input-handlers)

                                                (g/connect selection            :renderable                view-id          :tool-renderables)
                                                (g/connect selection            :input-handler             view-id          :input-handlers)
                                                (g/connect selection            :picking-rect              view-id          :picking-rect)
                                                (g/connect view-id              :picking-selection         selection        :picking-selection)
                                                (g/connect app-view             :sub-selection             selection        :selection)

                                                (g/connect camera :camera rulers :camera)
                                                (g/connect rulers :renderables view-id :aux-renderables)
                                                (g/connect view-id :viewport rulers :viewport)
                                                (g/connect view-id :cursor-pos rulers :cursor-pos))))]
      (when parent
        (let [^Node pane (scene/make-gl-pane! node-id view opts "update-curve-view" false)]
          (ui/context! parent :curve-view {:view-id node-id} (SubSelectionProvider. app-view))
          (ui/fill-control pane)
          (ui/children! view [pane])
          (let [converter (proxy [StringConverter] []
                            (fromString ^Object [s] nil)
                            (toString ^String [item] (if (:property item)
                                                       (properties/label (:property item))
                                                       "")))
                selected-callback (reify Callback
                                    (call ^ObservableValue [this item]
                                      (let [hidden-curves (g/node-value node-id :hidden-curves)]
                                        (doto (SimpleBooleanProperty. (not (contains? hidden-curves (:keyword item))))
                                          (ui/observe (fn [observable old new]
                                                        (let [kw (:keyword item)]
                                                          (if new
                                                            (g/update-property! node-id :hidden-curves disj kw)
                                                            (g/update-property! node-id :hidden-curves conj kw)))))))))]
            (let [items (ui/selection list)]
              (ui/observe-list list items (fn [_ values] (on-list-selection app-view values))))
            (doto (.getSelectionModel list)
              (.setSelectionMode SelectionMode/MULTIPLE))
            (.setCellFactory list
              (reify Callback
                (call ^ListCell [this list]
                  (proxy [CheckBoxListCell] [selected-callback converter]
                    (updateItem [item empty]
                      (let [this ^CheckBoxListCell this]
                        (proxy-super updateItem item empty)
                        (when (and item (not empty))
                          (let [[r g b] (colors/hsl->rgb (:hue item) 1.0 0.75)]
                            (proxy-super setStyle (format "-fx-text-fill: rgb(%d, %d, %d);" (int (* 255 r)) (int (* 255 g)) (int (* 255 b)))))))))))))))
      node-id)))

(defn- reload-curve-view []
  (when @view-state
    (let [{:keys [app-view graph ^Parent parent ^ListView list ^AnchorPane view opts view-id]} @view-state]
      (ui/run-now
        (destroy-view! parent view view-id list)
        (make-view! app-view graph parent list view opts true)))))

(defn- delete-cps [sub-selection]
  (let [m (sub-selection->map sub-selection)]
    (g/transact
      (for [[[nid prop] idx] m]
        (g/update-property nid prop types/geom-delete idx)))))

(handler/defhandler :delete :curve-view
  (enabled? [selection] (not (empty? selection)))
  (run [selection] (delete-cps selection)))

(handler/defhandler :frame-selection :curve-view
  (run [view-id selection] (frame-selection view-id selection true)))
