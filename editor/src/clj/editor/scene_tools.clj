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

(defprotocol Movable
  (move [self ^Vector3d delta]))
(defprotocol Rotatable
  (rotate [self ^Quat4d delta]))

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
                color (if (#{GL/GL_LINES GL/GL_POINTS} mode) (float-array (assoc color 3 1.0)) (float-array color))]
          :when (> vertex-count 0)]
    (gl/with-enabled gl [shader vertex-binding]
      (shader/set-uniform shader gl "color" color)
      (gl/gl-draw-arrays gl mode 0 vertex-count))))

; Vertex generation and transformations

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

(defn- gen-point []
  [[GL/GL_POINTS [[0.0 0.0 0.0]]]])

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

(defn- gen-circle [segs]
  [[GL/GL_LINES (reduce concat (partition 2 1 (map #(let [angle (* 2.0 Math/PI (/ (double %) segs))]
                                                     [(Math/cos angle) (Math/sin angle) 0.0]) (range (inc segs)))))]])

(defn- gen-arrow [sub-divs]
  (concat
    (vtx-add [90.0 0.0 0.0] (vtx-scale [20.0 5.0 5.0] (gen-cone sub-divs)))
    (vtx-add [15.0 0.0 0.0] (vtx-scale [85.0 1.0 1.0] (gen-line)))))

(defn- gen-vertex-buffer [vertices vtx-count]
  (let [vbuf  (->pos-vtx vtx-count)]
    (doseq [vertex vertices]
      (conj! vbuf vertex))
    (persistent! vbuf)))

; Transform tool manipulators

(def axis-rotation-radius 100.0)
(def screen-rotation-radius 120.0)

(defn- manip->normal [manip]
  (case manip
    :move-x [1.0 0.0 0.0]
    :move-y [0.0 1.0 0.0]
    :move-z [0.0 0.0 1.0]
    :move-xy [0.0 0.0 1.0]
    :move-xz [0.0 1.0 0.0]
    :move-yz [1.0 0.0 0.0]
    :move-screen [0.0 0.0 1.0]
    :rot-x [1.0 0.0 0.0]
    :rot-y [0.0 1.0 0.0]
    :rot-z [0.0 0.0 1.0]
    :rot-screen [0.0 0.0 1.0]))

