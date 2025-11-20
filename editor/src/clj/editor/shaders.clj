;; Copyright 2020-2025 The Defold Foundation
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

(ns editor.shaders
  (:require [editor.gl.shader :as shader]
            [editor.graphics.types :as graphics.types]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(defn vertex-description [editor-shader]
  {:post [(graphics.types/vertex-description? %)]}
  (::vertex-description editor-shader))

(defn- with-vertex-description [shader]
  (let [attribute-reflection-infos (shader/attribute-reflection-infos shader nil)
        vertex-description (graphics.types/make-vertex-description attribute-reflection-infos)]
    (assoc shader ::vertex-description vertex-description)))

(defn- editor-shader [opts & shader-paths]
  (with-vertex-description
    (apply shader/classpath-shader opts shader-paths)))

(def basic-color-local-space
  (editor-shader
    {:coordinate-space :coordinate-space-local
     :uniforms {"mtx_world_view_proj" :world-view-proj}}
    "shaders/basic-color.vp"
    "shaders/basic-color.fp"))

(def basic-color-world-space
  (editor-shader
    {:coordinate-space :coordinate-space-world
     :uniforms {"mtx_world_view_proj" :view-proj}}
    "shaders/basic-color.vp"
    "shaders/basic-color.fp"))

(def basic-texture-local-space
  (editor-shader
    {:coordinate-space :coordinate-space-local
     :uniforms {"mtx_world_view_proj" :world-view-proj}}
    "shaders/basic-texture.vp"
    "shaders/basic-texture.fp"))

(def basic-texture-world-space
  (editor-shader
    {:coordinate-space :coordinate-space-world
     :uniforms {"mtx_world_view_proj" :view-proj}}
    "shaders/basic-texture.vp"
    "shaders/basic-texture.fp"))

(def basic-texture-color-local-space
  (editor-shader
    {:coordinate-space :coordinate-space-local
     :uniforms {"mtx_world_view_proj" :world-view-proj}}
    "shaders/basic-texture-color.vp"
    "shaders/basic-texture-color.fp"))

(def basic-texture-color-world-space
  (editor-shader
    {:coordinate-space :coordinate-space-world
     :uniforms {"mtx_world_view_proj" :view-proj}}
    "shaders/basic-texture-color.vp"
    "shaders/basic-texture-color.fp"))

(def basic-texture-paged-local-space
  (editor-shader
    {:coordinate-space :coordinate-space-local
     :max-page-count shader/max-array-samplers
     :uniforms {"mtx_world_view_proj" :world-view-proj}}
    "shaders/basic-texture-paged.vp"
    "shaders/basic-texture-paged.fp"))

(def basic-texture-paged-world-space
  (editor-shader
    {:coordinate-space :coordinate-space-world
     :max-page-count shader/max-array-samplers
     :uniforms {"mtx_world_view_proj" :view-proj}}
    "shaders/basic-texture-paged.vp"
    "shaders/basic-texture-paged.fp"))

(def basic-texture-paged-color-local-space
  (editor-shader
    {:coordinate-space :coordinate-space-local
     :max-page-count shader/max-array-samplers
     :uniforms {"mtx_world_view_proj" :world-view-proj}}
    "shaders/basic-texture-paged-color.vp"
    "shaders/basic-texture-paged-color.fp"))

(def basic-texture-paged-color-world-space
  (editor-shader
    {:coordinate-space :coordinate-space-world
     :max-page-count shader/max-array-samplers
     :uniforms {"mtx_world_view_proj" :view-proj}}
    "shaders/basic-texture-paged-color.vp"
    "shaders/basic-texture-paged-color.fp"))

(def mesh-preview-local-space
  (editor-shader
    {:coordinate-space :coordinate-space-local
     :uniforms {"mtx_view" :view
                "mtx_proj" :projection}}
    "shaders/mesh-preview.vp"
    "shaders/mesh-preview.fp"))

(def selection-attribute-local-space
  (editor-shader
    {:coordinate-space :coordinate-space-local
     :uniforms {"mtx_world_view_proj" :world-view-proj}}
    "shaders/selection-attribute.vp"
    "shaders/selection-attribute.fp"))

(def selection-attribute-world-space
  (editor-shader
    {:coordinate-space :coordinate-space-world
     :uniforms {"mtx_world_view_proj" :view-proj}}
    "shaders/selection-attribute.vp"
    "shaders/selection-attribute.fp"))

(def selection-attribute-paged-local-space
  (editor-shader
    {:coordinate-space :coordinate-space-local
     :max-page-count shader/max-array-samplers
     :uniforms {"mtx_world_view_proj" :world-view-proj}}
    "shaders/selection-attribute-paged.vp"
    "shaders/selection-attribute-paged.fp"))

(def selection-attribute-paged-world-space
  (editor-shader
    {:coordinate-space :coordinate-space-world
     :max-page-count shader/max-array-samplers
     :uniforms {"mtx_world_view_proj" :view-proj}}
    "shaders/selection-attribute-paged.vp"
    "shaders/selection-attribute-paged.fp"))

(def selection-instance-local-space
  (editor-shader
    {:coordinate-space :coordinate-space-local
     :uniforms {"mtx_view_proj" :view-proj}}
    "shaders/selection-instance.vp"
    "shaders/selection-instance.fp"))

(def selection-instance-paged-local-space
  (editor-shader
    {:coordinate-space :coordinate-space-local
     :max-page-count shader/max-array-samplers
     :uniforms {"mtx_view_proj" :view-proj}}
    "shaders/selection-instance-paged.vp"
    "shaders/selection-instance-paged.fp"))

(def selection-uniform-local-space
  (editor-shader
    {:coordinate-space :coordinate-space-local
     :uniforms {"mtx_world_view_proj" :world-view-proj
                "id_color" :id-color}}
    "shaders/selection-uniform.vp"
    "shaders/selection-uniform.fp"))

(def selection-uniform-world-space
  (editor-shader
    {:coordinate-space :coordinate-space-world
     :uniforms {"mtx_world_view_proj" :view-proj
                "id_color" :id-color}}
    "shaders/selection-uniform.vp"
    "shaders/selection-uniform.fp"))

(def selection-uniform-paged-local-space
  (editor-shader
    {:coordinate-space :coordinate-space-local
     :max-page-count shader/max-array-samplers
     :uniforms {"mtx_world_view_proj" :world-view-proj
                "id_color" :id-color}}
    "shaders/selection-uniform-paged.vp"
    "shaders/selection-uniform-paged.fp"))

(def selection-uniform-paged-world-space
  (editor-shader
    {:coordinate-space :coordinate-space-world
     :max-page-count shader/max-array-samplers
     :uniforms {"mtx_world_view_proj" :view-proj
                "id_color" :id-color}}
    "shaders/selection-uniform-paged.vp"
    "shaders/selection-uniform-paged.fp"))
