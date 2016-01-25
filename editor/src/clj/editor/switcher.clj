(ns editor.switcher
  (:require [clojure.edn :as edn]
            [clojure.java.io :as io]
            [clojure.pprint :refer [pprint]]
            [dynamo.graph :as g]
            [editor.camera :as c]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.shader :as shader]
            [editor.gl.vertex :as vtx]
            [editor.project :as project]
            [editor.types :as types]
            [editor.workspace :as workspace]
            [editor.scene :as scene]
            [editor.gl.pass :as pass])
  (:import [com.dynamo.graphics.proto Graphics$Cubemap Graphics$TextureImage Graphics$TextureImage$Image Graphics$TextureImage$Type]
           [com.jogamp.opengl.util.awt TextRenderer]
           [editor.types Region Animation Camera Image TexturePacking Rect EngineFormatTexture AABB TextureSetAnimationFrame TextureSetAnimation TextureSet]
           [java.awt.image BufferedImage]
           [java.io PushbackReader]
           [javax.media.opengl GL GL2 GLContext GLDrawableFactory]
           [javax.media.opengl.glu GLU]
           [javax.vecmath Matrix4d Point3d]))

(def switcher-icon "icons/board_game.png")

; Config

(def palette-columns 2)
(def group-spacing 20)
(def cell-size 10)
(def cell-size-half (/ cell-size 2.0))
(def palette-cell-size 80)
(def palette-cell-size-half (/ palette-cell-size 2.0))

(def switcher-atlas-file "/switcher/switcher.atlas")
(def candy-colors ["red" "blue" "yellow" "green" "orange" "purple"])

(def palette
  [{:name "Regular"
    :images (map (fn [color] (str color "_candy")) candy-colors)}
   {:name "Striped"
    :images (map (fn [color] (str color "_polka_horisontal")) candy-colors)}
   #_DEMO
   #_{:name "Drop"
     :images ["cherry" "hazelnut"]}])

; Render assets

(vtx/defvertex texture-vtx
  (vec4 position)
  (vec2 texcoord0)
  (vec4 color))

(shader/defshader vertex-shader
  (attribute vec4 position)
  (attribute vec2 texcoord0)
  (attribute vec4 color)
  (varying vec2 var_texcoord0)
  (varying vec4 var_color)
  (defn void main []
    (setq gl_Position (* gl_ModelViewProjectionMatrix position))
    (setq var_texcoord0 texcoord0)
    (setq var_color color)))

(shader/defshader fragment-shader
  (varying vec2 var_texcoord0)
  (uniform sampler2D texture)
  (varying vec4 var_color)
  (defn void main []
    (setq gl_FragColor (* var_color (texture2D texture var_texcoord0.xy)))
    #_(setq gl_FragColor (vec4 var_texcoord0.xy 0 1))
    ))

; TODO - macro of this
(def shader (shader/make-shader ::shader vertex-shader fragment-shader))

; Layout

(defn- layout-row [row x y cell-size]
  (let [column-f (fn [idx image] { :x (+ x (* cell-size idx)) :y y :image image})]
    (map-indexed column-f row)))

(defn- layout-group [group x y cell-size]
  (let [row-f (fn [idx row] (layout-row row x (+ y (* idx cell-size)) cell-size))
        images (:images group)
        rows (partition-all palette-columns images)]
    {:name (:name group)
     :x x :y y
     :height (* cell-size (count rows))
     :cells (flatten (map-indexed row-f rows))}))

