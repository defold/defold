(ns editor.background
  (:require [dynamo.graph :as g]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.colors :as colors]
            [editor.gl.pass :as pass])
  (:import [com.jogamp.opengl GL2]))

(set! *warn-on-reflection* true)

(defn render-background [^GL2 gl render-args renderables count]
  (let [viewport (:viewport render-args)
        x0 (.left viewport)
        x1 (.right viewport)
        y0 (.top viewport)
        y1 (.bottom viewport)]
    (gl/gl-quads gl
      (gl/gl-color colors/scene-background)
      (gl/gl-vertex-2f x0 y1)
      (gl/gl-vertex-2f x1 y1)
      (gl/gl-vertex-2f x1 y0)
      (gl/gl-vertex-2f x0 y0))))

(g/defnode Background
  (output renderable pass/RenderData (g/fnk [] {pass/background [{:world-transform geom/Identity4d :render-fn render-background}]})))
