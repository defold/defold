(ns editor.rulers
  (:require [dynamo.graph :as g]
            [editor.camera :as c]
            [editor.colors :as colors]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.shader :as shader]
            [editor.gl.vertex :as vtx]
            [editor.gl.pass :as pass]
            [editor.math :as math]
            [editor.colors :as colors]
            [editor.scene-cache :as scene-cache]
            [editor.scene-text :as scene-text]
            [editor.types :as types])
  (:import  [javax.media.opengl GL GL2]
            [javax.vecmath Point3d]
            [editor.types Camera]))

(set! *warn-on-reflection* true)

(def ^:private width 35)
(def ^:private height 15)

(def ^:private horizontal-label-spacing 45)
(def ^:private vertical-label-spacing 30)
(def ^:private tick-length 11)

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

(defn render-lines [^GL2 gl render-args renderables rcount]
  (let [camera (:camera render-args)
        viewport (:viewport render-args)]
    (doseq [renderable renderables
            :let [lines (get-in renderable [:user-data :lines])
                  tris (get-in renderable [:user-data :tris])
                  texts (get-in renderable [:user-data :texts])]]
      (when tris
        (let [vcount (count tris)
              vertex-binding (vtx/use-with ::tris tris line-shader)]
          (gl/with-gl-bindings gl render-args [line-shader vertex-binding]
            (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 vcount))))
      (when texts
        (doseq [[[x y] msg align] texts]
          (let [[w h] (scene-text/bounds gl msg)
                x (Math/round (double (if (= align :right) (- x w 2) (+ x 2))))
                y (Math/round (double (if (= align :top) (- (- y) h 2) (- 2 y))))]
            (scene-text/overlay gl msg x y))))
      (when lines
        (let [vcount (count lines)
              vertex-binding (vtx/use-with ::lines lines line-shader)]
          (gl/with-gl-bindings gl render-args [line-shader vertex-binding]
            (gl/gl-draw-arrays gl GL/GL_LINES 0 vcount)))))))

(defn- vb [vs vcount]
  (when (< 0 vcount)
    (let [vb (->color-vtx vcount)]
      (doseq [v vs]
        (conj! vb v))
      (persistent! vb))))

(defn- quad [x0 y0 x1 y1 color]
  (->> [[x0 y0 0] [x1 y0 0] [x1 y1 0]
        [x0 y0 0] [x0 y1 0] [x1 y1 0]]
    (mapv (fn [v] (reduce conj v color)))))

(defn- chain [vs0 vs1]
  (reduce conj vs0 vs1))

(defn- label-points [min max count screen-fn screen-filter-fn]
  (let [d (- max min)
        dist (/ d count)
        exp (Math/log10 dist)
        factor (Math/pow 10.0 (Math/round exp))
        enough? (> (* count factor) d)
        factor (if enough? factor (* 2.0 factor))
        enough? (> (* count factor) d)
        factor (if enough? factor (* 2.5 factor))
        start (/ min factor)
        offset (Math/ceil start)]
    (->> (for [i (range count)]
           (* (+ offset i) factor))
      (reduce (fn [res vw]
                (let [vs (screen-fn vw)]
                  (if (screen-filter-fn vs)
                    (conj res [vw vs])
                    res)))
              []))))

(defn- unproject [camera viewport x y]
  (let [p (c/camera-unproject camera viewport x y 0)]
    [(.x p) (.y p)]))

(g/defnk produce-renderables [camera viewport cursor-pos]
  (let [[min-x min-y] (unproject camera viewport 0 (:bottom viewport))
        [max-x max-y] (unproject camera viewport (:right viewport) 0)
        tmp-p (Point3d.)
        [cursor-x cursor-y] cursor-pos
        xs (label-points min-x max-x (Math/ceil (/ (:right viewport) horizontal-label-spacing))
                         #(-> (c/camera-project camera viewport (doto tmp-p (.setX %)))
                            (.x))
                         #(> % width))
        ys (label-points min-y max-y (Math/ceil (/ (:bottom viewport) vertical-label-spacing))
                         #(-> (c/camera-project camera viewport (doto tmp-p (.setY %)))
                            (.y))
                         #(> (- (:bottom viewport) height) %))
        line-vs (cond->
                  (->>
                    (let [x0 width
                          y0 0
                          x1 (:right viewport)
                          y1 (- (:bottom viewport) height)]
                      (-> [[x0 y0 0] [x0 y1 0]
                           [x0 y1 0] [x1 y1 0]]
                        (into (reduce (fn [res [xw xs]]
                                        (-> res
                                          (conj [xs y1 0])
                                          (conj [xs (:bottom viewport) 0])))
                                      [] xs))
                        (into (reduce (fn [res [yw ys]]
                                        (-> res
                                          (conj [0 (+ 1 ys) 0])
                                          (conj [width (+ 1 ys) 0])))
                                      [] ys))))
                    (mapv (fn [v] (reduce conj v colors/bright-grey))))
                  (and cursor-x (> cursor-x width)) (into (map #(reduce conj % colors/defold-red) [[cursor-x (- (:bottom viewport) height) 0] [cursor-x (:bottom viewport) 0]]))
                  (and cursor-y (< cursor-y (- (:bottom viewport) height))) (into (map #(reduce conj % colors/defold-green) [[0 cursor-y 0] [width cursor-y 0]])))
        line-vcount (count line-vs)
        lines-vb (vb line-vs line-vcount)
        tris-vs (-> (quad 0 0 width (:bottom viewport) colors/dark-black)
                  (chain (quad 0 (- (:bottom viewport) height) (:right viewport) (:bottom viewport) colors/dark-black)))
        tris-vcount (count tris-vs)
        tris-vb (vb tris-vs tris-vcount)
        texts (let [texts (-> []
                            (into (mapv (fn [[yw ys]] [[width ys] (format "%.1f" yw) :right]) ys))
                            (into (mapv (fn [[xw xs]] [[xs (- (:bottom viewport) height)] (format "%.1f" xw) :top]) xs)))]
                texts)
        renderables [{:render-fn render-lines
                      :batch-key nil
                      :user-data {:lines lines-vb
                                  :tris tris-vb
                                  :texts texts}}]]
    (into {} (map #(do [% renderables]) [pass/overlay]))))

(g/defnode Rulers
  (input camera Camera)
  (input viewport g/Any)
  (input cursor-pos types/Vec2)
  (output renderables pass/RenderData :cached produce-renderables))
