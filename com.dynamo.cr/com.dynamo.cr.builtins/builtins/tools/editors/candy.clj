(ns editors.candy
  (:require [clojure.edn :as edn] 
            [clojure.pprint :refer [pprint]]
            [plumbing.core :refer [fnk defnk]]
            [schema.core :as s]
            [dynamo.background :as background]
            [dynamo.buffers :refer :all]
            [dynamo.camera :refer :all]
            [dynamo.editors :as ed]
            [dynamo.file :as file]
            [dynamo.file.protobuf :as protobuf :refer [pb->str]]
            [dynamo.geom :as g]
            [dynamo.gl :as gl]
            [dynamo.gl.shader :as shader]
            [dynamo.gl.texture :as texture]
            [dynamo.gl.vertex :as vtx]
            [dynamo.grid :as grid]
            [dynamo.image :refer :all]
            [dynamo.node :as n]
            [dynamo.project :as p]
            [dynamo.property :as dp]
            [dynamo.selection :as sel]
            [dynamo.system :as ds]
            [internal.ui.scene-editor :as ius]
            [dynamo.texture :as tex]
            [dynamo.types :as t :refer :all]
            [dynamo.ui :refer :all]
            [internal.render.pass :as pass])
  (:import  [com.jogamp.opengl.util.awt TextRenderer]
            [dynamo.types Animation Camera Image TextureSet Rect AABB]
            [javax.media.opengl GL GL2]
            [javax.media.opengl.glu GLU]))

(set! *warn-on-reflection* true)

(defn load-level [str]
  (edn/read-string str))

(def test-level
  {:width 2
   :height 2
   :game-mode "foo"
   :blocks {[0 0] "red_candy"
            [0 1] "blue_candy"
            [1 0] "yellow_candy"
            [1 1] "red_candy"}})

(defn- get-image-color [image]
  (case image
        "red_candy" [0.8 0 0 1]
        "blue_candy" [0 0 0.8 1]
        "yellow_candy" [0.8 0.8 0 1]
        [0.8 0.8 0.8 1]))

(defn- get-selected-image-color [image]
  (case image
        "red_candy" [1.0 0 0 1]
        "blue_candy" [0 0 1.0 1]
        "yellow_candy" [1.0 1.0 0 1]
        [1.0 1.0 1.0 1]))

(defn- render-quad [^GL2 gl x y size]
  (gl/gl-quads gl
               (gl/gl-color-3fv (gl/color (* x 16) 255 0) 0)
               (gl/gl-vertex-2f (- x size) (- y size))
               (gl/gl-vertex-2f (- x size) (+ y size))
               (gl/gl-vertex-2f (+ x size) (+ y size))
               (gl/gl-vertex-2f (+ x size) (- y size))))

(shader/defshader vertex-shader
  (attribute vec4 position)
  (defn void main []
    (setq gl_Position (* gl_ModelViewProjectionMatrix position))))

(shader/defshader fragment-shader
  (uniform vec4 color)
  (defn void main []
    (setq gl_FragColor color)))

(def shader (shader/make-shader vertex-shader fragment-shader))

