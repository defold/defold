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

(def ^{:arglists '([editor-shader])} vertex-description ::vertex-description)

(defn- with-vertex-description [shader]
  (let [attribute-reflection-infos (shader/attribute-reflection-infos shader nil)
        vertex-description (graphics.types/make-vertex-description attribute-reflection-infos)]
    (assoc shader ::vertex-description vertex-description)))

(defn- editor-shader [opts & shader-paths]
  (with-vertex-description
    (apply shader/classpath-shader opts shader-paths)))

(def attribute-selection-local-space
  (editor-shader
    {:coordinate-space :coordinate-space-local
     :uniforms {"mtx_world_view_proj" :world-view-proj}}
    "shaders/attribute-selection.vp"
    "shaders/attribute-selection.fp"))

(def attribute-selection-world-space
  (editor-shader
    {:coordinate-space :coordinate-space-world
     :uniforms {"mtx_world_view_proj" :view-proj}}
    "shaders/attribute-selection.vp"
    "shaders/attribute-selection.fp"))

(def attribute-selection-paged-local-space
  (editor-shader
    {:coordinate-space :coordinate-space-local
     :max-page-count shader/max-array-samplers
     :uniforms {"mtx_world_view_proj" :world-view-proj}}
    "shaders/attribute-selection-paged.vp"
    "shaders/attribute-selection-paged.fp"))

(def attribute-selection-paged-world-space
  (editor-shader
    {:coordinate-space :coordinate-space-world
     :max-page-count shader/max-array-samplers
     :uniforms {"mtx_world_view_proj" :view-proj}}
    "shaders/attribute-selection-paged.vp"
    "shaders/attribute-selection-paged.fp"))

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

(def instance-selection-local-space
  (editor-shader
    {:coordinate-space :coordinate-space-local
     :uniforms {"mtx_view_proj" :view-proj}}
    "shaders/instance-selection.vp"
    "shaders/instance-selection.fp"))

(def instance-selection-paged-local-space
  (editor-shader
    {:coordinate-space :coordinate-space-local
     :uniforms {"mtx_view_proj" :view-proj}}
    "shaders/instance-selection-paged.vp"
    "shaders/instance-selection-paged.fp"))

(def mesh-preview-local-space
  (editor-shader
    {:coordinate-space :coordinate-space-local
     :uniforms {"mtx_view" :view
                "mtx_proj" :projection}}
    "shaders/mesh-preview.vp"
    "shaders/mesh-preview.fp"))

(def uniform-selection-local-space
  (editor-shader
    {:coordinate-space :coordinate-space-local
     :uniforms {"mtx_world_view_proj" :world-view-proj
                "id_color" :id-color}}
    "shaders/uniform-selection.vp"
    "shaders/uniform-selection.fp"))

(def uniform-selection-world-space
  (editor-shader
    {:coordinate-space :coordinate-space-world
     :uniforms {"mtx_world_view_proj" :view-proj
                "id_color" :id-color}}
    "shaders/uniform-selection.vp"
    "shaders/uniform-selection.fp"))

(def uniform-selection-paged-local-space
  (editor-shader
    {:coordinate-space :coordinate-space-local
     :max-page-count shader/max-array-samplers
     :uniforms {"mtx_world_view_proj" :world-view-proj
                "id_color" :id-color}}
    "shaders/uniform-selection-paged.vp"
    "shaders/uniform-selection-paged.fp"))

(def uniform-selection-paged-world-space
  (editor-shader
    {:coordinate-space :coordinate-space-world
     :max-page-count shader/max-array-samplers
     :uniforms {"mtx_world_view_proj" :view-proj
                "id_color" :id-color}}
    "shaders/uniform-selection-paged.vp"
    "shaders/uniform-selection-paged.fp"))
