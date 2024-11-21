;; Copyright 2020-2024 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;; 
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;; 
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns editor.scene-tools
  (:require [clojure.set :as set]
            [dynamo.graph :as g]
            [editor.camera :as c]
            [editor.colors :as colors]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.pass :as pass]
            [editor.gl.shader :as shader]
            [editor.gl.vertex :as vtx]
            [editor.math :as math]
            [editor.prefs :as prefs]
            [editor.scene-picking :as scene-picking])
  (:import [com.jogamp.opengl GL GL2]
           [java.lang Math Runnable]
           [javax.vecmath AxisAngle4d Matrix3d Matrix4d Point3d Quat4d Tuple3d Vector3d]))

(set! *warn-on-reflection* true)

(defmulti manip-movable? (fn [node-id] (g/node-type-kw node-id)))
(defmethod manip-movable? :default [_] false)
(defmulti manip-move (fn [evaluation-context node-id ^Vector3d delta]
                       (g/node-type-kw (:basis evaluation-context) node-id)))
(defmulti manip-move-manips (fn [node-id] (g/node-type-kw node-id)))
(defmethod manip-move-manips :default
  [_]
  [:move-x :move-y :move-z :move-xy :move-xz :move-yz :move-screen])

(defmulti manip-rotatable? (fn [node-id] (g/node-type-kw node-id)))
(defmethod manip-rotatable? :default [_] false)
(defmulti manip-rotate (fn [evaluation-context node-id ^Quat4d delta]
                         (g/node-type-kw (:basis evaluation-context) node-id)))
(defmulti manip-rotate-manips (fn [node-id] (g/node-type-kw node-id)))
(defmethod manip-rotate-manips :default
  [_]
  [:rot-x :rot-y :rot-z :rot-screen])

(defmulti manip-scalable? (fn [node-id] (g/node-type-kw node-id)))
(defmethod manip-scalable? :default [_] false)
(defmulti manip-scale (fn [evaluation-context node-id ^Vector3d delta]
                        (g/node-type-kw (:basis evaluation-context) node-id)))
(defmulti manip-scale-manips (fn [node-id] (g/node-type-kw node-id)))
(defmethod manip-scale-manips :default
  [node-id]
  [:scale-x :scale-y :scale-z :scale-xy :scale-xz :scale-yz :scale-uniform])

; Render assets

(vtx/defvertex pos-vtx
  (vec3 position))

(shader/defshader vertex-shader
  (attribute vec4 position)
  (defn void main []
    (setq gl_Position (* gl_ModelViewProjectionMatrix position))))

(shader/defshader fragment-shader
  (uniform vec4 color) ; `color` also used in selection pass to render picking id
  (defn void main []
    (setq gl_FragColor color)))

; TODO - macro of this
(def shader (shader/make-shader ::shader vertex-shader fragment-shader))

; Rendering

(defn- scale-factor [camera viewport ^Tuple3d reference-point]
  (let [offset-point (doto (Point3d.) (.add reference-point (c/camera-right-vector camera)))
        screen-point-a (c/camera-project camera viewport reference-point)
        screen-point-b (c/camera-project camera viewport offset-point)]
    (/ 1.0 (Math/abs (- (.x screen-point-a) (.x screen-point-b))))))

(defn- manip->screen? [manip]
  (case manip
    (:move-screen :rot-screen) true
    false))

(defn- manip->direction [manip]
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
    :rot-screen [0.0 0.0 1.0]
    :scale-x [1.0 0.0 0.0]
    :scale-y [0.0 1.0 0.0]
    :scale-z [0.0 0.0 1.0]
    :scale-xy [0.0 0.0 1.0]
    :scale-xz [0.0 1.0 0.0]
    :scale-yz [1.0 0.0 0.0]
    :scale-uniform [0.0 0.0 1.0]))

(defn- manip->normal
  ^Vector3d [manip ^Quat4d rotation]
  (let [[x y z] (manip->direction manip)
        normal (Vector3d. x y z)]
    (math/rotate rotation normal)))

(defn- manip-visible? [manip manip-rotation ^Matrix4d view]
  (if (manip->screen? manip)
    true
    (let [dir (manip->normal manip manip-rotation)]
      (.transform view dir)
      (case manip
        (:move-x :move-y :move-z :scale-x :scale-y :scale-z) (< (Math/abs (.z dir)) 0.99)
        (:move-xy :move-xz :move-yz :scale-xy :scale-xz :scale-yz) (> (Math/abs (.z dir)) 0.06)
        true))))