(defn- manip->sub-manips [manip]
  (case manip
    :move-x #{}
    :move-y #{}
    :move-z #{}
    :move-xy #{:move-x :move-y}
    :move-xz #{:move-x :move-z}
    :move-yz #{:move-y :move-z}
    :move-screen #{}
    :rot-x #{}
    :rot-y #{}
    :rot-z #{}
    :rot-screen #{}))

(defn- manip->color [manip active-manip hot-manip tool-active?]
  (let [hot (= manip hot-manip)
        active (or (= manip active-manip) (and tool-active? (contains? (manip->sub-manips active-manip) manip)))
        alpha (if (= manip :move-screen) 0.0 1.0)]
    (cond
      hot (conj hot-color alpha)
      active (conj selected-color alpha)
      true (case manip
             (:move-x :move-y :move-z :rot-x :rot-y :rot-z) (conj (manip->normal manip) 1.0)
             (:move-xy :move-xz :move-yz) (conj (manip->normal manip) 0.2)
             (:move-screen :rot-screen) [(/ 100.0 255) (/ 220.0 255) 1.0 0.0]))))

(defn- manip->rotation [manip] ^AxisAngle4d
  (case manip
    :move-x (AxisAngle4d.)
    :move-y (AxisAngle4d. (Vector3d. 0.0 0.0 1.0) (* 0.5 (Math/PI)))
    :move-z (AxisAngle4d. (Vector3d. 0.0 1.0 0.0) (- (* 0.5 (Math/PI))))
    :move-xy (AxisAngle4d.)
    :move-xz (AxisAngle4d. (Vector3d. 1.0 0.0 0.0) (* 0.5 (Math/PI)))
    :move-yz (AxisAngle4d. (Vector3d. 0.0 1.0 0.0) (- (* 0.5 (Math/PI))))
    :move-screen (AxisAngle4d.)
    :rot-x (AxisAngle4d. (Vector3d. 0.0 1.0 0.0) (- (* 0.5 (Math/PI))))
    :rot-y (AxisAngle4d. (Vector3d. 1.0 0.0 0.0) (* 0.5 (Math/PI)))
    :rot-z (AxisAngle4d.)
    :rot-screen (AxisAngle4d.)))

(defn- manip->screen? [manip]
  (case manip
    (:move-screen :rot-screen) true
    false))

(defn- manip-enabled? [manip active-manip tool-active?]
  (if (#{:rot-x :rot-y :rot-z :rot-xy :rot-xz :rot-yz :rot-screen} manip)
    true
    (if tool-active?
       (or (= manip active-manip)
           (and (#{:move-x :move-y :move-z} manip)
                (or (= active-manip :move-screen) (contains? (manip->sub-manips active-manip) manip))))
       true)))

(defn- manip-visible? [manip ^Matrix4d view]
  (if (manip->screen? manip)
    true
    (let [dir (Vector3d. (double-array (manip->normal manip)))
          _ (.transform view dir)]
      (case manip
        (:move-x :move-y :move-z) (< (Math/abs (.z dir)) 0.99)
        (:move-xy :move-xz :move-yz) (> (Math/abs (.z dir)) 0.06)
        true))))

(defn- manip->vertices [manip]
  (case manip
    (:move-x :move-y :move-z) (gen-arrow 10)
    (:move-xy :move-xz :move-yz) (vtx-add [65.0 65.0 0.0] (vtx-scale [8.0 8.0 1.0] (gen-square true)))
    :move-screen (concat
                (vtx-scale [8.0 8.0 1.0] (gen-square true))
                (gen-point))
    (:rot-x :rot-y :rot-z) (vtx-scale [axis-rotation-radius axis-rotation-radius 1.0] (gen-circle 64))
    :rot-screen (vtx-scale [screen-rotation-radius screen-rotation-radius 1.0] (gen-circle 64))))

(defn- gen-manip-renderable [id manip ^Matrix4d world-transform ^AxisAngle4d rotation vertices color ^Matrix4d inv-view]
  (let [vertices-by-mode (reduce (fn [m [mode vs]] (merge-with concat m {mode vs})) {} vertices)
        vertex-buffers (map (fn [[mode vs]]
                              (let [count (count vs)]
                                [mode (gen-vertex-buffer vs count) count])) vertices-by-mode)
        rotation-mat (doto (Matrix4d.) (.set rotation))
        wt (Matrix4d. world-transform)
        _ (when (manip->screen? manip)
            (let [rotation (Matrix3d.)
                  _ (.get inv-view rotation)]
              (.setRotation wt rotation)))
        _ (.mul wt rotation-mat)]
    {:id id
    :user-data manip
    :render-fn (g/fnk [gl pass camera]
                      (when (manip-visible? manip (c/camera-view-matrix camera))
                        (render-manip gl pass color vertex-buffers)))
    :world-transform wt}))

(def transform-tools
  (let [move-manips [:move-x :move-y :move-z :move-xy :move-xz :move-yz :move-screen]
        rot-manips [:rot-x :rot-y :rot-z :rot-screen]]
    {:move {:manips move-manips
            :label "Move"
            :filter-fn (fn [n] (satisfies? Movable n))}
     :rotate {:manips rot-manips
              :label "Rotate"
              :filter-fn (fn [n] (satisfies? Rotatable n))}}))

(defn- transform->translation
  [^Matrix4d m]
  (let [v ^Vector3d (Vector3d.)]
    (.get m v)
    v))

(g/defnk produce-renderables [self active-tool camera viewport selected-renderables active-manip hot-manip start-action]
  (if (not (contains? transform-tools active-tool))
    {}
    (let [tool (get transform-tools active-tool)
          filter-fn (:filter-fn tool)
          now (g/now)
          selected-renderables (filter #(filter-fn (g/node-by-id now (:id %))) selected-renderables)]
      (if (empty? selected-renderables)
        {}
        (let [tool-active (not (nil? start-action))
              scale (scale-factor camera viewport)
              world-transform (doto (Matrix4d. ^Matrix4d (:world-transform (last selected-renderables))) (.setRotation (doto (Matrix3d.) (.setIdentity))) (.setScale scale))
              rotation-fn #(manip->rotation %)
              color-fn #(manip->color % active-manip hot-manip tool-active)
              filter-fn #(manip-enabled? % active-manip tool-active)
              vertices-fn #(manip->vertices %)
              manips (get-in transform-tools [active-tool :manips])
              inv-view (doto (c/camera-view-matrix camera) (.invert))
              renderables (map #(gen-manip-renderable (g/node-id self) % world-transform (rotation-fn %) (vertices-fn %) (color-fn %) inv-view)
                               (filter filter-fn manips))]
          (reduce #(assoc %1 %2 renderables) {} [pass/manipulator pass/manipulator-selection]))))))

(defn- line->circle [^Point3d line-pos ^Vector3d line-dir ^Point3d manip-pos ^Vector3d manip-normal ^Double radius] ^Point3d
  (if-let [intersection ^Point3d (math/line-plane-intersection line-pos line-dir manip-pos manip-normal)]
    (let [dist-sq (.distanceSquared intersection manip-pos)]
      (when (> dist-sq math/epsilon-sq)
        (Point3d. (doto (Vector3d. intersection) (.sub manip-pos) (.normalize) (.scaleAdd radius manip-pos)))))
    (let [closest ^Point3d (math/project-lines manip-pos manip-normal line-pos line-dir)
          plane-dist (math/project (doto (Vector3d. closest) (.sub manip-pos)) manip-normal)
          closest (doto (Point3d. manip-normal) (.scaleAdd ^Double (- plane-dist) closest))
          radius-sq (* radius radius)
          dist-sq (.distanceSquared closest manip-pos)]
      (if (< dist-sq radius-sq)
        (doto (Point3d. line-dir) (.scaleAdd (Math/sqrt (- radius-sq dist-sq)) closest))
        (Point3d. (doto (Vector3d. closest) (.sub manip-pos) (.normalize) (.scaleAdd (Math/sqrt radius-sq) manip-pos)))))))

(defn- action->line [action]
  [(:world-pos action) (:world-dir action)])

(defn- action->manip-pos [action ^Matrix4d lead-transform manip proj-fn]
  (let [manip-dir ^Vector3d (Vector3d. (double-array (manip->normal manip)))
        _ (.transform lead-transform manip-dir)
        _ (.normalize manip-dir)
        manip-pos (Vector3d.)
        _ (.get lead-transform manip-pos)
        [action-pos action-dir] (action->line action)]
    (proj-fn action-pos action-dir (Point3d. manip-pos) manip-dir)))

(defn- manip->project-fn [manip camera viewport]
  (case manip
    (:move-x :move-y :move-z) math/project-lines
    (:move-xy :move-xz :move-yz :move-screen) math/line-plane-intersection
    (:rot-x :rot-y :rot-z :rot-screen)
    (let [scale (scale-factor camera viewport)
          radius (* scale
                    (case manip
                      (:rot-x :rot-y :rot-z) axis-rotation-radius
                      :rot-screen screen-rotation-radius))]
      (fn [pos dir manip-pos manip-dir] (line->circle pos dir manip-pos manip-dir radius)))))

(defn- manip->apply-fn [manip manip-pos original-values]
  (case manip
    (:move-x :move-y :move-z :move-xy :move-xz :move-yz :move-screen)
    (fn [start-pos pos]
      (let [total-delta (doto (Vector3d.) (.sub pos start-pos))]
        (for [[node _] original-values]
          (move node total-delta))))
    (:rot-x :rot-y :rot-z :rot-screen)
    (fn [start-pos pos]
      (let [[start-dir dir] (map #(doto (Vector3d.) (.sub % manip-pos) (.normalize)) [start-pos pos])
            rotation (math/from-to->quat start-dir dir)]
        (for [[node ^Matrix4d world-transform] original-values
              :let [world-rot (Quat4d.)
                    _ (.get world-transform world-rot)
                    rotation (doto (Quat4d. world-rot) (.conjugate) (.mul rotation) (.mul world-rot) (.normalize))]]
          (rotate node rotation))))))

(defn- apply-manipulator [original-values manip start-action prev-action action camera viewport]
  (let [manip-origin ^Vector3d (transform->translation (last (last original-values)))
        lead-transform (if (manip->screen? manip)
                         (doto (c/camera-view-matrix camera) (.invert) (.setTranslation manip-origin))
                         (doto (Matrix4d.) (.set manip-origin)))]
    (let [proj-fn (manip->project-fn manip camera viewport)
          apply-fn (manip->apply-fn manip manip-origin original-values)
          [start-pos pos] (map #(action->manip-pos % lead-transform manip proj-fn) [start-action action])
          total-delta (doto (Vector3d.) (.sub pos start-pos))]
      (apply-fn start-pos pos))))

(defn handle-input [self action user-data]
  (case (:type action)
    :mouse-pressed (if-let [manip (first (get user-data (g/node-id self)))]
                     (let [active-tool (:active-tool self)
                           tool (get transform-tools active-tool)
                           filter-fn (:filter-fn tool)
                           now (g/now)
                           selected-renderables (filter #(filter-fn (g/node-by-id now (:id %))) (g/node-value self :selected-renderables))
                           original-values (map #(do [(g/node-by-id (g/now) (:id %)) (:world-transform %)]) selected-renderables)]
                       (when (not (empty? original-values))
                         (g/transact
                            (concat
                              (g/set-property self :start-action action)
                              (g/set-property self :prev-action action)
                              (g/set-property self :original-values original-values)
                              (g/set-property self :active-manip manip)
                              (g/set-property self :hot-manip nil)
                              (g/set-property self :op-seq (gensym)))))
                       nil)
                     action)
    :mouse-released (if (:start-action self)
                      (do
                        (g/transact
                          (concat
                            (g/set-property self :start-action nil)
                            (g/set-property self :prev-action nil)
                            (g/set-property self :original-values nil)))
                        nil)
                      action)
    :mouse-moved (if-let [start-action (:start-action self)]
                   (let [prev-action (:prev-action self)
                         active-tool (:active-tool self)
                         original-values (:original-values self)
                         manip (:active-manip self)
                         op-seq (:op-seq self)
                         camera (g/node-value self :camera)
                         viewport (g/node-value self :viewport)]
                     (g/transact
                       (concat
                         (g/operation-label (get-in transform-tools [active-tool :label]))
                         (g/operation-sequence op-seq)
                         (apply-manipulator original-values manip start-action prev-action action camera viewport)))
                     (g/transact (g/set-property self :prev-action action))
                     nil)
                   (let [manip (first (get user-data (g/node-id self)))]
                     (when (not= manip (:hot-manip self))
                       (g/transact (g/set-property self :hot-manip manip)))
                     action))
    action))

(g/defnode ToolController
  (property start-action t/Any)
  (property prev-action t/Any)
  (property original-values t/Any)
  
  (property hot-manip t/Any)
  (property active-manip t/Any)
  (property op-seq t/Any)
  
  (input active-tool t/Keyword)
  (input camera t/Any)
  (input viewport t/Any)
  (input selected-renderables t/Any)

  (output renderables pass/RenderData :cached produce-renderables)
  (output input-handler Runnable :cached (g/fnk [] handle-input)))
