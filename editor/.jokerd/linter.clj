;; Copyright 2020-2022 The Defold Foundation
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

(ns benchmark.graph-benchmark)
(def world)

(ns clojure.set)
(def map-invert)

(ns com.sun.javafx.css.StyleManager)
(def getInstance)

(ns editor.atlas)
(def ->texture-vtx)
(def pos-uv-frag)
(def pos-uv-vert)

(ns editor.code.data)
(def dest-buffer)
(def dest-offset)
(def length)

(ns editor.model-scene)
(def ->vtx-pos-nrm-tex)
(def model-id-fragment-shader)
(def model-id-vertex-shader)
(def shader-frag-pos-nrm-tex)
(def shader-ver-pos-nrm-tex)

(ns editor.collision-object)
(def ->color-vtx)
(def fragment-shader)
(def shape-id-fragment-shader)
(def shape-id-vertex-shader)
(def vertex-shader)

(ns editor.cubemap)
(def ->normal-vtx)
(def pos-norm-frag)
(def pos-norm-vert)

(ns editor.curve-view)
(def ->color-vtx)
(def line-fragment-shader)
(def line-vertex-shader)

(ns editor.font)
(def ->DefoldVertex)
(def ->DFVertex)

(ns editor.gl)
(def gl-context)

(ns editor.gl.pass)
(def render-passes)
(def selection-passes)

(ns editor.gl.shader)
(defmacro defshader [name & body])

(ns editor.gui)
(def ->color-vtx)
(def ->color-vtx-vb)
(def ->uv-color-vtx)
(def color-vtx-put!)
(def fragment-shader)
(def gui-id-fragment-shader)
(def gui-id-vertex-shader)
(def line-fragment-shader)
(def line-vertex-shader)
(def uv-color-vtx-put!)
(def vertex-shader)

(ns editor.label)
(def ->color-vtx)
(def color-vtx-put!)
(def fragment-shader)
(def label-id-fragment-shader)
(def label-id-vertex-shader)
(def line-fragment-shader)
(def line-vertex-shader)
(def vertex-shader)

(ns editor.particlefx)
(def ->color-vtx)
(def line-fragment-shader)
(def line-id-fragment-shader)
(def line-id-vertex-shader)
(def line-vertex-shader)

(ns editor.render)
(def ->vtx-pos-col)
(def shader-frag-outline)
(def shader-frag-tex-tint)
(def shader-ver-outline)
(def shader-ver-tex-col)

(ns editor.rulers)
(def ->color-uv-vtx)
(def color-uv-vtx-put!)
(def tex-fragment-shader)
(def tex-vertex-shader)

(ns editor.scene-tools)
(def ->pos-vtx)
(def fragment-shader)
(def vertex-shader)

(ns editor.sprite)
(def ->color-vtx)
(def ->texture-vtx)
(def fragment-shader)
(def outline-fragment-shader)
(def outline-vertex-shader)
(def sprite-id-fragment-shader)
(def sprite-id-vertex-shader)
(def vertex-shader)

(ns editor.spine)
(def spine-id-fragment-shader)
(def spine-id-vertex-shader)

(ns editor.system)
(def defold-engine-sha1)

(ns editor.tile-map)
(def ->color-vtx)
(def ->pos-uv-vtx)
(def color-vtx-put!)
(def pos-color-frag)
(def pos-color-vert)
(def pos-uv-frag)
(def pos-uv-vert)
(def pos-uv-vtx-put!)
(def tile-map-id-fragment-shader)
(def tile-map-id-vertex-shader)

(ns editor.tile-source)
(def ->pos-color-vtx)
(def ->pos-uv-vtx)
(def pos-color-frag)
(def pos-color-vert)
(def pos-uv-frag)
(def pos-uv-vert)

(ns editor.ui)
(def now)

(ns javafx.application.Platform)
(def exit)

(ns javafx.scene.Cursor)
(def DEFAULT)
(def DISAPPEAR)
(def HAND)
(def TEXT)

(ns Thread$State)
(def TERMINATED)

(ns joker.core)
(def clojure.core.protocols.CollReduce)
(def clojure.core.protocols.IKVReduce)
(def editor.gl.vertex2.VertexBuffer)
(def internal.node.NodeImpl)
(def internal.node.NodeTypeRef)
(def internal.node.ValueTypeRef)
(def javafx.scene.control.Tab)
(def javafx.scene.input.KeyCharacterCombination)
(def javafx.scene.input.KeyCodeCombination)
(def javafx.stage.Stage)
(def javafx.stage.Window)
(def org.eclipse.jgit.transport.UsernamePasswordCredentialsProvider)
(def schema.core.Predicate)
(def schema.utils.ValidationError)
(defmacro defproject [& _])
(defmacro deftest [& shut-up])
(defmacro is [& quiet-please])
(defmacro testing [& yawn])
(defmacro use-fixtures [& _])

(ns util.profiler-test)
(def threads)


