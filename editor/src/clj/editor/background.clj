;; Copyright 2020-2026 The Defold Foundation
;; Copyright 2014-2020 King
;; Copyright 2009-2014 Ragnar Svensson, Christian Murray
;; Licensed under the Defold License version 1.0 (the "License"); you may not use
;; this file except in compliance with the License.
;;
;; You may obtain a copy of the License, together with FAQs at
;; https://www.defold.com/license
;;
;; Unless required by applicable law or agreed to in writing, software distributed
;; under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
;; CONDITIONS OF ANY KIND, either express or implied. See the License for the
;; specific language governing permissions and limitations under the License.

(ns editor.background
  (:require [dynamo.graph :as g]
            [editor.colors :as colors]
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.gl.pass :as pass]
            [editor.gl.vertex2 :as vtx]
            [editor.shaders :as shaders])
  (:import [com.jogamp.opengl GL2]
           [editor.types Region]))

(set! *warn-on-reflection* true)

(vtx/defvertex bg-color-vtx
  (vec3 position)
  (vec4 color))

(defn render-background [^GL2 gl render-args renderables count]
  (let [viewport ^Region (:viewport render-args)
        x0 (.left viewport)
        x1 (.right viewport)
        y0 (.top viewport)
        y1 (.bottom viewport)
        [cr cg cb ca] colors/scene-background
        vbuf (-> (->bg-color-vtx 4)
                 (bg-color-vtx-put! x0 y1 0 cr cg cb ca)
                 (bg-color-vtx-put! x1 y1 0 cr cg cb ca)
                 (bg-color-vtx-put! x1 y0 0 cr cg cb ca)
                 (bg-color-vtx-put! x0 y0 0 cr cg cb ca)
                 (vtx/flip!))
        vb (vtx/use-with ::background vbuf shaders/basic-color-world-space)]
    (gl/with-gl-bindings gl render-args [shaders/basic-color-world-space vb]
      (gl/gl-draw-arrays gl GL2/GL_TRIANGLE_FAN 0 (count vbuf)))))

(g/defnode Background
  (output renderable pass/RenderData (g/fnk [] {pass/background [{:world-transform geom/Identity4d :render-fn render-background}]})))
