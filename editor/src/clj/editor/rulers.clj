(ns editor.rulers
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.camera :as c]
            [editor.colors :as colors]
            [editor.gl :as gl]
            [editor.gl.shader :as shader]
            [editor.gl.texture :as texture]
            [editor.gl.vertex2 :as vtx]
            [editor.gl.pass :as pass]
            [editor.image-util :as image-util]
            [editor.math :as math]
            [editor.types :as types])
  (:import  [com.jogamp.opengl GL GL2]
            [java.awt.image BufferedImage]
            [java.nio BufferOverflowException]
            [javax.vecmath Matrix4d Point3d]
            [editor.types Camera]))

(set! *warn-on-reflection* true)

(def ^:private width 15)
(def ^:private height 15)

(def ^:private horizontal-label-spacing 60)
(def ^:private vertical-label-spacing 50)
(def ^:private tick-length 11)

(def ^:private label-color colors/scene-ruler-label)
(def ^:private marker-color colors/scene-ruler-marker)

(defonce ^:private numbers-path "numbers.png")
(defonce ^:private ^BufferedImage numbers-image (image-util/read-image (io/resource numbers-path)))
(defonce ^:private img-dims [(.getWidth numbers-image) (.getHeight numbers-image)])
(defonce ^:private img-dims-recip (mapv #(/ 1 %) img-dims))
(defonce ^:private numbers-texture-params (merge texture/default-image-texture-params {:min-filter gl/nearest :mag-filter gl/nearest}))
(defonce ^:private numbers-texture (texture/image-texture ::number-texture numbers-image numbers-texture-params))

(def ^:private max-ticks 100)
(def ^:private max-label-chars 15)
(def ^:private vertex-buffer-size (+ (* 2 max-ticks) ;; tick lines
                                    (* 6 max-ticks max-label-chars) ;; labels
                                    (* 2 2) ;; cursor markers
                                    (* 6 2) ;; backgrounds
                                    (* 2 2) ;; borders
                                    ))

(defn- char->w ^long [c]
  (case c
    \. 3
    \- 5
    \1 5
    6))

(def ^:private characters "0123456789-.")
(def ^:private char->x (into {} (loop [xs []
                                       cs characters
                                       x 0]
                                  (if-let [c (first cs)]
                                    (recur (conj xs [c x]) (rest cs) (+ x (char->w c)))
                                    xs))))

(vtx/defvertex color-uv-vtx
  (vec2 position)
  (vec2 texcoord0)
  (vec4 color))

(shader/defshader tex-vertex-shader
  (attribute vec2 position)
  (attribute vec2 texcoord0)
  (attribute vec4 color)
  (varying vec4 var_color)
  (varying vec2 var_texcoord0)
  (defn void main []
    (setq var_texcoord0 texcoord0)
    (setq var_color color)
    (setq gl_Position (* gl_ModelViewProjectionMatrix (vec4 position 0.0 1.0)))))

(shader/defshader tex-fragment-shader
  (varying vec4 var_color)
  (varying vec2 var_texcoord0)
  (uniform sampler2D texture)
  (defn void main []
    (setq gl_FragColor (* (texture2D texture var_texcoord0.xy) var_color))))

(def tex-shader (shader/make-shader ::tex-shader tex-vertex-shader tex-fragment-shader))

;; Render functions

(defn- label! [vb x y text hori?]
  (let [[img-w img-h] img-dims
        [img-w-recip img-h-recip] img-dims-recip
        [r g b a] label-color
        bottom (if hori? y x)
        top (- bottom img-h)]
    (loop [cursor (if hori? x y)
           text text]
      (if-let [c (first text)]
        (let [cx (char->x c img-w)
              cw (char->w c)
              cursor' (if hori? (+ cursor cw) (- cursor cw))
              u0 (* cx img-w-recip)
              u1 (* (+ cx cw) img-w-recip)]
          (if hori?
            (do
              (color-uv-vtx-put! vb cursor bottom u0 0 r g b a)
              (color-uv-vtx-put! vb cursor' bottom u1 0 r g b a)
              (color-uv-vtx-put! vb cursor top u0 1 r g b a)
              (color-uv-vtx-put! vb cursor' bottom u1 0 r g b a)
              (color-uv-vtx-put! vb cursor' top u1 1 r g b a)
              (color-uv-vtx-put! vb cursor top u0 1 r g b a))
            (do
              (color-uv-vtx-put! vb bottom cursor u0 0 r g b a)
              (color-uv-vtx-put! vb bottom cursor' u1 0 r g b a)
              (color-uv-vtx-put! vb top cursor u0 1 r g b a)
              (color-uv-vtx-put! vb bottom cursor' u1 0 r g b a)
              (color-uv-vtx-put! vb top cursor' u1 1 r g b a)
              (color-uv-vtx-put! vb top cursor u0 1 r g b a)))
          (recur cursor' (rest text)))
        vb))))

(defn- quad! [vb x0 y0 x1 y1 r g b a]
  (color-uv-vtx-put! vb x0 y0 1 1 r g b a)
  (color-uv-vtx-put! vb x1 y0 1 1 r g b a)
  (color-uv-vtx-put! vb x0 y1 1 1 r g b a)
  (color-uv-vtx-put! vb x1 y0 1 1 r g b a)
  (color-uv-vtx-put! vb x1 y1 1 1 r g b a)
  (color-uv-vtx-put! vb x0 y1 1 1 r g b a))

(defn- line! [vb x0 y0 x1 y1 r g b a]
  (color-uv-vtx-put! vb x0 y0 1 1 r g b a)
  (color-uv-vtx-put! vb x1 y1 1 1 r g b a))

(defn render-rulers [^GL2 gl render-args renderables rcount]
  (doseq [renderable renderables
          :let [user-data (:user-data renderable)
                {:keys [vb tri-count line-count]} user-data]]
    (let [vertex-binding (vtx/use-with ::vertex-buffer vb tex-shader)]
      (gl/with-gl-bindings gl render-args [numbers-texture tex-shader vertex-binding]
        (shader/set-uniform tex-shader gl "texture" 0)
        (gl/gl-draw-arrays gl GL/GL_TRIANGLES 0 tri-count)
        (gl/gl-draw-arrays gl GL/GL_LINES tri-count line-count)))))

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

(defn- num-format [vals]
  (or (when (seq vals)
        (let [max-diff (- (reduce max vals) (reduce min vals))]
          (when (> max-diff 0)
            (let [mag (int (Math/floor (Math/log10 max-diff)))]
              (when (<= mag 0)
                (format "%%.%df" (inc (Math/abs mag))))))))
    "%.0f"))

(g/defnk produce-renderables [camera viewport cursor-pos vertex-buffer]
  (let [z (some-> camera
            c/camera-view-matrix
            (.getElement 2 2))]
    (if (and (not (types/empty-space? viewport)) z (= z 1.0))
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
           user-data (do
                       (vtx/clear! vertex-buffer)
                       (try
                         ;; backgrounds
                         (let [[r g b a] colors/scene-background]
                           (quad! vertex-buffer 0 0 width (:bottom viewport) r g b a)
                           (quad! vertex-buffer 0 (- (:bottom viewport) height) (:right viewport) (:bottom viewport) r g b a))
                         ;; x-labels
                         (let [y (- (:bottom viewport) (/ (- height (second img-dims)) 2))
                               ws (mapv first xs)
                               nf (num-format ws)]
                           (doseq [[x-w x-s] xs
                                   :let [text (String/format nil nf (to-array [x-w]))]]
                             (label! vertex-buffer (+ 2 x-s) y text true)))
                         ;; y-labels
                         (let [x width
                               ws (mapv first ys)
                               nf (num-format ws)]
                           (doseq [[y-w y-s] ys
                                   :let [text (String/format nil nf (to-array [y-w]))]]
                             (label! vertex-buffer (- x 2) (- y-s 1) text false)))
                         ;; This is unfortunate but should not have any severe consequence
                         (catch BufferOverflowException _))
                       ;; end of triangles; start of lines
                       (let [tri-count (count vertex-buffer)]
                         (try
                           (let [[r g b a] marker-color]
                             (let [x0 width
                                   y0 0
                                   x1 (:right viewport)
                                   y1 (- (:bottom viewport) height)]
                               ;; borders
                               (line! vertex-buffer x0 y0 x0 y1 r g b a)
                               (line! vertex-buffer x0 y1 x1 y1 r g b a)
                               ;; x tick markers
                               (doseq [[_ x-s] xs
                                       :let [y0 (:bottom viewport)]]
                                 (line! vertex-buffer x-s y0 x-s y1 r g b a))
                               ;; y tick markers
                               (doseq [[_ y-s] ys
                                       :let [y-s (inc y-s)]]
                                 (line! vertex-buffer 0 y-s width y-s r g b a))
                               ;; mouse markers
                               (when (and cursor-x (> cursor-x width))
                                 (let [y0 (:bottom viewport)]
                                   (apply line! vertex-buffer cursor-x y0 cursor-x y1 colors/scene-grid-x-axis)))
                               (when (and cursor-y (< cursor-y y1))
                                 (apply line! vertex-buffer 0 cursor-y width cursor-y colors/scene-grid-y-axis))))
                           ;; This is unfortunate but should not have any severe consequence
                           (catch BufferOverflowException _))
                         (vtx/flip! vertex-buffer)
                         {:vb vertex-buffer
                          :tri-count tri-count
                          :line-count (- (count vertex-buffer) tri-count)}))]
        {pass/overlay [{:render-fn render-rulers
                        :batch-key nil
                        :user-data user-data}]})
      {})))

(g/defnode Rulers
  (input camera Camera)
  (input viewport g/Any)
  (input cursor-pos types/Vec2)
  (output renderables pass/RenderData :cached produce-renderables)
  (output vertex-buffer g/Any :cached (g/fnk [] (->color-uv-vtx vertex-buffer-size))))
