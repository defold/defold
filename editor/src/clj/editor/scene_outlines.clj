(ns editor.scene-outlines
  (:require [clojure.java.io :as io]
            [dynamo.graph :as g]
            [editor.camera :as c]
            [editor.colors :as colors]
            [editor.gl :as gl]
            [editor.gl.shader :as shader]
            [editor.gl.texture :as texture]
            [editor.gl.vertex2 :as vtx]
            [editor.gl.pass :as pass]
            [editor.image :as image]
            [editor.math :as math]
            [editor.types :as types])
  (:import  [com.jogamp.opengl GL GL2]
            [java.awt.image BufferedImage]
            [javax.vecmath Matrix4d Point3d]
            [editor.types Camera]))

(set! *warn-on-reflection* true)

(def color colors/bright-grey)
(def selected-color colors/defold-turquoise)

(def aabb-vertex-count 24) ;; Worst-case wire-frame box
(def initial-vb-capacity (* aabb-vertex-count 100)) ;; Random number to start with

;; Shaders

(vtx/defvertex color-vtx
  (vec3 position)
  (vec4 color))

(shader/defshader line-vertex-shader
  (attribute vec4 position)
  (attribute vec4 color)
  (varying vec4 var_color)
  (defn void main []
    (setq var_color color)
    (setq gl_Position (* gl_ModelViewProjectionMatrix position))))

(shader/defshader line-fragment-shader
  (varying vec4 var_color)
  (defn void main []
    (setq gl_FragColor var_color)))

(def line-shader (shader/make-shader ::line-shader line-vertex-shader line-fragment-shader))

;; Render functions

(defn render-lines [^GL2 gl render-args renderables rcount]
  (doseq [renderable renderables
          :let [user-data (:user-data renderable)
                vb (:vb user-data)]]
    (let [vertex-binding (vtx/use-with ::lines vb line-shader)]
      (gl/with-gl-bindings gl render-args [line-shader vertex-binding]
        (gl/gl-draw-arrays gl GL/GL_LINES 0 (count vb))))))

;; Vertex buffer functions

(defn- put-aabb! [vb x0 y0 z0 x1 y1 z1 r g b a]
  ;; front face

  (color-vtx-put! vb x0 y0 z0 r g b a)
  (color-vtx-put! vb x1 y0 z0 r g b a)
  
  (color-vtx-put! vb x1 y0 z0 r g b a)
  (color-vtx-put! vb x1 y1 z0 r g b a)
  
  (color-vtx-put! vb x1 y1 z0 r g b a)
  (color-vtx-put! vb x0 y1 z0 r g b a)
  
  (color-vtx-put! vb x0 y1 z0 r g b a)
  (color-vtx-put! vb x0 y0 z0 r g b a)
  
  (let [z-diff (Math/abs (double (- z1 z0)))]
    (when (> z-diff 0.001)
      ;; back face

      (color-vtx-put! vb x0 y0 z1 r g b a)
      (color-vtx-put! vb x1 y0 z1 r g b a)
      
      (color-vtx-put! vb x1 y0 z1 r g b a)
      (color-vtx-put! vb x1 y1 z1 r g b a)
      
      (color-vtx-put! vb x1 y1 z1 r g b a)
      (color-vtx-put! vb x0 y1 z1 r g b a)
      
      (color-vtx-put! vb x0 y1 z1 r g b a)
      (color-vtx-put! vb x0 y0 z1 r g b a)

      ;; side connectors
      (color-vtx-put! vb x0 y0 z0 r g b a)
      (color-vtx-put! vb x0 y0 z1 r g b a)
      
      (color-vtx-put! vb x1 y0 z0 r g b a)
      (color-vtx-put! vb x1 y0 z1 r g b a)
      
      (color-vtx-put! vb x1 y1 z0 r g b a)
      (color-vtx-put! vb x1 y1 z1 r g b a)
      
      (color-vtx-put! vb x0 y1 z0 r g b a)
      (color-vtx-put! vb x0 y1 z1 r g b a)))
  vb)

(defn- put-renderable-fn [^Point3d p r g b a]
  (fn [vb ren]
    (if-let [ol (:outline ren)]
      (let [^Matrix4d wt (:world-transform ren)]
        (reduce (fn [vb [x y z]]
                  (do
                    (.set p x y z)
                    (.transform wt p)
                    (color-vtx-put! vb (+ 200 (.x p)) (.y p) (.z p) r g b a)))
          vb ol))
      (let [aabb (:aabb ren)
            min (types/min-p aabb)
            x0 (+ 200 (.x min))
            y0 (.y min)
            z0 (.z min)
            max (types/max-p aabb)
            x1 (+ 200 (.x max))
            y1 (.y max)
            z1 (.z max)]
        (put-aabb! vb x0 y0 z0 x1 y1 z1 r g b a)))))

(defn- put-renderables! [vb xf filter-fn [r g b a] renderables]
  (let [p (Point3d.)]
    (transduce (comp xf (filter filter-fn))
      (completing (put-renderable-fn p r g b a))
      vb renderables)))

(defn- ->vb
  ([capacity]
    (->color-vtx initial-vb-capacity))
  ([vb capacity]
    (if (> (vtx/capacity vb) capacity)
      vb
      (->vb initial-vb-capacity))))

(defn- renderable->vcount [r]
  (if-let [ol (:outline r)]
    (count ol)
    aabb-vertex-count))

(g/defnk produce-renderables [vertex-buffer scene-renderables selection]
  (let [sel? (fn [r] (if-let [np (:node-path r)]
                       (boolean (some selection np))
                       (contains? selection (:node-id r))))
        outline-xf (comp
                     ;; filter render passes
                     (keep (fn [[pass rs]] (when (and (types/model-transform? pass) (not (types/selection? pass))) rs)))
                     ;; concat renderables
                     cat
                     ;; filter for either outline or aabb
                     (filter (fn [r] (or (:outline r) (:aabb r)))))
        vtx-count-xf (comp outline-xf (map renderable->vcount))
        vb-capacity (transduce vtx-count-xf
                      + scene-renderables)
        vb (swap! vertex-buffer ->vb vb-capacity)
        [sel non-sel] [(into [] (comp outline-xf (filter sel?)) scene-renderables)
                       (into [] (comp outline-xf (filter (complement sel?))) scene-renderables)]]
    (vtx/clear! vb)
    (put-renderables! vb outline-xf (complement sel?) color scene-renderables)
    (put-renderables! vb outline-xf sel? selected-color scene-renderables)
    (vtx/flip! vb)
    {pass/outline [{:render-fn render-lines
                    :batch-key nil
                    :user-data {:vb vb}}]
     pass/selection (let [put! (put-renderable-fn (Point3d.) 1.0 1.0 1.0 1.0)]
                      (into [] (comp outline-xf
                                 (map (fn [r]
                                        (prn (keys r))
                                        (let [c (if (sel? r)
                                                  selected-color
                                                  color)]
                                          (assoc r
                                            :render-fn render-lines
                                            :user-data {:vb (-> (->vb (renderable->vcount r))
                                                              (put! r)
                                                              (vtx/flip!))})))))
                        scene-renderables))}))

(g/defnode Outlines
  (input selection g/Any)
  (output selection g/Any :cached (g/fnk [selection] (set selection)))
  (input scene-renderables g/Any)
  (output renderables pass/RenderData :cached produce-renderables)
  (output vertex-buffer g/Any :cached (g/fnk [] (atom (->vb initial-vb-capacity)))))
