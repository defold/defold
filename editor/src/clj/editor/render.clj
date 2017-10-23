(ns editor.render
  (:require [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.shader :as shader]
            [editor.gl.vertex :as vtx]
            [editor.types :as types]
            [editor.scene :as scene]
            [editor.gl.pass :as pass])
  (:import [com.jogamp.opengl GL GL2]
           [editor.types AABB]
           [javax.vecmath Point3d]))

(set! *warn-on-reflection* true)

(vtx/defvertex vtx-pos-tex-col
  (vec3 position)
  (vec2 texcoord0)
  (vec4 color))

(shader/defshader shader-ver-tex-col
  (attribute vec4 position)
  (attribute vec2 texcoord0)
  (attribute vec4 color)
  (varying vec2 var_texcoord0)
  (varying vec4 var_color)
  (defn void main []
    (setq gl_Position (* gl_ModelViewProjectionMatrix position))
    (setq var_texcoord0 texcoord0)
    (setq var_color color)))

(shader/defshader shader-frag-tex-tint
  (varying vec2 var_texcoord0)
  (varying vec4 var_color)
  (uniform sampler2D texture)
  (defn void main []
    (setq gl_FragColor (* var_color (texture2D texture var_texcoord0.xy)))
    (setq gl_FragColor (vec4 var_texcoord0.x var_texcoord0.y 0 1.0))
    (setq gl_FragColor (texture2D texture var_texcoord0.xy))))

(def shader-tex-tint (shader/make-shader ::shader shader-ver-tex-col shader-frag-tex-tint))

(vtx/defvertex vtx-pos-col
  (vec3 position)
  (vec4 color))

(shader/defshader shader-ver-outline
  (attribute vec4 position)
  (attribute vec4 color)
  (varying vec4 var_color)
  (defn void main []
    (setq gl_Position (* gl_ModelViewProjectionMatrix position))
    (setq var_color color)))

(shader/defshader shader-frag-outline
  (varying vec4 var_color)
  (defn void main []
    (setq gl_FragColor var_color)))

(def shader-outline (shader/make-shader ::shader-outline shader-ver-outline shader-frag-outline))

(def outline-color (scene/select-color pass/outline false [1.0 1.0 1.0]))
(def selected-outline-color (scene/select-color pass/outline true [1.0 1.0 1.0]))

(def ^:private line-order
  [[5 4]
   [5 0]
   [4 7]
   [7 0]
   [6 5]
   [1 0]
   [3 4]
   [2 7]
   [6 3]
   [6 1]
   [3 2]
   [2 1]])

(defn- conj-aabb-lines!
  [vbuf ^AABB aabb cr cg cb]
  (let [min (types/min-p aabb)
        max (types/max-p aabb)
        x0 (.x min)
        y0 (.y min)
        z0 (.z min)
        x1 (.x max)
        y1 (.y max)
        z1 (.z max)
        vs [[x1 y1 z1 cr cg cb 1.0]
            [x1 y1 z0 cr cg cb 1.0]
            [x1 y0 z0 cr cg cb 1.0]
            [x0 y0 z0 cr cg cb 1.0]
            [x0 y0 z1 cr cg cb 1.0]
            [x0 y1 z1 cr cg cb 1.0]
            [x0 y1 z0 cr cg cb 1.0]
            [x1 y0 z1 cr cg cb 1.0]]]
    (reduce (fn [vbuf [v0 v1]]
              (-> vbuf
                  (conj! (nth vs v0))
                  (conj! (nth vs v1))))
            vbuf
            line-order)))

(defn- renderable->aabb-box!
  [vbuf renderable]
  (let [color (if (:selected renderable) selected-outline-color outline-color)
        [cr cg cb _] color
        aabb (:aabb renderable)]
    (conj-aabb-lines! vbuf aabb cr cg cb)))

(defn gen-outline-vb [renderables n]
  (let [vbuf (->vtx-pos-col (* n 2 (count line-order)))]
    (persistent! (reduce renderable->aabb-box! vbuf renderables))))

(defn render-aabb-outline
  [^GL2 gl render-args request-id renderables n]
  (let [vbuf (gen-outline-vb renderables n)
        outline-vertex-binding (vtx/use-with request-id vbuf shader-outline)]
    (gl/with-gl-bindings gl render-args [shader-outline outline-vertex-binding]
      (gl/gl-draw-arrays gl GL/GL_LINES 0 (count vbuf)))))
