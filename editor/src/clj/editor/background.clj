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
            [editor.geom :as geom]
            [editor.gl :as gl]
            [editor.colors :as colors]
            [editor.gl.pass :as pass])
  (:import [com.jogamp.opengl GL2]
           [editor.types Region]))

(set! *warn-on-reflection* true)

(defn render-background [^GL2 gl render-args renderables count]
  (let [viewport ^Region (:viewport render-args)
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
