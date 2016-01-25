(ns editor.background
  (:require [dynamo.graph :as g]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.colors :as colors]
            [editor.gl.pass :as p]
            [plumbing.core :refer [fnk]])
  (:import [javax.media.opengl GL2]))

(def grad-top-color    colors/mid-grey)
(def grad-bottom-color colors/bright-black)

(defn render-gradient [^GL2 gl pass renderables count]
  (let [x0           (float -1.0)
        x1           (float 1.0)
        y0           (float -1.0)
        y1           (float 1.0)]
    (gl/gl-quads gl
      (gl/gl-color grad-top-color)
      (gl/gl-vertex-2f x0 y1)
      (gl/gl-vertex-2f x1 y1)
      (gl/gl-color grad-bottom-color)
      (gl/gl-vertex-2f x1 y0)
      (gl/gl-vertex-2f x0 y0))))

(g/defnode Gradient
  (output renderable p/RenderData (fnk [this] {p/background [{:world-transform geom/Identity4d :render-fn render-gradient}]})))
