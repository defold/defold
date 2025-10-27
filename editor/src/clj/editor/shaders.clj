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
            [editor.graphics :as graphics]))

(set! *warn-on-reflection* true)
(set! *unchecked-math* :warn-on-boxed)

(def ^{:arglists '([editor-shader])} vertex-description ::vertex-description)

(defn- with-vertex-description [shader]
  (let [attribute-reflection-infos (:attribute-reflection-infos shader)
        vertex-description (graphics/make-vertex-description attribute-reflection-infos)]
    (assoc shader ::vertex-description vertex-description)))

(defn- editor-shader [opts & shader-paths]
  (with-vertex-description
    (apply shader/classpath-shader opts shader-paths)))

(defn- basic-shader-opts [page-mode coordinate-space]
  (let [transform-render-arg-key
        (case coordinate-space
          :coordinate-space-local :world-view-proj
          :coordinate-space-world :view-proj)

        uniform-values-by-name
        {"mtx_world_view_proj" transform-render-arg-key}

        max-page-count
        (case page-mode
          :unpaged 0
          :paged shader/max-array-samplers)]

    {:coordinate-space coordinate-space
     :max-page-count max-page-count
     :uniforms uniform-values-by-name}))

(def basic-color-local-space
  (editor-shader
    (basic-shader-opts :unpaged :coordinate-space-local)
    "shaders/basic-color.vp"
    "shaders/basic-color.fp"))

(def basic-color-world-space
  (editor-shader
    (basic-shader-opts :unpaged :coordinate-space-world)
    "shaders/basic-color.vp"
    "shaders/basic-color.fp"))

(def basic-selection-local-space
  (editor-shader
    (basic-shader-opts :unpaged :coordinate-space-local)
    "shaders/basic-selection.vp"
    "shaders/basic-selection.fp"))

(def basic-selection-world-space
  (editor-shader
    (basic-shader-opts :unpaged :coordinate-space-world)
    "shaders/basic-selection.vp"
    "shaders/basic-selection.fp"))

(def basic-selection-paged-local-space
  (editor-shader
    (basic-shader-opts :paged :coordinate-space-local)
    "shaders/basic-selection-paged.vp"
    "shaders/basic-selection-paged.fp"))

(def basic-selection-paged-world-space
  (editor-shader
    (basic-shader-opts :paged :coordinate-space-world)
    "shaders/basic-selection-paged.vp"
    "shaders/basic-selection-paged.fp"))

(def basic-texture-local-space
  (editor-shader
    (basic-shader-opts :unpaged :coordinate-space-local)
    "shaders/basic-texture.vp"
    "shaders/basic-texture.fp"))

(def basic-texture-world-space
  (editor-shader
    (basic-shader-opts :unpaged :coordinate-space-world)
    "shaders/basic-texture.vp"
    "shaders/basic-texture.fp"))

(def basic-texture-color-local-space
  (editor-shader
    (basic-shader-opts :unpaged :coordinate-space-local)
    "shaders/basic-texture-color.vp"
    "shaders/basic-texture-color.fp"))

(def basic-texture-color-world-space
  (editor-shader
    (basic-shader-opts :unpaged :coordinate-space-world)
    "shaders/basic-texture-color.vp"
    "shaders/basic-texture-color.fp"))

(def basic-texture-paged-local-space
  (editor-shader
    (basic-shader-opts :paged :coordinate-space-local)
    "shaders/basic-texture-paged.vp"
    "shaders/basic-texture-paged.fp"))

(def basic-texture-paged-world-space
  (editor-shader
    (basic-shader-opts :paged :coordinate-space-world)
    "shaders/basic-texture-paged.vp"
    "shaders/basic-texture-paged.fp"))

(def basic-texture-paged-color-local-space
  (editor-shader
    (basic-shader-opts :paged :coordinate-space-local)
    "shaders/basic-texture-paged-color.vp"
    "shaders/basic-texture-paged-color.fp"))

(def basic-texture-paged-color-world-space
  (editor-shader
    (basic-shader-opts :paged :coordinate-space-world)
    "shaders/basic-texture-paged-color.vp"
    "shaders/basic-texture-paged-color.fp"))

(def mesh-preview-local-space
  (editor-shader
    {:coordinate-space :coordinate-space-local
     :uniforms {"mtx_view" :view
                "mtx_proj" :projection}}
    "shaders/mesh-preview.vp"
    "shaders/mesh-preview.fp"))

(def mesh-selection-local-space
  (editor-shader
    {:coordinate-space :coordinate-space-local
     :uniforms {"mtx_view_proj" :view-proj}}
    "shaders/mesh-selection.vp"
    "shaders/mesh-selection.fp"))
