(ns editor.scene-tools
  (:require [clojure.set :as set]
            [dynamo.background :as background]
            [dynamo.geom :as geom]
            [dynamo.gl :as gl]
            [dynamo.gl.shader :as shader]
            [dynamo.gl.vertex :as vtx]
            [dynamo.graph :as g]
            [dynamo.grid :as grid]
            [dynamo.types :as t]
            [dynamo.types :refer [IDisposable dispose]]
            [dynamo.util :as util]
            [editor.camera :as c]
            [editor.core :as core]
            [editor.input :as i]
            [editor.workspace :as workspace]
            [editor.math :as math]
            [internal.render.pass :as pass]
            [service.log :as log])
  (:import [com.defold.editor Start UIUtil]
           [com.jogamp.opengl.util.awt TextRenderer Screenshot]
           [dynamo.types Camera AABB Region Rect]
           [java.awt Font]
           [java.awt.image BufferedImage]
           [java.lang Runnable Math]
           [java.nio IntBuffer ByteBuffer ByteOrder]
           [javafx.animation AnimationTimer]
           [javafx.application Platform]
           [javafx.beans.value ChangeListener]
           [javafx.collections FXCollections ObservableList]
           [javafx.embed.swing SwingFXUtils]
           [javafx.event ActionEvent EventHandler]
           [javafx.geometry BoundingBox]
           [javafx.scene Scene Node Parent]
           [javafx.scene.control Tab]
           [javafx.scene.image Image ImageView WritableImage PixelWriter]
           [javafx.scene.input MouseEvent]
           [javafx.scene.layout AnchorPane Pane]
           [javax.media.opengl GL GL2 GLContext GLProfile GLAutoDrawable GLOffscreenAutoDrawable GLDrawableFactory GLCapabilities]
           [javax.media.opengl.glu GLU]
           [javax.vecmath Point2i Point3d Quat4d Matrix4d Vector4d Matrix3d Vector3d AxisAngle4d]))

(set! *warn-on-reflection* true)

(defprotocol Manipulatable
  (manip-fn [self type]))

; Render assets

(vtx/defvertex pos-vtx
  (vec3 position))

(shader/defshader vertex-shader
  (attribute vec4 position)
  (defn void main []
    (setq gl_Position (* gl_ModelViewProjectionMatrix position))))

(shader/defshader fragment-shader
  (uniform vec4 color)
  (defn void main []
    (setq gl_FragColor color)))

(def shader (shader/make-shader vertex-shader fragment-shader))

; Rendering

(def selected-color [1.0 1.0 0.0])
(def hot-color [1.0 (/ 211.0 255) (/ 149.0 255)])

(defn- scale-factor [camera viewport]
  (let [inv-view (doto (Matrix4d. (c/camera-view-matrix camera)) (.invert))
        x-axis   (Vector4d.)
        _        (.getColumn inv-view 0 x-axis)
        cp1      (c/camera-project camera viewport (Point3d.))
        cp2      (c/camera-project camera viewport (Point3d. (.x x-axis) (.y x-axis) (.z x-axis)))]
    (/ 1.0 (Math/abs (- (.x cp1) (.x cp2))))))

(defn render-manip [^GL2 gl pass color vertex-buffers]
  (doseq [[mode vertex-buffer vertex-count] vertex-buffers
          :let [vertex-binding (vtx/use-with vertex-buffer shader)
                color (if (= mode GL/GL_LINES) (float-array (assoc color 3 1.0)) (float-array color))]
          :when (> vertex-count 0)]
    (gl/with-enabled gl [shader vertex-binding]
      (shader/set-uniform shader gl "color" color)
      (gl/gl-draw-arrays gl mode 0 vertex-count))))

(defn- vtx-apply [f vs & args]
  (map (fn [[mode vs]] [mode (map (fn [v] (apply map f v args)) vs)]) vs))

(defn- vtx-scale [s vs]
  (vtx-apply * vs s))

(defn- vtx-add [p vs]
  (vtx-apply + vs p))

