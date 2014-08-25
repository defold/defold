(ns internal.ui.background
  (:require [plumbing.core :refer [defnk]]
            [dynamo.geom :as g]
            [dynamo.node :as n]
            [dynamo.types :as t]
            [internal.ui.gl :refer :all]
            [internal.render.pass :as p]))

(def bg-top-color (color 123 143 167))
(def bg-bottom-color (color 28 29 31))

(defnk background-renderable :- t/RenderData
  [this]
  {p/background
   [{:world-transform g/Identity4d
     :render-fn
     (fn [context gl glu]
       (let [x0           (float -1.0)
             x1           (float 1.0)
             y0           (float -1.0)
             y1           (float 1.0)]
         (gl-quads gl
                   (gl-color-3fv bg-top-color 0)
                   (gl-vertex-2f x0 y1)
                   (gl-vertex-2f x1 y1)
                   (gl-color-3fv bg-bottom-color 0)
                   (gl-vertex-2f x1 y0)
                   (gl-vertex-2f x0 y0))))}]})

(n/defnode Background
  {:transforms {:renderable #'background-renderable}})