(defn- get-manip-rotation
  ^Quat4d [manip-space manip-world-rotation]
  (case manip-space
    :local manip-world-rotation
    :world geom/NoRotation))

(defn render-manips [^GL2 gl render-args renderables n]
  (let [camera (:camera render-args)
        renderable (first renderables)
        world-transform (:world-transform renderable)
        user-data (:user-data renderable)
        manip (:manip user-data)
        manip-rotation (:manip-rotation user-data)
        color (if (= (:pass render-args) pass/manipulator-selection)
                (scene-picking/picking-id->color (:picking-id renderable))
                (:color user-data))
        vertex-buffers (:vertex-buffers user-data)]
    (when (manip-visible? manip manip-rotation (c/camera-view-matrix camera))
      (gl/gl-push-matrix gl
        (gl/gl-mult-matrix-4d gl world-transform)
        (doseq [[mode vertex-buffer vertex-count] vertex-buffers
                :let [vertex-binding (vtx/use-with mode vertex-buffer shader)
                      color (if (#{GL/GL_LINES GL/GL_POINTS} mode)
                              (float-array (assoc color 3 1.0))
                              (float-array color))]
                :when (> vertex-count 0)]
          (gl/with-gl-bindings gl render-args [shader vertex-binding]
            (shader/set-uniform shader gl "color" color)
            (gl/gl-draw-arrays gl mode 0 vertex-count)))))))

; Vertex generation and transformations

(defn- vtx-apply [f vs & args]
  (map (fn [[mode vs]] [mode (map (fn [v] (apply map f v args)) vs)]) vs))

(defn- vtx-scale [s vs]
  (vtx-apply * vs s))

(defn- vtx-add [p vs]
  (vtx-apply + vs p))

(defn- vtx-rot [^AxisAngle4d r vs]
  (let [mat ^Matrix3d (doto (Matrix3d.) (.set r))
        vec (Vector3d.)]
    (map (fn [[mode vs]] [mode (map (fn [v]
                                      (.set vec (double-array v))
                                      (.transform mat vec)
                                      [(.x vec) (.y vec) (.z vec)]) vs)]) vs)))

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

(defn- gen-square [outline? filled?]
  (let [corners (for [x [-1.0 1.0]
                      y [-1.0 1.0]]
                  [x y 0.0])]
    (concat
      (if outline?
        [[GL/GL_LINES (map #(nth corners %) [0 2 2 3 3 1 1 0])]]
        [])
      (if filled?
        [[GL/GL_TRIANGLES (map #(nth corners %) [0 2 1 2 3 1])]]
        []))))

(defn- gen-cube [outline? filled?]
  (let [square (vtx-add [0.0 0.0 1.0] (gen-square outline? filled?))
        mirror-sides (concat
                       square
                       (vtx-rot (AxisAngle4d. (Vector3d. 0 1 0) Math/PI) square))]
    (concat
      mirror-sides
      (vtx-rot (AxisAngle4d. (Vector3d. 0 1 0) (* 0.5 Math/PI)) mirror-sides)
      (vtx-rot (AxisAngle4d. (Vector3d. 1 0 0) (* 0.5 Math/PI)) mirror-sides))))

(defn- gen-circle [segs]
  [[GL/GL_LINES (reduce concat (partition 2 1 (map #(let [angle (* 2.0 Math/PI (/ (double %) segs))]
                                                     [(Math/cos angle) (Math/sin angle) 0.0]) (range (inc segs)))))]])

(defn- gen-arrow [sub-divs]
  (concat
    (vtx-add [90.0 0.0 0.0] (vtx-scale [14.0 6.0 6.0] (gen-cone sub-divs)))
    (vtx-add [15.0 0.0 0.0] (vtx-scale [85.0 1.0 1.0] (gen-line)))))

(defn- gen-vertex-buffer [vertices vtx-count]
  (let [vbuf  (->pos-vtx vtx-count)]
    (doseq [vertex vertices]
      (conj! vbuf vertex))
    (persistent! vbuf)))

; Transform tool manipulators

(def axis-rotation-radius 100.0)
(def screen-rotation-radius 120.0)

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
    :rot-screen #{}
    :scale-x #{}
    :scale-y #{}
    :scale-z #{}
    :scale-xy #{:scale-x :scale-y}
    :scale-xz #{:scale-x :scale-z}
    :scale-yz #{:scale-y :scale-z}
    :scale-uniform #{}))

(def manip-colors
  {:move-x colors/defold-red
   :move-y colors/defold-green
   :move-z colors/defold-blue
   :move-xy colors/defold-blue
   :move-xz colors/defold-green
   :move-yz colors/defold-red
   :move-screen colors/defold-blue
   :rot-x colors/defold-red
   :rot-y colors/defold-green
   :rot-z colors/defold-blue
   :rot-screen colors/defold-blue
   :scale-x colors/defold-red
   :scale-y colors/defold-green
   :scale-z colors/defold-blue
   :scale-xy colors/defold-blue
   :scale-xz colors/defold-green
   :scale-yz colors/defold-red
   :scale-uniform colors/defold-blue})

(defn- manip->color [manip active-manip hot-manip tool-active?]
  (let [hot (= manip hot-manip)
        active (or (= manip active-manip) (and tool-active? (contains? (manip->sub-manips active-manip) manip)))
        alpha (if (= manip :move-screen) 0.0 1.0)
        screen-manip-color colors/defold-turquoise]
    (cond
      hot (colors/alpha colors/defold-yellow alpha)
      active (colors/alpha colors/defold-orange alpha)
      true (case manip
             (:move-xy :move-xz :move-yz :scale-xy :scale-xz :scale-yz) (colors/alpha (manip manip-colors) 0.2)
             (:move-screen) (colors/alpha screen-manip-color 0.0)
             (:rot-screen :scale-uniform) (colors/alpha screen-manip-color 1.0)
             (manip manip-colors)))))

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
    :rot-y (AxisAngle4d. (Vector3d. 1.0 0.0 0.0) (- (* 0.5 (Math/PI))))
    :rot-z (AxisAngle4d.)
    :rot-screen (AxisAngle4d.)
    :scale-x (AxisAngle4d.)
    :scale-y (AxisAngle4d. (Vector3d. 0.0 0.0 1.0) (* 0.5 (Math/PI)))
    :scale-z (AxisAngle4d. (Vector3d. 0.0 1.0 0.0) (- (* 0.5 (Math/PI))))
    :scale-xy (AxisAngle4d.)
    :scale-xz (AxisAngle4d. (Vector3d. 1.0 0.0 0.0) (* 0.5 (Math/PI)))
    :scale-yz (AxisAngle4d. (Vector3d. 0.0 1.0 0.0) (- (* 0.5 (Math/PI))))
    :scale-uniform (AxisAngle4d.)))

(defn- manip-enabled? [manip active-manip tool-active?]
  (if (#{:rot-x :rot-y :rot-z :rot-xy :rot-xz :rot-yz :rot-screen} manip)
    true
    (if tool-active?
       (or (= manip active-manip)
           (and (#{:move-x :move-y :move-z} manip)
                (or (= active-manip :move-screen) (contains? (manip->sub-manips active-manip) manip)))
           (and (#{:scale-x :scale-y :scale-z} manip)
                (#{:scale-xy :scale-xz :scale-yz :scale-uniform} active-manip)))
       true)))

(let [move (gen-arrow 10)
      move-plane (vtx-add [65.0 65.0 0.0] (vtx-scale [7.0 7.0 1.0] (gen-square true true)))
      move-screen (concat
                    (vtx-scale [7.0 7.0 1.0] (gen-square true true))
                    (gen-point))
      rot (vtx-scale [axis-rotation-radius axis-rotation-radius 1.0] (gen-circle 64))
      rot-screen (vtx-scale [screen-rotation-radius screen-rotation-radius 1.0] (gen-circle 64))
      scale (vtx-add [85 0 0] (vtx-scale [7 7 7] (gen-cube false true)))
      scale-uniform (vtx-scale [7 7 7] (gen-cube false true))]
  (def ^:private manip->vertices
    {:move-x move :move-y move :move-z move
     :move-xy move-plane :move-xz move-plane :move-yz move-plane
     :scale-xy move-plane :scale-xz move-plane :scale-yz move-plane
     :move-screen move-screen
     :rot-x rot :rot-y rot :rot-z rot
     :rot-screen rot-screen
     :scale-x scale :scale-y scale :scale-z scale
     :scale-uniform scale-uniform}))

(defn- gen-manip-renderable [id manip manip-space manip-world-rotation ^Matrix4d manip-world-transform ^AxisAngle4d rotation vertices color ^Matrix4d inv-view]
  (let [vertices-by-mode (reduce (fn [m [mode vs]] (merge-with concat m {mode vs})) {} vertices)
        vertex-buffers (mapv (fn [[mode vs]]
                               (let [count (count vs)]
                                 [mode (gen-vertex-buffer vs count) count])) vertices-by-mode)
        rotation-mat (doto (Matrix4d.) (.set rotation))
        world-transform (Matrix4d. manip-world-transform)]
    (when (manip->screen? manip)
      (let [rotation (Matrix3d.)]
        (.get inv-view rotation)
        (.setRotation world-transform rotation)))
    (.mul world-transform rotation-mat)
    {:node-id id
     :selection-data manip
     :render-fn render-manips
     :user-data {:color color :vertex-buffers vertex-buffers :manip manip :manip-rotation (get-manip-rotation manip-space manip-world-rotation)}
     :world-transform world-transform}))

(def transform-tools
  {:move {:manips-fn manip-move-manips
          :manip-spaces #{:local :world}
          :label "Move"
          :filter-fn manip-movable?}
   :rotate {:manips-fn manip-rotate-manips
            :manip-spaces #{:local :world}
            :label "Rotate"
            :filter-fn manip-rotatable?}
   :scale {:manips-fn manip-scale-manips
           :manip-spaces #{:local}
           :label "Scale"
           :filter-fn manip-scalable?}})

(defn- supported-manips
  [active-tool node-ids]
  (let [manips-fn (get-in transform-tools [active-tool :manips-fn])]
    (->>  node-ids
          (map (comp set manips-fn))
          (reduce set/intersection))))

(defn supported-manip-spaces [active-tool]
  (get-in transform-tools [active-tool :manip-spaces]))

(defn- manip-world-transform [reference-renderable manip-space ^double scale-factor]
  (let [world-translation ^Vector3d (:world-translation reference-renderable)
        world-rotation ^Matrix3d (case manip-space
                                   :local (doto (Matrix3d.) (.set ^Quat4d (:world-rotation reference-renderable)))
                                   :world geom/Identity3d)]
    (Matrix4d. world-rotation world-translation scale-factor)))

(g/defnk produce-renderables [_node-id active-tool manip-space camera viewport selected-renderables active-manip hot-manip start-action]
  (if (not (contains? transform-tools active-tool))
    {}
    (let [tool (get transform-tools active-tool)
          filter-fn (:filter-fn tool)
          selected-renderables (filter #(filter-fn (:node-id %)) selected-renderables)]
      (if (empty? selected-renderables)
        {}
        (let [tool-active (not (nil? start-action))
              reference-renderable (last selected-renderables)
              world-translation (:world-translation reference-renderable)
              world-rotation (:world-rotation reference-renderable)
              scale (scale-factor camera viewport world-translation)
              world-transform (manip-world-transform reference-renderable manip-space scale)
              rotation-fn #(manip->rotation %)
              color-fn #(manip->color % active-manip hot-manip tool-active)
              filter-fn #(manip-enabled? % active-manip tool-active)
              vertices-fn #(manip->vertices %)
              manips (supported-manips active-tool (map :node-id selected-renderables))
              inv-view (doto (c/camera-view-matrix camera) (.invert))
              renderables (vec (map #(gen-manip-renderable _node-id % manip-space world-rotation world-transform (rotation-fn %) (vertices-fn %) (color-fn %) inv-view)
                                    (filter filter-fn manips)))]
          (reduce #(assoc %1 %2 renderables) {} [pass/manipulator pass/manipulator-selection]))))))

(defn- action->line [action]
  [(:world-pos action) (:world-dir action)])

(defn- action->manip-pos [action ^Matrix4d lead-transform manip manip-rotation proj-fn]
  (case manip
    :scale-uniform (:screen-pos action)
    (let [manip-dir (manip->normal manip manip-rotation)
          _ (.transform lead-transform manip-dir)
          _ (.normalize manip-dir)
          manip-pos (math/translation lead-transform)
          [action-pos action-dir] (action->line action)]
      (proj-fn action-pos action-dir (Point3d. manip-pos) manip-dir))))

(defn- manip->project-fn [manip camera viewport]
  (case manip
    (:move-x :move-y :move-z :scale-x :scale-y :scale-z) math/project-lines
    (:move-xy :move-xz :move-yz :move-screen :scale-xy :scale-xz :scale-yz) math/line-plane-intersection
    (:rot-x :rot-y :rot-z :rot-screen)
    (fn [pos dir manip-pos manip-dir]
      (let [scale (scale-factor camera viewport manip-pos)
            radius (* scale
                      (case manip
                        (:rot-x :rot-y :rot-z) axis-rotation-radius
                        :rot-screen screen-rotation-radius))]
        (math/project-line-circle pos dir manip-pos manip-dir radius)))
    :scale-uniform identity))

(defn- manip->apply-fn [manip-opts evaluation-context manip manip-pos original-values]
  (case manip
    (:move-x :move-y :move-z :move-xy :move-xz :move-yz :move-screen)
    (let [move-snap-fn (or (:move-snap-fn manip-opts) identity)]
      (fn [start-pos pos]
        (let [manip-delta (doto (Vector3d.) (.sub pos start-pos))
              snapped-delta (move-snap-fn manip-delta)]
          (for [{:keys [node-id parent-world-transform]} original-values
                :let [world->local (math/inverse parent-world-transform)
                      local-delta (math/transform-vector world->local snapped-delta)]]
            (manip-move evaluation-context node-id local-delta)))))
    (:rot-x :rot-y :rot-z :rot-screen)
    (fn [start-pos pos]
      (let [[start-dir dir] (map #(doto (Vector3d.) (.sub % manip-pos) (.normalize)) [start-pos pos])
            manip-rotation (math/from-to->quat start-dir dir)]
        (for [{:keys [node-id ^Quat4d world-rotation]} original-values
              :let [local-rotation (doto (Quat4d. world-rotation) (.conjugate) (.mul manip-rotation) (.mul world-rotation) (.normalize))]]
          (manip-rotate evaluation-context node-id local-rotation))))
    (:scale-x :scale-y :scale-z :scale-xy :scale-xz :scale-yz)
    (fn [start-pos pos]
      (let [start-delta (doto (Vector3d.) (.sub start-pos manip-pos))
            delta (doto (Vector3d.) (.sub pos manip-pos))
            div-fn (fn [v ^Double sv] (if (> (Math/abs sv) math/epsilon) (/ v sv) 1.0))
            scale-factor (Vector3d.
                           (case manip
                             (:scale-x :scale-xy :scale-xz)
                             (div-fn (.x delta) (.x start-delta))
                             1.0)
                           (case manip
                             (:scale-y :scale-xy :scale-yz)
                             (div-fn (.y delta) (.y start-delta))
                             1.0)
                           (case manip
                             (:scale-z :scale-xz :scale-yz)
                             (div-fn (.z delta) (.z start-delta))
                             1.0))]
        (for [{:keys [node-id]} original-values]
          (manip-scale evaluation-context node-id scale-factor))))
    :scale-uniform
    (fn [^Vector3d start-pos ^Vector3d pos]
      (let [factor (+ 1 (* 0.02 (- (.x pos) (.x start-pos))))
            s (Vector3d. factor factor factor)]
        (for [{:keys [node-id]} original-values]
          (manip-scale evaluation-context node-id s))))))

(defn- apply-manipulator [manip-opts evaluation-context original-values manip manip-space start-action prev-action action camera viewport]
  (let [{:keys [world-rotation world-transform]} (peek original-values)
        manip-origin (math/translation world-transform)
        manip-rotation (get-manip-rotation manip-space world-rotation)
        lead-transform (if (or (manip->screen? manip) (= manip :scale-uniform))
                         (doto (c/camera-view-matrix camera) (.invert) (.setTranslation manip-origin))
                         (doto (Matrix4d.) (.set manip-origin)))]
    (let [proj-fn (manip->project-fn manip camera viewport)
          apply-fn (manip->apply-fn manip-opts evaluation-context manip manip-origin original-values)
          [start-pos pos] (map #(action->manip-pos % lead-transform manip manip-rotation proj-fn) [start-action action])]
      (apply-fn start-pos pos))))

(def ^:private original-values #(select-keys % [:node-id :world-rotation :world-transform :parent-world-transform]))

(defn handle-input [self action selection-data]
  (case (:type action)
    :mouse-pressed (if-let [manip (first (get selection-data self))]
                     (let [evaluation-context   (g/make-evaluation-context)
                           active-tool          (g/node-value self :active-tool evaluation-context)
                           tool                 (get transform-tools active-tool)
                           filter-fn            (:filter-fn tool)
                           selected-renderables (filter #(filter-fn (:node-id %)) (g/node-value self :selected-renderables evaluation-context))
                           original-values      (mapv original-values selected-renderables)]
                       (when (not (empty? original-values))
                         (g/transact
                            (concat
                              (g/set-property self :start-action action)
                              (g/set-property self :prev-action action)
                              (g/set-property self :original-values original-values)
                              (g/set-property self :initial-evaluation-context (atom evaluation-context))
                              (g/set-property self :active-manip manip)
                              (g/set-property self :hot-manip nil)
                              (g/set-property self :op-seq (gensym)))))
                       nil)
                     action)
    :mouse-released (if (g/node-value self :start-action)
                      (do
                        (g/transact
                          (concat
                            (g/set-property self :start-action nil)
                            (g/set-property self :prev-action nil)
                            (g/set-property self :original-values nil)))
                        nil)
                      action)
    :mouse-moved (if-let [start-action (g/node-value self :start-action)]
                   (let [prev-action     (g/node-value self :prev-action)
                         active-tool     (g/node-value self :active-tool)
                         original-values (g/node-value self :original-values)
                         manip           (g/node-value self :active-manip)
                         manip-opts      (g/node-value self :manip-opts)
                         manip-space     (g/node-value self :manip-space)
                         op-seq          (g/node-value self :op-seq)
                         camera          (g/node-value self :camera)
                         viewport        (g/node-value self :viewport)
                         evaluation-context @(g/node-value self :initial-evaluation-context)]
                     (g/transact
                       (concat
                         (g/operation-label (get-in transform-tools [active-tool :label]))
                         (g/operation-sequence op-seq)
                         (apply-manipulator manip-opts evaluation-context original-values manip manip-space start-action prev-action action camera viewport)))
                     (g/transact (g/set-property self :prev-action action))
                     nil)
                   (let [manip (first (get selection-data self))]
                     (when (not= manip (g/node-value self :hot-manip))
                       (g/transact (g/set-property self :hot-manip manip)))
                     action))
    action))

(defn move-whole-pixels? [prefs]
  (prefs/get prefs [:scene :move-whole-pixels]))

(defn set-move-whole-pixels! [prefs enabled]
  {:pre [(boolean? enabled)]}
  (prefs/set! prefs [:scene :move-whole-pixels] enabled))

(defn move-snap-fn [prefs]
  (if (move-whole-pixels? prefs)
    math/round-vector
    identity))

(g/defnk produce-manip-opts [prefs]
  {:move-snap-fn (move-snap-fn prefs)})

(g/defnk produce-manip-space [active-tool manip-space]
  (let [supported-manip-spaces (supported-manip-spaces active-tool)]
    (if (contains? supported-manip-spaces manip-space)
      manip-space
      (first supported-manip-spaces))))

(g/defnode ToolController
  (property prefs g/Any)
  (property start-action g/Any)
  (property prev-action g/Any)
  (property original-values g/Any)
  (property initial-evaluation-context g/Any)

  (property hot-manip g/Any)
  (property active-manip g/Any)
  (property op-seq g/Any)

  (input active-tool g/Keyword)
  (input manip-space g/Keyword)
  (input camera g/Any)
  (input viewport g/Any)
  (input selected-renderables g/Any)

  (output renderables pass/RenderData :cached produce-renderables)
  (output input-handler Runnable :cached (g/constantly handle-input))
  (output info-text g/Str (g/constantly nil))
  (output manip-opts g/Any produce-manip-opts)
  (output manip-space g/Keyword produce-manip-space))