(defn- frange [n step]
  (map #(* step %) (range n)))

(def palette-columns 2)
(def group-spacing 5)
(def cell-size 10)
(def cell-size-half (/ cell-size 2.0))

(def palette
  [{:name "Blocks"
    :images ["red_candy" "blue_candy" "yellow_candy"]}
   {:name "Misc"
    :images ["red_fish" "blue_fish"]}])

(defn- hit? [cell world-pos]
  (let [xc (:x cell)
        yc (:y cell)
        d cell-size-half]
    (when (and (<= (- xc d) (.x world-pos) (+ xc d))
               (<= (- yc d) (.y world-pos) (+ yc d)))
      cell)))

(defn- layout-row [row y]
  (let [column-f (fn [idx image] { :x (* cell-size idx) :y y :image image})]
    (map-indexed column-f row)))

(defn- layout-group [group y]
  (let [row-f (fn [idx row] (layout-row row (- y (* idx cell-size))))
        images (:images group)
        rows (partition-all palette-columns images)]
    {:name (:name group)
     :x 0 :y y
     :height (* cell-size (count rows))
     :cells (flatten (map-indexed row-f rows))}))

(defn- layout-palette [groups y]
  (when (seq groups)
    (let [g' (layout-group (first groups) y)]
      (conj (layout-palette (rest groups) 
                    (- y group-spacing (:height g'))) g'))))

(defn render-text
  [ctx ^GL2 gl ^TextRenderer text-renderer ^String chars ^Float xloc ^Float yloc ^Float scalex ^Float scaley]
  (gl/gl-push-matrix gl
    (.setColor text-renderer 1 1 1 1)
    (.begin3DRendering text-renderer)
    (.draw3D text-renderer chars xloc yloc scalex scaley)
    (.end3DRendering text-renderer)))

(defn layout-level [level]
  (let [left 40
        width (:width level)
        height (:height level)]
    (for [i (range width)
          j (range height)
          :let [x (+ 40 (* i cell-size))
                y (- (* j cell-size))
                idx [i j]]]
      {:x x :y y :idx idx :image ((:blocks level) idx)})))

(defn render-cells [^GL2 gl cells active-brush]
  (gl/with-enabled gl [shader]
    (doseq [cell cells]
      (let [color (if (= active-brush (:image cell)) (get-selected-image-color (:image cell)) (get-image-color (:image cell)))]
        (shader/set-uniform shader gl "color" (float-array color))
        (render-quad gl (+ 0 (:x cell)) (:y cell) (/ cell-size-half 1.1))))))

(defn render-palette [ctx ^GL2 gl text-renderer groups active-brush]
  (doseq [group groups]
    (render-text ctx gl text-renderer (:name group) (- (:x group) cell-size-half) (+ cell-size-half (:y group)) 0 0.2)
    (render-cells gl (:cells group) active-brush)))

(defn render-level [^GL2 gl level textureset]
  (render-cells gl (layout-level level) nil))

(n/defnode CandyRender
  (input level s/Any)
  (input textureset s/Any)
  (input active-brush s/Str)
  (output renderable t/RenderData 
          (fnk [this level textureset active-brush]
               {pass/overlay [{:world-transform g/Identity4d 
                               :render-fn (fn [ctx gl glu text-renderer]
                                            (render-palette ctx gl text-renderer (layout-palette palette 0) active-brush))}]
                pass/transparent [{:world-transform g/Identity4d 
                                   :render-fn (fn [ctx gl glu text-renderer] 
                                                (render-level gl level textureset)
                                                (render-palette ctx gl text-renderer (layout-palette palette 0) active-brush))}]})))

(def prev-move-event (atom {:x 0 :y 0}))

(n/defnode CandyNode
  (inherits n/OutlineNode)
  (input camera s/Any)
  (property level s/Any)
  (property width s/Int)
  (property height s/Int)
  (property active-brush s/Str (default "red_candy"))  
  
  (on :mouse-move
      (reset! prev-move-event event))
  (on :mouse-down
      (let [camera (first (n/get-node-inputs self :camera))
            world-pos (camera-unproject camera (:x event) (:y event) 0)
            level (:level self)
            palette-cells (mapcat :cells (layout-palette palette 0))
            palette-hit (some #(hit? % world-pos) palette-cells)
            level-cells (layout-level level)
            level-hit (some #(hit? % world-pos) level-cells)]
        (when palette-hit
          (ds/set-property self :active-brush (:image palette-hit)))
        (when level-hit
          (ds/set-property self :level (assoc-in level [:blocks (:idx level-hit)] (:active-brush self))))))
  (on :load
      (let [project (:project event)
            level (load-level (slurp (:filename self)))]
        (ds/set-property self :width (:width level))
        (ds/set-property self :height (:height level))
        (ds/set-property self :level level))))

(defn broadcast-event [this event]
  (let [[controllers] (n/get-node-inputs this :controllers)]
    (doseq [controller controllers]
      (t/process-one-event controller event))))

(n/defnode BroadcastController
  (input controllers [s/Any])
  (on :mouse-down (broadcast-event self event))
  (on :mouse-up (broadcast-event self event))
  (on :mouse-double-click (broadcast-event self event))
  (on :mouse-enter (broadcast-event self event))
  (on :mouse-exit (broadcast-event self event))
  (on :mouse-hover (broadcast-event self event))
  (on :mouse-move (broadcast-event self event))
  (on :mouse-wheel (broadcast-event self event))
  (on :key-down (broadcast-event self event))
  (on :key-up (broadcast-event self event)))

(defn on-edit
  [project-node editor-site candy-node]
  (let [editor (n/construct ed/SceneEditor :name "editor")]
    (ds/in (ds/add editor)
      (let [candy-render   (ds/add (n/construct CandyRender))
            controller     (ds/add (n/construct BroadcastController))
            background     (ds/add (n/construct background/Gradient))
            grid           (ds/add (n/construct grid/Grid))
            camera         (ds/add (n/construct CameraController :camera (make-camera :orthographic)))
            atlas-node      (t/lookup project-node "/candy/fish.atlas")]
        (ds/connect camera         :camera      grid           :camera)
        (ds/connect camera         :camera      editor         :view-camera)
        (ds/connect camera         :camera      candy-node     :camera)
        (ds/connect camera         :self        controller     :controllers)
        (ds/connect candy-node     :self        controller     :controllers)
        (ds/connect controller     :self        editor         :controller)
        (ds/connect background     :renderable  editor         :renderables)
        (ds/connect grid           :renderable  editor         :renderables)
        (ds/connect candy-render   :renderable  editor         :renderables)
        (ds/connect candy-node     :level       candy-render   :level)
        (ds/connect candy-node     :active-brush candy-render   :active-brush)
        (ds/connect atlas-node     :textureset  candy-render   :textureset))
      editor)))

(when (ds/in-transaction?)
  (p/register-editor "level" #'on-edit)
  (p/register-node-type "level" CandyNode))
