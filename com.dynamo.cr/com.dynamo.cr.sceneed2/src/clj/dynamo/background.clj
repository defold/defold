(ns dynamo.background
  (:require [plumbing.core :refer [fnk]]
            [dynamo.geom :as g]
            [dynamo.node :as n]
            [dynamo.types :as t]
            [dynamo.gl :as gl]
            [internal.render.pass :as p])
  (:import [javax.media.opengl GL2]))

(def grad-top-color    (gl/color 123 143 167))
(def grad-bottom-color (gl/color  28  29  31))

(defn render-gradient
  [context ^GL2 gl glu text-renderer]
  (let [x0           (float -1.0)
        x1           (float 1.0)
        y0           (float -1.0)
        y1           (float 1.0)]
    (gl/gl-quads gl
      (gl/gl-color-3fv grad-top-color 0)
      (gl/gl-vertex-2f x0 y1)
      (gl/gl-vertex-2f x1 y1)
      (gl/gl-color-3fv grad-bottom-color 0)
      (gl/gl-vertex-2f x1 y0)
      (gl/gl-vertex-2f x0 y0))))

(n/defnode Gradient
  (output renderable t/RenderData (fnk [this] {p/background [{:world-transform g/Identity4d :render-fn #'render-gradient}]})))