(defn- gen-cone [sub-divs]
  (let [tip [1.0 0.0 0.0]
        origin [0.0 0.0 0.0]
        circle (partition 2 1 (map #(let [angle (* 2.0 Math/PI (/ (double %) sub-divs))]
                                      [0.0 (Math/cos angle) (Math/sin angle)]) (range (inc sub-divs))))]
    [[GL/GL_TRIANGLES (mapcat (fn [p] (mapcat #(conj % p) circle)) [tip origin])]]))

(defn- gen-line []
  [[GL/GL_LINES [[0.0 0.0 0.0] [1.0 0.0 0.0]]]])

(defn- gen-square [filled?]
  (let [corners (for [x [-1.0 1.0]
                      y [-1.0 1.0]]
                  [x y 0.0])]
    (concat
      [[GL/GL_LINES (map #(nth corners %) [0 2 2 3 3 1 1 0])]]
      (if filled?
        [[GL/GL_TRIANGLES (map #(nth corners %) [0 2 1 2 3 1])]]
        []))))

(defn- gen-arrow [sub-divs]
  (concat
    (vtx-add [90.0 0.0 0.0] (vtx-scale [20.0 5.0 5.0] (gen-cone sub-divs)))
    (vtx-add [15.0 0.0 0.0] (vtx-scale [85.0 1.0 1.0] (gen-line)))))

(defn- gen-vertex-buffer [vertices vtx-count]
  (let [vbuf  (->pos-vtx vtx-count)]
    (doseq [vertex vertices]
      (conj! vbuf vertex))
    (persistent! vbuf)))

(defn- manip->normal [manip]
  (case manip
    :x [1.0 0.0 0.0]
    :y [0.0 1.0 0.0]
    :z [0.0 0.0 1.0]
    :xy [0.0 0.0 1.0]
    :xz [0.0 1.0 0.0]
    :yz [1.0 0.0 0.0]
    nil))

(defn- manip->axis-indices [manip]
  (case manip
    :x [0]
    :y [1]
    :z [2]
    :xy [0 1]
    :xz [0 2]
    :yz [1 2]
    :screen []))

(defn- manip->color [manip active-manip hot-manip tool-active?]
  (let [hot (= manip hot-manip)
        axis-indices (manip->axis-indices manip)
        active-axis-indices (manip->axis-indices active-manip)
        active (or (= manip active-manip) (and tool-active? (not (empty? axis-indices)) (every? (set active-axis-indices) axis-indices)))]
    (cond
      hot (conj hot-color 1.0)
      active (conj selected-color 1.0)
      true (case manip
             (:x :y :z) (conj (manip->normal manip) 1.0)
             (:xy :xz :yz) (conj (manip->normal manip) 0.2)
             :screen [(/ 100.0 255) (/ 220.0 255) 1.0 0.0]))))

(defn- manip->rotation [manip] ^AxisAngle4d
  (case manip
    :x (AxisAngle4d.)
    :y (AxisAngle4d. (Vector3d. 0.0 0.0 1.0) (* 0.5 (Math/PI)))
    :z (AxisAngle4d. (Vector3d. 0.0 1.0 0.0) (- (* 0.5 (Math/PI))))
    :xy (AxisAngle4d.)
    :xz (AxisAngle4d. (Vector3d. 1.0 0.0 0.0) (* 0.5 (Math/PI)))
    :yz (AxisAngle4d. (Vector3d. 0.0 1.0 0.0) (- (* 0.5 (Math/PI))))
    :screen (AxisAngle4d.)))

(defn- manip->visible? [manip ^Vector3d view-dir]
  (if-let [normal (manip->normal manip)]
    (let [dir (Vector3d. (double-array normal))]
      (case manip
        (:x :y :z) (< (Math/abs (.dot dir view-dir)) 0.95)
        (:xy :xz :yz) (> (Math/abs (.dot dir view-dir)) 0.05)))
    true))

(defn- index->axis [index]
  (vec (take 3 (drop (- 3 index) (cycle [1.0 0.0 0.0])))))

(defn- camera->view-dir [camera]
  (let [view ^Matrix4d (c/camera-view-matrix camera)
        v4 (Vector4d.)
        _ (.getColumn view 2 v4)]
    (Vector3d. (.x v4) (.y v4) (.z v4))))

(defn- gen-manip-renderable [id manip ^Vector3d position ^AxisAngle4d rotation ^Double scale vertex-buffers color]
  (let [wt (Matrix4d. ^Matrix3d (doto (Matrix3d.) (.set rotation)) position scale)]
    {:id id
    :user-data manip
    :render-fn (g/fnk [gl pass camera]
                      (when (manip->visible? manip (camera->view-dir camera))
                        (render-manip gl pass color vertex-buffers)))
    :world-transform wt}))

(g/defnk produce-renderables [self active-tool camera viewport selected-renderables active-manip hot-manip start]
  (if (empty? selected-renderables)
    {}
    (let [active-axis-indices (manip->axis-indices active-manip)
          scale (scale-factor camera viewport)
          wt ^Matrix4d (:world-transform (last selected-renderables))
          pos4 (Vector4d.)
          _ (.getColumn wt 3 pos4)
          pos3 (Vector3d. (.x pos4) (.y pos4) (.z pos4))
          color-fn #(manip->color % active-manip hot-manip start)
          rotation-fn #(manip->rotation %)
          renderables (concat
                        (let [arrow-vertices (gen-arrow 10)
                              arrow-vertices-by-mode (reduce (fn [m [mode vs]] (merge-with concat m {mode vs})) {} arrow-vertices)
                              arrow-vertex-buffers (map (fn [[mode vs]]
                                                          (let [count (count vs)]
                                                            [mode (gen-vertex-buffer vs count) count])) arrow-vertices)
                              manip-filter-fn (if (and start (= (count active-axis-indices) 1)) #(= % active-manip) (constantly true))]
                          (map #(gen-manip-renderable (g/node-id self) % pos3 (rotation-fn %) scale arrow-vertex-buffers (color-fn %)) (filter manip-filter-fn [:x :y :z])))
                        (let [plane-vertices (vtx-add [65.0 65.0 0.0] (vtx-scale [8.0 8.0 1.0] (gen-square true)))
                              plane-vertices-by-mode (reduce (fn [m [mode vs]] (merge-with concat m {mode vs})) {} plane-vertices)
                              plane-vertex-buffers (map (fn [[mode vs]]
                                                          (let [count (count vs)]
                                                            [mode (gen-vertex-buffer vs count) count])) plane-vertices)
                              manip-filter-fn (if start #(= % active-manip) (constantly true))]
                          (map #(gen-manip-renderable (g/node-id self) % pos3 (rotation-fn %) scale plane-vertex-buffers (color-fn %)) (filter manip-filter-fn [:xy :xz :yz])))
                        (let [screen-vertices (vtx-scale [8.0 8.0 1.0] (gen-square true))
                              screen-vertices-by-mode (reduce (fn [m [mode vs]] (merge-with concat m {mode vs})) {} screen-vertices)
                              screen-vertex-buffers (map (fn [[mode vs]]
                                                           (let [count (count vs)]
                                                             [mode (gen-vertex-buffer vs count) count])) screen-vertices)
                              manip-filter-fn (if start #(= % active-manip) (constantly true))]
                          (map #(gen-manip-renderable (g/node-id self) % pos3 (rotation-fn %) scale screen-vertex-buffers (color-fn %)) (filter manip-filter-fn [:screen]))))]
      (reduce #(assoc %1 %2 renderables) {} [pass/manipulator pass/manipulator-selection]))))

(defn squash-delta [manip ^Vector3d v]
  (case manip
    :x (do (.setY v 0.0) (.setZ v 0.0))
    :y (do (.setX v 0.0) (.setZ v 0.0))
    :z (do (.setX v 0.0) (.setY v 0.0))
    :xy (.setZ v 0.0)
    :xz (.setY v 0.0)
    :yz (.setX v 0.0)
    :screen nil))

(defn handle-input [self action user-data]
  (let [camera (g/node-value self :camera)
        viewport (g/node-value self :viewport)
        active-tool (:active-tool self)
        world-v4 (c/camera-unproject camera viewport (:x action) (:y action) 0)
        world-v3 (Vector3d. (.x world-v4) (.y world-v4) (.z world-v4))]
    (case (:type action)
      :mouse-pressed (if-let [manip (first (get user-data (g/node-id self)))]
                       (let [selected-renderables (g/node-value self :selected-renderables)
                             now (g/now)
                             originals (map #(g/node-by-id (g/now) (:id %)) selected-renderables)]
                         (g/transact
                             (concat
                               (g/set-property self :start world-v3)
                               (g/set-property self :originals originals)
                               (g/set-property self :active-manip manip)
                               (g/set-property self :hot-manip nil)
                               (g/set-property self :op-seq (gensym))))
                         nil)
                       action)
      :mouse-released (if (:start self)
                        (do
                          (g/transact
                            (concat
                              (g/set-property self :start nil)
                              (g/set-property self :originals nil)))
                          nil)
                        action)
      :mouse-moved (if-let [start (:start self)]
                     (let [type (:active-tool self)
                           originals (:originals self)
                           manip (:active-manip self)
                           op-seq (:op-seq self)
                           delta (doto (Vector3d. world-v3) (.sub start))
                           _ (squash-delta manip delta)]
                       (g/transact
                         (concat
                           (g/operation-label "Move")
                           (g/operation-sequence op-seq)
                           (for [original originals
                                 :let [type-fn (manip-fn original type)]]
                             (type-fn original delta))))
                       nil)
                     (let [manip (first (get user-data (g/node-id self)))]
                       (when (not= manip (:hot-manip self))
                         (g/transact (g/set-property self :hot-manip manip)))
                       action))
      action)))

(g/defnode ToolController
  (property active-tool t/Keyword)
  (property start (t/maybe Vector3d))
  (property originals t/Any)
  
  (property hot-manip (t/maybe t/Keyword))
  (property active-manip (t/maybe t/Keyword) (default :screen))
  (property op-seq t/Any)
  
  (input camera t/Any)
  (input viewport t/Any)
  (input selected-renderables t/Any)

  (output renderables pass/RenderData :cached produce-renderables)
  (output input-handler Runnable :cached (g/fnk [] handle-input)))
