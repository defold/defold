(ns editor.render
  (:require [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.shader :as shader]
            [editor.gl.vertex :as vtx]
            [editor.types :as types]
            [editor.scene :as scene]
            [editor.gl.pass :as pass])
  (:import [editor.types AABB]
           [javax.vecmath Point3d]))

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

(defn- gen-outline-vertex [x y z cr cg cb]
  [x y z cr cg cb 1])

(defn- conj-outline-quad! [vbuf ^AABB aabb cr cg cb]
  (let [min (types/min-p aabb)
        max (types/max-p aabb)
        x0 (.x min)
        y0 (.y min)
        x1 (.x max)
        y1 (.y max)
        z (+ (* 0.5 (- (.z max) (.z min))) (.z min))
        v0 (gen-outline-vertex x0 y0 z cr cg cb)
        v1 (gen-outline-vertex x1 y0 z cr cg cb)
        v2 (gen-outline-vertex x1 y1 z cr cg cb)
        v3 (gen-outline-vertex x0 y1 z cr cg cb)]
    (-> vbuf (conj! v0) (conj! v1) (conj! v1) (conj! v2) (conj! v2) (conj! v3) (conj! v3) (conj! v0))))

(def outline-color (scene/select-color pass/outline false [1.0 1.0 1.0]))
(def selected-outline-color (scene/select-color pass/outline true [1.0 1.0 1.0]))

(defn- renderable->quad! [vbuf renderable]
  (let [color (if (:selected renderable) selected-outline-color outline-color)
        [cr cg cb _] color
        aabb (:aabb renderable)]
    (conj-outline-quad! vbuf aabb cr cg cb)))

(defn gen-outline-vb [renderables count]
  (let [vbuf (->vtx-pos-col (* count 8))]
    (persistent! (reduce renderable->quad! vbuf renderables))))
