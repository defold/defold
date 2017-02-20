(ns editor.background
  (:require [dynamo.graph :as g]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.colors :as colors]
            [editor.gl.pass :as p])
  (:import [com.jogamp.opengl GL2]))

(set! *warn-on-reflection* true)

(defn render-background [^GL2 gl pass renderables count]
  (let [x0           (float -1.0)
        x1           (float 1.0)
        y0           (float -1.0)
        y1           (float 1.0)]
    (gl/gl-quads gl
      (gl/gl-color colors/scene-background)
      (gl/gl-vertex-2f x0 y1)
      (gl/gl-vertex-2f x1 y1)
      (gl/gl-vertex-2f x1 y0)
      (gl/gl-vertex-2f x0 y0))))

(g/defnode Background
  (output renderable p/RenderData (g/fnk [] {p/background [{:world-transform geom/Identity4d :render-fn render-background}]})))