(defn- layout-palette
  ([groups]
    (layout-palette groups (+ (* 0.5 group-spacing) palette-cell-size-half) (+ group-spacing palette-cell-size-half)))
  ([groups x y]
    (when (seq groups)
      (let [g' (layout-group (first groups) x y palette-cell-size)]
        (conj (layout-palette (rest groups) x
                              (+ y group-spacing (:height g'))) g')))))

(defn layout-level [level]
  (let [width (:width level)
        height (:height level)
        offset-x (* (- 1 width) cell-size 0.5)
        offset-y (* (- 1 height) cell-size 0.5)]
    (for [i (range width)
          j (range height)
          :let [x (+ offset-x (* i cell-size))
                y (- (+ offset-y (* j cell-size)))
                idx [i j]]]
      {:x x :y y :idx idx :image ((:blocks level) idx)})))

; Rendering

(defn render-palette [^GL2 gl render-args gpu-texture vertex-binding layout]
  (doseq [group layout]
    (scene/overlay-text gl (:name group) (- (:x group) palette-cell-size-half) (+ (* 0.25 group-spacing) (- palette-cell-size-half (:y group)))))
  (let [cell-count (reduce (fn [v0 v1] (+ v0 (count (:cells v1)))) 0 layout)]
   (gl/with-gl-bindings gl render-args [gpu-texture shader vertex-binding]
    (shader/set-uniform shader gl "texture" 0)
    (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 (* 6 cell-count)))))

(defn render-level [^GL2 gl render-args level gpu-texture vertex-binding layout]
  (let [cell-count (count layout)]
    (gl/with-gl-bindings gl render-args [gpu-texture shader vertex-binding]
      (shader/set-uniform shader gl "texture" 0)
      (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 (* 6 cell-count)))))

; Vertex generation

(defn anim-uvs [anim-data id]
  (let [anim (get anim-data id)
        frame (first (:frames anim))]
    (:tex-coords frame)))

(defn flip [v0 v1]
  [v1 v0])

(defn gen-vertex [x y u v color]
  (doall (concat [x y 0 1 u v] color)))

(defn gen-quad [anim-data cell size-half flip-vert]
  (let [x (:x cell)
        y (:y cell)
        x0 (- x size-half)
        y0 (- y size-half)
        x1 (+ x size-half)
        y1 (+ y size-half)
        [[u0 v0] [u1 v1] [u2 v2] [u3 v3]] (anim-uvs anim-data (:image cell))
        color (if (:color cell) (:color cell) [1 1 1 1])]
    [(gen-vertex x0 y0 u0 v0 color)
     (gen-vertex x1 y0 u3 v3 color)
     (gen-vertex x0 y1 u1 v1 color)
     (gen-vertex x1 y0 u3 v3 color)
     (gen-vertex x1 y1 u2 v2 color)
     (gen-vertex x0 y1 u1 v1 color)]))

(defn cells->quads [anim-data cells size-half flip-vert]
  (mapcat (fn [cell] (gen-quad anim-data cell size-half flip-vert)) cells))

(defn gen-palette-vertex-buffer
  [anim-data layout size-half active-brush]
  (let [cell-count (reduce (fn [v0 v1] (+ v0 (count (:cells v1)))) 0 layout)
        vbuf  (->texture-vtx (* 6 cell-count))]
    (doseq [group layout]
      (doseq [vertex (cells->quads anim-data (map (fn [cell] (if (= active-brush (:image cell)) cell (assoc cell :color [0.9 0.9 0.9 0.6]))) (:cells group)) palette-cell-size-half false)]
        (conj! vbuf vertex)))
    (persistent! vbuf)))

(def active-cell (atom {}))

(defn gen-level-vertex-buffer
  [anim-data layout size-half]
  (let [cell-count (count layout)
        vbuf  (->texture-vtx (* 6 cell-count))]
    (doseq [vertex (cells->quads anim-data layout cell-size-half true)]
      (conj! vbuf vertex))
    (persistent! vbuf)))

; Node defs

(g/defnk produce-controller-renderable
  [this level gpu-texture palette-vertex-binding level-vertex-binding active-brush palette-layout level-layout]
  {pass/overlay [{:world-transform geom/Identity4d
                  :render-fn (fn [gl render-args renderables count]
                               (render-palette gl render-args gpu-texture palette-vertex-binding palette-layout))}]
   pass/transparent [{:world-transform geom/Identity4d
                   :render-fn (fn [gl render-args renderables count]
                                (render-level gl render-args level gpu-texture level-vertex-binding level-layout))}]})

(defn- hit? [cell pos cell-size-half]
  (let [xc (:x cell)
        yc (:y cell)
        d cell-size-half]
    (when (and (<= (- xc d) (:x pos) (+ xc d))
               (<= (- yc d) (:y pos) (+ yc d)))
      cell)))

(defn handle-input [self action source camera viewport]
  (case (:type action)
    :mouse-moved
    (let [pos           {:x (:x action) :y (:y action)}
          world-pos-v4  (c/camera-unproject camera viewport (:x action) (:y action) 0)
          world-pos     {:x (.x world-pos-v4) :y (.y world-pos-v4)}
          level         (g/node-value self :level)
          palette-cells (mapcat :cells (layout-palette palette))
          palette-hit   (some #(hit? % pos palette-cell-size-half) palette-cells)
          level-cells   (layout-level level)
          level-hit     (if palette-hit nil (some #(hit? % world-pos cell-size-half) level-cells))]
      (reset! active-cell level-hit)
      (when-not active-cell
        action))
    :mouse-pressed
    (let [pos           {:x (:x action) :y (:y action)}
          world-pos-v4  (c/camera-unproject camera viewport (:x action) (:y action) 0)
          world-pos     {:x (.x world-pos-v4) :y (.y world-pos-v4)}
          level         (g/node-value self :level)
          palette-cells (mapcat :cells (layout-palette palette))
          palette-hit   (some #(hit? % pos palette-cell-size-half) palette-cells)
          level-cells   (layout-level level)
          level-hit     (if palette-hit nil (some #(hit? % world-pos cell-size-half) level-cells))]
      (cond
        palette-hit
        (g/transact
         [(g/operation-label "Select Brush")
          (g/set-property self :active-brush (:image palette-hit))])

        level-hit
        (g/transact
         [(g/operation-label "Paint Cell")
          (g/set-property self :level (assoc-in level [:blocks (:idx level-hit)] (g/node-value self :active-brush)))])
        :default
        action))
    action))

(g/defnode SwitcherController
 (property active-brush g/Str (default "red_candy"))

 (input source   g/Any)
 (input camera   g/Any)
 (input viewport Region)

 (output input-handler Runnable     (g/fnk [source camera viewport] (fn [self action] (handle-input self action source camera viewport))))
 (output renderable    pass/RenderData produce-controller-renderable))

(g/defnk produce-save-data [resource level]
  {:resource resource
   :content (with-out-str (pprint level))})

(g/defnk produce-scene
   [_node-id aabb level gpu-texture anim-data]
   (let [level-layout (layout-level level)
         level-vertex-binding (vtx/use-with ::switcher (gen-level-vertex-buffer anim-data level-layout cell-size-half) shader)]
     {:id _node-id
      :aabb aabb
      :renderable {:render-fn (fn [gl render-args renderables count] (render-level gl render-args level gpu-texture level-vertex-binding level-layout))
                   :passes [pass/transparent]}}))

(g/defnode SwitcherNode
  (inherits project/ResourceNode)

  (property blocks       g/Any (dynamic visible (g/always false)))
  (property width        g/Int (default 1))
  (property height       g/Int (default 1))

  (input anim-data g/Any)
  (input gpu-texture g/Any)

  (output level g/Any (g/fnk [blocks width height] {:width width :height height :blocks blocks}))
  (output aabb AABB (g/fnk [width height]
                           (let [half-width (* 0.5 cell-size width)
                                 half-height (* 0.5 cell-size height)]
                             (types/->AABB (Point3d. (- half-width) (- half-height) 0)
                                       (Point3d. half-width half-height 0)))))
  (output save-data g/Any :cached produce-save-data)
  (output scene     g/Any :cached produce-scene))

(defn load-level [project self resource]
  (with-open [reader (PushbackReader. (io/reader resource))]
    (let [level      (edn/read reader)
          atlas-resource (workspace/resolve-resource resource switcher-atlas-file)]
      (concat
        (g/set-property self :width (:width level))
        (g/set-property self :height (:height level))
        (g/set-property self :blocks (:blocks level))
        (project/connect-resource-node project
                                       atlas-resource self
                                       [[:anim-data :anim-data]
                                        [:gpu-texture :gpu-texture]])))))

(defn register-resource-types [workspace]
  (workspace/register-resource-type workspace
                                    :ext "switcher"
                                    :node-type SwitcherNode
                                    :load-fn load-level
                                    :icon switcher-icon
                                    :view-types [:scene]))
